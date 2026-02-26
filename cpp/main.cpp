#include "common.hpp"
#include "menu.hpp"
#include "engine.hpp"
#include "setting.hpp"
#include <conio.h>
#include "localisation.hpp"

int main() {
    SettingsManager::getInstance().load();
    LocalizationManager::getInstance().loadLanguage(SettingsManager::getInstance().get().language);
    setupConsole();

    MainMenu menu;
    NovelEngine game;
    menu.playIntro(game.getAudio());

    while (true) {
        int result = menu.show(); 
        
        if (result == 1 || result == 2) {
            clearScreen();
            
            std::string currentFile;

            if (result == 2) {
                if (game.loadGame(1)) {
                    currentFile = game.getNextChapter();
                    game.resetChapterFlag(); 
                } else {
                    currentFile = "res/scenario/scenario.txt";
                }
            } else {
                // Новая игра
                currentFile = "res/scenario/scenario.txt";
                game.currentEventIdx = 0;
            }
            
            while (!currentFile.empty()) {
                game.clearEvents();
                if (game.loadScenario(currentFile)) {
                    game.applySettings();
                    game.resetChapterFlag(); 

                    game.run(); 
                    
                    if (game.isChapterFinished()) {
                        currentFile = game.getNextChapter();
                        game.resetChapterFlag();
                        game.currentEventIdx = 0;
                        clearScreen();
                    } else {
                        currentFile = ""; 
                    }
                } else {
                    break;
                }
            }
            std::cout << LocalizationManager::getInstance().get("game_over_prompt") << std::endl;
            _getch();
            game.stopAudio();
        } else if (result == 0) {
            break;
        }
    }

    return 0;
}