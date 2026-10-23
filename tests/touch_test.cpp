// The remake's controls for touch screens and Android (not in the
// original): the touch screen as the mouse (`platform/touch.h`), the view
// drags and pinches of the star map and the System Display, the lists' and
// the research tree's drags and wheel, the system's
// Back (`WinMgr::system_back`), and the game kept in `resume.gam` when the
// system sends the program to the background (`app/corn.h`), resumed from
// there by the next start (OA_DATA install for the screens and the app).

#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keycode.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <tuple>
#include <utility>
#include <vector>

#include "app/command_line.h"
#include "app/corn.h"
#include "core_session.h"
#include "game/cosmos.h"
#include "game/save.h"
#include "game_data.h"
#include "platform/display.h"
#include "platform/input.h"
#include "platform/touch.h"
#include "screens/bat_mode_wnd.h"
#include "screens/cos_wnd.h"
#include "screens/new_game_wnd.h"
#include "test_dirs.h"
#include "ui/game_core_session.h"
#include "ui/list_box.h"
#include "ui_rig.h"

namespace oa {
namespace {

InputEvent finger(EventType type, std::uint64_t id, int x, int y, std::uint64_t ms) {
  return InputEvent{.type = type, .x = x, .y = y, .finger = id, .time_ms = ms};
}

TEST_CASE("touch: a tap is a left click where the finger was put, after the pointer moved there", "[platform]") {
  Input input;
  TouchMapper touch(input);
  touch.feed(finger(EventType::kFingerDown, 1, 100, 200, 0));
  const InputState& s = input.poll();
  CHECK(s.x == 100);
  CHECK(s.y == 200);
  CHECK(s.moved);
  CHECK(!s.left_changed);
  touch.feed(finger(EventType::kFingerMove, 1, 103, 198, 50));
  touch.feed(finger(EventType::kFingerUp, 1, 104, 197, 120));
  touch.update(130);
  input.poll();
  CHECK(s.left_down);
  CHECK(s.left_changed);
  CHECK(s.x == 100);
  CHECK(!s.right_down);
  input.poll();
  CHECK(!s.left_down);
  CHECK(s.left_changed);
  CHECK(!s.gesture.started);
}

TEST_CASE("touch: a long press holds the right button until the finger is lifted", "[platform]") {
  Input input;
  TouchMapper touch(input);
  touch.feed(finger(EventType::kFingerDown, 7, 300, 300, 1000));
  touch.update(1000 + TouchMapper::kLongPress - 1);
  const InputState& s = input.poll();
  CHECK(!s.right_down);
  touch.update(1000 + TouchMapper::kLongPress);
  input.poll();
  CHECK(s.right_down);
  CHECK(s.right_changed);
  input.poll();
  CHECK(s.right_down);
  touch.feed(finger(EventType::kFingerUp, 7, 300, 300, 2500));
  input.poll();
  CHECK(!s.right_down);
  CHECK(!s.left_down);
  CHECK(!s.left_changed);
}

TEST_CASE("touch: a long press is what the query answers for the place the finger was put", "[platform]") {
  Input input;
  TouchMapper touch(input);
  LongPress answer = LongPress::kHelp;
  std::vector<std::pair<int, int>> asked;
  touch.set_long_press_query([&](int x, int y) {
    asked.emplace_back(x, y);
    return answer;
  });
  const InputState& s = input.poll();
  // Held still `kLongPress` from `ms` at (x, y).
  const auto hold = [&](int x, int y, std::uint64_t ms) {
    touch.feed(finger(EventType::kFingerDown, 1, x, y, ms));
    touch.update(ms + TouchMapper::kLongPress);
  };

  SECTION("a help click: Shift with the press only, once") {
    hold(100, 120, 0);
    CHECK(asked == std::vector<std::pair<int, int>>{{100, 120}});
    input.poll();
    CHECK(s.shift);
    CHECK(s.left_down);
    CHECK(s.left_changed);
    input.poll();
    CHECK(!s.shift);
    CHECK(!s.left_down);
    CHECK(s.left_changed);
    touch.feed(finger(EventType::kFingerMove, 1, 160, 120, 600));
    touch.feed(finger(EventType::kFingerUp, 1, 160, 120, 700));
    input.poll();
    CHECK(!s.left_changed);
    CHECK(!s.right_down);
    CHECK(s.x == 100);
  }
  SECTION("the left button held until the lift") {
    answer = LongPress::kLeftHold;
    hold(100, 120, 0);
    input.poll();
    CHECK(s.left_down);
    CHECK(!s.shift);
    CHECK(!s.right_down);
    input.poll();
    CHECK(s.left_down);
    touch.feed(finger(EventType::kFingerUp, 1, 100, 120, 900));
    input.poll();
    CHECK(!s.left_down);
    CHECK(s.left_changed);
  }
  SECTION("a move drag: the pointer follows, no button until the lift's click there") {
    answer = LongPress::kMoveDrag;
    hold(100, 120, 0);
    touch.feed(finger(EventType::kFingerMove, 1, 180, 150, 600));
    input.poll();
    CHECK(s.x == 180);
    CHECK(s.y == 150);
    CHECK(!s.left_down);
    CHECK(!s.right_down);
    CHECK(!s.gesture.started);
    touch.feed(finger(EventType::kFingerUp, 1, 180, 150, 700));
    input.poll();
    CHECK(s.left_down);
    CHECK(s.x == 180);
    input.poll();
    CHECK(!s.left_down);
  }
  SECTION("where a long press drags the pointer, so does a drag at once, without turning the view") {
    answer = LongPress::kMoveDrag;
    touch.feed(finger(EventType::kFingerDown, 1, 100, 120, 0));
    touch.feed(finger(EventType::kFingerMove, 1, 140, 130, 50));
    input.poll();
    CHECK(asked == std::vector<std::pair<int, int>>{{100, 120}});
    CHECK(s.x == 140);
    CHECK(s.y == 130);
    CHECK(!s.gesture.started);
    CHECK(!s.left_changed);
    touch.update(5000);  // already moving: no other long press
    CHECK(asked.size() == 1);
    touch.feed(finger(EventType::kFingerUp, 1, 140, 130, 5000));
    input.poll();
    CHECK(s.left_down);
    CHECK(s.x == 140);
  }
  SECTION("a move drag cancelled by more fingers clicks nothing") {
    answer = LongPress::kMoveDrag;
    hold(100, 120, 0);
    touch.feed(finger(EventType::kFingerDown, 2, 300, 120, 600));
    touch.feed(finger(EventType::kFingerDown, 3, 200, 300, 600));
    touch.feed(finger(EventType::kFingerUp, 1, 100, 120, 700));
    input.poll();
    CHECK(!s.left_changed);
  }
}

TEST_CASE("touch: a moving finger drags the view without a click; two fingers pinch", "[platform]") {
  Input input;
  TouchMapper touch(input);
  touch.feed(finger(EventType::kFingerDown, 1, 200, 200, 0));
  touch.feed(finger(EventType::kFingerMove, 1, 230, 190, 40));
  const InputState& s = input.poll();
  CHECK(s.gesture.started);
  CHECK(s.gesture.active);
  CHECK(s.gesture.x == 200);
  CHECK(s.gesture.y == 200);
  CHECK(s.gesture.dx == 30);
  CHECK(s.gesture.dy == -10);
  touch.feed(finger(EventType::kFingerMove, 1, 235, 192, 60));
  touch.update(5000);  // moved: never a long press
  input.poll();
  CHECK(!s.gesture.started);
  CHECK(s.gesture.dx == 5);
  CHECK(s.gesture.dy == 2);
  touch.feed(finger(EventType::kFingerUp, 1, 235, 192, 5000));
  input.poll();
  CHECK(!s.gesture.active);
  input.poll();
  CHECK(!s.left_changed);
  CHECK(!s.right_changed);

  // A pinch: the fingers 100 then 200 pixels apart.
  touch.feed(finger(EventType::kFingerDown, 2, 300, 100, 6000));
  touch.feed(finger(EventType::kFingerDown, 3, 400, 100, 6010));
  touch.feed(finger(EventType::kFingerMove, 3, 500, 100, 6000 + TouchMapper::kPinchDelay + 10));
  input.poll();
  CHECK(s.gesture.started);
  CHECK(s.gesture.x == 350);
  CHECK(s.gesture.zoom == 2.0F);
  touch.feed(finger(EventType::kFingerUp, 2, 300, 100, 6200));
  touch.feed(finger(EventType::kFingerUp, 3, 500, 100, 6210));
  input.poll();
  CHECK(!s.gesture.active);
  input.poll();
  CHECK(!s.left_changed);
  CHECK(!s.left_down);
}

TEST_CASE("touch: a pinch zooms once a third finger can no longer join it", "[platform]") {
  Input input;
  TouchMapper touch(input);
  touch.feed(finger(EventType::kFingerDown, 1, 300, 100, 0));
  touch.feed(finger(EventType::kFingerDown, 2, 400, 100, 20));
  touch.feed(finger(EventType::kFingerMove, 2, 450, 100, 40));
  const InputState& s = input.poll();
  CHECK(!s.gesture.started);
  CHECK(s.gesture.zoom == 1.0F);
  touch.update(20 + TouchMapper::kPinchDelay);
  input.poll();
  CHECK(s.gesture.started);
  CHECK(s.gesture.x == 350);
  CHECK(s.gesture.zoom == 1.5F);
  touch.feed(finger(EventType::kFingerMove, 2, 600, 100, 200));
  input.poll();
  CHECK(s.gesture.zoom == 2.0F);
  touch.feed(finger(EventType::kFingerUp, 1, 300, 100, 300));
  touch.feed(finger(EventType::kFingerUp, 2, 600, 100, 300));
  input.poll();
  CHECK(!s.gesture.active);

  // Lifted before the delay: still a pinch.
  touch.feed(finger(EventType::kFingerDown, 3, 300, 100, 1000));
  touch.feed(finger(EventType::kFingerDown, 4, 400, 100, 1000));
  touch.feed(finger(EventType::kFingerMove, 4, 500, 100, 1030));
  touch.feed(finger(EventType::kFingerUp, 3, 300, 100, 1050));
  touch.feed(finger(EventType::kFingerUp, 4, 500, 100, 1050));
  input.poll();
  CHECK(s.gesture.started);
  CHECK(!s.gesture.active);
  CHECK(s.gesture.zoom == 2.0F);
  input.poll();
  CHECK(!s.left_changed);
}

TEST_CASE("touch: a pinch's middle drags as it zooms; the finger left after it drags on", "[platform]") {
  Input input;
  TouchMapper touch(input);
  const InputState& s = input.poll();
  // A drag, then a second finger: the same gesture goes on.
  touch.feed(finger(EventType::kFingerDown, 1, 200, 200, 0));
  touch.feed(finger(EventType::kFingerMove, 1, 220, 200, 10));
  touch.feed(finger(EventType::kFingerDown, 2, 320, 200, 20));
  input.poll();
  CHECK(s.gesture.started);
  CHECK(s.gesture.dx == 20);
  touch.update(20 + TouchMapper::kPinchDelay);
  // Both fingers 40 pixels to the right, the second 60 more: the middle
  // 70 to the right, twice the spread.
  touch.feed(finger(EventType::kFingerMove, 1, 260, 200, 200));
  touch.feed(finger(EventType::kFingerMove, 2, 420, 200, 200));
  input.poll();
  CHECK(!s.gesture.started);
  CHECK(s.gesture.active);
  CHECK(s.gesture.dx == 70);
  CHECK(s.gesture.dy == 0);
  CHECK(std::abs(s.gesture.zoom - 1.6F) < 1e-5F);
  // The second lifted: the first drags on, the gesture never ended.
  touch.feed(finger(EventType::kFingerUp, 2, 420, 200, 300));
  touch.feed(finger(EventType::kFingerMove, 1, 230, 210, 310));
  input.poll();
  CHECK(!s.gesture.started);
  CHECK(s.gesture.active);
  CHECK(s.gesture.dx == -30);
  CHECK(s.gesture.dy == 10);
  CHECK(s.gesture.zoom == 1.0F);
  // A second finger again: a pinch again; the first lifted, the second drags.
  touch.feed(finger(EventType::kFingerDown, 3, 330, 210, 400));
  touch.feed(finger(EventType::kFingerUp, 1, 230, 210, 450));
  touch.feed(finger(EventType::kFingerMove, 3, 300, 220, 460));
  input.poll();
  CHECK(!s.gesture.started);
  CHECK(s.gesture.active);
  CHECK(s.gesture.dx == -30);
  CHECK(s.gesture.dy == 10);
  touch.feed(finger(EventType::kFingerUp, 3, 300, 220, 500));
  input.poll();
  CHECK(!s.gesture.active);
  input.poll();
  CHECK(!s.left_changed);
}

// Puts three fingers down around (cx, cy), `radius` pixels from it, at
// `ms` and the next 2 milliseconds.
void three_fingers(TouchMapper& touch, int cx, int cy, int radius, std::uint64_t ms) {
  touch.feed(finger(EventType::kFingerDown, 1, cx - radius, cy, ms));
  touch.feed(finger(EventType::kFingerDown, 2, cx + radius, cy, ms + 1));
  touch.feed(finger(EventType::kFingerDown, 3, cx, cy + radius, ms + 2));
}

// Moves the three fingers of `three_fingers` to `radius` pixels from the
// centre.
void move_three(TouchMapper& touch, int cx, int cy, int radius, std::uint64_t ms) {
  touch.feed(finger(EventType::kFingerMove, 1, cx - radius, cy, ms));
  touch.feed(finger(EventType::kFingerMove, 2, cx + radius, cy, ms));
  touch.feed(finger(EventType::kFingerMove, 3, cx, cy + radius, ms));
}

TEST_CASE("touch: three fingers coming together are Back, once, without clicks, drags or zooms", "[platform]") {
  Input input;
  TouchMapper touch(input);
  const InputState& s = input.poll();
  // Not together enough yet.
  three_fingers(touch, 320, 240, 150, 0);
  move_three(touch, 320, 240, 100, 50);
  touch.update(1000);  // no long press either
  input.poll();
  CHECK(!s.back);
  // Together: Back, once.
  int backs = 0;
  for (const int radius : {80, 60, 40, 20}) {
    move_three(touch, 320, 240, radius, 1100);
    input.poll();
    backs += s.back ? 1 : 0;
    CHECK(!s.left_changed);
    CHECK(!s.right_changed);
    CHECK(s.gesture.zoom == 1.0F);
    CHECK(s.gesture.dx == 0);
    CHECK(s.gesture.dy == 0);
  }
  CHECK(backs == 1);
  for (const std::uint64_t id : {1, 2, 3}) {
    touch.feed(finger(EventType::kFingerUp, id, 320, 240, 1200));
  }
  input.poll();
  CHECK(!s.back);
  CHECK(!s.left_changed);
  CHECK(!s.right_changed);

  // A finger lifted first: nothing, until a new touch, which taps again.
  three_fingers(touch, 320, 240, 150, 2000);
  touch.feed(finger(EventType::kFingerUp, 3, 320, 390, 2050));
  touch.feed(finger(EventType::kFingerMove, 1, 310, 240, 2100));
  touch.feed(finger(EventType::kFingerMove, 2, 330, 240, 2100));
  touch.feed(finger(EventType::kFingerUp, 1, 310, 240, 2150));
  touch.feed(finger(EventType::kFingerUp, 2, 330, 240, 2150));
  input.poll();
  CHECK(!s.back);
  CHECK(s.gesture.zoom == 1.0F);
  touch.feed(finger(EventType::kFingerDown, 4, 100, 100, 3000));
  touch.feed(finger(EventType::kFingerUp, 4, 100, 100, 3050));
  input.poll();
  CHECK(s.left_changed);
  CHECK(!s.back);

  // A pinch then a third finger: the pinch ends, the fingers together are
  // Back.
  touch.feed(finger(EventType::kFingerDown, 5, 100, 240, 4000));
  touch.feed(finger(EventType::kFingerDown, 6, 500, 240, 4000));
  touch.update(4000 + TouchMapper::kPinchDelay);
  input.poll();
  input.poll();
  CHECK(s.gesture.active);
  touch.feed(finger(EventType::kFingerDown, 7, 300, 400, 4200));
  input.poll();
  CHECK(!s.gesture.active);
  touch.feed(finger(EventType::kFingerMove, 5, 250, 260, 4300));
  touch.feed(finger(EventType::kFingerMove, 6, 350, 260, 4300));
  touch.feed(finger(EventType::kFingerMove, 7, 300, 300, 4300));
  input.poll();
  CHECK(s.back);
  CHECK(s.gesture.zoom == 1.0F);
}

TEST_CASE("mouse: the left button moved past a few pixels drags the view; the press stays a click", "[platform]") {
  Input input;
  input.feed(InputEvent{.type = EventType::kMouseDown, .x = 50, .y = 60, .button = MouseButton::kLeft});
  input.feed(InputEvent{.type = EventType::kMouseMove, .x = 52, .y = 61});
  const InputState& s = input.poll();
  CHECK(s.left_down);
  CHECK(s.left_changed);
  CHECK(!s.gesture.started);
  input.feed(InputEvent{.type = EventType::kMouseMove, .x = 60, .y = 58});
  input.poll();
  CHECK(s.gesture.started);
  CHECK(s.gesture.x == 50);
  CHECK(s.gesture.dx == 10);
  CHECK(s.gesture.dy == -2);
  input.feed(InputEvent{.type = EventType::kMouseUp, .x = 60, .y = 58, .button = MouseButton::kLeft});
  input.poll();
  CHECK(!s.gesture.active);
  CHECK(!s.left_down);
}

TEST_CASE("mouse: the wheel's notches add up until the poll, the pointer where the wheel turned", "[platform]") {
  Input input;
  input.feed(InputEvent{.type = EventType::kMouseWheel, .x = 70, .y = 80, .wheel = 1});
  input.feed(InputEvent{.type = EventType::kMouseWheel, .x = 70, .y = 80, .wheel = 2});
  const InputState& s = input.poll();
  CHECK(s.wheel == 3);
  CHECK(s.x == 70);
  CHECK(s.y == 80);
  CHECK(!s.left_changed);
  input.poll();
  CHECK(s.wheel == 0);
  // A mouse drag is marked as such (a finger's is not).
  input.feed(InputEvent{.type = EventType::kMouseDown, .x = 50, .y = 60, .button = MouseButton::kLeft});
  input.feed(InputEvent{.type = EventType::kMouseMove, .x = 50, .y = 70});
  input.poll();
  CHECK(s.gesture.started);
  CHECK(s.gesture.mouse);
}

TEST_CASE("mouse: the pointer drawn between passes follows the moves at once, the game's at the poll", "[platform]") {
  Input input;
  input.poll();
  const InputState& s = input.poll();
  input.feed(InputEvent{.type = EventType::kMouseMove, .x = 100, .y = 50});
  CHECK(input.pointer() == std::pair{100, 50});
  input.feed(InputEvent{.type = EventType::kMouseMove, .x = 700, .y = -3});
  CHECK(input.pointer() == std::pair{kScreenWidth - 1, 0});  // kept on the screen
  CHECK(s.x == kScreenWidth / 2);
  CHECK(!s.moved);
  input.poll();
  CHECK(s.x == kScreenWidth - 1);
  CHECK(s.y == 0);
  CHECK(s.moved);
}

TEST_CASE("the 4:3 letterbox: centred, clear of the display cutout", "[platform]") {
  // A foldable's inner screen (near square) and outer one (tall), both ways.
  CHECK(letterbox(2208, 1840, {}) == PictureRect{0, 92, 2208, 1656});
  CHECK(letterbox(1840, 2208, {}) == PictureRect{0, 414, 1840, 1380});
  CHECK(letterbox(2092, 1080, {}) == PictureRect{326, 0, 1440, 1080});
  CHECK(letterbox(1080, 2092, {}) == PictureRect{0, 641, 1080, 810});
  // A camera hole in the top band: the picture below it.
  CHECK(letterbox(2208, 1840, {0, 120, 0, 0}) == PictureRect{0, 152, 2208, 1656});
  // On the left of a tall screen held sideways: in the black bar already.
  CHECK(letterbox(2092, 1080, {100, 0, 0, 0}) == PictureRect{376, 0, 1440, 1080});
  // A cutout leaving almost nothing is ignored.
  CHECK(letterbox(640, 480, {0, 400, 0, 0}) == PictureRect{0, 0, 640, 480});
}

TEST_CASE("the full screen's pointer confinement: the picture's window units, reaching its every edge", "[platform]") {
  CHECK(pointer_confinement(letterbox(1920, 1080, {}), 1) == WindowRect{240, 0, 1440, 1080});
  // A Retina display: two pixels per window unit.
  CHECK(pointer_confinement(letterbox(2880, 1800, {}), 2) == WindowRect{120, 0, 1200, 900});
  // Partly covered units are in.
  CHECK(pointer_confinement(letterbox(1366, 768, {}), 1.5F) == WindowRect{114, 0, 683, 512});
  for (const auto& [w, h, density] : std::array<std::tuple<int, int, float>, 5>{
           {{1920, 1080, 1}, {2880, 1800, 2}, {1366, 768, 1.5F}, {1280, 1024, 1.25F}, {3440, 1440, 1}}}) {
    INFO(w << "x" << h << " @" << density);
    const PictureRect p = letterbox(w, h, {});
    const WindowRect r = pointer_confinement(p, density);
    // The screen pixels of the rectangle's first and last units (as
    // `Display` maps the mouse), clamped by `Input`.
    const auto screen = [&](int unit, float origin, float size, int pixels) {
      return static_cast<int>(std::floor((static_cast<float>(unit) * density - origin) * pixels / size));
    };
    CHECK(screen(r.x, p.x, p.w, kScreenWidth) <= 0);
    CHECK(screen(r.x, p.x, p.w, kScreenWidth) >= -1);
    CHECK(screen(r.x + r.w - 1, p.x, p.w, kScreenWidth) >= kScreenWidth - 1);
    CHECK(screen(r.x + r.w - 1, p.x, p.w, kScreenWidth) <= kScreenWidth);
    CHECK(screen(r.y, p.y, p.h, kScreenHeight) <= 0);
    CHECK(screen(r.y + r.h - 1, p.y, p.h, kScreenHeight) >= kScreenHeight - 1);
    CHECK(screen(r.y + r.h - 1, p.y, p.h, kScreenHeight) <= kScreenHeight);
  }
}

// Sends SDL event `e` and returns the display's last input event.
std::optional<InputEvent> through(Display& display, SDL_Event e) {
  REQUIRE(SDL_PushEvent(&e));
  std::optional<InputEvent> last;
  while (const std::optional<InputEvent> in = display.poll()) {
    last = in;
  }
  return last;
}

TEST_CASE("a resized window: the letterbox and the pointer and fingers mapping follow at once", "[platform]") {
  SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
  struct Quit {
    ~Quit() { SDL_Quit(); }
  } quit;
  Display display("touch test", 1, Scaling::kSharp);
  for (const auto& [w, h] :
       std::array<std::pair<int, int>, 5>{{{2208, 1840}, {1840, 2208}, {2092, 1080}, {1080, 2092}, {640, 480}}}) {
    INFO(w << "x" << h);
    display.set_window_size(w, h);
    while (display.poll()) {
    }
    const PictureRect p = display.picture();
    REQUIRE(p == letterbox(w, h, {}));
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.x = p.x + p.w / 2;
    e.motion.y = p.y + p.h / 2;
    std::optional<InputEvent> in = through(display, e);
    REQUIRE(in);
    CHECK(in->type == EventType::kMouseMove);
    CHECK(in->x == 320);
    CHECK(in->y == 240);
    // The picture's bottom-right pixel, and left of the picture.
    e.motion.x = p.x + p.w - 0.5F;
    e.motion.y = p.y + p.h - 0.5F;
    in = through(display, e);
    CHECK(in->x == 639);
    CHECK(in->y == 479);
    e.motion.x = p.x - 1;
    in = through(display, e);
    CHECK(in->x < 0);

    e = SDL_Event{};
    e.type = SDL_EVENT_FINGER_DOWN;
    e.tfinger.fingerID = 5;
    e.tfinger.x = (p.x + p.w / 4) / static_cast<float>(w);
    e.tfinger.y = (p.y + p.h * 3 / 4) / static_cast<float>(h);
    in = through(display, e);
    REQUIRE(in);
    CHECK(in->type == EventType::kFingerDown);
    CHECK(in->x == 160);
    CHECK(in->y == 360);
    CHECK(in->finger == 5);
  }
}

// A game begun as the oracle's runs (`cosmos.txt` of 50 stars, 7 races),
// on the star map, under the window manager.
struct GameRig {
  GameRig(const GameFiles& files, const std::filesystem::path& dir)
      : session(files, dir, false), rig((start(), files), &session, dir), touch(rig.input) {
    wait(40);
    click(318, 295);  // FRESHGAME
    wait(60);
    click(594, 452);  // Begin New Game
    wait(60);
    click(320, 240);  // the species history
    wait(60);
  }

