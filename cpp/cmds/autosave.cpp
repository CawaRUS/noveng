#include "command.hpp"
#include "engine.hpp"
#include "logger.hpp"

class CmdAutosave : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        Logger::getInstance().info("Command: autosave triggered from scenario.");
        eng->autosave();
    }
};
REGISTER_COMMAND(CmdAutosave, "autosave")
