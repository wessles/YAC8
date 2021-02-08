#pragma once

#include <SDL_audio.h>

namespace yac8 {
    class c8_noisemaker {
    private:
        SDL_AudioSpec audio_spec;
        SDL_AudioDeviceID audio_device;
        int framerate = 60;
        bool playing = false;
        float x = 0;

        const double minimumReserveRatio = 1.0 / 10.0;
        double minReserve = 0.0;

        Uint16 *queueFillingBuffer;
    public:
        float volume = 1.0f;
        float pitch = 1.0f;
        c8_noisemaker();
        ~c8_noisemaker();
        void play();
        void stop();
    };
}
