#include "command.hpp"
#include "engine.hpp"
#include "expression.hpp"
#include "common.hpp"
#include "logger.hpp"

class CmdClamp : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.size() < 3 || args[0].empty() || args[1].empty() || args[2].empty()) return;
        if (!isValidIdentifier(args[0])) {
            Logger::getInstance().warn("Command: clamp refused invalid variable name: " + args[0]);
            return;
        }

        try {
            ExpressionEvaluator eval(eng->variables);
            float minVal = variantToFloat(eval.evaluate(args[1]));
            float maxVal = variantToFloat(eval.evaluate(args[2]));
            if (minVal > maxVal) std::swap(minVal, maxVal);

            auto& var = eng->variables[args[0]];
            float value = variantToFloat(var);
            if (value < minVal) value = minVal;
            if (value > maxVal) value = maxVal;

            if (std::holds_alternative<int>(var)) {
                var = static_cast<int>(value);
            } else if (std::holds_alternative<float>(var)) {
                var = value;
            } else {
                var = value;
            }
            Logger::getInstance().debug("Command: clamp set " + args[0] + " = " + variantToString(var));
        } catch (const std::exception& e) {
            Logger::getInstance().warn("Command: clamp failed: " + std::string(e.what()));
        }
    }
};
REGISTER_COMMAND(CmdClamp, "clamp")
