// The SDL3 audio output replacing the Miles AIL digital driver: pulls 16-bit
// mono samples at 22050 Hz from a `Mixer` on SDL's audio thread (SDL
// converts them to the device's format and rate).
#pragma once

#include "audio/mixer.h"

struct SDL_AudioStream;

namespace oa {

class AudioDevice {
 public:
  // Opens the default playback device and starts it; `mixer` must outlive
  // the device. Throws `std::runtime_error` if SDL fails.
  explicit AudioDevice(Mixer& mixer);
  ~AudioDevice();
  AudioDevice(const AudioDevice&) = delete;
  AudioDevice& operator=(const AudioDevice&) = delete;

  // Pauses or resumes the output (the program in the background).
  void set_paused(bool paused);

 private:
  Mixer& mixer_;
  SDL_AudioStream* stream_ = nullptr;
};

}  // namespace oa
