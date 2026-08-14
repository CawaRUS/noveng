#include "command.hpp"
#include "engine.hpp"
#include "setting.hpp"
#include "logger.hpp"

class CmdTypeSpeed : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.empty() || args[0].empty()) return;
        try {
            int speed = std::stoi(args[0]);
            if (speed < 0) speed = 0;
            if (speed > 200) speed = 200;
            SettingsManager::getInstance().get().typingSpeed = speed;
            Logger::getInstance().info("Command: type_speed set to " + std::to_string(speed) + " ms");
        } catch (const std::exception& e) {
            Logger::getInstance().warn("Command: type_speed failed: " + std::string(e.what()));
        }
    }
};
REGISTER_COMMAND(CmdTypeSpeed, "type_speed")
