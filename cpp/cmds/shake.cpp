#include "command.hpp"
#include "engine.hpp"
#include <thread>
#include <chrono>
#include <string>

extern int safeStoi(const std::string& str, int defaultVal);

class CmdShake : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        int duration = 15;
        int intensity = 3;

        try {
            if (!args.empty() && !args[0].empty()) duration = std::stoi(args[0]);
            if (args.size() > 1 && !args[1].empty()) intensity = std::stoi(args[1]);
        } catch (...) {
        }

        duration = std::max(0, duration);
        intensity = std::max(0, std::abs(intensity));

        int oldX = eng->offsetX;
        int oldY = eng->offsetY;

        for (int i = 0; i < duration; ++i) {
            eng->offsetX = (intensity == 0) ? 0 : (std::rand() % (intensity * 2 + 1)) - intensity;
            eng->offsetY = (intensity == 0) ? 0 : (std::rand() % (intensity / 2 + 1));
            
            eng->render(); 
            
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        eng->offsetX = oldX;
        eng->offsetY = oldY;
        eng->render();
    }
};

REGISTER_COMMAND(CmdShake, "shake")