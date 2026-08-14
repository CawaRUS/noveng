#include "menu.hpp"
#include "common.hpp"
#include "crypto_wrapper.hpp"
#include "save_manager.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <conio.h>
#include <chrono>
#include "setting.hpp"
#include <thread>
#include <filesystem>
#include "localisation.hpp"
#include "logger.hpp"
#include <regex>

namespace fs = std::filesystem;

static bool isMusicReady = false;
static bool isHoverReady = false;
static bool decodersInitialized = false;

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
    Logger::getInstance().info("Starting Intro sequence...");
    auto loadAsset = [](const std::string& path) -> std::vector<char> {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return {};
        std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        #if USE_DECRYPT == 1
            std::string key = ASSET_KEY;
            if (!key.empty()) {
                std::vector<uint8_t> data(buffer.begin(), buffer.end());
                if (CryptoWrapper::decryptBuffer(data, key)) {
                    buffer.assign(data.begin(), data.end());
                }
            }
        #endif
        return buffer;
    };

    std::string hoverPath = (fs::path(DIR_RES) / DIR_SFX / "hover.mp3").string();
    std::string musicPath = (fs::path(DIR_RES) / DIR_MUSIC / "menu.mp3").string();
    static std::vector<char> hoverData = loadAsset(hoverPath);
    static std::vector<char> musicData = loadAsset(musicPath);
    static ma_decoder hoverDec, musicDec;

    if (decodersInitialized) {
        if (isHoverReady) ma_decoder_uninit(&hoverDec);
        if (isMusicReady) ma_decoder_uninit(&musicDec);
        isHoverReady = false;
        isMusicReady = false;
    }

    if (!hoverData.empty()) {
        ma_result res = ma_decoder_init_memory(hoverData.data(), hoverData.size(), NULL, &hoverDec);
        if (res == MA_SUCCESS) {
            if (ma_sound_init_from_data_source(audio, &hoverDec, 0, NULL, &hoverSfx) == MA_SUCCESS) {
                isHoverReady = true;
                decodersInitialized = true;
            }
        } else {
            Logger::getInstance().error("Failed to decode hover SFX from memory!");
        }
    } else {
        Logger::getInstance().error("Hover SFX file not found: " + hoverPath);
    }

    if (!musicData.empty()) {
        ma_result res = ma_decoder_init_memory(musicData.data(), musicData.size(), NULL, &musicDec);
        if (res == MA_SUCCESS) {
            if (ma_sound_init_from_data_source(audio, &musicDec, 0, NULL, &menuMusic) == MA_SUCCESS) {
                ma_sound_set_volume(&menuMusic, SettingsManager::getInstance().get().musicVolume);
                isMusicReady = true;
                decodersInitialized = true;
            }
        } else {
            Logger::getInstance().error("Failed to decode menu music from memory!");
        }
    } else {
        Logger::getInstance().error("Menu music file not found: " + musicPath);
    }

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
                     _   _  _____     _______ _   _  ____ 
                    | \ | |/ _ \ \   / / ____| \ | |/ ___|
                    |  \| | | | \ \ / /|  _| |  \| | |  _ 
                    | |\  | |_| |\ V / | |___| |\  | |_| |
                    |_| \_|\___/  \_/  |_____|_| \_|\____|                                       
    )";

    if (isMusicReady) ma_sound_set_looping(&menuMusic, MA_TRUE);
    if (isMusicReady) ma_sound_start(&menuMusic);

    auto startTime = std::chrono::steady_clock::now();
    bool skipped = false;

    std::cout << "\n\n\n";
    typeWrite("\t\t   ~ " + LocalizationManager::getInstance().get("intro_presents") + " ~\n", 40);
    if (!skipped) skipped = waitOrSkip(1200);

    if (!skipped) {
        clearScreen();
        std::cout << "\n\n";
        for (char c : logo) {
            std::cout << c << std::flush;
            if (c == '\n') {
                std::this_thread::sleep_for(std::chrono::milliseconds(120));
                if (_kbhit() && _getch() == 13) {
                    skipped = true;
                    break;
                }
            }
        }
    }

    if (!skipped) skipped = waitOrSkip(800);

    if (!skipped) {
        std::cout << "\n";
        typeWrite("\t\t       [ " + LocalizationManager::getInstance().get("intro_start") + " ]", 35);
        skipped = waitOrSkip(500);
    }

    if (!skipped) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        long long remaining = 13000 - elapsed;
        if (remaining > 0) {
            skipped = waitOrSkip(static_cast<int>(remaining));
        }
    }

    if (skipped) {
        Logger::getInstance().info("Intro skipped by user.");
        ma_uint64 frame = ma_engine_get_sample_rate(audio) * 11.6;
        if (isMusicReady) ma_sound_seek_to_pcm_frame(&menuMusic, frame);
    }
}

