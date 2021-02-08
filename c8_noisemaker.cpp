#include "c8_noisemaker.hpp"

#include <SDL_audio.h>
#include <iostream>


namespace yac8 {

    c8_noisemaker::c8_noisemaker() {
        audio_spec = {};
        audio_spec.freq=64100;
        audio_spec.format=AUDIO_S16SYS;
        audio_spec.channels=1;
        audio_spec.samples=1024;
        audio_spec.callback=nullptr;

        // minimum size of queue to maintain, if we are to continue looping without breaks
        minReserve = minimumReserveRatio * audio_spec.freq;
        queueFillingBuffer = new Uint16[(int)minReserve];

        // opening an audio device:
        audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
        // unpausing the audio device (starts playing):
        SDL_PauseAudioDevice(audio_device, 0);
    }

    float squareWave(float x) {
        return ((int)x % 2) * 2 - 1;
    }

    void c8_noisemaker::play() {
        if(!playing) {
            playing = true;
            SDL_PauseAudioDevice(audio_device, 0);
        }
        Uint32 queuedSound = SDL_GetQueuedAudioSize(audio_device);
        if(queuedSound < minReserve) {
            // fill sound queue to desired size
            Uint32 soundToAdd = minReserve - queuedSound;
            if (soundToAdd > 0) {
                for (int i = 0; i < soundToAdd; i++) {
                    queueFillingBuffer[i] = (squareWave(x * 0.4 * pitch) + squareWave(x * 0.8 * pitch)) * 200 * volume;
                    x += .010f;
                }
                SDL_QueueAudio(audio_device, queueFillingBuffer, sizeof(int16_t) * soundToAdd);
            }
        }
    }

    void c8_noisemaker::stop() {
        if(playing) {
            playing = false;
            x = 0.0f;
            SDL_ClearQueuedAudio(audio_device);
            SDL_PauseAudioDevice(audio_device,1);
        }
    }

    c8_noisemaker::~c8_noisemaker() {
        delete[] queueFillingBuffer;
        SDL_CloseAudio();
    }
}