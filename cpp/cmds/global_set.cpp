#include "command.hpp"
#include "engine.hpp"
#include "common.hpp"
#include "logger.hpp"
#include "expression.hpp"

class CmdGlobalSet : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.size() < 2 || args[0].empty() || args[1].empty()) return;
        if (!isValidIdentifier(args[0])) {
            Logger::getInstance().warn("Command: global_set refused invalid variable name: " + args[0]);
            return;
        }

        Variant value;
        bool useExpression = (args.size() < 3);

        if (!useExpression) {
            try {
                value = parseVariant(args[1], args[2]);
            } catch (const std::exception& e) {
                Logger::getInstance().warn("Command: global_set could not parse value '" + args[1] + "' as " + args[2] + ": " + e.what());
                return;
            }
        } else {
            bool evalFailed = false;
            try {
                ExpressionEvaluator eval(eng->variables);
                value = eval.evaluate(args[1]);
            } catch (const std::exception&) {
                evalFailed = true;
            }

            if (evalFailed) {
                try {
                    value = autoDetectVariant(args[1]);
                } catch (const std::exception& e) {
                    Logger::getInstance().warn("Command: global_set could not parse value '" + args[1] + "': " + e.what());
                    return;
                }
            }
        }

        eng->persistentVariables[args[0]] = value;
        eng->variables[args[0]] = value;
        eng->savePersistent();
        Logger::getInstance().debug("Set persistent variable " + args[0] + " = " + variantToString(value));
    }
};
REGISTER_COMMAND(CmdGlobalSet, "global_set")
