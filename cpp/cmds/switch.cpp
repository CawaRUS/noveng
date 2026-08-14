#include "command.hpp"
#include "engine.hpp"
#include "expression.hpp"
#include "logger.hpp"

class CmdSwitch : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.size() < 3) {
            Logger::getInstance().error("Command: switch requires expression|case|file pairs and optionally a default file.");
            return;
        }

        bool hasDefault = (args.size() % 2 == 0);
        size_t defaultIndex = hasDefault ? args.size() - 1 : 0;

        try {
            ExpressionEvaluator eval(eng->variables);
            std::string value = variantToString(eval.evaluate(args[0]));

            for (size_t i = 1; i + 1 < args.size() - (hasDefault ? 1 : 0); i += 2) {
                if (args[i] == value) {
                    std::string resolved = resolveScenarioPath(args[i + 1]);
                    if (resolved.empty()) {
                        Logger::getInstance().error("Command: switch refused unsafe target: " + args[i + 1]);
                        return;
                    }
                    eng->chapterFinished = true;
                    eng->nextChapterFile = resolved;
                    Logger::getInstance().info("Command: switch matched '" + value + "' -> " + args[i + 1]);
                    return;
                }
            }

            if (hasDefault) {
                std::string resolved = resolveScenarioPath(args[defaultIndex]);
                if (resolved.empty()) {
                    Logger::getInstance().error("Command: switch refused unsafe default target: " + args[defaultIndex]);
                    return;
                }
                eng->chapterFinished = true;
                eng->nextChapterFile = resolved;
                Logger::getInstance().info("Command: switch no case matched, using default -> " + args[defaultIndex]);
            } else {
                Logger::getInstance().info("Command: switch no case matched for '" + value + "'");
            }
        } catch (const std::exception& e) {
            Logger::getInstance().error("Command: switch failed: " + std::string(e.what()));
        }
    }
};
REGISTER_COMMAND(CmdSwitch, "switch")
