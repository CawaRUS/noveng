#include "engine.hpp"
#include "common.hpp"
#include "logger.hpp"
#include "setting.hpp"
#include "localisation.hpp"
#include "command.hpp"
#include "crypto_wrapper.hpp"
#include "save_manager.hpp"
#include "menu.hpp"
#include "scenario_validator.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <utility>

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

std::string resolveScenarioPath(const std::string& filename) {
    if (filename.empty()) return "";
    // Reject any path containing parent directory references outright.
    if (filename.find("..") != std::string::npos) {
        Logger::getInstance().error("Scenario name contains '..' traversal: " + filename);
        return "";
    }

    fs::path scenarioDir = fs::path(DIR_RES) / DIR_SCENARIO;
    fs::path target = scenarioDir / filename;

    try {
        fs::path canonicalTarget = fs::weakly_canonical(target);
        fs::path canonicalBase = fs::weakly_canonical(scenarioDir);

        auto [baseEnd, targetEnd] = std::mismatch(canonicalBase.begin(), canonicalBase.end(),
                                                  canonicalTarget.begin(), canonicalTarget.end());
        if (baseEnd != canonicalBase.end()) {
            Logger::getInstance().error("Scenario path escapes scenario directory: " + filename);
            return "";
        }
    } catch (const fs::filesystem_error& e) {
        Logger::getInstance().error("Invalid scenario path: " + filename);
        return "";
    }

    if (target.extension() != ".txt") {
        Logger::getInstance().error("Scenario file must have .txt extension: " + filename);
        return "";
    }

    return target.string();
}

