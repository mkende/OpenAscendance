// The input layer: one key per poll, mouse edges, the shift state.

#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL_keycode.h>

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

}  // namespace
}  // namespace oa
