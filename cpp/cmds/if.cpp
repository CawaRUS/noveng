#include "command.hpp"
#include "engine.hpp"

class CmdIf : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.size() < 4 || args[0].empty() || args[1].empty()) return;

        try {
            int threshold = std::stoi(args[1]);
            bool cond = (eng->variables.count(args[0]) && eng->variables[args[0]] >= threshold);
            std::string nextFile = cond ? args[2] : args[3];
            eng->chapterFinished = true;
            eng->nextChapterFile = (std::filesystem::path(DIR_RES) / DIR_SCENARIO / nextFile).string();
        } catch (const std::exception&) {
            return;
        }
    }
};
REGISTER_COMMAND(CmdIf, "if")