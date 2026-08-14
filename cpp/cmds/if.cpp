#include "command.hpp"
#include "engine.hpp"
#include "logger.hpp"
#include "expression.hpp"

class CmdIf : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.empty()) return;

        std::string expression;
        std::string trueFile;
        std::string falseFile;

        if (args.size() == 3) {
            // New syntax: {if:expression|true.txt|false.txt}
            expression = args[0];
            trueFile = args[1];
            falseFile = args[2];
        } else if (args.size() == 4) {
            // Legacy syntax: {if:key|threshold|true.txt|false.txt}
            expression = "$" + args[0] + "$ >= " + args[1];
            trueFile = args[2];
            falseFile = args[3];
        } else {
            Logger::getInstance().error("Command: if requires 3 arguments (expression|true|false) or 4 legacy arguments (key|threshold|true|false). Got " + std::to_string(args.size()));
            return;
        }

        try {
            ExpressionEvaluator eval(eng->variables);
            bool cond = eval.evaluateBool(expression);
            std::string nextFile = cond ? trueFile : falseFile;
            std::string resolved = resolveScenarioPath(nextFile);
            if (resolved.empty()) {
                Logger::getInstance().error("Command: if refused unsafe target: " + nextFile);
                return;
            }
            eng->chapterFinished = true;
            eng->nextChapterFile = resolved;
            Logger::getInstance().info("Command: if evaluated '" + expression + "' -> " + (cond ? "true" : "false") + " -> " + nextFile);
        } catch (const std::exception& e) {
            Logger::getInstance().error("Command: if failed to evaluate expression '" + expression + "': " + e.what());
        }
    }
};
REGISTER_COMMAND(CmdIf, "if")
