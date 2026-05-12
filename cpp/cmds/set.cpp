#include "command.hpp"
#include "engine.hpp"

class CmdSet : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.size() < 2 || args[0].empty() || args[1].empty()) return;
        try {
            eng->variables[args[0]] = std::stoi(args[1]);
        } catch (const std::exception&) {
            return;
        }
    }
};
REGISTER_COMMAND(CmdSet, "set")