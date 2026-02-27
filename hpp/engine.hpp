#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
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
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GameState, currentScene, eventIndex, characterColors, currentMusic, variables)
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
    void stopAudio();
    
    void run();

    size_t currentEventIdx = 0; 
    std::string currentChapterFile = "res/scenario/scenario.txt";
    bool isChapterFinished() { return chapterFinished; }
    std::string getNextChapter() { return nextChapterFile; }
    void resetChapterFlag() { chapterFinished = false; nextChapterFile = ""; }

private:
    std::vector<SceneEvent> events;
    ma_engine audio;
    bool chapterFinished = false;
    std::string nextChapterFile = "";
    std::map<std::string, int> variables;
    
    std::string nameColor = CLR_NAME;
    std::string currentMusicFile = "";
    std::map<std::string, std::string> characterColors;
    std::vector<std::unique_ptr<ma_sound>> activeSounds;

    void executeCommand(const std::string& cmd);
    void cleanupSounds();
};