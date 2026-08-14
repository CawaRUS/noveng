#include "command.hpp"
#include "engine.hpp"
#include "logger.hpp"
#include <filesystem>

class CmdPlay : public ICommand {
public:
    void execute(NovelEngine* eng, const std::vector<std::string>& args) override {
        if (args.empty()) return;

        // Keep currently playing music if the request is for the same file.
        if (eng->currentMusicFile == args[0] && eng->musicSound && eng->musicSound->initialized) {
            return;
        }

        eng->stopAudio();
        eng->currentMusicFile = args[0];
        
        std::filesystem::path path = std::filesystem::path(DIR_RES) / DIR_MUSIC / args[0];
        auto data = eng->readFile(path.string());
        if (data.empty()) {
            Logger::getInstance().error("Music file not found or empty: " + path.string());
            return;
        }
        auto pActiveSound = std::make_unique<ActiveSound>();
        pActiveSound->data = std::move(data);
        
        ma_result res = ma_decoder_init_memory(pActiveSound->data.data(), pActiveSound->data.size(), NULL, &pActiveSound->decoder);
        if (res != MA_SUCCESS) {
            Logger::getInstance().error("Failed to decode BGM from memory: " + args[0]);
            return;
        }

        if (ma_sound_init_from_data_source(&eng->audio, &pActiveSound->decoder, 0, NULL, &pActiveSound->sound) == MA_SUCCESS) {
            pActiveSound->initialized = true;
            bool looping = false;
            for (const auto& arg : args) {
                if (arg == "loop") looping = true;
            }
            
            ma_sound_set_looping(&pActiveSound->sound, looping ? MA_TRUE : MA_FALSE);
            ma_sound_start(&pActiveSound->sound);
            eng->musicSound = std::move(pActiveSound);
            Logger::getInstance().debug("BGM Started (Memory): " + args[0]);
        } else {
            Logger::getInstance().error("Failed to init sound from data source: " + args[0]);
            ma_decoder_uninit(&pActiveSound->decoder);
        }
    }
};

REGISTER_COMMAND(CmdPlay, "play")