#include "command.hpp"
#include "engine.hpp"
#include "expression.hpp"
#include "localisation.hpp"
#include <iostream>
#include <algorithm>
#include <conio.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include "logger.hpp"

namespace fs = std::filesystem;

class CmdChoice : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        Logger::getInstance().info("Command: choice started.");
        eng->autosave();

        if (args.empty()) {
            Logger::getInstance().error("Command: choice requires at least one option.");
            return;
        }

        bool hasConditions = detectConditionFormat(args, eng);
        int stride = hasConditions ? 3 : 2;

        if (args.size() % stride != 0) {
            Logger::getInstance().error("Command: choice arguments do not match expected format (text|file or text|file|condition). Args count: " + std::to_string(args.size()));
            return;
        }

        struct Option {
            std::string text;
            std::string file;
            std::string condition;
            int originalIndex;
            bool visible;
        };

        std::vector<Option> options;
        int optionCount = static_cast<int>(args.size() / stride);
        for (int i = 0; i < optionCount; ++i) {
            Option opt;
            opt.text = args[i * stride];
            opt.file = args[i * stride + 1];
            opt.condition = hasConditions ? args[i * stride + 2] : "";
            opt.originalIndex = i;
            opt.visible = true;
            if (hasConditions && !opt.condition.empty()) {
                try {
                    ExpressionEvaluator eval(eng->variables);
                    opt.visible = eval.evaluateBool(opt.condition);
                } catch (const std::exception& e) {
                    Logger::getInstance().warn("Command: choice failed to evaluate condition '" + opt.condition + "': " + e.what() + "; showing option anyway.");
                    opt.visible = true;
                }
            }
            options.push_back(opt);
        }

        options.erase(std::remove_if(options.begin(), options.end(), [](const Option& o) { return !o.visible; }), options.end());

        if (options.empty()) {
            Logger::getInstance().error("Command: choice has no visible options.");
            return;
        }

        int numOptions = static_cast<int>(options.size());
        if (numOptions > 9) {
            Logger::getInstance().warn("Command: choice has more than 9 visible options. Clamping to 9.");
            numOptions = 9;
            options.resize(numOptions);
        }

        std::cout << "\n" << CLR_NAME << LocalizationManager::getInstance().get("choice_header") << CLR_RESET << std::endl;
        for (int i = 0; i < numOptions; ++i) {
            std::cout << "  " << (i + 1) << ". " << options[i].text << std::endl;
        }

        int selected = 0;
        while (selected < 1 || selected > numOptions) {
            if (_kbhit()) {
                int ch = _getch();
                if (ch == 0xE0 || ch == 0) {
                    ch = _getch();
                    if (ch == 72) selected = std::max(1, selected - 1); // Up
                    if (ch == 80) selected = std::min(numOptions, selected + 1); // Down
                } else if (ch >= '1' && ch <= ('0' + numOptions)) {
                    selected = ch - '0';
                } else if (ch == 13 && selected >= 1) {
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        if (selected < 1 || selected > numOptions) selected = 1;

        std::string nextFile = options[selected - 1].file;
        std::string resolved = resolveScenarioPath(nextFile);
        if (resolved.empty()) {
            Logger::getInstance().error("Command: choice refused unsafe target: " + nextFile);
            return;
        }
        eng->chapterFinished = true;
        eng->nextChapterFile = resolved;
        Logger::getInstance().info("User made choice: " + std::to_string(selected));
    }

private:
    bool detectConditionFormat(const std::vector<std::string>& args, NovelEngine* eng) {
        if (args.size() % 3 != 0) return false;

        for (size_t i = 2; i < args.size(); i += 3) {
            if (args[i].empty()) continue;
            try {
                ExpressionEvaluator eval(eng->variables);
                eval.evaluate(args[i]);
            } catch (...) {
                return false;
            }
        }
        return true;
    }
};
REGISTER_COMMAND(CmdChoice, "choice")
