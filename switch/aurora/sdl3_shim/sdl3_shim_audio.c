// SDL 3 audio for the Switch shim: the one playback stream Aurora opens
// (stereo 16-bit at the DSP's 32 or 48 kHz), played through libnx audout,
// which runs at 48 kHz. Pushed samples wait in a bounded queue of input
// frames; SDL_GetAudioStreamQueued reports it in input bytes, as SDL does, for
// Aurora's throttling and stretching. An output thread keeps audout's buffers
// full, resampling linearly to 48 kHz, and plays silence while the stream is
// paused (SDL starts device streams paused) or the queue runs dry.
#include <SDL3/SDL.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "sdl3_shim.h"

#define OUTPUT_RATE 48000
#define CHANNELS 2
#define BUFFER_COUNT 4
#define BUFFER_FRAMES 1024                     // 21.3 ms at 48 kHz
#define BUFFER_BYTES (BUFFER_FRAMES * CHANNELS * sizeof(s16))  // 0x1000, as audout requires
#define QUEUE_FRAMES 48000                     // one second at the highest input rate

struct SDL_AudioStream {
    Mutex lock;
    s16* queue;  // QUEUE_FRAMES interleaved stereo frames
    size_t head;
    size_t count;
    int input_rate;
    double position;  // fraction of the way from the queue's first frame to its second
    bool playing;
    bool stop;
    Thread thread;
    AudioOutBuffer buffers[BUFFER_COUNT];
    void* memory[BUFFER_COUNT];
};

static bool valid_input(const SDL_AudioSpec* spec) {
    return spec != NULL && spec->format == SDL_AUDIO_S16 && spec->channels == CHANNELS &&
           (spec->freq == 32000 || spec->freq == 48000);
}

static const s16* frame_at(const SDL_AudioStream* stream, size_t index) {
    return &stream->queue[((stream->head + index) % QUEUE_FRAMES) * CHANNELS];
}

// Fills one output buffer at 48 kHz from the queue. Caller holds the lock.
static void render(SDL_AudioStream* stream, s16* out) {
    const double step = (double)stream->input_rate / OUTPUT_RATE;
    for (size_t frame = 0; frame < BUFFER_FRAMES; ++frame) {
        if (!stream->playing || stream->count < 2) {
            out[frame * CHANNELS] = out[frame * CHANNELS + 1] = 0;
            continue;
        }
        const s16* a = frame_at(stream, 0);
        const s16* b = frame_at(stream, 1);
        const double t = stream->position;
        for (int channel = 0; channel < CHANNELS; ++channel)
            out[frame * CHANNELS + channel] = (s16)(a[channel] + (b[channel] - a[channel]) * t);
        stream->position += step;
        while (stream->position >= 1.0 && stream->count >= 2) {
            stream->position -= 1.0;
            stream->head = (stream->head + 1) % QUEUE_FRAMES;
            --stream->count;
        }
    }
}

static void output_thread(void* argument) {
    SDL_AudioStream* stream = argument;
    for (int i = 0; i < BUFFER_COUNT; ++i) {
        memset(stream->memory[i], 0, BUFFER_BYTES);
        audoutAppendAudioOutBuffer(&stream->buffers[i]);
    }
    while (!__atomic_load_n(&stream->stop, __ATOMIC_ACQUIRE)) {
        AudioOutBuffer* released = NULL;
        u32 count = 0;
        if (R_FAILED(audoutWaitPlayFinish(&released, &count, 100000000ULL)) || released == NULL)
            continue;
        mutexLock(&stream->lock);
        render(stream, released->buffer);
        mutexUnlock(&stream->lock);
        audoutAppendAudioOutBuffer(released);
    }
}

