// The mixer and the sound system (music resume positions, the 7 SFX voices).

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

#include "audio/mixer.h"
#include "audio/sound_system.h"
#include "game_data.h"

namespace oa {
namespace {

SampleData samples(std::vector<std::uint8_t> v) { return std::make_shared<const std::vector<std::uint8_t>>(v); }

TEST_CASE("the mixer converts unsigned 8-bit samples, scales by volume and clamps", "[audio]") {
  Mixer m;
  m.set_music_volume(127);
  m.play_music(samples({255, 0, 128}), 0);
  m.play_sfx(0, samples({255, 0}), 127);
  std::vector<std::int16_t> out(4);
  m.render(out);
  CHECK(out[0] == 32767);  // 2 x 127 * 256, clamped
  CHECK(out[1] == -32768);
  CHECK(out[2] == 0);
  CHECK(out[3] == 0);
  CHECK_FALSE(m.music_playing());  // the music ended by itself
  CHECK_FALSE(m.sfx_playing(0));
  m.set_music_volume(200);
  CHECK(m.music_volume() == 127);
}

TEST_CASE("the mixer scales the music and the sound effects by their levels", "[audio]") {
  Mixer m;
  CHECK(m.music_level() == 100);
  CHECK(m.sfx_level() == 100);
  m.set_music_volume(SoundSystem::kDefaultMusicVolume);
  m.set_music_level(50);
  m.set_sfx_level(60);
  m.play_music(samples({255, 255, 255}), 0);
  std::vector<std::int16_t> out(1);
  m.render(out);
  CHECK(out[0] == 127 * 256 * 100 / 127 * 50 / 100);
  m.stop_music();
  // The effects at the AIL volume of the original, then the level.
  m.play_sfx(0, samples({255, 0, 255}), 127);
  m.render(out);
  CHECK(out[0] == 127 * 256 * 60 / 100);
  // A new level applies to the voices playing.
  m.set_sfx_level(0);
  m.render(out);
  CHECK(out[0] == 0);
  m.set_sfx_level(100);
  m.render(out);
  CHECK(out[0] == 127 * 256);
  m.set_sfx_level(150);
  CHECK(m.sfx_level() == 100);
  m.set_music_level(-3);
  CHECK(m.music_level() == 0);
}

TEST_CASE("the sound system's levels reach the mixer and survive muting and the music volume", "[audio][content]") {
  OA_REQUIRE_GAME_FILES(files);
  Mixer m;
  SoundSystem sound(files, &m);
  sound.set_music_level(30);
  sound.set_sfx_level(120);
  CHECK(sound.music_level() == 30);
  CHECK(sound.sfx_level() == 100);
  CHECK(m.music_level() == 30);
  sound.set_music_muted(true);
  sound.set_music_muted(false);
  // `SoundSystem_set_music_volume` keeps its own volume, the level apart.
  sound.set_music_volume(64);
  CHECK(m.music_volume() == 64);
  CHECK(m.music_level() == 30);
}

TEST_CASE("music positions are kept per tune and resumed", "[audio][content]") {
  OA_REQUIRE_GAME_FILES(files);
  Mixer m;
  SoundSystem sound(files, &m);
  CHECK(sound.tune_count() == 29);
  REQUIRE(sound.play_tune(0, false));
  std::vector<std::int16_t> out(1000);
  m.render(out);
  sound.stop_tune();
  CHECK_FALSE(sound.tune_playing());
  CHECK(sound.saved_position(0) == 1000);
  // Another tune starts from its start, then tune 0 resumes.
  sound.play_tune(5, false);
  m.render(out);
  sound.play_tune(0, true);
  CHECK(sound.saved_position(5) == 1000);
  m.render(out);
  sound.stop_tune();
  CHECK(sound.saved_position(0) == 2000);
  // Muted, nothing starts.
  sound.set_music_muted(true);
  CHECK_FALSE(sound.play_tune(1, false));
}

TEST_CASE("a tune that ends is rewound and stops", "[audio][content]") {
  OA_REQUIRE_GAME_FILES(files);
  Mixer m;
  SoundSystem sound(files, &m);
  sound.play_tune(5, false);
  std::vector<std::int16_t> out(1 << 22);  // longer than victory.raw
  m.render(out);
  sound.stream_music();
  CHECK_FALSE(sound.tune_playing());
  CHECK(sound.saved_position(5) == 0);
}

TEST_CASE("sound effects take the first free of 7 voices, the 8th is dropped", "[audio][content]") {
  OA_REQUIRE_GAME_FILES(files);
  Mixer m;
  SoundSystem sound(files, &m);
  for (int i = 0; i < 8; ++i) {
    sound.play_sfx(0);
  }
  for (int v = 0; v < Mixer::kSfxVoices; ++v) {
    CHECK(m.sfx_playing(v));
  }
  sound.set_sfx_muted(true);
  m.stop_all_sfx();
  sound.play_sfx(0);
  CHECK_FALSE(m.sfx_playing(0));
}

TEST_CASE("the stored music volume ignores values outside 0..127", "[audio][content]") {
  OA_REQUIRE_GAME_FILES(files);
  Mixer m;
  SoundSystem sound(files, &m);
  CHECK(sound.music_volume() == SoundSystem::kDefaultMusicVolume);
  sound.set_music_volume(-127);
  CHECK(sound.music_volume() == SoundSystem::kDefaultMusicVolume);
  sound.set_music_volume(30);
  CHECK(m.music_volume() == 30);
}

}  // namespace
}  // namespace oa