void NovelEngine::render() {
    std::cout << "\033[2J\033[H"; 
    
    for(int y = 0; y < offsetY; ++y) std::cout << "\n";

    for (const auto& entry : history) {
        if (!entry.speaker.empty()) {
            std::cout << "\n" << entry.color << ">>> " << entry.speaker << " <<<" << CLR_RESET << std::endl;
        }

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

    if (loadPersistent()) {
        Logger::getInstance().info("Persistent variables loaded successfully.");
    }
}

void NovelEngine::setMenu(MainMenu* m) {
    menu = m;
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

bool NovelEngine::saveGame(int slot) {
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
    state.currentSpeaker = this->currentSpeaker;
    state.saveTimestamp = SaveManager::getCurrentTimestamp();
    state.totalEvents = events.size();

    fs::path savePath = SaveManager::getSavePath(slot, false);
    try {
        json j = state;
        std::string plainText = j.dump(4);
        std::vector<uint8_t> buffer(plainText.begin(), plainText.end());

        std::string saveKey = CryptoWrapper::deriveSaveKey(ASSET_KEY);
        if (!CryptoWrapper::encryptBuffer(buffer, saveKey)) {
            Logger::getInstance().error("Failed to encrypt save slot " + std::to_string(slot));
            return false;
        }

        std::ofstream file(savePath, std::ios::binary);
        if (!file.is_open()) {
            Logger::getInstance().error("Failed to open save file for writing: " + savePath.string());
            return false;
        }
        file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());

        std::string speaker = LocalizationManager::getInstance().get("system_name");
        std::string msg = LocalizationManager::getInstance().get("system_save_message") + " [" + std::to_string(slot) + "]";
        history.push_back({speaker, msg, CLR_SYSTEM});

        size_t maxHistory = SettingsManager::getInstance().get().historySize;
        if (history.size() > maxHistory) {
            history.erase(history.begin());
        }
        render();
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Exception while saving game: " + std::string(e.what()));
        return false;
    }
}

bool NovelEngine::loadGame(int slot, bool isAutosave) {
    fs::path savePath = SaveManager::getSavePath(slot, isAutosave);
    if (!fs::exists(savePath)) return false;

    try {
        std::ifstream file(savePath, std::ios::binary);
        if (!file.is_open()) return false;

        std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        std::string plainText;
        std::string saveKey = CryptoWrapper::deriveSaveKey(ASSET_KEY);

        if (CryptoFormat::hasMagic(buffer) && CryptoWrapper::decryptBuffer(buffer, saveKey)) {
            plainText.assign(buffer.begin(), buffer.end());
            Logger::getInstance().info("Save slot " + std::to_string(slot) + " decrypted successfully.");
        } else {
            // Fallback for unencrypted (legacy) save files.
            plainText.assign(buffer.begin(), buffer.end());
            Logger::getInstance().warn("Save slot " + std::to_string(slot) + " appears unencrypted or uses a different key. Loading as plaintext.");
        }

        json j = json::parse(plainText);
        GameState state = j.get<GameState>();

        this->currentChapterFile = state.currentScene;
        this->characterColors = state.characterColors;
        this->characterPitches = state.characterPitches;
        this->variables = state.variables;
        this->currentSpeaker = state.currentSpeaker;

        this->currentEventIdx = state.eventIndex;
        Logger::getInstance().info("Loaded eventIndex: " + std::to_string(state.eventIndex));

        this->history.clear();
        this->lastSpeaker.clear();
        this->lastFullText.clear();

        stopAudio();

        if (!state.currentMusic.empty()) {
            this->currentMusicFile = state.currentMusic;
        }

        mergePersistentIntoVariables();
        this->chapterFinished = false;
        return true;
    } catch (...) { return false; }
}

void NovelEngine::autosave() {
    int slot = SaveManager::pickAutosaveSlot();
    Logger::getInstance().info("Autosaving to slot " + std::to_string(slot) + "...");

    fs::path savePath = SaveManager::getSavePath(slot, true);
    fs::path saveDir = savePath.parent_path();
    if (!fs::exists(saveDir)) fs::create_directories(saveDir);

    try {
        GameState state;
        state.currentScene = currentChapterFile;
        state.currentMusic = this->currentMusicFile;
        state.characterColors = this->characterColors;
        state.characterPitches = this->characterPitches;
        state.eventIndex = currentEventIdx;
        state.variables = this->variables;
        state.currentSpeaker = this->currentSpeaker;
        state.saveTimestamp = SaveManager::getCurrentTimestamp();
        state.totalEvents = events.size();

        json j = state;
        std::string plainText = j.dump(4);
        std::vector<uint8_t> buffer(plainText.begin(), plainText.end());

        std::string saveKey = CryptoWrapper::deriveSaveKey(ASSET_KEY);
        if (!CryptoWrapper::encryptBuffer(buffer, saveKey)) {
            Logger::getInstance().error("Failed to encrypt autosave slot " + std::to_string(slot));
            return;
        }

        std::ofstream file(savePath, std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
            dialogCountSinceAutosave = 0;
            Logger::getInstance().info("Autosave complete: " + savePath.string());
        } else {
            Logger::getInstance().error("Failed to open autosave file for writing: " + savePath.string());
        }
    } catch (const std::exception& e) {
        Logger::getInstance().error("Exception while autosaving: " + std::string(e.what()));
    }
}

void NovelEngine::mergePersistentIntoVariables() {
    for (const auto& [key, value] : persistentVariables) {
        if (!variables.count(key)) {
            variables[key] = value;
        }
    }
}

bool NovelEngine::loadPersistent() {
    fs::path persistentPath = fs::path(DIR_RES) / DIR_SAVE / "persistent.json";
    if (!fs::exists(persistentPath)) {
        Logger::getInstance().info("No persistent save file found. Starting with empty persistent variables.");
        return true;
    }

    try {
        std::ifstream file(persistentPath, std::ios::binary);
        if (!file.is_open()) return false;

        std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        std::string plainText;
        std::string saveKey = CryptoWrapper::deriveSaveKey(ASSET_KEY);

        if (CryptoFormat::hasMagic(buffer) && CryptoWrapper::decryptBuffer(buffer, saveKey)) {
            plainText.assign(buffer.begin(), buffer.end());
            Logger::getInstance().info("Persistent file decrypted successfully.");
        } else {
            plainText.assign(buffer.begin(), buffer.end());
            Logger::getInstance().warn("Persistent file appears unencrypted or uses a different key. Loading as plaintext.");
        }

        json j = json::parse(plainText);
        PersistentState state = j.get<PersistentState>();
        persistentVariables = state.variables;
        mergePersistentIntoVariables();
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Exception while loading persistent variables: " + std::string(e.what()));
        return false;
    }
}

bool NovelEngine::savePersistent() {
    fs::path saveDir = fs::path(DIR_RES) / DIR_SAVE;
    if (!fs::exists(saveDir)) fs::create_directories(saveDir);

    try {
        PersistentState state;
        state.variables = persistentVariables;

        json j = state;
        std::string plainText = j.dump(4);
        std::vector<uint8_t> buffer(plainText.begin(), plainText.end());

        std::string saveKey = CryptoWrapper::deriveSaveKey(ASSET_KEY);
        if (!CryptoWrapper::encryptBuffer(buffer, saveKey)) {
            Logger::getInstance().error("Failed to encrypt persistent file.");
            return false;
        }

        fs::path persistentPath = saveDir / "persistent.json";
        std::ofstream file(persistentPath, std::ios::binary);
        if (!file.is_open()) {
            Logger::getInstance().error("Failed to open persistent file for writing: " + persistentPath.string());
            return false;
        }
        file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        Logger::getInstance().info("Persistent variables saved.");
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Exception while saving persistent variables: " + std::string(e.what()));
        return false;
    }
}

void NovelEngine::applySettings() {
    auto& cfg = SettingsManager::getInstance().get();
    if (musicMuted) {
        ma_engine_set_volume(&audio, 0.0f);
    } else {
        ma_engine_set_volume(&audio, cfg.musicVolume);
    }
}

void NovelEngine::stopAudio() {
    std::lock_guard<std::mutex> lock(soundsMutex);
    for (auto& s : activeSounds) {
        if (s->initialized) {
            ma_sound_stop(&s->sound);
        }
    }
    activeSounds.clear();

    if (musicSound && musicSound->initialized) {
        ma_sound_stop(&musicSound->sound);
    }
    musicSound.reset();
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

        std::unique_lock<std::mutex> lock(soundsMutex);
        if (activeSounds.size() > 20) {
            lock.unlock();
            cleanupSounds();
            lock.lock();
        }
        activeSounds.push_back(std::move(pActiveSound));
    } else {
        Logger::getInstance().error("Failed to init sound source: " + filename);
        ma_decoder_uninit(&pActiveSound->decoder);
    }
}

