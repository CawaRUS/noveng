#include "menu.hpp"
#include "common.hpp"
#include <iostream>
#include <string>
#include <vector> 
#include <conio.h>
#include <chrono>
#include "setting.hpp"
#include <thread>
#include <filesystem>
#include "localisation.hpp"
#include <regex>

namespace fs = std::filesystem;


std::string stripANSI(const std::string& str) {

    return std::regex_replace(str, std::regex("\x1B\\[[0-9;]*[mK]"), "");
}

int getVisibleLength(const std::string& str) {
    std::string cleanStr = stripANSI(str);
    int length = 0;
    for (size_t i = 0; i < cleanStr.length(); i++) {
        if ((static_cast<unsigned char>(cleanStr[i]) & 0xC0) != 0x80) {
            length++;
        }
    }
    return length;
}

void MainMenu::playIntro(ma_engine* audio) {
    fs::path hoverPath = fs::path(DIR_RES) / DIR_SFX / "hover.mp3";
    fs::path musicPath = fs::path(DIR_RES) / DIR_MUSIC / "menu.mp3";

    ma_sound_init_from_file(audio, hoverPath.string().c_str(), 0, NULL, NULL, &hoverSfx);
    ma_sound_init_from_file(audio, musicPath.string().c_str(), 0, NULL, NULL, &menuMusic);
    
    clearScreen();

    auto typeWrite = [](const std::string& text, int delayMs = 50) {
        for (char c : text) {
            std::cout << c << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }
    };

    auto waitOrSkip = [](int ms) -> bool {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start).count() < ms) {
            if (_kbhit() && _getch() == 13) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        return false;
    };

    const std::string logo = R"(
            ,-----.                                     ,--.               
            '  .--./ ,--,--.,--.   ,--. ,--,--. ,---.     |  |,--,--,  ,---. 
            |  |    ' ,-.  ||  |.'.|  |' ,-.  |(  .-'     |  ||  |  |  \| .--' 
            '  '--'\\ '-'  ||  .'.  |\ '-'  |.-'  `)     |  ||  ||  |\ `--. 
            `-----' `--`--''--'   '--' `--`--'`----'      `--'`--''--' `---' 
    )";

    ma_sound_set_looping(&menuMusic, MA_TRUE);
    ma_sound_start(&menuMusic);

    auto startTime = std::chrono::steady_clock::now();
    bool skipped = false;

    std::cout << "\n\n\n";
    typeWrite("\t\t   ~ " + LocalizationManager::getInstance().get("intro_presents") + " ~\n", 40);
    if (waitOrSkip(1200)) { skipped = true; goto done; }

    clearScreen();
    std::cout << "\n\n";
    for (char c : logo) {
        std::cout << c << std::flush;
        if (c == '\n') {
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
            if (_kbhit() && _getch() == 13) { skipped = true; goto done; }
        }
    }
    if (waitOrSkip(800)) { skipped = true; goto done; }

    std::cout << "\n";
    typeWrite("\t\t      [ " + LocalizationManager::getInstance().get("intro_start") + " ]", 35);
    if (waitOrSkip(500)) { skipped = true; goto done; }

    {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        long long remaining = 13000 - elapsed;
        if (remaining > 0 && waitOrSkip(static_cast<int>(remaining)))
            skipped = true;
    }

done:
    if (skipped) {
        ma_uint64 frame = ma_engine_get_sample_rate(audio) * 13;
        ma_sound_seek_to_pcm_frame(&menuMusic, frame);
    }
}

void MainMenu::showSettings() {
    auto& settings = SettingsManager::getInstance().get();
    int selected = 0;
    const int maxOptions = 3;

    while (true) {
        clearScreen();
        std::cout << "\n    --- " << LocalizationManager::getInstance().get("settings_title") << " ---\n\n";
        
        if (selected == 0) std::cout << " > "; else std::cout << "   ";
        std::cout << LocalizationManager::getInstance().get("setting_volume") << ": " << (int)(settings.musicVolume * 100) << "%\n";

        if (selected == 1) std::cout << " > "; else std::cout << "   ";
        std::cout << LocalizationManager::getInstance().get("setting_speed") << ": " << settings.typingSpeed << " ms\n";

        if (selected == 2) std::cout << " > "; else std::cout << "   ";
        std::cout << LocalizationManager::getInstance().get("setting_lang") << ": " << settings.language << "\n";

        std::cout << "\n [ Esc - " << LocalizationManager::getInstance().get("btn_save_exit") << " | \x1B[2D\x1B[2C - " << LocalizationManager::getInstance().get("btn_change") << " ]";

        int key = _getch();
        if (key == 27) break; // ESC

        if (key == 0xE0 || key == 0) {
            int prevSelected = selected;
            key = _getch();
            if (key == 72) selected = (selected - 1 + maxOptions) % maxOptions; // Вверх
            if (key == 80) selected = (selected + 1) % maxOptions; // Вниз

            if (selected != prevSelected) {
                ma_sound_seek_to_pcm_frame(&hoverSfx, 0);
                ma_sound_start(&hoverSfx);
            }
            
            if (selected == 0) {
                if (key == 75) settings.musicVolume = std::max(0.0f, settings.musicVolume - 0.05f);
                if (key == 77) settings.musicVolume = std::min(1.0f, settings.musicVolume + 0.05f);
            }
            if (selected == 1) {
                if (key == 75) settings.typingSpeed = std::max(0, settings.typingSpeed - 5);
                if (key == 77) settings.typingSpeed = std::min(200, settings.typingSpeed + 5);
            }
            if (selected == 2) {
                if (key == 75 || key == 77) {
                    LocalizationManager::getInstance().switchLanguage(key == 77, settings.language);
                }
            }
        }
    }
    SettingsManager::getInstance().save();
}

