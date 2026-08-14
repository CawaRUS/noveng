#include "command.hpp"
#include "engine.hpp"
#include "logger.hpp"
#include <thread>
#include <chrono>

class CmdWait : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.empty() || args[0].empty()) return;
        try {
            int ms = std::stoi(args[0]);
            if (ms < 0) ms = 0;
            Logger::getInstance().debug("Command: wait " + std::to_string(ms) + " ms");
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        } catch (const std::exception& e) {
            Logger::getInstance().warn("Command: wait failed: " + std::string(e.what()));
        }
    }
};
REGISTER_COMMAND(CmdWait, "wait")
