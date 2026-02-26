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
namespace fs = std::filesystem;

void MainMenu::playIntro(ma_engine* audio) {
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
            '  '--'\\ '-'  ||   .'.   |\ '-'  |.-'  `)    |  ||  ||  |\ `--. 
            `-----' `--`--''--'   '--' `--`--'`----'     `--'`--''--' `---' 
                                                                    
    )";

    ma_sound_init_from_file(audio, "res/music/menu.mp3", 0, NULL, NULL, &menuMusic);
    ma_sound_set_looping(&menuMusic, MA_TRUE);
    ma_sound_start(&menuMusic);

    auto startTime = std::chrono::steady_clock::now();
    bool skipped = false;

    std::cout << "\n\n\n";
    typeWrite("\t\t   ~ T H E   T H E C A W A   P R E S E N T S ~\n", 40);
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
    typeWrite("\t\t      [ Press ENTER to start ]", 35);
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

    while (true) {
        clearScreen();
        std::cout << "\n     --- НАСТРОЙКИ ---\n\n";
        
        // Рендерим пункты
        if (selected == 0) std::cout << " > "; else std::cout << "   ";
        std::cout << "Громкость музыки: " << (int)(settings.musicVolume * 100) << "%\n";

        if (selected == 1) std::cout << " > "; else std::cout << "   ";
        std::cout << "Скорость текста: " << settings.typingSpeed << " мс\n";

        std::cout << "\n [ Esc - Сохранить и выйти | ← → - Изменить ]";

        int key = _getch();
        if (key == 27) break; // ESC

        if (key == 0xE0 || key == 0) {
            key = _getch();
            if (key == 72) selected = 0; // Вверх
            if (key == 80) selected = 1; // Вниз
            
            if (selected == 0) {
                if (key == 75) settings.musicVolume = std::max(0.0f, settings.musicVolume - 0.05f); // Влево
                if (key == 77) settings.musicVolume = std::min(1.0f, settings.musicVolume + 0.05f); // Вправо
            }
            if (selected == 1) {
                if (key == 75) settings.typingSpeed = std::max(0, settings.typingSpeed - 5); // Влево
                if (key == 77) settings.typingSpeed = std::min(200, settings.typingSpeed + 5); // Вправо
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

    // Проверяем наличие сейва
    bool hasSave = fs::exists("res/save/save1.json");

    std::vector<std::string> items;
    if (hasSave) items.push_back("  Продолжить   ");
    items.push_back("  Новая игра   ");                 
    items.push_back("  Настройки    ");
    items.push_back("  Об игре      ");
    items.push_back("  Выход        ");

    int selected = 0;
    const int itemCount = static_cast<int>(items.size());

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = false;
    SetConsoleCursorInfo(hConsole, &cursorInfo);

    auto render = [&]() {
        clearScreen();
        std::cout << "\n\n  ╔══════════════════════════╗\n";
        std::cout << "  ║        NOVENG  v0.1      ║\n";
        std::cout << "  ╠══════════════════════════╣\n";
        std::cout << "  ║                          ║\n";

        for (int i = 0; i < itemCount; i++) {
            if (i == selected)
                std::cout << "  ║  > " << items[i] << "       ║\n";
            else
                std::cout << "  ║    " << items[i] << "       ║\n";
        }

        std::cout << "  ║                          ║\n";
        std::cout << "  ╚══════════════════════════╝\n";
        std::cout << "\n  [ ↑ ↓ — навигация | Enter — выбор ]\n";
    };

    while (true) {
        render();
        int key = _getch();

        if (key == 0xE0 || key == 0) {
            key = _getch();
            if (key == 72) selected = (selected - 1 + itemCount) % itemCount; // ↑
            if (key == 80) selected = (selected + 1) % itemCount;             // ↓
            continue;
        }

        if (key == 13) { // Enter
            cursorInfo.bVisible = true;
            SetConsoleCursorInfo(hConsole, &cursorInfo);

            int action = selected;
            if (!hasSave) action += 1;

            switch (action) {
                case 0:
                    ma_sound_stop(&menuMusic);
                    return 2;
                case 1:
                    ma_sound_stop(&menuMusic);
                    return 1; 
                case 2:
                    showSettings();
                    hasSave = fs::exists("res/save/save1.json"); 
                    break;
                case 3: 
                    showAbout(); 
                    break;
                case 4: 
                    return 0; // Выход
            }
        }
        if (key == 27) return 0; // Esc для выхода
    }
}

void MainMenu::showAbout() {
    clearScreen();
    std::cout << "Данный движок был разработан TheCawa ( CawaRUS )" << std::endl;
    std::cout << "\nНажмите Enter, чтобы вернуться...";
    std::cin.get();
}