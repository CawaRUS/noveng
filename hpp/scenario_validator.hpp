#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "engine.hpp"

// Validates a parsed scenario for common syntax and semantic errors.
// Logs problems via Logger and returns false if any critical error is found.
class ScenarioValidator {
public:
    struct ValidationResult {
        bool valid = true;
        int errors = 0;
        int warnings = 0;
    };

    // Validates a list of scene events against known commands and argument rules.
    static ValidationResult validate(
        const std::vector<SceneEvent>& events,
        const std::map<std::string, std::shared_ptr<ICommand>>& knownCommands);

private:
    static bool isKnownCommand(const std::string& name,
                               const std::map<std::string, std::shared_ptr<ICommand>>& knownCommands);
    static bool checkArgumentCount(const std::string& action, const std::vector<std::string>& args);
    static bool checkScenarioTarget(const std::string& targetFile);
    static std::vector<std::string> splitArgs(const std::string& rawArgs);
    static std::string trim(const std::string& s);
};