bool NovelEngine::typeText(const std::string& text, int speedMs) {
    int finalSpeed = SettingsManager::getInstance().get().typingSpeed;
    float currentPitch = characterPitches.count(currentSpeaker) ? characterPitches[currentSpeaker] : 1.0f;

    for(int x = 0; x < offsetX; ++x) std::cout << " ";

    bool paused = false;
    int charCount = 0;
    for (size_t i = 0; i < text.length(); ) {
        size_t charLen = 1;
        unsigned char lead = static_cast<unsigned char>(text[i]);
        if (lead >= 0xF0) charLen = 4;
        else if (lead >= 0xE0) charLen = 3;
        else if (lead >= 0xC0) charLen = 2;

        if (i + charLen > text.length()) {
            charLen = text.length() - i;
        }

        std::string character = text.substr(i, charLen);

        while (paused) {
            if (_kbhit()) {
                int ch = _getch();
                if (ch == 32) { // Пробел - снять паузу
                    paused = false;
                    break;
                } else if (ch == 27) { // ESC - меню паузы
                    if (i < text.length()) std::cout << text.substr(i);
                    std::cout << std::endl;
                    return false;
                } else if (ch == 13) { // Enter - пропустить
                    if (i < text.length()) std::cout << text.substr(i);
                    std::cout << std::endl;
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::cout << character << std::flush;

        bool isCharSpace = false;
        if (charLen == 1 && std::isspace(static_cast<unsigned char>(character[0]))) {
            isCharSpace = true;
        }
        if (!isCharSpace && (charCount % 2 == 0)) {
            playSFX("type.mp3", currentPitch);
        }
        charCount++;

        if (charCount % 10 == 0) cleanupSounds();

        if (_kbhit()) {
            int ch = _getch();
            if (ch == 13) { // Enter - пропустить анимацию
                if (i + charLen < text.length()) std::cout << text.substr(i + charLen);
                break;
            } else if (ch == 27) { // ESC - меню паузы
                if (i + charLen < text.length()) std::cout << text.substr(i + charLen);
                std::cout << std::endl;
                return false;
            } else if (ch == 32) { // Пробел - пауза
                paused = true;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(finalSpeed));
        i += charLen;
    }
    std::cout << std::endl;
    return true;
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
        if (!key.empty() && CryptoFormat::hasMagic(buffer)) {
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
        if (!isValidIdentifier(varName)) {
            Logger::getInstance().warn("Invalid variable name in macro: " + varName);
            startPos = endPos + 1;
            continue;
        }
        std::string replacement = variables.count(varName) ? variantToString(variables[varName]) : "0";
        text.replace(startPos, (endPos - startPos) + 1, replacement);
        startPos += replacement.length();
    }
    return text;
}

bool NovelEngine::loadScenario(const std::string& filename) {
    if (filename.empty()) return false;

    fs::path requestedPath = fs::path(filename);
    fs::path basePath = fs::path(DIR_RES);

    if (requestedPath.extension() != ".txt") {
        Logger::getInstance().error("Scenario file must have .txt extension: " + filename);
        return false;
    }

    try {
        fs::path canonicalRequested = fs::weakly_canonical(requestedPath);
        fs::path canonicalBase = fs::weakly_canonical(basePath / DIR_SCENARIO);

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
    if (data.empty()) {
        // Distinguish between a missing file and an existing empty scenario file.
        try {
            if (fs::exists(filename) && fs::is_regular_file(filename) && fs::file_size(filename) == 0) {
                events.clear();
                currentChapterFile = filename;
                return true;
            }
        } catch (const fs::filesystem_error&) {
            // Fall through to the generic failure below.
        }
        Logger::getInstance().error("Scenario file not found or could not be read: " + filename);
        return false;
    }

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

    auto validation = ScenarioValidator::validate(events, commandRegistry);
    if (!validation.valid) {
        Logger::getInstance().error("Scenario validation failed for: " + filename);
        events.clear();
        return false;
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

    auto handlePause = [this]() -> std::pair<bool, bool> {
        if (!menu) return {true, false};
        PauseResult result = menu->showPauseScreen();
        if (result.action == PauseResult::MainMenu) {
            Logger::getInstance().info("User selected return to main menu from pause");
            return {true, false};
        }
        if (result.action == PauseResult::Load) {
            if (loadGame(result.slot, result.isAutosave)) {
                loadScenario(currentChapterFile);
                applySettings();
                Logger::getInstance().info("Pause load: game loaded from slot " + std::to_string(result.slot));
                return {false, true};
            }
            Logger::getInstance().warn("Pause load failed for slot " + std::to_string(result.slot));
            return {false, false};
        }
        if (result.action == PauseResult::Save) {
            saveGame(result.slot);
        } else if (result.action == PauseResult::Settings) {
            applySettings();
        }
        render();
        return {false, false};
    };

    auto quickLoad = [this]() -> bool {
        SaveManager::SaveInfo latest = SaveManager::getLatestSave();
        if (latest.exists && loadGame(latest.slot, latest.isAutosave)) {
            loadScenario(currentChapterFile);
            applySettings();
            render();
            Logger::getInstance().info("Quick load from latest save");
            return true;
        }
        Logger::getInstance().warn("Quick load failed: no valid save found");
        return false;
    };

    auto toggleMute = [this]() {
        auto& cfg = SettingsManager::getInstance().get();
        if (musicMuted) {
            musicMuted = false;
            cfg.musicVolume = preMuteVolume;
        } else {
            musicMuted = true;
            preMuteVolume = cfg.musicVolume;
        }
        applySettings();
    };

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

            size_t maxHistory = SettingsManager::getInstance().get().historySize;
            if(history.size() > maxHistory) history.erase(history.begin());

            this->lastSpeaker = ev.name;
            this->lastFullText = processedText;
            std::cout << "\n" << currentColor << ">>> " << ev.name << " <<<" << CLR_RESET << std::endl;
            std::cout << CLR_TEXT;
            bool finishedTyping = typeText(processedText, 30);
            std::cout << CLR_RESET;

            if (!finishedTyping) {
                auto [exitRun, replay] = handlePause();
                if (exitRun) return;
                if (replay) { --currentEventIdx; continue; }
                render();
                goto wait_input;
            }

        wait_input:
            while (true) {
                cleanupSounds();
                if (_kbhit()) {
                    int ch = _getch();
                    if (ch == 27) {
                        auto [exitRun, replay] = handlePause();
                        if (exitRun) return;
                        if (replay) { --currentEventIdx; break; }
                        render();
                        continue;
                    }
                    if (ch == 's' || ch == 'S') { saveGame(1); render(); continue; }
                    if (ch == 'l' || ch == 'L') {
                        if (quickLoad()) {
                            --currentEventIdx;
                            break;
                        }
                        continue;
                    }
                    if (ch == 'h' || ch == 'H') { render(); continue; }
                    if (ch == 'm' || ch == 'M') { toggleMute(); continue; }
                    if (ch == 13) break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            dialogCountSinceAutosave++;
            if (dialogCountSinceAutosave >= AUTOSAVE_INTERVAL) {
                autosave();
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