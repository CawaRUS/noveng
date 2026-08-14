#pragma once
#include "miniaudio.h"
#include <string>

class NovelEngine;

struct MenuResult {
    enum Action {
        Exit,
        NewGame,
        Continue,
        LoadSlot,
        SaveSlot,
        Settings,
        About
    };
    Action action = Exit;
    int slot = 0;
    bool isAutosave = false;
};

struct SaveSlotSelection {
    bool valid = false;
    bool isAutosave = false;
    int slot = 0;
};

struct PauseResult {
    enum Action {
        Resume,
        Load,
        Save,
        Settings,
        MainMenu
    };
    Action action = Resume;
    int slot = 0;
    bool isAutosave = false;
};

class MainMenu {
public:
    void playIntro(ma_engine* audio);
    void showSettings();
    void applySettings();
    MenuResult show();
    void setEngine(NovelEngine* eng);
public:
    PauseResult showPauseScreen();

private:
    void showAbout();
    SaveSlotSelection showSaveLoadScreen(bool saveMode);
    NovelEngine* engine = nullptr;
    ma_sound menuMusic;
    ma_sound hoverSfx;
};
