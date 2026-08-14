#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "engine.hpp"

namespace fs = std::filesystem;

class SaveManager {
public:
    static constexpr int MAX_AUTOSAVES = 5;

    struct SaveInfo {
        int slot = 0;
        bool isAutosave = false;
        std::string timestamp;
        std::string chapterName;
        int progressPercent = 0;
        bool exists = false;
        std::filesystem::file_time_type lastModified;
    };

    static fs::path getSavePath(int slot, bool isAutosave = false);

    static bool saveExists(int slot, bool isAutosave = false);

    // Lists all existing manual saves, sorted by slot number.
    static std::vector<SaveInfo> listManualSaves();

    // Lists all existing autosaves, sorted by timestamp (newest first).
    static std::vector<SaveInfo> listAutosaves();

    // Reads metadata from a save file without modifying the engine state.
    static SaveInfo getSaveInfo(int slot, bool isAutosave = false);

    static bool deleteSave(int slot, bool isAutosave = false);

    // Returns the most recently written save (manual or autosave).
    // If nothing exists, returns SaveInfo with exists == false.
    static SaveInfo getLatestSave();

    // Picks the autosave slot to overwrite (oldest file or first empty slot).
    static int pickAutosaveSlot();

    static std::string getCurrentTimestamp();

private:
    static bool readSaveFile(const fs::path& path, GameState& outState);
};
