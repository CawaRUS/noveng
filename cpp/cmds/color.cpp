#include "command.hpp"
#include "engine.hpp"
#include "logger.hpp"
#include <cctype>

static bool isANSIColorCode(const std::string& code) {
    if (code.empty()) return false;
    for (char c : code) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    int value = 0;
    try {
        value = std::stoi(code);
    } catch (...) {
        return false;
    }
    // Standard 8 foreground colors (30-37) and high-intensity colors (90-97)
    return (value >= 30 && value <= 37) || (value >= 90 && value <= 97);
}

class CmdColor : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.size() >= 2) {
            if (!isANSIColorCode(args[1])) {
                Logger::getInstance().warn("Command: color rejected invalid ANSI code: " + args[1]);
                return;
            }
            eng->characterColors[args[0]] = "\033[1;" + args[1] + "m";
        }
    }
};
REGISTER_COMMAND(CmdColor, "color")