  void start() {
    const game::CosmosTxt txt{.stars = 50, .races = 7, .seed = 1234, .species = {}};
    session.start(0, &txt);
  }
  void wait(int passes) {
    for (int i = 0; i < passes; ++i) {
      rig.pass();
    }
  }
  void click(int x, int y) {
    rig.move(x, y);
    rig.click(x, y);
  }
  // A finger dragged from (x0, y0) by (dx, dy) in `steps` moves, a pass
  // each.
  void finger_drag(int x0, int y0, int dx, int dy, int steps) {
    touch.feed(finger(EventType::kFingerDown, 1, x0, y0, 0));
    rig.pass();
    for (int i = 1; i <= steps; ++i) {
      touch.feed(finger(EventType::kFingerMove, 1, x0 + dx * i / steps, y0 + dy * i / steps, 10));
      rig.pass();
    }
    touch.feed(finger(EventType::kFingerUp, 1, x0 + dx, y0 + dy, 20));
    wait(3);
  }
  // A pinch about (x, y) from `from` to `to` pixels apart.
  void pinch(int x, int y, int from, int to) {
    touch.feed(finger(EventType::kFingerDown, 1, x - from / 2, y, 0));
    touch.feed(finger(EventType::kFingerDown, 2, x + from / 2, y, 0));
    rig.pass();
    touch.feed(finger(EventType::kFingerMove, 1, x - to / 2, y, 10));
    touch.feed(finger(EventType::kFingerMove, 2, x + to / 2, y, 10));
    rig.pass();
    touch.feed(finger(EventType::kFingerUp, 1, x - to / 2, y, 20));
    touch.feed(finger(EventType::kFingerUp, 2, x + to / 2, y, 20));
    wait(3);
  }

