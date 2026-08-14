#include "command.hpp"
#include "engine.hpp"
#include <filesystem>
#include "logger.hpp"

namespace fs = std::filesystem;

class CmdNextChapter : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.empty()) return;
        std::string resolved = resolveScenarioPath(args[0]);
        if (resolved.empty()) {
            Logger::getInstance().error("Command: next_chapter refused unsafe target: " + args[0]);
            return;
        }
        eng->chapterFinished = true;
        eng->nextChapterFile = resolved;
        Logger::getInstance().info("Command: next_chapter -> " + eng->nextChapterFile);
    }
};
REGISTER_COMMAND(CmdNextChapter, "next_chapter")