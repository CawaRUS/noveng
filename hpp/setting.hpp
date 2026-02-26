#pragma once
#include <string>
#include "json.hpp"

using json = nlohmann::json;

struct ConfigData {
    float musicVolume = 0.5f;
    int typingSpeed = 30;
    std::string language = "ru";
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ConfigData, musicVolume, typingSpeed, language)

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
    const std::string filePath = "res/json/settings.json";
};