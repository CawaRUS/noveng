#include "engine.hpp"
#include "common.hpp"
#include "logger.hpp"
#include "setting.hpp"
#include "localisation.hpp"
#include "command.hpp"
#include "crypto_wrapper.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

std::map<std::string, std::vector<char>> NovelEngine::fileCache;
#include <conio.h>
#include <map>
#include <sstream>
#include <memory>
#include <algorithm>
#include <ctime>
#include <filesystem>


namespace fs = std::filesystem;

int safeStoi(const std::string& str, int defaultVal = 0) {
    try { return std::stoi(str); } catch (...) { return defaultVal; }
}

void NovelEngine::render() {
    std::cout << "\033[2J\033[H"; 
    
    for(int y = 0; y < offsetY; ++y) std::cout << "\n";

    for (const auto& entry : history) {
        std::cout << "\n" << entry.color << ">>> " << entry.speaker << " <<<" << CLR_RESET << std::endl;
        
        if (&entry == &history.back()) {
            for(int x = 0; x < offsetX; ++x) std::cout << " ";
        }
        
        std::cout << CLR_TEXT << entry.text << CLR_RESET << std::endl;
    }
    std::cout << std::flush;
}
NovelEngine::NovelEngine() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    Logger::getInstance().info("Initializing Audio Engine (miniaudio)...");

    if (ma_engine_init(NULL, &audio) != MA_SUCCESS) {
        Logger::getInstance().error("CRITICAL: Failed to initialize audio engine!");
        std::cerr << LocalizationManager::getInstance().get("audio_error") << std::endl;
    }
}

NovelEngine::~NovelEngine() {
    Logger::getInstance().info("Shutting down NovelEngine...");
    stopAudio();
    ma_engine_uninit(&audio);
}

std::string trim(std::string s) {
    s.erase(0, s.find_first_not_of(" \t\n\r"));
    s.erase(s.find_last_not_of(" \t\n\r") + 1);
    return s;
}

void NovelEngine::registerCommands() {
    Logger::getInstance().info("Auto-registering commands from Factory...");
    auto& factoryCreators = CommandFactory::getInstance().getCreators();
    
    for (auto const& [name, creator] : factoryCreators) {
        commandRegistry[trim(name)] = creator();
        Logger::getInstance().debug("Command registered: " + name);
    }
}

void NovelEngine::executeCommand(const std::string& cmd) {
    if (cmd.empty()) return;

    size_t colonPos = cmd.find(':');
    std::string action = (colonPos != std::string::npos) ? cmd.substr(0, colonPos) : cmd;

    action.erase(0, action.find_first_not_of(" \t\n\r"));
    action.erase(action.find_last_not_of(" \t\n\r") + 1);

    Logger::getInstance().debug("executeCommand raw='" + cmd + "' action='" + action + "'");

    std::string rawArgs = (colonPos != std::string::npos) ? cmd.substr(colonPos + 1) : "";
    std::vector<std::string> args = split(rawArgs, '|');

    if (commandRegistry.count(action)) {
        commandRegistry[action]->execute(this, args);
    } else {
        Logger::getInstance().warn("Unknown command: '" + action + "'");
    }
}

void NovelEngine::saveGame(int slot) {
    Logger::getInstance().info("Saving game to slot " + std::to_string(slot) + "...");
    fs::path saveDir = fs::path(DIR_RES) / DIR_SAVE;
    if (!fs::exists(saveDir)) fs::create_directories(saveDir);

    GameState state;
    state.currentScene = currentChapterFile;
    state.currentMusic = this->currentMusicFile;
    state.characterColors = this->characterColors;
    state.characterPitches = this->characterPitches;
    state.eventIndex = currentEventIdx;
    state.variables = this->variables;

    fs::path savePath = saveDir / ("save" + std::to_string(slot) + ".json");
    std::ofstream file(savePath);
    if (file.is_open()) {
        json j = state;
        file << j.dump(4);
        std::cout << "\n" << LocalizationManager::getInstance().get("save_success") << " [" << slot << "]" << std::endl;
    }
}

bool NovelEngine::loadGame(int slot) {
    fs::path savePath = fs::path(DIR_RES) / DIR_SAVE / ("save" + std::to_string(slot) + ".json");
    if (!fs::exists(savePath)) return false;

    try {
        std::ifstream file(savePath);
        json j; file >> j;
        GameState state = j.get<GameState>();

        this->nextChapterFile = state.currentScene;
        this->characterColors = state.characterColors;
        this->characterPitches = state.characterPitches;
        this->variables = state.variables;

        this->currentEventIdx = state.eventIndex;
        Logger::getInstance().info("Loaded eventIndex: " + std::to_string(state.eventIndex));

        stopAudio();

        if (!state.currentMusic.empty()) {
            this->currentMusicFile = state.currentMusic;
        }

        this->chapterFinished = true;
        return true;
    } catch (...) { return false; }
}

