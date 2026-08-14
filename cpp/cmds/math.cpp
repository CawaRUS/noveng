#include "command.hpp"
#include "engine.hpp"
#include "common.hpp"
#include "logger.hpp"
#include "expression.hpp"

class CmdMath : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.size() < 2 || args[0].empty() || args[1].empty()) return;
        if (!isValidIdentifier(args[0])) {
            Logger::getInstance().warn("Command: math refused invalid variable name: " + args[0]);
            return;
        }

        try {
            ExpressionEvaluator eval(eng->variables);
            Variant result = eval.evaluate(args[1]);
            eng->variables[args[0]] = result;
            Logger::getInstance().debug("Command: math set " + args[0] + " = " + variantToString(result));
        } catch (const std::exception& e) {
            Logger::getInstance().error("Command: math failed to evaluate '" + args[1] + "': " + e.what());
        }
    }
};
REGISTER_COMMAND(CmdMath, "math")
