#include "platform/audio_device.h"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>

#include <format>
#include <stdexcept>
#include <vector>

#include "assets/pcm.h"
#include "core/log.h"

namespace oa {

namespace {

// SDL's stream callback: mixes what the device asks for.
void SDLCALL feed(void* userdata, SDL_AudioStream* stream, int additional, int /*total*/) {
  auto& mixer = *static_cast<Mixer*>(userdata);
  std::vector<std::int16_t> samples(static_cast<std::size_t>(additional) / sizeof(std::int16_t));
  mixer.render(samples);
  SDL_PutAudioStreamData(stream, samples.data(), static_cast<int>(samples.size() * sizeof(std::int16_t)));
}

}  // namespace

AudioDevice::AudioDevice(Mixer& mixer) : mixer_(mixer) {
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    throw std::runtime_error(std::format("SDL_InitSubSystem(AUDIO): {}", SDL_GetError()));
  }
  const SDL_AudioSpec spec{.format = SDL_AUDIO_S16, .channels = 1, .freq = kSampleRate};
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, &mixer_);
  if (stream_ == nullptr) {
    const std::string error = SDL_GetError();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    throw std::runtime_error(std::format("SDL_OpenAudioDeviceStream: {}", error));
  }
  SDL_ResumeAudioStreamDevice(stream_);
}

void AudioDevice::set_paused(bool paused) {
  if (!(paused ? SDL_PauseAudioStreamDevice(stream_) : SDL_ResumeAudioStreamDevice(stream_))) {
    log_warning("cannot {} the sound: {}", paused ? "pause" : "resume", SDL_GetError());
  }
}

AudioDevice::~AudioDevice() {
  SDL_DestroyAudioStream(stream_);
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

}  // namespace oa