void NovelEngine::applySettings() {
    auto& cfg = SettingsManager::getInstance().get();
    ma_engine_set_volume(&audio, cfg.musicVolume);
}

void NovelEngine::stopAudio() {
    std::lock_guard<std::mutex> lock(soundsMutex);
    for (auto& s : activeSounds) {
        if (s->initialized) {
            ma_sound_stop(&s->sound);
        }
    }
    activeSounds.clear();
}

void NovelEngine::cleanupSounds() {
    std::lock_guard<std::mutex> lock(soundsMutex);
    activeSounds.erase(
        std::remove_if(activeSounds.begin(), activeSounds.end(),
            [](const std::unique_ptr<ActiveSound>& s) {
                if (s->initialized && !ma_sound_is_playing(&s->sound)) {
                    return true;
                }
                return false;
            }),
        activeSounds.end()
    );
}

void NovelEngine::playSFX(const std::string& filename, float pitch) {
    {
        std::lock_guard<std::mutex> lock(soundsMutex);
        if (activeSounds.size() > 20) {
            soundsMutex.unlock();
            cleanupSounds();
            soundsMutex.lock();
        }
    }

    std::filesystem::path path = std::filesystem::path(DIR_RES) / DIR_SFX / filename;
    auto data = readFile(path.string());
    if (data.empty()) {
        Logger::getInstance().error("SFX file not found: " + path.string());
        return;
    }

    auto pActiveSound = std::make_unique<ActiveSound>();
    pActiveSound->data = std::move(data);

    ma_result res = ma_decoder_init_memory(pActiveSound->data.data(), pActiveSound->data.size(), NULL, &pActiveSound->decoder);
    if (res != MA_SUCCESS) {
        Logger::getInstance().error("Failed to decode SFX memory: " + filename);
        return;
    }

    res = ma_sound_init_from_data_source(&audio, &pActiveSound->decoder, MA_SOUND_FLAG_DECODE, NULL, &pActiveSound->sound);

    if (res == MA_SUCCESS) {
        pActiveSound->initialized = true;
        ma_sound_set_pitch(&pActiveSound->sound, pitch);
        ma_sound_start(&pActiveSound->sound);

        std::lock_guard<std::mutex> lock(soundsMutex);
        activeSounds.push_back(std::move(pActiveSound));
    } else {
        Logger::getInstance().error("Failed to init sound source: " + filename);
        ma_decoder_uninit(&pActiveSound->decoder);
    }
}

