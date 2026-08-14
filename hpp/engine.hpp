#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "miniaudio.h"
#include "common.hpp"
#include "json.hpp"
#include "variant.hpp"
#include "command.hpp"

using json = nlohmann::json;

std::vector<std::string> split(const std::string& s, char delimiter);

// Validates a scenario filename and returns an absolute path inside DIR_RES/DIR_SCENARIO.
// Returns empty string if the name is unsafe or does not have a .txt extension.
std::string resolveScenarioPath(const std::string& filename);

enum class EventType { TEXT, COMMAND };

struct SceneEvent {
    EventType type;
    std::string name;
    std::string content;
};

struct LogEntry {
    std::string speaker;
    std::string text;
    std::string color;
};

struct ActiveSound {
    std::vector<char> data;
    ma_decoder decoder;
    ma_sound sound;
    bool initialized = false;
    ~ActiveSound() {
        if (initialized) {
            ma_sound_uninit(&sound);
            ma_decoder_uninit(&decoder);
        }
    }
};
struct GameState {
    std::string currentScene;
    size_t eventIndex;
    std::map<std::string, std::string> characterColors;
    std::string currentMusic;
    std::map<std::string, float> characterPitches;
    std::map<std::string, Variant> variables;
    std::string currentSpeaker;
    std::string saveTimestamp;
    size_t totalEvents = 0;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GameState, currentScene, eventIndex, characterColors, characterPitches, currentMusic, variables, currentSpeaker, saveTimestamp, totalEvents)
};

struct PersistentState {
    std::map<std::string, Variant> variables;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(PersistentState, variables)
};

class MainMenu;

class NovelEngine {
public:
    NovelEngine();
    std::vector<LogEntry> history;
    int offsetX = 0;
    int offsetY = 0;
    std::string lastFullText = "";
    std::string lastSpeaker = "";
    void render();
    ~NovelEngine();  
    void applySettings();
    ma_engine audio;
    std::map<std::string, Variant> variables;
    std::map<std::string, Variant> persistentVariables;
    std::map<std::string, std::string> characterColors;
    std::map<std::string, float> characterPitches;
    std::vector<std::unique_ptr<ActiveSound>> activeSounds;
    std::unique_ptr<ActiveSound> musicSound;
    std::mutex soundsMutex;
    std::string currentMusicFile = "";
    bool chapterFinished = false;
    bool musicMuted = false;
    float preMuteVolume = 1.0f;
    ma_engine* getAudio() { return &audio; }
    void clearEvents() { events.clear(); }
    bool isChapterFinished() { return chapterFinished; }
    std::string getNextChapter() { return nextChapterFile; }
    void resetChapterFlag() { chapterFinished = false; nextChapterFile = ""; }
    std::string nextChapterFile = "";
    std::vector<char> readFile(const std::string& path);
    static std::map<std::string, std::vector<char>> fileCache;
    std::string currentSpeaker;
    bool saveGame(int slot = 1);
    bool loadGame(int slot = 1, bool isAutosave = false);
    void autosave();
    bool loadPersistent();
    bool savePersistent();
    void mergePersistentIntoVariables();
    static constexpr int AUTOSAVE_INTERVAL = 5;
    int dialogCountSinceAutosave = 0;
    void playSFX(const std::string& filename, float pitch = 1.0f);
    void stopAudio();
    void setMenu(MainMenu* menu);
    void run();
    void registerCommands();
    bool loadScenario(const std::string& filename);
    bool typeText(const std::string& text, int speedMs);
    size_t currentEventIdx = 0; 
    std::string currentChapterFile = "res/scenario/scenario.txt";

private:
    MainMenu* menu = nullptr;
    std::vector<SceneEvent> events;
    std::string replaceMacros(std::string text);
    std::string nameColor = CLR_NAME;
    std::map<std::string, std::shared_ptr<ICommand>> commandRegistry;
public:
    void executeCommand(const std::string& cmd);
    const std::map<std::string, std::shared_ptr<ICommand>>& getCommandRegistry() const { return commandRegistry; }
private:
    void cleanupSounds();
};