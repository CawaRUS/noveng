#include "command.hpp"
#include "engine.hpp"
#include "logger.hpp"
#include <filesystem>

class CmdJump : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.empty() || args[0].empty()) {
            Logger::getInstance().error("Command: jump requires a scenario file.");
            return;
        }
        std::string resolved = resolveScenarioPath(args[0]);
        if (resolved.empty()) {
            Logger::getInstance().error("Command: jump refused unsafe target: " + args[0]);
            return;
        }
        eng->chapterFinished = true;
        eng->nextChapterFile = resolved;
        Logger::getInstance().info("Command: jump -> " + args[0]);
    }
};
REGISTER_COMMAND(CmdJump, "jump")