void NovelEngine::typeText(const std::string& text, int speedMs) {
    int finalSpeed = SettingsManager::getInstance().get().typingSpeed;
    float currentPitch = characterPitches.count(currentSpeaker) ? characterPitches[currentSpeaker] : 1.0f;

    for(int x = 0; x < offsetX; ++x) std::cout << " ";

    bool paused = false;
    for (size_t i = 0; i < text.length(); ++i) {
        // Проверяем паузу ПЕРЕД выводом символа
        while (paused) {
            if (_kbhit()) {
                int ch = _getch();
                if (ch == 32) { // Пробел - снять паузу
                    paused = false;
                    break;
                } else if (ch == 27) { // ESC - выход в меню
                    if (i < text.length()) std::cout << text.substr(i);
                    std::cout << std::endl;
                    throw std::runtime_error("ESC_TO_MENU");
                } else if (ch == 13) { // Enter - пропустить
                    if (i < text.length()) std::cout << text.substr(i);
                    std::cout << std::endl;
                    return;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::cout << text[i] << std::flush;
        if (!std::isspace(static_cast<unsigned char>(text[i])) && (i % 2 == 0)) {
            playSFX("type.mp3", currentPitch);
        }
        if (i % 10 == 0) cleanupSounds();

        if (_kbhit()) {
            int ch = _getch();
            if (ch == 13) { // Enter - пропустить анимацию
                if (i + 1 < text.length()) std::cout << text.substr(i + 1);
                break;
            } else if (ch == 27) { // ESC - выход в меню
                if (i + 1 < text.length()) std::cout << text.substr(i + 1);
                std::cout << std::endl;
                throw std::runtime_error("ESC_TO_MENU");
            } else if (ch == 32) { // Пробел - пауза
                paused = true;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(finalSpeed));
    }
    std::cout << std::endl;
}

std::vector<char> NovelEngine::readFile(const std::string& path) {
    // Check cache first
    auto it = fileCache.find(path);
    if (it != fileCache.end()) {
        return it->second;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return {};
    std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    if (USE_DECRYPT) {
        std::string key = ASSET_KEY;
        if (!key.empty()) {
            std::vector<uint8_t> data(buffer.begin(), buffer.end());
            if (CryptoWrapper::decryptBuffer(data, key)) {
                buffer.assign(data.begin(), data.end());
            } else {
                std::cerr << "[ERROR] ChaCha20 decryption failed for: " << path << std::endl;
                return {};
            }
        }
    }

    // Cache the decrypted file
    fileCache[path] = buffer;
    return buffer;
}

std::string NovelEngine::replaceMacros(std::string text) {
    size_t startPos = 0;
    while ((startPos = text.find('$', startPos)) != std::string::npos) {
        size_t endPos = text.find('$', startPos + 1);
        if (endPos == std::string::npos) break;
        std::string varName = text.substr(startPos + 1, endPos - startPos - 1);
        std::string replacement = variables.count(varName) ? std::to_string(variables[varName]) : "0";
        text.replace(startPos, (endPos - startPos) + 1, replacement);
        startPos += replacement.length();
    }
    return text;
}

bool NovelEngine::loadScenario(const std::string& filename) {
    fs::path requestedPath = fs::path(filename);
    fs::path basePath = fs::path(DIR_RES);

    try {
        fs::path canonicalRequested = fs::weakly_canonical(requestedPath);
        fs::path canonicalBase = fs::weakly_canonical(basePath);

        auto [rootEnd, nothing] = std::mismatch(canonicalBase.begin(), canonicalBase.end(),
                                                 canonicalRequested.begin(), canonicalRequested.end());
        if (rootEnd != canonicalBase.end()) {
            Logger::getInstance().error("Path traversal attempt blocked: " + filename);
            return false;
        }
    } catch (const fs::filesystem_error& e) {
        Logger::getInstance().error("Invalid path: " + filename);
        return false;
    }

    auto data = readFile(filename);
    if (data.empty()) return false;

    events.clear();
    currentChapterFile = filename;

    std::string content(data.begin(), data.end());
    std::istringstream stream(content);
    std::string line, currentName = "System";

    while (std::getline(stream, line)) {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        if (line.empty()) continue;

        if (line.front() == '[' && line.back() == ']') {
            currentName = line.substr(1, line.size() - 2);
        } else if (line.front() == '{' && line.back() == '}') {
            events.push_back({EventType::COMMAND, "", line.substr(1, line.size() - 2)});
        } else {
            events.push_back({EventType::TEXT, currentName, line});
        }
    }
    return true;
}

void NovelEngine::run() {
    if (!currentMusicFile.empty() && activeSounds.empty()) {
        executeCommand("play:" + currentMusicFile + "|loop");
    }

    if (currentEventIdx > events.size()) {
        Logger::getInstance().warn("currentEventIdx exceeds events size, resetting to 0");
        currentEventIdx = 0;
    }

    for (size_t i = 0; i < currentEventIdx && i < events.size(); ++i) {
        if (events[i].type == EventType::COMMAND) {
            const std::string& cmd = events[i].content;
            if (cmd.find("color") != std::string::npos || cmd.find("set") != std::string::npos) {
                executeCommand(cmd);
            }
        }
    }
    chapterFinished = false; 

    for (; currentEventIdx < events.size(); ++currentEventIdx) {
        cleanupSounds();
        const auto& ev = events[currentEventIdx];

        if (ev.type == EventType::COMMAND) {
            executeCommand(ev.content);
            if (chapterFinished) return;
        } else {
            this->currentSpeaker = ev.name;
            std::string currentColor = characterColors.count(ev.name) ? characterColors[ev.name] : CLR_NAME;
            std::string processedText = replaceMacros(ev.content);
            history.push_back({ev.name, processedText, currentColor});

            // Ограничение размера истории (настраиваемое)
            size_t maxHistory = SettingsManager::getInstance().get().historySize;
            if(history.size() > maxHistory) history.erase(history.begin());

            this->lastSpeaker = ev.name;
            this->lastFullText = processedText;
            std::cout << "\n" << currentColor << ">>> " << ev.name << " <<<" << CLR_RESET << std::endl;
            std::cout << CLR_TEXT;
            try {
                typeText(processedText, 30);
            } catch (const std::runtime_error& e) {
                if (std::string(e.what()) == "ESC_TO_MENU") {
                    std::cout << CLR_RESET;
                    Logger::getInstance().info("User pressed ESC, returning to menu");
                    return; // Выход в главное меню
                }
                throw; // Пробрасываем другие исключения
            }
            std::cout << CLR_RESET;

            while (true) {
                cleanupSounds();
                if (_kbhit()) {
                    int ch = _getch();
                    if (ch == 27) { // ESC в режиме ожидания
                        Logger::getInstance().info("User pressed ESC, returning to menu");
                        return;
                    }
                    if (ch == 's' || ch == 'S') { saveGame(1); continue; }
                    if (ch == 13) break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
}

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        size_t first = token.find_first_not_of(" ");
        if (std::string::npos != first) {
            size_t last = token.find_last_not_of(" ");
            tokens.push_back(token.substr(first, (last - first + 1)));
        } else {
            tokens.push_back("");
        }
    }
    return tokens;
}