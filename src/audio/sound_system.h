// The sound system (`ssystem.cpp`, global `SoundSystem` 0xf8620): the
// streamed digital music of `music.txt` with per-tune resume positions, and
// the sound effects of `soundfx.txt` on 7 voices (`docs/02-audio-formats.md`).
// The MIDI path of the original is unused (no MDI data ships) and left out.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "assets/cfile.h"
#include "assets/sound_tables.h"
#include "audio/mixer.h"

namespace oa {

class SoundSystem {
 public:
  // The music volume at start (`+0x6fe`).
  static constexpr int kDefaultMusicVolume = 100;

  // `SoundSystem_init` (0x4a3c0) with `SoundSystem_init_music` (0x4aa6c):
  // reads `soundfx.txt` and `music.txt` through `files`. `mixer` (not owned,
  // may be null for no sound output) must outlive the system. Throws
  // `DataError` when a table is malformed.
  SoundSystem(const GameFiles& files, Mixer* mixer);

  // Whether the digital music is ready (`+0x223`: a mixer exists).
  bool music_ready() const { return mixer_ != nullptr; }
  bool music_muted() const { return music_muted_; }  // `+0x22b`
  bool sfx_muted() const { return sfx_muted_; }      // `+0x213`
  void set_music_muted(bool muted) { music_muted_ = muted; }
  void set_sfx_muted(bool muted) { sfx_muted_ = muted; }

  // Whether a tune plays (`+0x227`).
  bool tune_playing() const { return tune_playing_; }
  // The current (or last) tune (`+0x6fc`), -1 before the first.
  int current_tune() const { return current_tune_; }
  int tune_count() const { return static_cast<int>(tunes_.size()); }
  // The saved position of `tune` (`+0x392 + tune * 0x1c`), in samples.
  std::size_t saved_position(int tune) const { return tunes_.at(static_cast<std::size_t>(tune)).saved_position; }

  // `SoundSystem_play_tune` (0x4acdc): saves the position of the tune
  // playing, then starts `tune` at the music volume, from its saved position
  // with `resume`, else from the start. Nothing while muted or without
  // output. Returns whether it started. Throws `DataError` if the tune file
  // is missing.
  bool play_tune(int tune, bool resume);
  // `SoundSystem_stop_tune` (0x4ae2c): stops the tune playing, keeping its
  // position.
  void stop_tune();
  // `SoundSystem_stream_music` (0x4aec4), called by the main loop while it
  // waits: at the end of the file the tune's position returns to 0 and it
  // stops (the mixer streams the samples itself).
  void stream_music();
  // `SoundSystem_set_music_volume` (0x4b35c): stores `volume` when it is in
  // 0..127 (others are ignored), then applies the stored volume to the music.
  void set_music_volume(int volume);
  int music_volume() const { return music_volume_; }

  // `SoundSystem_play_sfx` (0x4afa0): plays sound `id` on the first free
  // voice; dropped when all 7 are busy, when muted or without output.
  // Throws `DataError` when the sound's file is missing or too short.
  void play_sfx(int id);

 private:
  struct Tune {
    std::string file;
    std::size_t saved_position = 0;
  };

  const GameFiles& files_;
  Mixer* mixer_;
  SoundFxTable sfx_table_;
  std::vector<Tune> tunes_;
  std::map<int, SampleData> sfx_cache_;
  bool music_muted_ = false;
  bool sfx_muted_ = false;
  bool tune_playing_ = false;
  int current_tune_ = -1;
  int music_volume_ = kDefaultMusicVolume;
};

}  // namespace oa