  CosWnd& map() { return dynamic_cast<CosWnd&>(*rig.wm.find_window("COSMOSWnd")); }
  // The visible star nearest to the camera, and the farthest.
  int nearest_star(bool farthest = false) {
    const CosWnd::Item* best = nullptr;
    for (const CosWnd::Item& it : map().items()) {
      if (it.kind == CosWnd::ItemKind::kStar && !it.clipped &&
          (best == nullptr || (farthest ? it.depth > best->depth : it.depth < best->depth))) {
        best = &it;
      }
    }
    REQUIRE(best != nullptr);
    return best->index;
  }
  // The projected item of star `star` on the map.
  const CosWnd::Item& star_item(int star) {
    for (const CosWnd::Item& it : map().items()) {
      if (it.kind == CosWnd::ItemKind::kStar && it.index == star) {
        return it;
      }
    }
    FAIL("star not on the map");
    throw;
  }

  GameCoreSession session;
  test::UiRig rig;
  TouchMapper touch;
};

TEST_CASE("the star map turns with a drag (the galaxy follows it) and zooms with a pinch", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  test::TempDir dir("oa_touch");
  GameRig r(files, dir.path);
  REQUIRE(r.rig.wm.current_screen() == 1);
  const game::Session& s = *r.session.core();
  const int home = s.game->cosmos.races[s.player_race].home;
  const int near = r.nearest_star();
  const int far = r.nearest_star(true);
  const std::int16_t near_x = r.star_item(near).x;
  const std::int16_t far_x = r.star_item(far).x;
  const CosWnd::Item& item = r.star_item(home);

  // From the home star (explored): a finger never clicks it (the system
  // stays shut). The galaxy turns about its vertical axis, its near side
  // following the finger.
  r.finger_drag(r.map().pane().x0 + item.x, r.map().pane().y0 + item.y, 20, 0, 2);
  CHECK(r.rig.wm.current_screen() == 1);
  CHECK(r.map().camera().orientation != std::array<float, 9>{1, 0, 0, 0, 1, 0, 0, 0, 1});
  CHECK(r.star_item(near).x > near_x);
  CHECK(r.star_item(far).x < far_x);
  const std::array<float, 9> turned = r.map().camera().orientation;
  const std::int16_t near_y = r.star_item(r.nearest_star()).y;
  const int near2 = r.nearest_star();

  // A vertical drag with the mouse, from an empty corner of the map: the
  // press is the original's click (nothing there), the moves turn it.
  r.rig.move(20, 20);
  r.rig.pass(InputEvent{.type = EventType::kMouseDown, .x = 20, .y = 20, .button = MouseButton::kLeft});
  for (int y = 26; y <= 30; y += 2) {
    r.rig.move(20, y);
  }
  r.rig.pass(InputEvent{.type = EventType::kMouseUp, .x = 20, .y = 30, .button = MouseButton::kLeft});
  CHECK(r.rig.wm.current_screen() == 1);
  CHECK(r.map().camera().orientation != turned);
  CHECK(r.star_item(near2).y > near_y);

  // Pinches: the field of view within the zoom's limits.
  const float fov = r.map().fov();
  r.pinch(240, 240, 100, 120);
  CHECK(r.map().fov() == fov / 1.2F);
  r.pinch(240, 240, 20, 400);
  CHECK(r.map().fov() == CosWnd::kMinFov);
  r.pinch(240, 240, 400, 20);
  CHECK(r.map().fov() == CosWnd::kMaxFov);

  // The window state round-trips the camera as it is.
  std::vector<std::uint8_t> state(0xbe);
  REQUIRE(r.map().save_state(state));
  const std::array<float, 9> orientation = r.map().camera().orientation;
  r.map().load_state(state);
  CHECK(r.map().camera().orientation == orientation);
  CHECK(r.map().fov() == CosWnd::kMaxFov);
}

TEST_CASE("the System Display orbits with a horizontal drag and zooms with a pinch within its limits",
          "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  test::TempDir dir("oa_touch");
  GameRig r(files, dir.path);
  const game::Session& s = *r.session.core();
  const CosWnd::Item& home = r.star_item(s.game->cosmos.races[s.player_race].home);
  r.click(r.map().pane().x0 + home.x, r.map().pane().y0 + home.y);
  r.wait(10);
  REQUIRE(r.rig.wm.current_screen() == 16);
  auto& view = dynamic_cast<BatModeWnd&>(*r.rig.wm.find_window("BatModeWnd"));
  const Pane p = view.pane();
  const int cx = (p.x0 + p.x1) / 2;
  const int cy = (p.y0 + p.y1) / 2;
  const float distance = view.distance();
  const auto position = view.camera().position;

  // A vertical drag: no other rotation in the original.
  r.finger_drag(cx, cy - 40, 0, 80, 4);
  CHECK(view.camera().position == position);
  r.finger_drag(cx - 40, cy, 80, 0, 4);
  CHECK(view.camera().position != position);
  CHECK(view.distance() == distance);
  CHECK(r.rig.wm.current_screen() == 16);

  r.pinch(cx, cy, 100, 400);
  CHECK(view.distance() < distance);
  r.pinch(cx, cy, 20, 400);
  r.pinch(cx, cy, 20, 400);
  CHECK(view.distance() == view.camera().focal);
  r.pinch(cx, cy, 400, 20);
  r.pinch(cx, cy, 400, 20);
  CHECK(view.distance() >= distance);  // the default distance, the farthest
}

// The new-game screen (screen 3) and its species list RACELIST (21 rows of
// 109 pixels in a window 327 pixels high), driven by a finger, the mouse and its
// wheel.
struct SpeciesListRig {
  explicit SpeciesListRig(const GameFiles& files) : rig(files), touch(rig.input) {
    rig.click(318, 295);  // FRESHGAME
    REQUIRE(rig.wm.current_screen() == 3);
    REQUIRE(list().scroll_bar());
  }

