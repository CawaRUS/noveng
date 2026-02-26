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
namespace fs = std::filesystem;

void MainMenu::playIntro(ma_engine* audio) {
    ma_sound_init_from_file(audio, "res/sfx/hover.mp3", 0, NULL, NULL, &hoverSfx);
    
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
                                                                                    
            ,-----.                                      ,--.               
            '  .--./ ,--,--.,--.   ,--. ,--,--. ,---.     |  |,--,--,  ,---. 
            |  |    ' ,-.  ||  |.'.|  |' ,-.  |(  .-'     |  ||      \| .--' 
            '  '--'\\ '-'  ||  .'.  |\ '-'  |.-'  `)     |  ||  ||  |\ `--. 
            `-----' `--`--''--'   '--' `--`--'`----'      `--'`--''--' `---' 
                                                                             
    )";

    ma_sound_init_from_file(audio, "res/music/menu.mp3", 0, NULL, NULL, &menuMusic);
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

int getUTF8Length(const std::string& str) {
    int length = 0;
    for (size_t i = 0; i < str.length(); i++) {
        if ((static_cast<unsigned char>(str[i]) & 0xC0) != 0x80) {
            length++;
        }
    }
    return length;
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

        std::cout << "\n [ Esc - " << LocalizationManager::getInstance().get("btn_save_exit") << " | ← → - " << LocalizationManager::getInstance().get("btn_change") << " ]";

        int key = _getch();
        if (key == 27) break; // ESC

        if (key == 0xE0 || key == 0) {
            int prevSelected = selected;
            key = _getch();
            if (key == 72) selected = (selected - 1 + maxOptions) % maxOptions; // Вверх
            if (key == 80) selected = (selected + 1) % maxOptions; // Вниз

            if (selected != prevSelected) {
                ma_sound_seek_to_pcm_frame(&hoverSfx, 0); // Сброс в начало
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
        bool hasSave = fs::exists("res/save/save1.json");
        auto& lm = LocalizationManager::getInstance();

        std::vector<std::string> items;
        if (hasSave) items.push_back(lm.get("menu_continue"));
        items.push_back(lm.get("menu_new_game"));
        items.push_back(lm.get("menu_settings"));
        items.push_back(lm.get("menu_about"));
        items.push_back(lm.get("menu_exit"));

        const int itemCount = static_cast<int>(items.size());
        const int menuWidth = 34;

        clearScreen();
        std::cout << "\n\n  ╔════════════════════════════════════╗\n";
        std::cout << "  ║           NOVENG  v0.2             ║\n";
        std::cout << "  ╠════════════════════════════════════╣\n";
        std::cout << "  ║                                    ║\n";

        for (int i = 0; i < itemCount; i++) {
            std::string prefix = (i == selected) ? "  > " : "    ";
            std::string text = items[i];
            int visibleLen = getUTF8Length(prefix + text);
            int spacesToAdd = menuWidth - visibleLen;
            if (spacesToAdd < 0) spacesToAdd = 0;
            std::cout << "  ║ " << prefix << text << std::string(spacesToAdd, ' ') << " ║\n";
        }

        std::cout << "  ║                                    ║\n";
        std::cout << "  ╚════════════════════════════════════╝\n";
        std::cout << "\n  [ " << lm.get("menu_hint") << " ]\n";
        
        int key = _getch();

        if (key == 0xE0 || key == 0) {
            int prevSelected = selected; // ТУТ ОБЪЯВЛЯЕМ
            key = _getch();
            if (key == 72) selected = (selected - 1 + itemCount) % itemCount; // ↑
            if (key == 80) selected = (selected + 1) % itemCount;             // ↓
            
            if (selected != prevSelected) {
                ma_sound_seek_to_pcm_frame(&hoverSfx, 0);
                ma_sound_start(&hoverSfx);
            }
            continue;
        }

        if (key == 13) { // Enter
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
    std::cout << LocalizationManager::getInstance().get("about_text") << std::endl;
    std::cout << "\n" << LocalizationManager::getInstance().get("btn_back") << "...";
    _getch();
}