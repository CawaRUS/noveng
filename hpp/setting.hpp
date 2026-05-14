#pragma once
#include <string>
#include "json.hpp"

using json = nlohmann::json;

#ifndef DEFAULT_LANG
#define DEFAULT_LANG "ru"
#endif

struct ConfigData {
    float musicVolume = 0.5f;
    int typingSpeed = 30;
    std::string language = DEFAULT_LANG;
    int historySize = 20;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ConfigData, musicVolume, typingSpeed, language, historySize)

class SettingsManager {
public:
    static SettingsManager& getInstance() {
        static SettingsManager instance;
        return instance;
    }

    void load();
    void save();

    ConfigData& get() { return data; }

private:
    SettingsManager() {}
    ConfigData data;
    const std::string filePath = std::string(DIR_RES) + "/json/settings.json";
};