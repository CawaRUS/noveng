#include "command.hpp"
#include "engine.hpp"
#include "common.hpp"
#include "logger.hpp"
#include "expression.hpp"

class CmdGlobalMath : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.size() < 2 || args[0].empty() || args[1].empty()) return;
        if (!isValidIdentifier(args[0])) {
            Logger::getInstance().warn("Command: global_math refused invalid variable name: " + args[0]);
            return;
        }

        try {
            ExpressionEvaluator eval(eng->variables);
            Variant result = eval.evaluate(args[1]);
            eng->persistentVariables[args[0]] = result;
            eng->variables[args[0]] = result;
            eng->savePersistent();
            Logger::getInstance().debug("Command: global_math set " + args[0] + " = " + variantToString(result));
        } catch (const std::exception& e) {
            Logger::getInstance().error("Command: global_math failed to evaluate '" + args[1] + "': " + e.what());
        }
    }
};
REGISTER_COMMAND(CmdGlobalMath, "global_math")