void MainMenu::showSettings() {
    Logger::getInstance().info("Entering Settings menu.");
    auto& settings = SettingsManager::getInstance().get();
    int selected = 0;
    const int maxOptions = 4;

    while (true) {
        clearScreen();
        std::cout << "\n    --- " << LocalizationManager::getInstance().get("settings_title") << " ---\n\n";

        if (selected == 0) std::cout << " > "; else std::cout << "   ";
        std::cout << LocalizationManager::getInstance().get("setting_volume") << ": " << (int)(settings.musicVolume * 100) << "%\n";

        if (selected == 1) std::cout << " > "; else std::cout << "   ";
        std::cout << LocalizationManager::getInstance().get("setting_speed") << ": " << settings.typingSpeed << " ms\n";

        if (selected == 2) std::cout << " > "; else std::cout << "   ";
        std::cout << LocalizationManager::getInstance().get("setting_lang") << ": " << settings.language << "\n";

        if (selected == 3) std::cout << " > "; else std::cout << "   ";
        std::cout << LocalizationManager::getInstance().get("setting_history") << ": " << settings.historySize << "\n";

        std::cout << "\n [ Esc - " << LocalizationManager::getInstance().get("btn_save_exit") << " | \x1B[2D\x1B[2C - " << LocalizationManager::getInstance().get("btn_change") << " ]";

        int key = _getch();
        if (key == 27) break; // ESC

        if (key == 0xE0 || key == 0) {
            int prevSelected = selected;
            key = _getch();
            if (key == 72) selected = (selected - 1 + maxOptions) % maxOptions; // Вверх
            if (key == 80) selected = (selected + 1) % maxOptions; // Вниз

            if (selected != prevSelected) {
                if (isHoverReady) ma_sound_seek_to_pcm_frame(&hoverSfx, 0);
                if (isHoverReady) ma_sound_start(&hoverSfx);
            }

            if (selected == 0) {
                if (key == 75) settings.musicVolume = std::max(0.0f, settings.musicVolume - 0.05f);
                if (key == 77) settings.musicVolume = std::min(1.0f, settings.musicVolume + 0.05f);
                if (isMusicReady) ma_sound_set_volume(&menuMusic, settings.musicVolume);
            }
            if (selected == 1) {
                if (key == 75) settings.typingSpeed = std::max(0, settings.typingSpeed - 5);
                if (key == 77) settings.typingSpeed = std::min(200, settings.typingSpeed + 5);
            }
            if (selected == 2) {
                if (key == 75 || key == 77) {
                    LocalizationManager::getInstance().switchLanguage(key == 77, settings.language);
                    Logger::getInstance().info("Language switched to: " + settings.language);
                }
            }
            if (selected == 3) {
                if (key == 75) settings.historySize = std::max(5, settings.historySize - 5);
                if (key == 77) settings.historySize = std::min(50, settings.historySize + 5);
            }
        }
    }
    SettingsManager::getInstance().save();
    Logger::getInstance().info("Settings saved and exited.");
}

void MainMenu::setEngine(NovelEngine* eng) {
    engine = eng;
}

