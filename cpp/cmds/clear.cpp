#include "command.hpp"
#include "engine.hpp"
#include "common.hpp"
#include <iostream>

class CmdClear : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        eng->history.clear(); 
        eng->lastFullText = "";
        eng->lastSpeaker = "";
        clearScreen();
    }
};

REGISTER_COMMAND(CmdClear, "clear")