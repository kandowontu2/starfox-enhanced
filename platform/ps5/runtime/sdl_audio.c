// SDL3 owns resampling and the playback thread; AudioOut supplies 48 kHz stereo.
#include "SDL_internal.h"
#include "audio/SDL_sysaudio.h"
#include "native.h"

// A second sceAudioOutInit in a process answers "already initialised".
#define PS_AUDIO_OUT_ERROR_ALREADY_INIT ((int)0x8026000e)
#define PS_AUDIO_OUT_CHANNELS_LR 3
#define PS_AUDIO_OUT_VOLUME_0DB 0x8000

struct SDL_PrivateAudioData { int handle; Uint8 *buffer; };

static bool PS_Open(SDL_AudioDevice *device)
{
    device->hidden = SDL_calloc(1, sizeof(*device->hidden));
    if (!device->hidden) return false;
    device->hidden->handle = -1;
    device->spec.format = SDL_AUDIO_S16;
    device->spec.channels = 2;
    device->spec.freq = 48000;
    device->sample_frames = 512;
    SDL_UpdatedAudioDeviceFormat(device);
    device->hidden->buffer = SDL_calloc(1, device->buffer_size);
    if (!device->hidden->buffer) return false;
    device->hidden->handle = sceAudioOutOpen(0xff, 0, 0, 512, 48000, 1);
    if (device->hidden->handle < 0)
        return SDL_SetError("sceAudioOutOpen: 0x%x", device->hidden->handle);
    // The working PS5 titles set the port's volume rather than trusting its
    // initial level (ProsperoEden headless/audio.cpp, native_audio.hpp).
    int32_t volumes[8];
    for (int i = 0; i < 8; ++i) volumes[i] = PS_AUDIO_OUT_VOLUME_0DB;
    int result = sceAudioOutSetVolume(device->hidden->handle, PS_AUDIO_OUT_CHANNELS_LR, volumes);
    if (result < 0) return SDL_SetError("sceAudioOutSetVolume: 0x%x", result);
    return true;
}
static bool PS_Play(SDL_AudioDevice *device, const Uint8 *buffer, int size)
{
    (void)size;
    int result = sceAudioOutOutput(device->hidden->handle, buffer);
    return result >= 0 || SDL_SetError("sceAudioOutOutput: 0x%x", result);
}
static bool PS_Wait(SDL_AudioDevice *device)
{
    (void)device; // AudioOutOutput blocks for one hardware period.
    return true;
}
static Uint8 *PS_Buffer(SDL_AudioDevice *device, int *size)
{
    *size = device->buffer_size;
    return device->hidden->buffer;
}
static void PS_Close(SDL_AudioDevice *device)
{
    if (!device->hidden) return;
    if (device->hidden->handle >= 0) {
        // Drain what was queued before the port goes away.
        sceAudioOutOutput(device->hidden->handle, NULL);
        sceAudioOutClose(device->hidden->handle);
    }
    SDL_free(device->hidden->buffer);
    SDL_free(device->hidden);
    device->hidden = NULL;
}
static bool PS_Init(SDL_AudioDriverImpl *impl)
{
    // AudioOut is initialized once per process (ps5-payload-dev/SDL); SDL
    // may initialize its audio subsystem again after a quit.
    static bool initialized;
    if (!initialized) {
        int result = sceAudioOutInit();
        if (result < 0 && result != PS_AUDIO_OUT_ERROR_ALREADY_INIT)
            return SDL_SetError("sceAudioOutInit: 0x%x", result);
        initialized = true;
    }
    impl->OpenDevice = PS_Open;
    impl->PlayDevice = PS_Play;
    impl->WaitDevice = PS_Wait;
    impl->GetDeviceBuf = PS_Buffer;
    impl->CloseDevice = PS_Close;
    impl->OnlyHasDefaultPlaybackDevice = true;
    return true;
}
AudioBootStrap STARFOXPS5AUDIO_bootstrap = {
    "ps5", "PS5 AudioOut", PS_Init, false, false
};
