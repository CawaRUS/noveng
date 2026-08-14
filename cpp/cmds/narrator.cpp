#include "command.hpp"
#include "engine.hpp"
#include "setting.hpp"
#include "logger.hpp"

class CmdNarrator : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.empty() || args[0].empty()) return;
        std::string text = args[0];
        eng->history.push_back({"", text, CLR_SYSTEM});

        size_t maxHistory = SettingsManager::getInstance().get().historySize;
        if (eng->history.size() > maxHistory) {
            eng->history.erase(eng->history.begin());
        }

        std::cout << "\n" << CLR_SYSTEM << text << CLR_RESET << std::endl;
        Logger::getInstance().debug("Command: narrator: " + text);
    }
};
REGISTER_COMMAND(CmdNarrator, "narrator")
