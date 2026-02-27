#include "engine.hpp"
#include "common.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <conio.h>
#include <map>
#include <sstream>
#include "setting.hpp"
#include "localisation.hpp"
#include <memory>
#include <algorithm>
#include <ctime>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <filesystem>
namespace fs = std::filesystem;



int safeStoi(const std::string& str, int defaultVal = 0) {
    try {
        return std::stoi(str);
    } catch (...) {
        return defaultVal;
    }
}

NovelEngine::NovelEngine() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    if (ma_engine_init(NULL, &audio) != MA_SUCCESS) {
        std::cerr << LocalizationManager::getInstance().get("audio_error") << std::endl;
    }
}

NovelEngine::~NovelEngine() {
    stopAudio();
    ma_engine_uninit(&audio);
}

void NovelEngine::saveGame(int slot) {
    fs::path saveDir = fs::path(DIR_RES) / DIR_SAVE;
    if (!fs::exists(saveDir)) fs::create_directories(saveDir);

    GameState state;
    state.currentScene = currentChapterFile;
    state.currentMusic = this->currentMusicFile;
    state.characterColors = this->characterColors;
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
        json j;
        file >> j;
        GameState state = j.get<GameState>();

        this->nextChapterFile = state.currentScene;
        this->currentEventIdx = state.eventIndex;
        this->characterColors = state.characterColors;
        this->currentMusicFile = state.currentMusic;
        this->variables = state.variables;
        
        stopAudio();
        this->chapterFinished = true; 
        return true;
    } catch (...) {
        return false;
    }
}

void NovelEngine::applySettings() {
    auto& cfg = SettingsManager::getInstance().get();
    ma_engine_set_volume(&audio, cfg.musicVolume);
}

void NovelEngine::stopAudio() {
    for (auto& s : activeSounds) {
        ma_sound_stop(s.get());
        ma_sound_uninit(s.get());
    }
    activeSounds.clear();
}

void NovelEngine::cleanupSounds() {
    activeSounds.erase(
        std::remove_if(activeSounds.begin(), activeSounds.end(),
            [](const std::unique_ptr<ma_sound>& s) {
                if (!ma_sound_is_playing(s.get())) {
                    ma_sound_uninit(s.get());
                    return true;
                }
                return false;
            }),
        activeSounds.end()
    );
}

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        size_t first = token.find_first_not_of(" \t\r\n");
        if (first != std::string::npos) {
            size_t last = token.find_last_not_of(" \t\r\n");
            tokens.push_back(token.substr(first, (last - first + 1)));
        }
    }
    return tokens;
}

void NovelEngine::playSFX(const std::string& filename) {
    fs::path path = fs::path(DIR_RES) / DIR_SFX / filename;
    auto pSfx = std::make_unique<ma_sound>();
    
    if (ma_sound_init_from_file(&audio, path.string().c_str(), 0, NULL, NULL, pSfx.get()) == MA_SUCCESS) {
        ma_sound_start(pSfx.get());
        activeSounds.push_back(std::move(pSfx));
    }
}

