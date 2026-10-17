// The digital mixer replacing the Miles AIL 3.02 digital driver: one
// streamed music voice (AIL sample handle 0) and 7 sound-effect voices
// (handles 1..7), all 8-bit unsigned mono at 22050 Hz, mixed to signed
// 16-bit mono at the same rate. Thread-safe: the audio thread renders while
// the game thread controls the voices.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace oa {

// Samples shared between the game (cache) and the mixer.
using SampleData = std::shared_ptr<const std::vector<std::uint8_t>>;

class Mixer {
 public:
  // The sound-effect voices (sample handles 1..7).
  static constexpr int kSfxVoices = 7;
  // The AIL volume range (`AIL_set_sample_volume`): 0..127.
  static constexpr int kMaxVolume = 127;

  // Starts `music` from sample `position` at the current music volume,
  // replacing any music playing.
  void play_music(SampleData music, std::size_t position);
  // Stops the music; returns the position reached (0 if none played).
  std::size_t stop_music();
  // Whether music plays (false once it ended: the voice then stops by itself).
  bool music_playing() const;
  // Sets the music volume (clamped to 0..127).
  void set_music_volume(int volume);
  int music_volume() const;

  // Plays `sample` on voice `voice` (0..6) at `volume` (0..127).
  void play_sfx(int voice, SampleData sample, int volume);
  // Whether voice `voice` (0..6) still plays.
  bool sfx_playing(int voice) const;
  void stop_all_sfx();

  // Mixes the next `out.size()` output samples (audio thread).
  void render(std::span<std::int16_t> out);

 private:
  struct Voice {
    SampleData data;
    std::size_t position = 0;
    int volume = kMaxVolume;
    bool playing = false;
  };

  mutable std::mutex mutex_;
  Voice music_;
  std::array<Voice, kSfxVoices> sfx_;
};

}  // namespace oa
