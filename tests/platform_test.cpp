// The input layer: one key per poll, mouse edges, the shift state; the
// sound output opened again once lost.

#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_timer.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "assets/pcm.h"
#include "audio/mixer.h"
#include "platform/audio_device.h"
#include "platform/clock.h"
#include "platform/input.h"

namespace oa {
namespace {

InputEvent key(std::uint32_t k, std::uint16_t mods = 0) {
  return InputEvent{.type = EventType::kKeyDown, .key = k, .mods = mods};
}

TEST_CASE("input_poll reads one BIOS key per pass", "[platform]") {
  Input in;
  in.feed(key(SDLK_ESCAPE));
  in.feed(key(SDLK_X, SDL_KMOD_LALT));
  const InputState& s = in.poll();
  CHECK(s.key_event);
  CHECK(s.scan_code == 0x01);
  CHECK(s.ascii == 0x1b);
  const InputState& t = in.poll();
  CHECK(t.key_event);
  CHECK(t.scan_code == 0x2d);
  CHECK(t.ascii == 0);
  CHECK(t.alt);
  CHECK_FALSE(in.poll().key_event);
}

TEST_CASE("the keyboard buffer holds 15 keys", "[platform]") {
  Input in;
  for (int i = 0; i < 20; ++i) {
    in.feed(key(SDLK_A));
  }
  int keys = 0;
  while (in.poll().key_event) {
    ++keys;
  }
  CHECK(keys == 15);
}

TEST_CASE("a click shows as a press then a release on two passes", "[platform]") {
  Input in;
  in.feed(InputEvent{.type = EventType::kMouseDown, .x = 10, .y = 20, .button = MouseButton::kLeft});
  in.feed(InputEvent{.type = EventType::kMouseUp, .x = 10, .y = 20, .button = MouseButton::kLeft});
  InputState s = in.poll();
  CHECK(s.moved);
  CHECK(s.x == 10);
  CHECK(s.left_down);
  CHECK(s.left_changed);
  s = in.poll();
  CHECK_FALSE(s.moved);
  CHECK_FALSE(s.left_down);
  CHECK(s.left_changed);
  s = in.poll();
  CHECK_FALSE(s.left_changed);
}

TEST_CASE("the pointer stays on the 640x480 screen", "[platform]") {
  Input in;
  in.feed(InputEvent{.type = EventType::kMouseMove, .x = -5, .y = 900});
  const InputState& s = in.poll();
  CHECK(s.x == 0);
  CHECK(s.y == 479);
}

TEST_CASE("the virtual clock moves only when waiting", "[platform]") {
  Clock c(true);
  CHECK(c.ticks() == 0);
  c.wait_until_tick(3);
  CHECK(c.ticks() == 3);
  CHECK(c.micros() == 30'000);
  c.wait_vblank();
  CHECK(c.micros() == 2 * (1'000'000 / Clock::kVblankHz));
}

TEST_CASE("the sound device is opened again after the background, once lost, and while it does not play",
          "[platform]") {
  AudioRecovery r;
  r.opened(true, 1000);
  CHECK_FALSE(r.should_reopen(1500, 1490));
  // In the background: never, whatever happens meanwhile.
  r.set_paused(true);
  r.lost();
  CHECK_FALSE(r.should_reopen(60'000, 1490));
  r.set_paused(false);
  CHECK(r.should_reopen(60'000, 1490));
  r.opened(true, 60'000);
  CHECK_FALSE(r.should_reopen(60'010, 60'005));
  // Back from the background without a loss: opened again too.
  r.set_paused(true);
  r.set_paused(false);
  CHECK(r.should_reopen(60'010, 60'005));
  r.opened(true, 60'010);
  // A device lost.
  r.lost();
  CHECK(r.should_reopen(60'020, 60'015));
  r.opened(true, 60'020);
  // The device stopped pulling samples.
  CHECK_FALSE(r.should_reopen(60'020 + AudioRecovery::kStallMs - 1, 60'020));
  CHECK(r.should_reopen(60'020 + AudioRecovery::kStallMs, 60'020));
  // Failed: tried again a little later.
  r.opened(false, 70'000);
  CHECK_FALSE(r.should_reopen(70'000 + AudioRecovery::kRetryMs - 1, 0));
  CHECK(r.should_reopen(70'000 + AudioRecovery::kRetryMs, 0));
}

// The physical playback device (SDL's dummy driver has one).
SDL_AudioDeviceID playback_device() {
  int count = 0;
  SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);
  const SDL_AudioDeviceID device = count > 0 ? devices[0] : 0;
  SDL_free(devices);
  return device;
}

TEST_CASE("the sound plays on a new device after the background and once the device is lost", "[platform]") {
  SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
  Mixer mixer;
  AudioDevice device(mixer);
  const auto effect = std::make_shared<const std::vector<std::uint8_t>>(kSampleRate / 20, 200);
  // Plays the effect, updating the device as the main loop does; returns
  // whether it was played to its end within 5 seconds.
  const auto plays = [&] {
    mixer.play_sfx(0, effect, Mixer::kMaxVolume);
    for (const std::uint64_t end = SDL_GetTicks() + 5000; SDL_GetTicks() < end; SDL_Delay(5)) {
      device.update();
      if (!mixer.sfx_playing(0)) {
        return true;
      }
    }
    return false;
  };
  CHECK(plays());

  device.set_paused(true);
  const SDL_AudioDeviceID before_background = playback_device();
  mixer.play_sfx(0, effect, Mixer::kMaxVolume);
  SDL_Delay(200);
  device.update();
  CHECK(mixer.sfx_playing(0));
  mixer.stop_all_sfx();
  device.set_paused(false);
  CHECK(plays());
  // The whole audio subsystem was opened again: Android's only (default)
  // device may be the lost one.
  const SDL_AudioDeviceID after_background = playback_device();
  CHECK(after_background != before_background);

  SDL_Event removed{};
  removed.adevice.type = SDL_EVENT_AUDIO_DEVICE_REMOVED;
  removed.adevice.which = after_background;
  REQUIRE(SDL_PushEvent(&removed));
  CHECK(plays());
  CHECK(playback_device() != after_background);
}

}  // namespace
}  // namespace oa