MenuResult MainMenu::show() {
    if (isMusicReady && ma_sound_is_playing(&menuMusic) == MA_FALSE) {
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
        SaveManager::SaveInfo latestSave = SaveManager::getLatestSave();
        bool hasSave = latestSave.exists;

        auto& lm = LocalizationManager::getInstance();

        struct MenuItem {
            std::string text;
            MenuResult::Action action;
        };

        std::vector<MenuItem> items;
        if (hasSave) items.push_back({lm.get("menu_continue"), MenuResult::Continue});
        items.push_back({lm.get("menu_load"), MenuResult::LoadSlot});
        items.push_back({lm.get("menu_new_game"), MenuResult::NewGame});
        items.push_back({lm.get("menu_settings"), MenuResult::Settings});
        items.push_back({lm.get("menu_about"), MenuResult::About});
        items.push_back({lm.get("menu_exit"), MenuResult::Exit});

        const int itemCount = static_cast<int>(items.size());
        const int innerWidth = 36;

        clearScreen();

        std::cout << "\n\n     \x1B[1;36m╔════════════════════════════════════╗\x1B[0m\n";

        std::string titleText = std::string(APP_NAME) + "  v" + std::string(APP_VERSION);
        int titleLen = getVisibleLength(titleText);
        int padLeft = (innerWidth - titleLen) / 2;
        int padRight = innerWidth - titleLen - padLeft;

        std::cout << "     \x1B[1;36m║\x1B[0m" << std::string(padLeft, ' ') << titleText << std::string(padRight, ' ') << "\x1B[1;36m║\x1B[0m\n";

        std::cout << "     \x1B[1;36m╠════════════════════════════════════╣\x1B[0m\n";
        std::cout << "     \x1B[1;36m║\x1B[0m" << std::string(innerWidth, ' ') << "\x1B[1;36m║\x1B[0m\n";

        for (int i = 0; i < itemCount; i++) {
            std::string prefix = (i == selected) ? " > " : "   ";
            std::string text = items[i].text;
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
                if (isHoverReady) ma_sound_seek_to_pcm_frame(&hoverSfx, 0);
                if (isHoverReady) ma_sound_start(&hoverSfx);
            }
            continue;
        }

        if (key == 13) {
            cursorInfo.bVisible = true;
            SetConsoleCursorInfo(hConsole, &cursorInfo);

            MenuResult::Action action = items[selected].action;
            Logger::getInstance().info("Menu selection: action " + std::to_string(action));

            if (action == MenuResult::LoadSlot || action == MenuResult::SaveSlot) {
                bool saveMode = (action == MenuResult::SaveSlot);
                SaveSlotSelection sel = showSaveLoadScreen(saveMode);
                if (sel.valid) {
                    if (isMusicReady) ma_sound_stop(&menuMusic);
                    if (saveMode) {
                        MenuResult result;
                        result.action = MenuResult::SaveSlot;
                        result.slot = sel.slot;
                        result.isAutosave = sel.isAutosave;
                        return result;
                    } else {
                        MenuResult result;
                        result.action = MenuResult::LoadSlot;
                        result.slot = sel.slot;
                        result.isAutosave = sel.isAutosave;
                        return result;
                    }
                }
            } else if (action == MenuResult::Continue) {
                if (isMusicReady) ma_sound_stop(&menuMusic);
                return MenuResult{MenuResult::Continue, 0, false};
            } else if (action == MenuResult::NewGame) {
                if (isMusicReady) ma_sound_stop(&menuMusic);
                return MenuResult{MenuResult::NewGame, 0, false};
            } else if (action == MenuResult::Exit) {
                return MenuResult{MenuResult::Exit, 0, false};
            } else if (action == MenuResult::Settings) {
                showSettings();
            } else if (action == MenuResult::About) {
                showAbout();
            }

            cursorInfo.bVisible = false;
            SetConsoleCursorInfo(hConsole, &cursorInfo);
        }

        if (key == 27) return MenuResult{MenuResult::Exit, 0, false};
    }
}

