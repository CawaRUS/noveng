#include "common.hpp"
#include "menu.hpp"
#include "engine.hpp"
#include "setting.hpp"
#include <conio.h>

int main() {
    SettingsManager::getInstance().load();
    setupConsole();

    MainMenu menu;
    NovelEngine game;
    menu.playIntro(game.getAudio());

    while (true) {
        int result = menu.show(); 
        
        if (result == 1 || result == 2) { // 1 - Новая игра, 2 - Продолжить
            clearScreen();
            
            std::string currentFile;

            if (result == 2) {
                // Пытаемся загрузить данные из JSON
                if (game.loadGame(1)) {
                    currentFile = game.getNextChapter(); // Путь из сейва лег в nextChapterFile
                    game.resetChapterFlag(); 
                } else {
                    currentFile = "res/scenario/scenario.txt";
                }
            } else {
                // Новая игра
                currentFile = "res/scenario/scenario.txt";
                game.currentEventIdx = 0; // Начинаем с начала
            }
            
            while (!currentFile.empty()) {
                game.clearEvents();
                if (game.loadScenario(currentFile)) {
                    game.applySettings();
                    
                    
                    game.run();
                    
                    if (game.isChapterFinished()) {
                        currentFile = game.getNextChapter();
                        game.resetChapterFlag();
                        game.currentEventIdx = 0; // Новая глава всегда с 0
                        game.stopAudio();
                        clearScreen();
                    } else {
                        currentFile = ""; 
                    }
                } else {
                    std::cerr << "Ошибка загрузки: " << currentFile << std::endl;
                    break;
                }
            }
            std::cout << "\nКонец игры или выход в меню. Нажмите любую клавишу...";
            _getch();
            game.stopAudio();
        } else if (result == 0) {
            break;
        }
    }

    return 0;
}