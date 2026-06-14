#pragma once
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <iostream>
#include "json.hpp"
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

class LocalizationManager {
private:
    std::map<std::string, std::string> translations;
    std::string currentLang;
    std::vector<std::string> availableLangs;
    size_t currentLangIdx = 0;

    LocalizationManager() : currentLang("ru") {}

public:
    static LocalizationManager& getInstance() {
        static LocalizationManager instance;
        return instance;
    }

    bool loadLanguage(const std::string& langCode) {
        std::string path = std::string(DIR_RES) + "/localisation/" + langCode + ".json";
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Не удалось найти файл локализации: " << path << std::endl;
            return false;
        }

        try {
            json j;
            file >> j;
            translations = j.get<std::map<std::string, std::string>>();
            currentLang = langCode;
        } catch (...) {
            std::cerr << "Ошибка парсинга JSON локализации: " << langCode << std::endl;
            return false;
        }
        return true;
    }

    std::string get(const std::string& key) {
        auto it = translations.find(key);
        if (it != translations.end()) {
            return it->second;
        }
        return key;
    }

    void updateAvailableLanguages() {
        fs::path locDir = fs::path(DIR_RES) / "localisation";
        if (!fs::exists(locDir)) {
            fs::create_directories(locDir);
        }
        
        availableLangs.clear();
        for (const auto& entry : fs::directory_iterator(locDir)) {
            if (entry.path().extension() == ".json") {
                availableLangs.push_back(entry.path().stem().string());
            }
        }
        if (availableLangs.empty()) availableLangs.push_back("ru");
    }

    void switchLanguage(bool next, std::string& currentLangName) {
        if (availableLangs.empty()) updateAvailableLanguages();
        
        for (size_t i = 0; i < availableLangs.size(); ++i) {
            if (availableLangs[i] == currentLangName) {
                currentLangIdx = i;
                break;
            }
        }

        if (next) {
            currentLangIdx = (currentLangIdx + 1) % availableLangs.size();
        } else {
            currentLangIdx = (currentLangIdx - 1 + availableLangs.size()) % availableLangs.size();
        }

        currentLangName = availableLangs[currentLangIdx];
        loadLanguage(currentLangName);
    }
};