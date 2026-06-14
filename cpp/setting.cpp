#include "setting.hpp"
#include "logger.hpp"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

void SettingsManager::load() {
    Logger::getInstance().info("Loading application settings...");

    fs::path parentDir = fs::path(filePath).parent_path();
    if (!fs::exists(parentDir)) {
        Logger::getInstance().debug("Settings directory not found. Creating: " + parentDir.string());
        fs::create_directories(parentDir);
    }

    if (!fs::exists(filePath)) {
        Logger::getInstance().warn("Settings file not found. Creating default: " + filePath);
        save();
        return;
    }

    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open settings file for reading");
        }

        json j;
        file >> j;

        if (j.contains("musicVolume")) data.musicVolume = j["musicVolume"];
        if (j.contains("typingSpeed")) data.typingSpeed = j["typingSpeed"];
        if (j.contains("language")) data.language = j["language"];
        if (j.contains("historySize")) data.historySize = j["historySize"];

        Logger::getInstance().info("Settings loaded successfully. Language: " + data.language +
                                   ", Volume: " + std::to_string((int)(data.musicVolume * 100)) + "%");

        save();
    } catch (const std::exception& e) {
        Logger::getInstance().error("Error parsing settings: " + std::string(e.what()) + ". Resetting to defaults.");
        save();
    }
}

void SettingsManager::save() {
    Logger::getInstance().debug("Saving settings to " + filePath);
    
    std::ofstream file(filePath);
    if (file.is_open()) {
        json j = data;
        file << j.dump(4);
        Logger::getInstance().info("Settings saved successfully.");
    } else {
        Logger::getInstance().error("CRITICAL: Failed to open settings file for writing: " + filePath);
    }
}