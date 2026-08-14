#include "command.hpp"
#include "engine.hpp"
#include "logger.hpp"
#include <iostream>

class CmdBanner : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.empty() || args[0].empty()) return;
        std::string text = args[0];
        int width = 60;
        int pad = (width - static_cast<int>(text.length())) / 2;
        if (pad < 0) pad = 0;

        std::cout << "\n";
        std::cout << CLR_CHAPTER << std::string(width, '=') << CLR_RESET << "\n";
        std::cout << CLR_CHAPTER << std::string(pad, ' ') << text << CLR_RESET << "\n";
        std::cout << CLR_CHAPTER << std::string(width, '=') << CLR_RESET << "\n";
        std::cout << std::endl;
        Logger::getInstance().debug("Command: banner: " + text);
    }
};
REGISTER_COMMAND(CmdBanner, "banner")