SDL_AudioStream* SDL_OpenAudioDeviceStream(SDL_AudioDeviceID devid, const SDL_AudioSpec* spec,
                                           SDL_AudioStreamCallback callback, void* userdata) {
    (void)devid;
    (void)userdata;
    if (callback != NULL || !valid_input(spec)) {
        sdl3_shim_set_error("only pushed stereo 16-bit 32/48 kHz audio is supported");
        return NULL;
    }
    SDL_AudioStream* stream = calloc(1, sizeof *stream);
    if (stream == NULL)
        return NULL;
    stream->queue = calloc(QUEUE_FRAMES * CHANNELS, sizeof(s16));
    stream->input_rate = spec->freq;
    bool ready = stream->queue != NULL;
    for (int i = 0; ready && i < BUFFER_COUNT; ++i) {
        stream->memory[i] = memalign(0x1000, BUFFER_BYTES);
        ready = stream->memory[i] != NULL;
        if (ready) {
            stream->buffers[i].buffer = stream->memory[i];
            stream->buffers[i].buffer_size = BUFFER_BYTES;
            stream->buffers[i].data_size = BUFFER_BYTES;
            stream->buffers[i].data_offset = 0;
            stream->buffers[i].next = NULL;
        }
    }
    if (ready && (R_FAILED(audoutInitialize()) || R_FAILED(audoutStartAudioOut()))) {
        sdl3_shim_set_error("audout could not start");
        ready = false;
    } else if (ready && (audoutGetSampleRate() != OUTPUT_RATE ||
                         audoutGetChannelCount() != CHANNELS)) {
        sdl3_shim_set_error("audout is not 48 kHz stereo");
        audoutStopAudioOut();
        audoutExit();
        ready = false;
    }
    if (ready && (R_FAILED(threadCreate(&stream->thread, output_thread, stream, NULL, 0x4000,
                                        0x2B, -2)) ||
                  R_FAILED(threadStart(&stream->thread)))) {
        sdl3_shim_set_error("audio thread could not start");
        audoutStopAudioOut();
        audoutExit();
        ready = false;
    }
    if (!ready) {
        for (int i = 0; i < BUFFER_COUNT; ++i)
            free(stream->memory[i]);
        free(stream->queue);
        free(stream);
        return NULL;
    }
    return stream;
}

void SDL_DestroyAudioStream(SDL_AudioStream* stream) {
    if (stream == NULL)
        return;
    __atomic_store_n(&stream->stop, true, __ATOMIC_RELEASE);
    threadWaitForExit(&stream->thread);
    threadClose(&stream->thread);
    audoutStopAudioOut();
    audoutExit();
    for (int i = 0; i < BUFFER_COUNT; ++i)
        free(stream->memory[i]);
    free(stream->queue);
    free(stream);
}

bool SDL_ResumeAudioStreamDevice(SDL_AudioStream* stream) {
    if (stream == NULL)
        return false;
    mutexLock(&stream->lock);
    stream->playing = true;
    mutexUnlock(&stream->lock);
    return true;
}

// Only the input side changes (the DSP switching between 32 and 48 kHz).
bool SDL_SetAudioStreamFormat(SDL_AudioStream* stream, const SDL_AudioSpec* src_spec,
                              const SDL_AudioSpec* dst_spec) {
    (void)dst_spec;
    if (stream == NULL || (src_spec != NULL && !valid_input(src_spec)))
        return false;
    if (src_spec != NULL) {
        mutexLock(&stream->lock);
        stream->input_rate = src_spec->freq;
        mutexUnlock(&stream->lock);
    }
    return true;
}

bool SDL_PutAudioStreamData(SDL_AudioStream* stream, const void* buf, int len) {
    if (stream == NULL || buf == NULL || len < 0)
        return false;
    const s16* samples = buf;
    size_t frames = (size_t)len / (CHANNELS * sizeof(s16));
    mutexLock(&stream->lock);
    // A full queue keeps the oldest second and drops the rest.
    if (frames > QUEUE_FRAMES - stream->count)
        frames = QUEUE_FRAMES - stream->count;
    for (size_t i = 0; i < frames; ++i) {
        s16* slot = &stream->queue[((stream->head + stream->count) % QUEUE_FRAMES) * CHANNELS];
        slot[0] = samples[i * CHANNELS];
        slot[1] = samples[i * CHANNELS + 1];
        ++stream->count;
    }
    mutexUnlock(&stream->lock);
    return true;
}

int SDL_GetAudioStreamQueued(SDL_AudioStream* stream) {
    if (stream == NULL)
        return -1;
    mutexLock(&stream->lock);
    const int bytes = (int)(stream->count * CHANNELS * sizeof(s16));
    mutexUnlock(&stream->lock);
    return bytes;
}
