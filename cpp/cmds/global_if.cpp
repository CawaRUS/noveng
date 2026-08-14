#include "command.hpp"
#include "engine.hpp"
#include "logger.hpp"
#include "expression.hpp"

class CmdGlobalIf : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.size() != 3) {
            Logger::getInstance().error("Command: global_if requires 3 arguments (expression|true|false). Got " + std::to_string(args.size()));
            return;
        }

        try {
            ExpressionEvaluator eval(eng->variables);
            bool cond = eval.evaluateBool(args[0]);
            std::string nextFile = cond ? args[1] : args[2];
            std::string resolved = resolveScenarioPath(nextFile);
            if (resolved.empty()) {
                Logger::getInstance().error("Command: global_if refused unsafe target: " + nextFile);
                return;
            }
            eng->chapterFinished = true;
            eng->nextChapterFile = resolved;
            Logger::getInstance().info("Command: global_if evaluated '" + args[0] + "' -> " + (cond ? "true" : "false") + " -> " + nextFile);
        } catch (const std::exception& e) {
            Logger::getInstance().error("Command: global_if failed to evaluate expression '" + args[0] + "': " + e.what());
        }
    }
};
REGISTER_COMMAND(CmdGlobalIf, "global_if")
