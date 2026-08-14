#include "command.hpp"
#include "engine.hpp"
#include "expression.hpp"
#include "common.hpp"
#include "logger.hpp"

class CmdInc : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.empty() || args[0].empty()) return;
        if (!isValidIdentifier(args[0])) {
            Logger::getInstance().warn("Command: inc refused invalid variable name: " + args[0]);
            return;
        }

        float delta = 1.0f;
        if (args.size() >= 2 && !args[1].empty()) {
            try {
                ExpressionEvaluator eval(eng->variables);
                delta = variantToFloat(eval.evaluate(args[1]));
            } catch (const std::exception& e) {
                Logger::getInstance().warn("Command: inc failed to evaluate delta '" + args[1] + "': " + e.what());
                return;
            }
        }

        auto& var = eng->variables[args[0]];
        if (std::holds_alternative<int>(var)) {
            var = std::get<int>(var) + static_cast<int>(delta);
        } else if (std::holds_alternative<float>(var)) {
            var = std::get<float>(var) + delta;
        } else {
            var = variantToFloat(var) + delta;
        }
        Logger::getInstance().debug("Command: inc set " + args[0] + " = " + variantToString(var));
    }
};
REGISTER_COMMAND(CmdInc, "inc")