int MainMenu::show() {
    if (ma_sound_is_playing(&menuMusic) == MA_FALSE) {
        ma_sound_seek_to_pcm_frame(&menuMusic, 0);
        ma_sound_start(&menuMusic);
    }

    int selected = 0;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = false;
    SetConsoleCursorInfo(hConsole, &cursorInfo);

    while (true) {
        fs::path savePath = fs::path(DIR_RES) / DIR_SAVE / "save1.json";
        bool hasSave = fs::exists(savePath);
        
        auto& lm = LocalizationManager::getInstance();

        std::vector<std::string> items;
        if (hasSave) items.push_back(lm.get("menu_continue"));
        items.push_back(lm.get("menu_new_game"));
        items.push_back(lm.get("menu_settings"));
        items.push_back(lm.get("menu_about"));
        items.push_back(lm.get("menu_exit"));

        const int itemCount = static_cast<int>(items.size());
        const int innerWidth = 36;

        clearScreen();
        
        std::cout << "\n\n     \x1B[1;36m╔════════════════════════════════════╗\x1B[0m\n";
        
        std::string titleText = std::string(APP_NAME) + "  v" + std::string(APP_VERSION);
        int titleLen = getVisibleLength(titleText);
        int padLeft = (innerWidth - titleLen) / 2;
        int padRight = innerWidth - titleLen - padLeft;
        
        std::cout << "     \x1B[1;36m║\x1B[0m" << std::string(padLeft, ' ') << titleText << std::string(padRight, ' ') << "\x1B[1;36m║\x1B[0m\n";
        
        // 3. Разделитель
        std::cout << "     \x1B[1;36m╠════════════════════════════════════╣\x1B[0m\n";
        std::cout << "     \x1B[1;36m║\x1B[0m" << std::string(innerWidth, ' ') << "\x1B[1;36m║\x1B[0m\n";

        // 4. Пункты меню
        for (int i = 0; i < itemCount; i++) {
            std::string prefix = (i == selected) ? " > " : "   ";
            std::string text = items[i];
            std::string fullLine = prefix + text;
            
            int visibleLen = getVisibleLength(fullLine);
            int spacesToAdd = innerWidth - visibleLen - 2;
            if (spacesToAdd < 0) spacesToAdd = 0;

            std::cout << "     \x1B[1;36m║\x1B[0m  ";
            if (i == selected) {
                std::cout << "\x1B[1;33m" << fullLine << "\x1B[0m";
            } else {
                std::cout << fullLine;
            }
            std::cout << std::string(spacesToAdd, ' ') << "\x1B[1;36m║\x1B[0m\n";
        }

        // 5. Нижняя граница
        std::cout << "     \x1B[1;36m║\x1B[0m" << std::string(innerWidth, ' ') << "\x1B[1;36m║\x1B[0m\n";
        std::cout << "     \x1B[1;36m╚════════════════════════════════════╝\x1B[0m\n";
        std::cout << "\n     [ " << lm.get("menu_hint") << " ]\n";
        
        int key = _getch();

        if (key == 0xE0 || key == 0) {
            int prevSelected = selected;
            key = _getch();
            if (key == 72) selected = (selected - 1 + itemCount) % itemCount; // ↑
            if (key == 80) selected = (selected + 1) % itemCount;             // ↓
            
            if (selected != prevSelected) {
                ma_sound_seek_to_pcm_frame(&hoverSfx, 0);
                ma_sound_start(&hoverSfx);
            }
            continue;
        }

        if (key == 13) {
            cursorInfo.bVisible = true;
            SetConsoleCursorInfo(hConsole, &cursorInfo);
            
            int action = selected;
            if (!hasSave) action += 1; 

            switch (action) {
                case 0: ma_sound_stop(&menuMusic); return 2;
                case 1: ma_sound_stop(&menuMusic); return 1; 
                case 2: showSettings(); break;
                case 3: showAbout(); break;
                case 4: return 0;
            }
            
            cursorInfo.bVisible = false;
            SetConsoleCursorInfo(hConsole, &cursorInfo);
        }

        if (key == 27) return 0;
    }
}

void MainMenu::showAbout() {
    clearScreen();
    std::cout << "\x1B[1;32m" << "--- INFO ---" << "\x1B[0m\n\n";
    std::cout << LocalizationManager::getInstance().get("about_text") << std::endl;
    std::cout << "\n" << LocalizationManager::getInstance().get("btn_back") << "...";
    _getch();
}