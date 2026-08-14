#include "scenario_validator.hpp"
#include "common.hpp"
#include "logger.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

std::string ScenarioValidator::trim(const std::string& s) {
    size_t first = s.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = s.find_last_not_of(" \t\n\r");
    return s.substr(first, last - first + 1);
}

std::vector<std::string> ScenarioValidator::splitArgs(const std::string& rawArgs) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(rawArgs);
    while (std::getline(tokenStream, token, '|')) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

bool ScenarioValidator::isKnownCommand(
    const std::string& name,
    const std::map<std::string, std::shared_ptr<ICommand>>& knownCommands) {
    return knownCommands.count(name) > 0;
}

bool ScenarioValidator::checkScenarioTarget(const std::string& targetFile) {
    return !resolveScenarioPath(targetFile).empty();
}

bool ScenarioValidator::checkArgumentCount(const std::string& action, const std::vector<std::string>& args) {
    // Commands that require at least one argument.
    if (action == "next_chapter" || action == "jump" || action == "banner" ||
        action == "narrator" || action == "wait" || action == "type_speed" ||
        action == "autosave" || action == "clear" || action == "shake" ||
        action == "stop_music") {
        // Most of these need at least one arg; clear/stop_music/shake/autosave are fine with zero.
        if ((action == "next_chapter" || action == "jump" || action == "banner" ||
             action == "narrator" || action == "wait" || action == "type_speed") && args.empty()) {
            return false;
        }
        return true;
    }

    if (action == "set" || action == "math" || action == "global_set" || action == "global_math") {
        return args.size() >= 2;
    }

    if (action == "inc" || action == "dec") {
        return args.size() >= 1;
    }

    if (action == "clamp") {
        return args.size() == 3;
    }

    if (action == "random") {
        return args.size() == 3;
    }

    if (action == "if" || action == "global_if") {
        return args.size() == 3;
    }

    if (action == "choice") {
        // choice without conditions: text|file pairs (even)
        // choice with conditions: text|file|condition triples (multiple of 3)
        // We accept both, but warn if the count is neither even nor divisible by 3.
        return args.size() >= 2;
    }

    if (action == "chance") {
        return args.size() == 3;
    }

    if (action == "switch") {
        // expression|case|file... optionally ending with a default file
        return args.size() >= 3;
    }

    if (action == "color") {
        return args.size() == 2;
    }

    if (action == "play") {
        return args.size() >= 1;
    }

    if (action == "sfx") {
        return args.size() >= 1;
    }

    if (action == "type") {
        return args.size() == 2;
    }

    if (action == "volume") {
        return args.size() == 1;
    }

    return true;
}

