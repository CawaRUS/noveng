#include "common.hpp"
#include "menu.hpp"
#include "engine.hpp"
#include "setting.hpp"
#include "save_manager.hpp"
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
        menu.setEngine(&game);
        game.setMenu(&menu);

        logger.info("Playing intro...");
        menu.playIntro(game.getAudio());

        while (true) {
            MenuResult result = menu.show();

            if (result.action == MenuResult::Exit) {
                logger.info("Exiting application via menu.");
                break;
            }

            if (result.action == MenuResult::SaveSlot) {
                if (game.saveGame(result.slot)) {
                    logger.info("Game saved to slot " + std::to_string(result.slot));
                    std::cout << "\n" << LocalizationManager::getInstance().get("save_success") << std::endl;
                } else {
                    logger.error("Failed to save game to slot " + std::to_string(result.slot));
                    std::cout << "\n[ERROR] Failed to save game." << std::endl;
                }
                std::cout << "\n" << LocalizationManager::getInstance().get("btn_back") << "...";
                _getch();
                continue;
            }

            if (result.action == MenuResult::NewGame || result.action == MenuResult::Continue || result.action == MenuResult::LoadSlot) {
                std::string actionName;
                switch (result.action) {
                    case MenuResult::NewGame: actionName = "Starting New Game"; break;
                    case MenuResult::Continue: actionName = "Loading Latest Save"; break;
                    case MenuResult::LoadSlot: actionName = "Loading Save Slot"; break;
                    default: break;
                }
                logger.info(actionName);
                clearScreen();
                std::string currentFile;

                if (result.action == MenuResult::Continue) {
                    SaveManager::SaveInfo latest = SaveManager::getLatestSave();
                    if (latest.exists && game.loadGame(latest.slot, latest.isAutosave)) {
                        currentFile = game.currentChapterFile;
                        logger.info("Game loaded from " + std::string(latest.isAutosave ? "autosave" : "save") +
                                    " slot " + std::to_string(latest.slot) + ". Current chapter: " + currentFile);
                    } else {
                        logger.warn("Save file not found or corrupted. Starting from default scenario.");
                        currentFile = (fs::path(DIR_RES) / DIR_SCENARIO / "demo_0_6.txt").string();
                    }
                } else if (result.action == MenuResult::LoadSlot) {
                    if (game.loadGame(result.slot, result.isAutosave)) {
                        currentFile = game.currentChapterFile;
                        logger.info("Game loaded from " + std::string(result.isAutosave ? "autosave" : "save") +
                                    " slot " + std::to_string(result.slot) + ". Current chapter: " + currentFile);
                    } else {
                        logger.warn("Save file not found or corrupted. Starting from default scenario.");
                        currentFile = (fs::path(DIR_RES) / DIR_SCENARIO / "demo_0_6.txt").string();
                    }
                } else {
                    currentFile = (fs::path(DIR_RES) / DIR_SCENARIO / "demo_0_6.txt").string();
                    game.currentEventIdx = 0;
                    game.dialogCountSinceAutosave = 0;
                }

                clearScreen();

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
                        clearScreen();
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