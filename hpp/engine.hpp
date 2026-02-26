#pragma once
#include <string>
#include <vector>
#include <map>
#include "miniaudio.h"
#include "common.hpp"
#include "json.hpp"

using json = nlohmann::json;

enum class EventType { TEXT, COMMAND };


struct SceneEvent {
    EventType type;
    std::string name;
    std::string content;
};

struct GameState {
    std::string currentScene;
    size_t eventIndex;
    std::map<std::string, std::string> characterColors;
    std::string currentMusic;
    std::map<std::string, int> variables;
    
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GameState, currentScene, eventIndex, characterColors, currentMusic)
};

class NovelEngine {
public:
    NovelEngine();
    ~NovelEngine();
    void applySettings();
    ma_engine* getAudio() { return &audio; }
    bool loadScenario(const std::string& filename);
    void typeText(const std::string& text, int speedMs);
    void clearEvents() { events.clear(); }
    void saveGame(int slot = 1);
    bool loadGame(int slot = 1);
    void playSFX(const std::string& filename);
    size_t currentEventIdx = 0; 
    std::string currentChapterFile = "res/scenario/scenario.txt";
    bool isChapterFinished() { return chapterFinished; }
    void stopAudio();
    std::string getNextChapter() { return nextChapterFile; }
    void resetChapterFlag() { chapterFinished = false; nextChapterFile = ""; }
    void run();

private:
    std::vector<SceneEvent> events;
    ma_engine audio;
    bool chapterFinished = false;
    std::string nextChapterFile = "";
    std::map<std::string, int> variables;
    void executeCommand(const std::string& cmd);
    std::string nameColor = CLR_NAME;
    std::string currentMusicFile = "";
    std::map<std::string, std::string> characterColors;
    std::vector<ma_sound*> activeSounds;
};