  ListBox& list() { return dynamic_cast<ListBox&>(*rig.wm.find_window("RACELIST")); }
  int species() { return dynamic_cast<NewGameWnd&>(*rig.wm.find_window("NEWGAMEWND")).settings().species; }
  // A finger dragged from (x, y0) by `dy` in `steps` moves, a pass each.
  void finger_drag(int x, int y0, int dy, int steps) {
    touch.feed(finger(EventType::kFingerDown, 1, x, y0, 0));
    rig.pass();
    for (int i = 1; i <= steps; ++i) {
      touch.feed(finger(EventType::kFingerMove, 1, x, y0 + dy * i / steps, 10));
      rig.pass();
    }
    touch.feed(finger(EventType::kFingerUp, 1, x, y0 + dy, 20));
    rig.pass();
    rig.pass();
  }
  void tap(int x, int y) {
    touch.feed(finger(EventType::kFingerDown, 1, x, y, 0));
    rig.pass();
    touch.feed(finger(EventType::kFingerUp, 1, x, y, 50));
    rig.pass();
    rig.pass();
    rig.pass();
  }
  // Clicks the up or down arrow `n` times; returns the top row after.
  int arrow_clicks(bool down, int n) {
    for (int i = 0; i < n; ++i) {
      rig.click(628, down ? 368 : 50);
    }
    rig.move(560, 200);
    return list().top();
  }
  void wheel(int notches) {
    rig.pass(InputEvent{.type = EventType::kMouseWheel, .x = 560, .y = 200, .wheel = notches});
  }

