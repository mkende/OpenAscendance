#include "platform/audio_device.h"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>

#include <format>
#include <stdexcept>
#include <vector>

#include "assets/pcm.h"
#include "core/log.h"

namespace oa {

void AudioRecovery::opened(bool ok, std::uint64_t now) {
  open_ = ok;
  stale_ = false;
  attempt_ = now;
}

void AudioRecovery::set_paused(bool paused) {
  if (paused_ && !paused) {
    stale_ = true;
  }
  paused_ = paused;
}

void AudioRecovery::lost() { stale_ = true; }

bool AudioRecovery::should_reopen(std::uint64_t now, std::uint64_t last_fed) const {
  if (paused_) {
    return false;
  }
  if (!open_) {
    return now >= attempt_ + kRetryMs;
  }
  return stale_ || now >= last_fed + kStallMs;
}

void SDLCALL AudioDevice::feed(void* userdata, SDL_AudioStream* stream, int additional, int /*total*/) {
  auto& device = *static_cast<AudioDevice*>(userdata);
  device.last_fed_ = SDL_GetTicks();
  std::vector<std::int16_t> samples(static_cast<std::size_t>(additional) / sizeof(std::int16_t));
  device.mixer_.render(samples);
  SDL_PutAudioStreamData(stream, samples.data(), static_cast<int>(samples.size() * sizeof(std::int16_t)));
}

bool SDLCALL AudioDevice::watch(void* userdata, SDL_Event* event) {
  if (event->type == SDL_EVENT_AUDIO_DEVICE_REMOVED && !event->adevice.recording) {
    static_cast<AudioDevice*>(userdata)->lost_ = true;
  }
  return true;
}

AudioDevice::AudioDevice(Mixer& mixer) : mixer_(mixer) {
  // Held apart from the audio subsystem, which would otherwise quit the
  // events (and their watches) when it is opened again.
  if (!SDL_InitSubSystem(SDL_INIT_EVENTS)) {
    throw std::runtime_error(std::format("SDL_InitSubSystem(EVENTS): {}", SDL_GetError()));
  }
  if (const std::optional<std::string> error = open_stream()) {
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
    throw std::runtime_error(*error);
  }
  if (!SDL_AddEventWatch(watch, this)) {
    const std::string error = SDL_GetError();
    close_stream();
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
    throw std::runtime_error(std::format("SDL_AddEventWatch: {}", error));
  }
  recovery_.opened(true, SDL_GetTicks());
}

AudioDevice::~AudioDevice() {
  SDL_RemoveEventWatch(watch, this);
  close_stream();
  SDL_QuitSubSystem(SDL_INIT_EVENTS);
}

void AudioDevice::set_paused(bool paused) {
  recovery_.set_paused(paused);
  if (paused && stream_ != nullptr && !SDL_PauseAudioStreamDevice(stream_)) {
    log_warning("cannot pause the sound: {}", SDL_GetError());
  }
}

void AudioDevice::update() {
  if (lost_.exchange(false)) {
    recovery_.lost();
  }
  const std::uint64_t last_fed = last_fed_;
  const std::uint64_t now = SDL_GetTicks();
  if (!recovery_.should_reopen(now, last_fed)) {
    return;
  }
  close_stream();
  if (const std::optional<std::string> error = open_stream()) {
    if (!failing_) {
      log_warning("cannot open the sound again: {}", *error);
    }
    failing_ = true;
    recovery_.opened(false, now);
    return;
  }
  log_info("sound device opened again");
  failing_ = false;
  recovery_.opened(true, now);
}

std::optional<std::string> AudioDevice::open_stream() {
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    return std::format("SDL_InitSubSystem(AUDIO): {}", SDL_GetError());
  }
  const SDL_AudioSpec spec{.format = SDL_AUDIO_S16, .channels = 1, .freq = kSampleRate};
  last_fed_ = SDL_GetTicks();
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, this);
  if (stream_ == nullptr) {
    std::string error = std::format("SDL_OpenAudioDeviceStream: {}", SDL_GetError());
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return error;
  }
  if (!recovery_.paused() && !SDL_ResumeAudioStreamDevice(stream_)) {
    std::string error = std::format("SDL_ResumeAudioStreamDevice: {}", SDL_GetError());
    close_stream();
    return error;
  }
  return std::nullopt;
}

void AudioDevice::close_stream() {
  if (stream_ != nullptr) {
    SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
}

}  // namespace oa
