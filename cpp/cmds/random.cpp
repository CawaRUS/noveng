#include "command.hpp"
#include "engine.hpp"
#include "common.hpp"
#include "logger.hpp"
#include <cstdlib>

class CmdRandom : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.size() < 3 || args[0].empty() || args[1].empty() || args[2].empty()) return;
        if (!isValidIdentifier(args[2])) {
            Logger::getInstance().warn("Command: random refused invalid variable name: " + args[2]);
            return;
        }

        try {
            int minVal = std::stoi(args[0]);
            int maxVal = std::stoi(args[1]);
            if (minVal > maxVal) std::swap(minVal, maxVal);

            int range = maxVal - minVal + 1;
            int result = minVal + (std::rand() % range);
            eng->variables[args[2]] = result;
            Logger::getInstance().debug("Command: random set " + args[2] + " = " + std::to_string(result));
        } catch (const std::exception& e) {
            Logger::getInstance().error("Command: random failed: " + std::string(e.what()));
        }
    }
};
REGISTER_COMMAND(CmdRandom, "random")