  test::UiRig rig;
  TouchMapper touch;
};

TEST_CASE("a finger drag scrolls a list as its arrows do, row by row with the finger, and never picks",
          "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  SpeciesListRig r(files);
  constexpr int kRow = 109;
  REQUIRE(r.list().line_height() == kRow);
  // Up by two rows and a half: two steps, the species kept.
  r.finger_drag(560, 300, -(2 * kRow + kRow / 2), 6);
  CHECK(r.list().top() == 2);
  CHECK(r.species() == 0);
  CHECK(r.list().hover_row() == 2 + (300 - 46) / kRow);  // the hover back at the drag's end
  // Down again by one row less a pixel: none.
  r.finger_drag(560, 100, kRow - 1, 3);
  CHECK(r.list().top() == 2);
  // The same top row as two clicks on the down arrow from the top.
  r.finger_drag(560, 100, 3 * kRow, 3);
  CHECK(r.list().top() == 0);
  CHECK(r.arrow_clicks(true, 2) == 2);
  // A long drag stops where the arrows do.
  for (int i = 0; i < 10; ++i) {
    r.finger_drag(560, 370, -320, 8);
  }
  const int bottom = r.list().top();
  CHECK(bottom == NewGameWnd::kSpecies - (372 - 45) / kRow);  // the arrows' limit
  CHECK(r.arrow_clicks(true, 1) == bottom);
  CHECK(r.arrow_clicks(false, 3) == bottom - 3);
  CHECK(r.species() == 0);

  // A tap still picks the row under the finger, as a click does.
  r.tap(560, 46 + kRow / 2);
  CHECK(r.species() == bottom - 3);
}

TEST_CASE("the wheel scrolls a list by one arrow click per notch; the mouse does not drag RACELIST",
          "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  SpeciesListRig r(files);
  r.wheel(-1);
  CHECK(r.list().top() == 1);
  r.wheel(-3);
  CHECK(r.list().top() == 4);
  CHECK(r.list().hover_row() == 4 + (200 - 46) / r.list().line_height());
  r.wheel(10);
  CHECK(r.list().top() == 0);
  CHECK(r.arrow_clicks(true, 1) == 1);
  CHECK(r.species() == 0);

  // A row's press picks it in the original: a mouse drag keeps that press
  // and does not scroll.
  CHECK(!r.list().mouse_drag());
  CHECK(r.rig.wm.view_window_at(560, 300, true) == 0);
  CHECK(r.rig.wm.view_window_at(560, 300, false) == r.list().id());
  r.rig.move(560, 300);
  r.rig.pass(InputEvent{.type = EventType::kMouseDown, .x = 560, .y = 300, .button = MouseButton::kLeft});
  for (int y = 280; y >= 60; y -= 20) {
    r.rig.move(560, y);
  }
  r.rig.pass(InputEvent{.type = EventType::kMouseUp, .x = 560, .y = 60, .button = MouseButton::kLeft});
  CHECK(r.list().top() == 1);
  CHECK(r.species() == 1 + (300 - 46) / r.list().line_height());
}

TEST_CASE("the intelligence list takes mouse drags (its rows' press picks nothing), the others do not",
          "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  test::TempDir dir("oa_touch");
  GameRig r(files, dir.path);
  r.rig.wm.set_screen(14, true);
  r.wait(3);
  auto& list = dynamic_cast<ListBox&>(*r.rig.wm.find_window("INTELLIST"));
  CHECK(list.mouse_drag());
  const Pane p = list.pane();
  CHECK(list.takes_view_gesture(p.x0 + 10, p.y0 + 10, true));
  CHECK(r.rig.wm.view_window_at(p.x0 + 10, p.y0 + 10, true) == list.id());
  CHECK(!dynamic_cast<ListBox&>(*r.rig.wm.find_window("STATPLANLST")).mouse_drag());
}

TEST_CASE("the wheel zooms the star map and the System Display as a pinch, within their limits", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  test::TempDir dir("oa_touch");
  GameRig r(files, dir.path);
  const float fov = r.map().fov();
  r.rig.pass(InputEvent{.type = EventType::kMouseWheel, .x = 240, .y = 240, .wheel = 1});
  CHECK(r.map().fov() == fov / GWindow::kWheelZoom);
  r.rig.pass(InputEvent{.type = EventType::kMouseWheel, .x = 240, .y = 240, .wheel = 40});
  CHECK(r.map().fov() == CosWnd::kMinFov);
  r.rig.pass(InputEvent{.type = EventType::kMouseWheel, .x = 240, .y = 240, .wheel = -40});
  CHECK(r.map().fov() == CosWnd::kMaxFov);
  CHECK(r.rig.wm.current_screen() == 1);

  const game::Session& s = *r.session.core();
  const CosWnd::Item& home = r.star_item(s.game->cosmos.races[s.player_race].home);
  r.click(r.map().pane().x0 + home.x, r.map().pane().y0 + home.y);
  r.wait(10);
  REQUIRE(r.rig.wm.current_screen() == 16);
  auto& view = dynamic_cast<BatModeWnd&>(*r.rig.wm.find_window("BatModeWnd"));
  const float distance = view.distance();
  r.rig.pass(InputEvent{.type = EventType::kMouseWheel, .x = 240, .y = 240, .wheel = 2});
  CHECK(view.distance() < distance);
  r.rig.pass(InputEvent{.type = EventType::kMouseWheel, .x = 240, .y = 240, .wheel = 100});
  CHECK(view.distance() == view.camera().focal);
}

TEST_CASE("Back: an exit corner's action, Exit to DOS on the opening menu", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  test::TempDir dir("oa_touch");
  GameRig r(files, dir.path);
  REQUIRE(r.rig.wm.current_screen() == 1);
  r.click(r.map().pane().x0 + 1, r.map().pane().y0 + 1);  // nothing there
  r.rig.key('s', SDL_KMOD_LALT);
  r.wait(3);
  REQUIRE(r.rig.wm.current_screen() != 1);
  r.rig.pass(InputEvent{.type = EventType::kBack});
  r.wait(2);
  CHECK(r.rig.wm.current_screen() == 1);
  CHECK(r.rig.wm.running());

  r.rig.wm.set_screen(0, false);
  r.rig.pass(InputEvent{.type = EventType::kBack});
  r.wait(1);
  CHECK(!r.rig.wm.running());
}

TEST_CASE(
    "long presses: held rotation buttons, the System Display's right button, its move drag, Back and Esc "
    "cancelling a move",
    "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  test::TempDir dir("oa_touch");
  GameRig r(files, dir.path);
  WinMgr& wm = r.rig.wm;
  r.touch.set_long_press_query([&](int x, int y) { return wm.long_press(x, y); });
  const auto centre = [](const GWindow& w) {
    return std::pair{(w.pane().x0 + w.pane().x1) / 2, (w.pane().y0 + w.pane().y1) / 2};
  };
  const auto long_press_on = [&](const GWindow& w) {
    const auto [x, y] = centre(w);
    return wm.long_press(x, y);
  };
  std::uint64_t ms = 0;
  // A finger held on (x, y) past the long press, `passes` passes.
  const auto hold = [&](int x, int y, int passes) {
    r.touch.feed(finger(EventType::kFingerDown, 1, x, y, ms));
    r.rig.pass();
    ms += TouchMapper::kLongPress;
    r.touch.update(ms);
    r.wait(passes);
  };
  const auto lift = [&](int x, int y) {
    r.touch.feed(finger(EventType::kFingerUp, 1, x, y, ++ms));
    r.wait(3);
  };

  // The star map: help, but its buttons acting while held.
  REQUIRE(wm.current_screen() == 1);
  CHECK(long_press_on(r.map()) == LongPress::kHelp);
  CHECK(long_press_on(*wm.find_window("ROTRIGHT")) == LongPress::kLeftHold);
  CHECK(long_press_on(*wm.find_window("ZOOMIN")) == LongPress::kLeftHold);
  CHECK(long_press_on(*wm.find_window("CENTERHOLO")) == LongPress::kHelp);

  // A ship of the player that can move in its home system (none on day 0).
  game::Session& s = *r.session.core();
  const int home_star = s.game->cosmos.races[s.player_race].home;
  game::Ship* ship = game::cosmos_alloc_ship(s, s.player_race);
  REQUIRE(ship != nullptr);
  ship->location_kind = game::ShipLocation::kSystem;
  ship->power = 10;
  game::ship_enter_star_system(s, s.game->cosmos.stars[static_cast<std::size_t>(home_star)], *ship);
  const CosWnd::Item& home = r.star_item(home_star);
  r.click(r.map().pane().x0 + home.x, r.map().pane().y0 + home.y);
  r.wait(10);
  REQUIRE(wm.current_screen() == 16);
  auto& view = dynamic_cast<BatModeWnd&>(*wm.find_window("BatModeWnd"));
  const auto [cx, cy] = centre(view);
  CHECK(wm.long_press(cx, cy) == LongPress::kRight);

  // A rotation button turns the system while held, and only then.
  const GWindow& rotate = *wm.find_window("BATROTLEFT");
  REQUIRE(long_press_on(rotate) == LongPress::kLeftHold);
  const auto [rx, ry] = centre(rotate);
  const auto before = view.camera().position;
  hold(rx, ry, 10);
  CHECK(view.camera().position != before);
  lift(rx, ry);
  const auto stopped = view.camera().position;
  r.wait(10);
  CHECK(view.camera().position == stopped);

  // One of the player's ships picked in the panel's list: a move.
  auto& list = dynamic_cast<ListBox&>(*wm.find_window("BatListWnd"));
  const auto pick_ship = [&] {
    const int x = list.pane().x0 + 10;
    for (int y = list.pane().y0; y <= list.pane().y1; ++y) {
      if (const int row = list.row_at(x, y); row >= 0 && list.item(row).data >= 0) {
        CHECK(wm.long_press(x, y) == LongPress::kRight);
        r.click(x, y);
        r.wait(2);
        return;
      }
    }
    FAIL("no ship in the list");
  };
  pick_ship();
  REQUIRE(view.state() == BatModeWnd::State::kMovePoint);
  CHECK(wm.long_press(cx, cy) == LongPress::kMoveDrag);

  // Dragged at once: the target follows the finger, the view stays.
  const auto camera = view.camera().position;
  r.touch.feed(finger(EventType::kFingerDown, 1, cx - 60, cy + 60, ms));
  r.rig.pass();
  r.touch.feed(finger(EventType::kFingerMove, 1, cx - 60, cy + 90, ++ms));
  r.wait(1);
  const auto dragged = view.move_point();
  r.touch.feed(finger(EventType::kFingerMove, 1, cx - 20, cy + 90, ++ms));
  r.wait(1);
  CHECK(view.move_point() != dragged);
  CHECK(view.camera().position == camera);
  CHECK(view.state() == BatModeWnd::State::kMovePoint);
  // Esc instead of the lift's step: the move cancelled; the ship again.
  r.rig.key(SDLK_ESCAPE);
  lift(cx - 20, cy + 90);
  REQUIRE(view.state() == BatModeWnd::State::kIdle);
  pick_ship();
  REQUIRE(view.state() == BatModeWnd::State::kMovePoint);

  // Held and dragged: the target follows the finger, the lift fixes it.
  hold(cx - 60, cy + 60, 1);
  const auto first = view.move_point();
  r.touch.feed(finger(EventType::kFingerMove, 1, cx - 20, cy + 80, ++ms));
  r.wait(1);
  CHECK(view.state() == BatModeWnd::State::kMovePoint);
  CHECK(view.move_point() != first);
  lift(cx - 20, cy + 80);
  CHECK(view.state() == BatModeWnd::State::kMoveElevation);

  // Back between the steps: the move cancelled, the screen kept.
  r.rig.pass(InputEvent{.type = EventType::kBack});
  r.wait(2);
  CHECK(view.state() == BatModeWnd::State::kIdle);
  CHECK(view.selected() == -1);
  CHECK(wm.current_screen() == 16);

  // Esc too; without a move it leaves.
  pick_ship();
  REQUIRE(view.state() == BatModeWnd::State::kMovePoint);
  r.rig.key(SDLK_ESCAPE);
  CHECK(view.state() == BatModeWnd::State::kIdle);
  CHECK(wm.current_screen() == 16);
  r.rig.key(SDLK_ESCAPE);
  r.wait(3);
  CHECK(wm.current_screen() != 16);
}

TEST_CASE("three fingers coming together are Back, and neither turn nor zoom the view", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  test::TempDir dir("oa_touch");
  GameRig r(files, dir.path);
  REQUIRE(r.rig.wm.current_screen() == 1);
  // Over the map's centre, the fingers a little apart in time, a pass after
  // each event.
  const int cx = (r.map().pane().x0 + r.map().pane().x1) / 2;
  const int cy = (r.map().pane().y0 + r.map().pane().y1) / 2;
  const auto close = [&] {
    r.touch.feed(finger(EventType::kFingerDown, 1, cx - 120, cy, 0));
    r.rig.pass();
    r.touch.feed(finger(EventType::kFingerDown, 2, cx + 120, cy, 30));
    r.rig.pass();
    r.touch.feed(finger(EventType::kFingerMove, 2, cx + 110, cy + 5, 50));
    r.rig.pass();
    r.touch.feed(finger(EventType::kFingerDown, 3, cx, cy + 120, 60));
    r.rig.pass();
    for (int i = 1; i <= 4; ++i) {
      const int d = 120 - i * 25;
      r.touch.feed(finger(EventType::kFingerMove, 1, cx - d, cy, 60 + i * 20));
      r.touch.feed(finger(EventType::kFingerMove, 2, cx + d, cy, 60 + i * 20));
      r.touch.feed(finger(EventType::kFingerMove, 3, cx, cy + d, 60 + i * 20));
      r.rig.pass();
    }
    for (const std::uint64_t id : {1, 2, 3}) {
      r.touch.feed(finger(EventType::kFingerUp, id, cx, cy, 200));
    }
    r.wait(3);
  };
  r.rig.key('s', SDL_KMOD_LALT);
  r.wait(3);
  REQUIRE(r.rig.wm.current_screen() != 1);
  close();
  CHECK(r.rig.wm.current_screen() == 1);

  const std::array<float, 9> orientation = r.map().camera().orientation;
  const float fov = r.map().fov();
  r.rig.key('s', SDL_KMOD_LALT);
  r.wait(3);
  r.rig.pass(InputEvent{.type = EventType::kBack});
  r.wait(2);
  REQUIRE(r.rig.wm.current_screen() == 1);
  close();
  CHECK(r.map().camera().orientation == orientation);
  CHECK(r.map().fov() == fov);
  CHECK(r.rig.wm.current_screen() != 1);

  r.rig.wm.set_screen(0, false);
  close();
  CHECK(!r.rig.wm.running());
}

// Returns the save `bytes` without its clusters' content: `Cosmos_load`
// reads them into a local buffer only, so that a save after a load writes
// the clusters of the galaxy made at the program's start.
std::vector<std::uint8_t> without_clusters(const std::vector<std::uint8_t>& bytes) {
  game::SaveImage image = game::parse_save(bytes);
  for (game::Cluster& c : image.clusters) {
    c = {};
  }
  return game::serialize_save(image);
}

// Runs the program headless over the user folder `dir` with the
// `--keys` steps `keys`, until `frames` frames were shown.
void run_app(const Install& install, const std::filesystem::path& dir, std::string_view keys, int frames) {
  const std::vector<std::string_view> args{"--keys", keys};
  const CommandLine line = parse_command_line(args);
  const GameOptions options{.settings = {},
                            .settings_file = dir / "options.cfg",
                            .keys = line.keys,
                            .screenshot = dir / "shot.bmp",
                            .frames = frames,
                            .user_dir = dir,
                            .seed = 1234,
                            .trace = {}};
  REQUIRE(run_game(options, install) == 0);
}

TEST_CASE("sent to the background mid-game, the game is kept in resume.gam and resumed from it", "[app][content]") {
  const std::optional<Install> install = test::game_install();
  if (!install) {
    SKIP("OA_DATA is not set to an Ascendancy install");
  }
  test::TempDir dir("oa_resume");
  const std::filesystem::path resume = dir.path / "RESUME.GAM";
  // A new game (New Game on the remake's menu), the map turned by a drag,
  // then the background. The run ends on its last frame, without the
  // original's exit (which saves resume.gam too).
  run_app(*install, dir.path,
          "-40,click:318:282,-60,click:594:452,-60,click:320:240,-60,drag:15:460:200:400,-5,background,-20", 2000);
  REQUIRE(std::filesystem::exists(resume));
  const std::vector<std::uint8_t> saved = test::read_file(resume);
  {
    // Saved after the drag: the opening menu reads the turned map back.
    const GameFiles files(*install);
    GameCoreSession session(files, dir.path, false);
    const test::UiRig rig(files, &session, dir.path);
    REQUIRE(session.has_game());
    const auto& map = dynamic_cast<const CosWnd&>(*rig.wm.find_window("COSMOSWnd"));
    CHECK(map.camera().orientation != std::array<float, 9>{1, 0, 0, 0, 1, 0, 0, 0, 1});
  }

  // The next start reads it on the opening menu; Return to Current Game
  // (the menu's fourth button) goes on from it: kept again, it is the same.
  std::filesystem::remove(dir.path / "shot.bmp");
  run_app(*install, dir.path, "-40,background,-5", 600);
  const bool same_on_menu = without_clusters(test::read_file(resume)) == without_clusters(saved);
  CHECK(same_on_menu);
  run_app(*install, dir.path, "-40,click:318:317,-60,background,-5", 1500);
  const bool same_on_map = without_clusters(test::read_file(resume)) == without_clusters(saved);
  CHECK(same_on_map);
}

}  // namespace
}  // namespace oa