ScenarioValidator::ValidationResult ScenarioValidator::validate(
    const std::vector<SceneEvent>& events,
    const std::map<std::string, std::shared_ptr<ICommand>>& knownCommands) {
    ValidationResult result;

    for (size_t i = 0; i < events.size(); ++i) {
        const auto& ev = events[i];

        if (ev.type == EventType::COMMAND) {
            const std::string& cmd = ev.content;

            // Check braces balance for the raw command string.
            if (cmd.empty()) {
                Logger::getInstance().warn("Validator: empty command at event " + std::to_string(i));
                result.valid = false;
                ++result.errors;
                continue;
            }

            size_t colonPos = cmd.find(':');
            std::string action = (colonPos != std::string::npos) ? trim(cmd.substr(0, colonPos)) : trim(cmd);

            if (action.empty()) {
                Logger::getInstance().warn("Validator: empty command name at event " + std::to_string(i));
                result.valid = false;
                ++result.errors;
                continue;
            }

            if (!isKnownCommand(action, knownCommands)) {
                Logger::getInstance().error("Validator: unknown command '" + action + "' at event " + std::to_string(i));
                result.valid = false;
                ++result.errors;
                continue;
            }

            std::string rawArgs = (colonPos != std::string::npos) ? cmd.substr(colonPos + 1) : "";
            std::vector<std::string> args = splitArgs(rawArgs);

            if (!checkArgumentCount(action, args)) {
                Logger::getInstance().error("Validator: command '" + action + "' has wrong number of arguments at event " + std::to_string(i));
                result.valid = false;
                ++result.errors;
                continue;
            }

            // Validate variable identifiers where applicable.
            if ((action == "set" || action == "global_set" || action == "math" ||
                 action == "global_math" || action == "inc" || action == "dec" ||
                 action == "clamp" || action == "random") && !args.empty()) {
                if (!isValidIdentifier(args[0])) {
                    Logger::getInstance().error("Validator: invalid variable name '" + args[0] + "' for command '" + action + "' at event " + std::to_string(i));
                    result.valid = false;
                    ++result.errors;
                }
            }

            // Validate scenario file targets.
            if (action == "next_chapter" || action == "jump") {
                if (!args.empty() && !checkScenarioTarget(args[0])) {
                    Logger::getInstance().error("Validator: unsafe or invalid scenario target '" + args[0] + "' at event " + std::to_string(i));
                    result.valid = false;
                    ++result.errors;
                }
            }

            if (action == "if" || action == "global_if") {
                if (args.size() == 3) {
                    if (!checkScenarioTarget(args[1])) {
                        Logger::getInstance().error("Validator: unsafe or invalid true-branch target '" + args[1] + "' at event " + std::to_string(i));
                        result.valid = false;
                        ++result.errors;
                    }
                    if (!checkScenarioTarget(args[2])) {
                        Logger::getInstance().error("Validator: unsafe or invalid false-branch target '" + args[2] + "' at event " + std::to_string(i));
                        result.valid = false;
                        ++result.errors;
                    }
                }
            }

            if (action == "chance") {
                if (args.size() == 3) {
                    if (!checkScenarioTarget(args[1])) {
                        Logger::getInstance().error("Validator: unsafe or invalid win target '" + args[1] + "' at event " + std::to_string(i));
                        result.valid = false;
                        ++result.errors;
                    }
                    if (!checkScenarioTarget(args[2])) {
                        Logger::getInstance().error("Validator: unsafe or invalid lose target '" + args[2] + "' at event " + std::to_string(i));
                        result.valid = false;
                        ++result.errors;
                    }
                }
            }

            if (action == "switch") {
                bool hasDefault = (args.size() % 2 == 0);
                size_t caseEnd = hasDefault ? args.size() - 1 : args.size();
                for (size_t j = 1; j + 1 < caseEnd; j += 2) {
                    if (!checkScenarioTarget(args[j + 1])) {
                        Logger::getInstance().error("Validator: unsafe or invalid switch target '" + args[j + 1] + "' at event " + std::to_string(i));
                        result.valid = false;
                        ++result.errors;
                    }
                }
                if (hasDefault && !checkScenarioTarget(args.back())) {
                    Logger::getInstance().error("Validator: unsafe or invalid switch default target '" + args.back() + "' at event " + std::to_string(i));
                    result.valid = false;
                    ++result.errors;
                }
            }

            if (action == "choice") {
                if (args.size() % 2 != 0 && args.size() % 3 != 0) {
                    Logger::getInstance().error("Validator: choice argument count (" + std::to_string(args.size()) + ") does not match text|file or text|file|condition format at event " + std::to_string(i));
                    result.valid = false;
                    ++result.errors;
                }
                int stride = (args.size() % 3 == 0) ? 3 : 2;
                for (size_t j = 1; j < args.size(); j += stride) {
                    if (!checkScenarioTarget(args[j])) {
                        Logger::getInstance().error("Validator: unsafe or invalid choice target '" + args[j] + "' at event " + std::to_string(i));
                        result.valid = false;
                        ++result.errors;
                    }
                }
            }
        }
    }

    if (result.valid) {
        Logger::getInstance().info("Scenario validation passed. Events: " + std::to_string(events.size()));
    } else {
        Logger::getInstance().error("Scenario validation failed with " + std::to_string(result.errors) + " error(s) and " + std::to_string(result.warnings) + " warning(s).");
    }

    return result;
}
