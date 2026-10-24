// The SDL3 audio output replacing the Miles AIL digital driver: pulls 16-bit
// mono samples at 22050 Hz from a `Mixer` on SDL's audio thread (SDL
// converts them to the device's format and rate).
//
// The output outlives the system taking its device away (a remake addition):
// a phone call on Android, a headset unplugged. SDL keeps a stream opened on
// the default device on the lost device until another default device
// appears, which never happens on Android (SDL only knows one device there,
// the default one): the output would then stay silent. So the device is
// opened again, with the whole SDL audio subsystem, when the program comes
// back from the background, when SDL reports a playback device lost and when
// the device stops pulling samples (`AudioRecovery`).
#pragma once

#include <SDL3/SDL_stdinc.h>

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

#include "audio/mixer.h"

struct SDL_AudioStream;
union SDL_Event;

namespace oa {

// When `AudioDevice` opens its device again, apart from SDL. The times are
// in milliseconds on a steady clock.
class AudioRecovery {
 public:
  // The longest time the device may go without pulling samples, while the
  // output plays, before it is taken for lost.
  static constexpr std::uint64_t kStallMs = 2000;
  // The time between two attempts to open the device while it cannot be.
  static constexpr std::uint64_t kRetryMs = 2000;

  // Notes an attempt to open the device at `now` and whether it succeeded.
  void opened(bool ok, std::uint64_t now);
  // Notes the output paused (the program in the background) or resumed.
  // Once resumed the device is opened again: the system may have lost or
  // moved it meanwhile.
  void set_paused(bool paused);
  bool paused() const { return paused_; }
  // Notes that the system lost a playback device.
  void lost();
  // Returns whether to open the device again at `now`, the device having
  // last pulled samples at `last_fed`; never while the output is paused.
  bool should_reopen(std::uint64_t now, std::uint64_t last_fed) const;

 private:
  bool paused_ = false;
  bool open_ = false;
  // Whether the output was resumed or a device lost since the last attempt
  // to open the device.
  bool stale_ = false;
  std::uint64_t attempt_ = 0;
};

class AudioDevice {
 public:
  // Opens the default playback device and starts it; `mixer` must outlive
  // the device. Throws `std::runtime_error` if SDL fails.
  explicit AudioDevice(Mixer& mixer);
  ~AudioDevice();
  AudioDevice(const AudioDevice&) = delete;
  AudioDevice& operator=(const AudioDevice&) = delete;

  // Pauses or resumes the output (the program in the background). Resuming
  // opens the device again, at the next `update`.
  void set_paused(bool paused);
  // Opens the device again when `AudioRecovery` says so. Called on the main
  // thread, outside of SDL's event poll (once a main-loop pass).
  void update();

 private:
  // Initializes the SDL audio subsystem and opens the stream (paused when
  // the output is); returns SDL's error if it fails.
  std::optional<std::string> open_stream();
  // Closes the stream and quits the SDL audio subsystem, if open.
  void close_stream();
  // SDL's stream callback: mixes what the device asks for.
  static void SDLCALL feed(void* userdata, SDL_AudioStream* stream, int additional, int total);
  // SDL's event watch: notes the playback devices lost.
  static bool SDLCALL watch(void* userdata, SDL_Event* event);

  Mixer& mixer_;
  SDL_AudioStream* stream_ = nullptr;
  AudioRecovery recovery_;
  // When the device last pulled samples (`SDL_GetTicks`, set on the audio
  // thread).
  std::atomic<std::uint64_t> last_fed_ = 0;
  // Whether SDL reported a playback device lost since the last `update`.
  std::atomic<bool> lost_ = false;
  // Whether opening the device again failed since it was last open (logged
  // once).
  bool failing_ = false;
};

}  // namespace oa
