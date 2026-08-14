#include "save_manager.hpp"
#include "crypto_wrapper.hpp"
#include "logger.hpp"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

std::string SaveManager::getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm* timeinfo = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(timeinfo, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool SaveManager::readSaveFile(const fs::path& path, GameState& outState) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    std::string plainText;
    std::string saveKey = CryptoWrapper::deriveSaveKey(ASSET_KEY);

    if (CryptoFormat::hasMagic(buffer) && CryptoWrapper::decryptBuffer(buffer, saveKey)) {
        plainText.assign(buffer.begin(), buffer.end());
    } else {
        plainText.assign(buffer.begin(), buffer.end());
    }

    try {
        json j = json::parse(plainText);
        outState = j.get<GameState>();
        return true;
    } catch (...) {
        return false;
    }
}

fs::path SaveManager::getSavePath(int slot, bool isAutosave) {
    fs::path saveDir = fs::path(DIR_RES) / DIR_SAVE;
    std::string fileName = isAutosave
        ? "autosave_" + std::to_string(slot) + ".json"
        : "save" + std::to_string(slot) + ".json";
    return saveDir / fileName;
}

bool SaveManager::saveExists(int slot, bool isAutosave) {
    return fs::exists(getSavePath(slot, isAutosave));
}

std::vector<SaveManager::SaveInfo> SaveManager::listManualSaves() {
    std::vector<SaveInfo> result;
    fs::path saveDir = fs::path(DIR_RES) / DIR_SAVE;
    if (!fs::exists(saveDir)) return result;

    for (const auto& entry : fs::directory_iterator(saveDir)) {
        std::string name = entry.path().stem().string();
        if (entry.is_regular_file() && name.find("save") == 0 && name.find("autosave") != 0) {
            try {
                int slot = std::stoi(name.substr(4));
                SaveInfo info = getSaveInfo(slot, false);
                if (info.exists) result.push_back(info);
            } catch (...) {
                // Ignore malformed filenames
            }
        }
    }

    std::sort(result.begin(), result.end(), [](const SaveInfo& a, const SaveInfo& b) {
        return a.slot < b.slot;
    });
    return result;
}

std::vector<SaveManager::SaveInfo> SaveManager::listAutosaves() {
    std::vector<SaveInfo> result;
    for (int i = 1; i <= MAX_AUTOSAVES; ++i) {
        SaveInfo info = getSaveInfo(i, true);
        if (info.exists) result.push_back(info);
    }

    std::sort(result.begin(), result.end(), [](const SaveInfo& a, const SaveInfo& b) {
        if (a.lastModified != b.lastModified) {
            return a.lastModified > b.lastModified;
        }
        // Tie-breaker: higher slot number was written later by the round-robin allocator.
        return a.slot > b.slot;
    });
    return result;
}

SaveManager::SaveInfo SaveManager::getSaveInfo(int slot, bool isAutosave) {
    SaveInfo info;
    info.slot = slot;
    info.isAutosave = isAutosave;

    fs::path path = getSavePath(slot, isAutosave);
    if (!fs::exists(path)) return info;

    GameState state;
    if (!readSaveFile(path, state)) return info;

    info.exists = true;
    info.timestamp = state.saveTimestamp;
    info.chapterName = fs::path(state.currentScene).filename().string();
    try {
        info.lastModified = fs::last_write_time(path);
    } catch (...) {
        // leave as default if filesystem fails
    }

    if (state.totalEvents > 0) {
        info.progressPercent = static_cast<int>((state.eventIndex * 100) / state.totalEvents);
    } else {
        info.progressPercent = 0;
    }

    return info;
}

bool SaveManager::deleteSave(int slot, bool isAutosave) {
    fs::path path = getSavePath(slot, isAutosave);
    if (!fs::exists(path)) return false;
    try {
        fs::remove(path);
        Logger::getInstance().info("Deleted save: " + path.string());
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to delete save: " + std::string(e.what()));
        return false;
    }
}

SaveManager::SaveInfo SaveManager::getLatestSave() {
    SaveInfo latest;
    latest.exists = false;

    auto manual = listManualSaves();
    auto autosaves = listAutosaves();

    auto isNewer = [](const SaveInfo& candidate, const SaveInfo& current) -> bool {
        if (!current.exists) return true;
        if (candidate.lastModified != current.lastModified) {
            return candidate.lastModified > current.lastModified;
        }
        // Tie-breaker: prefer higher slot number (assumed to be the last written).
        return candidate.slot > current.slot;
    };

    for (const auto& info : manual) {
        if (isNewer(info, latest)) latest = info;
    }
    for (const auto& info : autosaves) {
        if (isNewer(info, latest)) latest = info;
    }
    return latest;
}

int SaveManager::pickAutosaveSlot() {
    int oldestSlot = 1;
    fs::file_time_type oldestTime;
    bool first = true;

    for (int i = 1; i <= MAX_AUTOSAVES; ++i) {
        fs::path path = getSavePath(i, true);
        if (!fs::exists(path)) {
            return i;
        }

        try {
            auto modified = fs::last_write_time(path);
            if (first || modified < oldestTime) {
                oldestTime = modified;
                oldestSlot = i;
                first = false;
            }
        } catch (...) {
            // If we can't read time, use this slot as fallback
            return i;
        }
    }

    return oldestSlot;
}