void NovelEngine::typeText(const std::string& text, int speedMs) {
    int finalSpeed = SettingsManager::getInstance().get().typingSpeed;
    for (size_t i = 0; i < text.length(); ++i) {
        std::cout << text[i] << std::flush;
        if (_kbhit()) {
            if (_getch() == 13) {
                if (i + 1 < text.length()) std::cout << text.substr(i + 1);
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(finalSpeed));
    }
    std::cout << std::endl;
}

bool NovelEngine::loadScenario(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;
    
    events.clear();
    currentChapterFile = filename;
    std::string line, currentName = LocalizationManager::getInstance().get("system_name");
    
    while (std::getline(file, line)) {
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

    for (size_t i = 0; i < currentEventIdx; ++i) {
        if (events[i].type == EventType::COMMAND) {
            const std::string& cmd = events[i].content;
            if (cmd.find("play:") == std::string::npos && 
                cmd.find("sfx:") == std::string::npos &&
                cmd.find("choice:") == std::string::npos &&
                cmd.find("next_chapter") == std::string::npos &&
                cmd.find("chance") == std::string::npos &&
                cmd.find("if") == std::string::npos) {
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
            std::string currentColor = CLR_NAME;
            if (characterColors.count(ev.name)) currentColor = characterColors[ev.name];

            std::cout << "\n" << currentColor << ">>> " << ev.name << " <<<" << CLR_RESET << std::endl;
            std::cout << CLR_TEXT;
            typeText(ev.content, 30);
            std::cout << CLR_RESET;

            while (true) {
                cleanupSounds();
                if (_kbhit()) {
                    int ch = _getch();
                    if (ch == 's' || ch == 'S') { 
                        saveGame(1);
                        continue; 
                    }
                    if (ch == 13) break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
}

void NovelEngine::executeCommand(const std::string& cmd) {
    std::string action;
    std::vector<std::string> args;

    size_t colonPos = cmd.find(':');
    if (colonPos != std::string::npos) {
        action = cmd.substr(0, colonPos);
        args = split(cmd.substr(colonPos + 1), '|');
    } else {
        action = cmd;
    }

    if (action == "play" && !args.empty()) {
        if (currentMusicFile == args[0] && !activeSounds.empty()) return;
        stopAudio(); 
        currentMusicFile = args[0];
        fs::path path = fs::path(DIR_RES) / DIR_MUSIC / args[0];
        
        auto pSound = std::make_unique<ma_sound>();
        if (ma_sound_init_from_file(&audio, path.string().c_str(), 0, NULL, NULL, pSound.get()) == MA_SUCCESS) {
            bool looping = false;
            for (const auto& arg : args) if (arg == "loop") looping = true;
            ma_sound_set_looping(pSound.get(), looping ? MA_TRUE : MA_FALSE);
            ma_sound_start(pSound.get());
            activeSounds.push_back(std::move(pSound));
        }
    } 
    else if (action == "sfx" && !args.empty()) {
        playSFX(args[0]);
    }
    else if (action == "color" && args.size() >= 2) {
        characterColors[args[0]] = "\033[1;" + args[1] + "m";
    }
    else if (action == "clear") { system("cls"); } 
    else if (action == "next_chapter" && !args.empty()) {
        chapterFinished = true;
        nextChapterFile = (fs::path(DIR_RES) / DIR_SCENARIO / args[0]).string();
    }
    else if (action == "set" && args.size() >= 2) {
        variables[args[0]] = safeStoi(args[1]);
    }
    else if (action == "chance" && args.size() >= 3) {
        int prob = safeStoi(args[0]);
        chapterFinished = true;
        std::string nextFile = (std::rand() % 100 < prob) ? args[1] : args[2];
        nextChapterFile = (fs::path(DIR_RES) / DIR_SCENARIO / nextFile).string();
    }
    else if (action == "if" && args.size() >= 4) {
        chapterFinished = true;
        std::string nextFile = (variables[args[0]] >= safeStoi(args[1])) ? args[2] : args[3];
        nextChapterFile = (fs::path(DIR_RES) / DIR_SCENARIO / nextFile).string();
    }
    else if (action == "choice") {
        std::cout << "\n" << CLR_NAME << LocalizationManager::getInstance().get("choice_header") << CLR_RESET << std::endl;
        int numOptions = (int)(args.size() / 2);
        for (int i = 0; i < numOptions; ++i) {
            std::cout << "  " << (i + 1) << ". " << args[i * 2] << std::endl;
        }

        int selected = 0;
        while (selected < 1 || selected > numOptions) {
            if (_kbhit()) {
                int ch = _getch();
                if (ch >= '1' && ch <= ('0' + numOptions)) selected = ch - '0';
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        chapterFinished = true;
        nextChapterFile = (fs::path(DIR_RES) / DIR_SCENARIO / args[(selected - 1) * 2 + 1]).string();
    }
}