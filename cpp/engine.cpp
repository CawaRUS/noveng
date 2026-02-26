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

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <filesystem>
namespace fs = std::filesystem;

void NovelEngine::saveGame(int slot) {
    if (!fs::exists("res/save")) fs::create_directories("res/save");

    GameState state;
    state.currentScene = currentChapterFile;
    state.currentMusic = this->currentMusicFile;
    state.characterColors = this->characterColors;
    state.eventIndex = currentEventIdx;

    std::string path = "res/save/save" + std::to_string(slot) + ".json";
    std::ofstream file(path);
    if (file.is_open()) {
        json j = state;
        file << j.dump(4);
        std::cout << "\n" << LocalizationManager::getInstance().get("save_success") << " [" << slot << "]" << std::endl;
    }
}

bool NovelEngine::loadGame(int slot) {
    std::string path = "res/save/save" + std::to_string(slot) + ".json";
    if (!fs::exists(path)) return false;

    std::ifstream file(path);
    json j;
    file >> j;
    GameState state = j.get<GameState>();

    this->nextChapterFile = state.currentScene;
    this->currentEventIdx = state.eventIndex;
    this->characterColors = state.characterColors;
    this->currentMusicFile = state.currentMusic;
    stopAudio();
    this->chapterFinished = true;
    
    return true;
}

NovelEngine::NovelEngine() {
    if (ma_engine_init(NULL, &audio) != MA_SUCCESS) {
        std::cerr << LocalizationManager::getInstance().get("audio_error") << std::endl;
    }
}

void NovelEngine::applySettings() {
    auto& cfg = SettingsManager::getInstance().get();
    ma_engine_set_volume(&audio, cfg.musicVolume);
}

NovelEngine::~NovelEngine() {
    stopAudio();
    ma_engine_uninit(&audio);
}

void NovelEngine::stopAudio() {
    for (auto s : activeSounds) {
        ma_sound_stop(s);
        ma_sound_uninit(s);
        delete s;
    }
    activeSounds.clear();
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
    std::string path = "res/sfx/" + filename;

    ma_sound* pSfx = new ma_sound();
    ma_result res = ma_sound_init_from_file(&audio, path.c_str(), 0, NULL, NULL, pSfx);
    
    if (res == MA_SUCCESS) {
        ma_sound_start(pSfx);
        activeSounds.push_back(pSfx); 
    } else {
        delete pSfx;
    }
}

void NovelEngine::typeText(const std::string& text, int speedMs) {
    int finalSpeed = SettingsManager::getInstance().get().typingSpeed;
    for (size_t i = 0; i < text.length(); ++i) {
        std::cout << text[i] << std::flush;

        if (_kbhit()) {
            if (_getch() == 13) { // Enter
                if (i + 1 < text.length()) {
                    std::cout << text.substr(i + 1);
                }
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
            if (events[i].content.find("play:") == std::string::npos && 
                events[i].content.find("choice:") == std::string::npos) {
                executeCommand(events[i].content);
            }
        }
    }
    for (; currentEventIdx < events.size(); ++currentEventIdx) {
        const auto& ev = events[currentEventIdx];
        if (ev.type == EventType::COMMAND) {
            executeCommand(ev.content);
            if (chapterFinished) return;
        } else {
            std::string currentColor = CLR_NAME;
            if (characterColors.count(ev.name)) {
                currentColor = characterColors[ev.name];
            }

            std::cout << "\n" << currentColor << ">>> " << ev.name << " <<<" << CLR_RESET << std::endl;
            std::cout << CLR_TEXT;
            typeText(ev.content, 30);
            std::cout << CLR_RESET;

            while (true) {
                if (_kbhit()) {
                    int ch = _getch();
                    if (ch == 's' || ch == 'S' || ch == 123) { 
                        saveGame(1);
                        std::cout << LocalizationManager::getInstance().get("system_save_message") << std::flush;
                        continue; 
                    }
                    if (ch == 13) break; // Enter
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
        std::string argsStr = cmd.substr(colonPos + 1);
        args = split(argsStr, '|');
    } else {
        action = cmd;
    }

    if (action == "play" && !args.empty()) {
        if (currentMusicFile == args[0] && !activeSounds.empty()) return;
        
        stopAudio(); 
        currentMusicFile = args[0];
        std::string path = "res/music/" + args[0];
        ma_sound* pSound = new ma_sound();
        ma_result res = ma_sound_init_from_file(&audio, path.c_str(), 0, NULL, NULL, pSound);
        if (res == MA_SUCCESS) {
            activeSounds.push_back(pSound);
            bool looping = false;
            for (const auto& arg : args) if (arg == "loop") looping = true;
            if (looping) ma_sound_set_looping(pSound, MA_TRUE);
            
            ma_sound_start(pSound);
        } else {
            delete pSound;
        }
    } 
    else if (action == "sfx" && !args.empty()) {
        playSFX(args[0]);
    }
    else if (action == "stop_music") {
        stopAudio();
        currentMusicFile = "";
    }
    else if (action == "color" && args.size() >= 2) {
        characterColors[args[0]] = "\033[1;" + args[1] + "m";
    }
    else if (action == "clear") { clearScreen(); } 
    else if (action == "next_chapter") {
        chapterFinished = true;
        nextChapterFile = "res/scenario/" + args[0];
    }

    else if (action == "set" && args.size() >= 2) {
        variables[args[0]] = std::stoi(args[1]);
    }
    else if (action == "chance" && args.size() >= 3) {
        int probability = std::stoi(args[0]);
        int roll = rand() % 100;
        chapterFinished = true;
        nextChapterFile = "res/scenario/" + (roll < probability ? args[1] : args[2]);
    }
    else if (action == "if" && args.size() >= 4) {
        chapterFinished = true;
        nextChapterFile = "res/scenario/" + (variables[args[0]] >= std::stoi(args[1]) ? args[2] : args[3]);
    }

    else if (action == "choice") {
        std::cout << "\n" << CLR_NAME << LocalizationManager::getInstance().get("choice_header") << CLR_RESET << std::endl;
        for (size_t i = 0; i < args.size(); i += 2) {
            std::cout << "  " << (i / 2 + 1) << ". " << args[i] << std::endl;
        }
        std::cout << CLR_NAME << LocalizationManager::getInstance().get("choice_footer") << CLR_RESET << std::endl;

        int selected = 0;
        int numOptions = (int)(args.size() / 2);

        while (selected < 1 || selected > numOptions) {
            if (_kbhit()) {
                int ch = _getch();
                if (ch >= '1' && ch <= ('0' + numOptions)) {
                    selected = ch - '0';
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        chapterFinished = true;
        nextChapterFile = "res/scenario/" + args[(selected - 1) * 2 + 1];
    }
}