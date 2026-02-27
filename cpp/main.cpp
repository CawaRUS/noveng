#include "common.hpp"
#include "menu.hpp"
#include "engine.hpp"
#include "setting.hpp"
#include <conio.h>
#include "localisation.hpp"
#include <filesystem>

namespace fs = std::filesystem;

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
                    currentFile = game.currentChapterFile; 
                } else {
                    currentFile = (fs::path(DIR_RES) / DIR_SCENARIO / "scenario.txt").string();
                }
            } else {
                currentFile = (fs::path(DIR_RES) / DIR_SCENARIO / "scenario.txt").string();
                game.currentEventIdx = 0;
            }
            
            while (!currentFile.empty()) {
                game.clearEvents();
                if (game.loadScenario(currentFile)) {
                    game.applySettings();
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
                    std::cerr << "Error: Cannot load " << currentFile << std::endl;
                    break;
                }
            }
            
            std::cout << "\n" << LocalizationManager::getInstance().get("game_over_prompt") << std::endl;
            _getch();
            game.stopAudio();
        } else if (result == 0) {
            break;
        }
    }

    return 0;
}