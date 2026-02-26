#pragma once
#include "miniaudio.h"

class MainMenu {
public:
    // Передаем движок, чтобы музыка не прерывалась
    void playIntro(ma_engine* audio); 
    void showSettings();
    void applySettings();
    int show();
private:
    void showAbout();
    ma_sound menuMusic; 
};