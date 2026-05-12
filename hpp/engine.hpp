#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "miniaudio.h"
#include "common.hpp"
#include "json.hpp"
#include "command.hpp"

using json = nlohmann::json;

std::vector<std::string> split(const std::string& s, char delimiter);

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
    std::map<std::string, int> variables;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GameState, currentScene, eventIndex, characterColors, characterPitches, currentMusic, variables)
};

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
    std::map<std::string, int> variables;
    std::map<std::string, std::string> characterColors;
    std::map<std::string, float> characterPitches;
    std::vector<std::unique_ptr<ActiveSound>> activeSounds;
    std::mutex soundsMutex;
    std::string currentMusicFile = "";
    bool chapterFinished = false;
    ma_engine* getAudio() { return &audio; }
    void clearEvents() { events.clear(); }
    bool isChapterFinished() { return chapterFinished; }
    std::string getNextChapter() { return nextChapterFile; }
    void resetChapterFlag() { chapterFinished = false; nextChapterFile = ""; }
    std::string nextChapterFile = "";
    std::vector<char> readFile(const std::string& path);
    std::string currentSpeaker;
    void saveGame(int slot = 1);
    bool loadGame(int slot = 1);
    void playSFX(const std::string& filename, float pitch = 1.0f);
    void stopAudio();
    void run();
    void registerCommands();
    bool loadScenario(const std::string& filename);
    void typeText(const std::string& text, int speedMs);
    size_t currentEventIdx = 0; 
    std::string currentChapterFile = "res/scenario/scenario.txt";

private:
    std::vector<SceneEvent> events;
    std::string replaceMacros(std::string text);
    std::string nameColor = CLR_NAME;
    std::map<std::string, std::shared_ptr<ICommand>> commandRegistry;
    void executeCommand(const std::string& cmd);
    void cleanupSounds();
};