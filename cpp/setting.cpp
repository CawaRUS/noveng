#include "setting.hpp"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

void SettingsManager::load() {
    if (!fs::exists("res/json")) {
        fs::create_directories("res/json");
    }

    if (!fs::exists(filePath)) {
        save();
        return;
    }

    try {
        std::ifstream file(filePath);
        json j;
        file >> j;
        data = j.get<ConfigData>();
    } catch (...) {
        save();
    }
}

void SettingsManager::save() {
    std::ofstream file(filePath);
    if (file.is_open()) {
        json j = data;
        file << j.dump(4);
    }
}