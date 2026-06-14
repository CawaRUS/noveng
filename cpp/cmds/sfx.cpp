#include "command.hpp"
#include "engine.hpp"

class CmdSfx : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.empty()) return;
        float pitch = 1.0f;
        if (args.size() > 1 && !args[1].empty()) {
            try {
                pitch = std::stof(args[1]);
            } catch (...) {
                pitch = 1.0f;
            }
        }
        eng->playSFX(args[0], pitch);
    }
};
REGISTER_COMMAND(CmdSfx, "sfx")