SaveSlotSelection MainMenu::showSaveLoadScreen(bool saveMode) {
    SaveSlotSelection result;
    auto& lm = LocalizationManager::getInstance();

    auto manualSaves = SaveManager::listManualSaves();
    auto autosaves = SaveManager::listAutosaves();

    struct SlotItem {
        bool isAutosave;
        int slot;
        bool exists;
        SaveManager::SaveInfo info;
    };

    std::vector<SlotItem> slots;

    if (saveMode) {
        // Show fixed range of manual slots plus a "new slot" entry.
        int maxShownSlot = 5;
        for (const auto& s : manualSaves) {
            maxShownSlot = std::max(maxShownSlot, s.slot);
        }
        for (int i = 1; i <= maxShownSlot + 1; ++i) {
            SlotItem item{};
            item.isAutosave = false;
            item.slot = i;
            auto it = std::find_if(manualSaves.begin(), manualSaves.end(),
                [i](const SaveManager::SaveInfo& s) { return s.slot == i; });
            item.exists = it != manualSaves.end();
            if (item.exists) item.info = *it;
            slots.push_back(item);
        }
    } else {
        // Load mode: show existing autosaves and manual saves.
        for (const auto& s : autosaves) {
            SlotItem item{};
            item.isAutosave = true;
            item.slot = s.slot;
            item.exists = true;
            item.info = s;
            slots.push_back(item);
        }
        for (const auto& s : manualSaves) {
            SlotItem item{};
            item.isAutosave = false;
            item.slot = s.slot;
            item.exists = true;
            item.info = s;
            slots.push_back(item);
        }
    }

    if (slots.empty()) {
        clearScreen();
        std::cout << "\n\n     " << lm.get("save_empty") << "\n";
        std::cout << "\n     " << lm.get("btn_back") << "...";
        _getch();
        return result;
    }

    int selected = 0;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = false;
    SetConsoleCursorInfo(hConsole, &cursorInfo);

    const int innerWidth = 70;

    auto clampChapterName = [](const std::string& name) -> std::string {
        if (name.length() <= 22) return name;
        return name.substr(0, 19) + "...";
    };

    while (true) {
        clearScreen();

        std::string titleKey = saveMode ? "save_title_save" : "save_title_load";
        std::string titleText = lm.get(titleKey);
        int titleLen = getVisibleLength(titleText);
        int padLeft = (innerWidth - titleLen) / 2;
        int padRight = innerWidth - titleLen - padLeft;

        std::cout << "\n\n     \x1B[1;36m+" << std::string(innerWidth, '=') << "+\x1B[0m\n";
        std::cout << "     \x1B[1;36m|\x1B[0m" << std::string(padLeft, ' ') << titleText << std::string(padRight, ' ') << "\x1B[1;36m|\x1B[0m\n";
        std::cout << "     \x1B[1;36m+" << std::string(innerWidth, '=') << "+\x1B[0m\n";

        for (size_t i = 0; i < slots.size(); ++i) {
            const auto& slot = slots[i];
            std::string line;
            if (slot.exists) {
                std::string typeLabel = slot.isAutosave ? lm.get("save_slot_autosave") : lm.get("save_slot_manual");
                std::string chapter = slot.info.chapterName.empty() ? "-" : clampChapterName(slot.info.chapterName);
                std::string ts = slot.info.timestamp;
                if (ts.length() > 16) ts = ts.substr(0, 16);
                line = "[#" + std::to_string(slot.slot) + "] " + typeLabel
                     + " | " + ts
                     + " | " + chapter
                     + " | " + std::to_string(slot.info.progressPercent) + "%";
            } else {
                line = "[#" + std::to_string(slot.slot) + "] " + lm.get("save_slot_empty");
            }

            std::string prefix = (i == static_cast<size_t>(selected)) ? " > " : "   ";
            std::string fullLine = prefix + line;
            int visibleLen = getVisibleLength(fullLine);
            int spacesToAdd = innerWidth - visibleLen - 2;
            if (spacesToAdd < 0) spacesToAdd = 0;

            std::cout << "     \x1B[1;36m|\x1B[0m  ";
            if (i == static_cast<size_t>(selected)) {
                std::cout << "\x1B[1;33m" << fullLine << "\x1B[0m";
            } else {
                std::cout << fullLine;
            }
            std::cout << std::string(spacesToAdd, ' ') << "\x1B[1;36m|\x1B[0m\n";
        }

        std::cout << "     \x1B[1;36m+" << std::string(innerWidth, '=') << "+\x1B[0m\n";
        std::cout << "\n     [ " << (saveMode ? lm.get("save_hint_save") : lm.get("save_hint_load")) << " ]\n";

        int key = _getch();

        if (key == 0xE0 || key == 0) {
            int prevSelected = selected;
            key = _getch();
            if (key == 72) selected = (selected - 1 + static_cast<int>(slots.size())) % static_cast<int>(slots.size());
            if (key == 80) selected = (selected + 1) % static_cast<int>(slots.size());

            if (selected != prevSelected) {
                if (isHoverReady) ma_sound_seek_to_pcm_frame(&hoverSfx, 0);
                if (isHoverReady) ma_sound_start(&hoverSfx);
            }
            continue;
        }

        if (key == 13) {
            const auto& slot = slots[selected];
            if (!saveMode && !slot.exists) continue;
            result.valid = true;
            result.slot = slot.slot;
            result.isAutosave = slot.isAutosave;
            cursorInfo.bVisible = true;
            SetConsoleCursorInfo(hConsole, &cursorInfo);
            clearScreen();
            return result;
        }

        if (key == 83 || key == 115) { // Del / s (lower/upper) - use Delete scan code
            // In load mode allow deleting existing saves.
            if (!saveMode) {
                const auto& slot = slots[selected];
                if (slot.exists) {
                    SaveManager::deleteSave(slot.slot, slot.isAutosave);
                    // Rebuild list
                    manualSaves = SaveManager::listManualSaves();
                    autosaves = SaveManager::listAutosaves();
                    slots.clear();
                    for (const auto& s : autosaves) {
                        slots.push_back({true, s.slot, true, s});
                    }
                    for (const auto& s : manualSaves) {
                        slots.push_back({false, s.slot, true, s});
                    }
                    if (slots.empty()) {
                        cursorInfo.bVisible = true;
                        SetConsoleCursorInfo(hConsole, &cursorInfo);
                        return result;
                    }
                    if (selected >= static_cast<int>(slots.size())) selected = static_cast<int>(slots.size()) - 1;
                }
            }
        }

        if (key == 27) {
            cursorInfo.bVisible = true;
            SetConsoleCursorInfo(hConsole, &cursorInfo);
            return result;
        }
    }
}

