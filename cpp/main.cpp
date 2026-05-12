#include "common.hpp"
#include "menu.hpp"
#include "engine.hpp"
#include "setting.hpp"
#include <conio.h>
#include "localisation.hpp"
#include "logger.hpp"
#include <filesystem>

namespace fs = std::filesystem;

int main() {
    auto& logger = Logger::getInstance();
    logger.info("=== Application Started: " + std::string(APP_NAME) + " v" + std::string(APP_VERSION) + " ===");

    try {
        logger.info("Loading settings...");
        SettingsManager::getInstance().load();

        logger.info("Setting up localization (" + SettingsManager::getInstance().get().language + ")...");
        LocalizationManager::getInstance().loadLanguage(SettingsManager::getInstance().get().language);

        logger.info("Initializing console graphics...");
        setupConsole();

        MainMenu menu;
        NovelEngine game;
        game.registerCommands();
        

        logger.info("Playing intro...");
        menu.playIntro(game.getAudio());
        

        while (true) {
            int result = menu.show(); 
            
            if (result == 1 || result == 2) {
                logger.info(result == 1 ? "Starting New Game" : "Loading Save Slot 1");
                clearScreen();
                std::string currentFile;

                if (result == 2) {
                    if (game.loadGame(1)) {
                        currentFile = game.currentChapterFile; 
                        logger.info("Game loaded. Current chapter: " + currentFile);
                    } else {
                        logger.warn("Save file not found or corrupted. Starting from default scenario.");
                        currentFile = (fs::path(DIR_RES) / DIR_SCENARIO / "scenario.txt").string();
                    }
                } else {
                    currentFile = (fs::path(DIR_RES) / DIR_SCENARIO / "scenario.txt").string();
                    game.currentEventIdx = 0;
                }
                
                while (!currentFile.empty()) {
                    game.clearEvents();
                    logger.debug("Attempting to load scenario: " + currentFile);

                    if (!fs::exists(currentFile)) {
                        logger.error("Scenario file not found: " + currentFile);
                        std::cout << "\n[ERROR] " << LocalizationManager::getInstance().get("game_over_prompt") << std::endl;
                        std::cout << "File: " << currentFile << std::endl;
                        _getch();
                        break;
                    }

                    if (game.loadScenario(currentFile)) {
                        game.applySettings();
                        logger.info("Running scenario: " + currentFile);
                        game.run(); 
                        
                        if (game.isChapterFinished()) {
                            std::string oldFile = currentFile;
                            currentFile = game.getNextChapter();
                            logger.info("Chapter transition: " + oldFile + " -> " + currentFile);
                            
                            game.resetChapterFlag();
                            game.currentEventIdx = 0;
                            clearScreen();
                        } else {
                            currentFile = "";
                        }
                    } else {
                        logger.error("CRITICAL: Cannot load scenario file: " + currentFile);
                        break;
                    }
                }
                
                logger.info("Returning to main menu.");
                std::cout << "\n" << LocalizationManager::getInstance().get("game_over_prompt") << std::endl;
                _getch();
                game.stopAudio();
            } else if (result == 0) {
                logger.info("Exiting application via menu.");
                break;
            }
        }
    } catch (const std::exception& e) {
        logger.error("UNHANDLED EXCEPTION: " + std::string(e.what()));
    } catch (...) {
        logger.error("UNKNOWN FATAL ERROR occurred.");
    }

    logger.info("=== Application Closed ===");
    return 0;
}