PauseResult MainMenu::showPauseScreen() {
    PauseResult result;
    auto& lm = LocalizationManager::getInstance();

    struct PauseItem {
        std::string text;
        PauseResult::Action action;
    };

    std::vector<PauseItem> items = {
        {lm.get("pause_resume"), PauseResult::Resume},
        {lm.get("pause_load"), PauseResult::Load},
        {lm.get("pause_save"), PauseResult::Save},
        {lm.get("pause_settings"), PauseResult::Settings},
        {lm.get("pause_main_menu"), PauseResult::MainMenu}
    };

    int selected = 0;
    const int itemCount = static_cast<int>(items.size());
    const int innerWidth = 36;

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = false;
    SetConsoleCursorInfo(hConsole, &cursorInfo);

    while (true) {
        clearScreen();

        std::string titleText = lm.get("pause_title");
        int titleLen = getVisibleLength(titleText);
        int padLeft = (innerWidth - titleLen) / 2;
        int padRight = innerWidth - titleLen - padLeft;

        std::cout << "\n\n     \x1B[1;36m+" << std::string(innerWidth, '=') << "+\x1B[0m\n";
        std::cout << "     \x1B[1;36m|\x1B[0m" << std::string(padLeft, ' ') << titleText << std::string(padRight, ' ') << "\x1B[1;36m|\x1B[0m\n";
        std::cout << "     \x1B[1;36m+" << std::string(innerWidth, '=') << "+\x1B[0m\n";
        std::cout << "     \x1B[1;36m|\x1B[0m" << std::string(innerWidth, ' ') << "\x1B[1;36m|\x1B[0m\n";

        for (int i = 0; i < itemCount; i++) {
            std::string prefix = (i == selected) ? " > " : "   ";
            std::string fullLine = prefix + items[i].text;
            int visibleLen = getVisibleLength(fullLine);
            int spacesToAdd = innerWidth - visibleLen - 2;
            if (spacesToAdd < 0) spacesToAdd = 0;

            std::cout << "     \x1B[1;36m|\x1B[0m  ";
            if (i == selected) {
                std::cout << "\x1B[1;33m" << fullLine << "\x1B[0m";
            } else {
                std::cout << fullLine;
            }
            std::cout << std::string(spacesToAdd, ' ') << "\x1B[1;36m|\x1B[0m\n";
        }

        std::cout << "     \x1B[1;36m|\x1B[0m" << std::string(innerWidth, ' ') << "\x1B[1;36m|\x1B[0m\n";
        std::cout << "     \x1B[1;36m+" << std::string(innerWidth, '=') << "+\x1B[0m\n";
        std::cout << "\n     [ " << lm.get("pause_hint") << " ]\n";

        int key = _getch();

        if (key == 0xE0 || key == 0) {
            int prevSelected = selected;
            key = _getch();
            if (key == 72) selected = (selected - 1 + itemCount) % itemCount;
            if (key == 80) selected = (selected + 1) % itemCount;

            if (selected != prevSelected) {
                if (isHoverReady) ma_sound_seek_to_pcm_frame(&hoverSfx, 0);
                if (isHoverReady) ma_sound_start(&hoverSfx);
            }
            continue;
        }

        if (key == 13) {
            PauseResult::Action action = items[selected].action;

            if (action == PauseResult::Load) {
                SaveSlotSelection sel = showSaveLoadScreen(false);
                if (sel.valid) {
                    result.action = PauseResult::Load;
                    result.slot = sel.slot;
                    result.isAutosave = sel.isAutosave;
                    cursorInfo.bVisible = true;
                    SetConsoleCursorInfo(hConsole, &cursorInfo);
                    return result;
                }
                continue;
            }

            if (action == PauseResult::Save) {
                SaveSlotSelection sel = showSaveLoadScreen(true);
                if (sel.valid) {
                    result.action = PauseResult::Save;
                    result.slot = sel.slot;
                    result.isAutosave = false;
                    cursorInfo.bVisible = true;
                    SetConsoleCursorInfo(hConsole, &cursorInfo);
                    return result;
                }
                continue;
            }

            if (action == PauseResult::Settings) {
                showSettings();
                continue;
            }

            result.action = action;
            cursorInfo.bVisible = true;
            SetConsoleCursorInfo(hConsole, &cursorInfo);
            return result;
        }

        if (key == 27) {
            result.action = PauseResult::Resume;
            cursorInfo.bVisible = true;
            SetConsoleCursorInfo(hConsole, &cursorInfo);
            return result;
        }
    }
}

void MainMenu::showAbout() {
    Logger::getInstance().info("Showing About screen.");
    clearScreen();
    auto& lang = LocalizationManager::getInstance();
    std::cout << "\x1B[1;32m" << "--- " << APP_NAME << " INFO ---" << "\x1B[0m\n\n";
    std::cout << lang.get("about_text") << std::endl;

    #if USE_CUSTOM_ABOUT == 1
        std::cout << "\n\x1B[1;33m" << "--- " << lang.get("extra_info_header") << " ---" << "\x1B[0m\n";
        std::cout << lang.get("about_extra_desc") << std::endl;
        std::cout << APP_NAME << " Version: " << APP_VERSION << std::endl;
    #endif

    std::cout << "\nDeveloped by: \x1B]8;;https://cawas.duckdns.org/me.html\x1B\\\x1B[1;34mTheCawa\x1B[0m\x1B]8;;\x1B\\" << std::endl;
    std::cout << "\n" << lang.get("btn_back") << "...";
    _getch();
}