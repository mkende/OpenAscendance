// The research screen (screen 20, `ResWnd`): the tree layout and projection
// numbers, the camera helpers, and the screen on the shipped day-820 game
// (`Source/RESUME.GAM`, the oracle's `tests/oracle/research_screen.txt`).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <SDL3/SDL_keycode.h>

#include "core_session.h"
#include "game/research.h"
#include "game/session.h"
#include "game_data.h"
#include "screen_shots.h"
#include "screens/res_wnd.h"
#include "ui_rig.h"

namespace oa {
namespace {

using Catch::Matchers::WithinAbs;

constexpr int kScreenResearch = 20;

// The day-820 game of the install on the research screen, the pointer at
// (`x`, `y`).
struct ResearchRig {
  explicit ResearchRig(const GameFiles& files, int x = 550, int y = 230)
      : session(files, files.load("RESUME.GAM")), rig(files, &session) {
    // What loading the game does: the race slots' shapes and the saved
    // window state (the research camera).
    rig.wm.restore_window_state();
    rig.move(x, y);
    rig.wm.set_screen(kScreenResearch, true);
    for (int i = 0; i < 3; ++i) {
      rig.pass();
    }
  }
  ResWnd& window() { return *dynamic_cast<ResWnd*>(rig.wm.find_window("RESWIN")); }
  game::Research& research() { return session.session().game->research; }

  test::CoreGameSession session;
  test::UiRig rig;
};

TEST_CASE("the research tree layout puts each level on a ring of the axis", "[research]") {
  // Level 0: 0 and 1 (2 items, radius 23); level 1: 2 (alone, radius 4);
  // level 2: 3 (needs 2); level 3: 4, 5, 6 (odd level, 3 items: the first
  // on the axis, the others 360 / 2 apart).
  const std::vector<std::array<std::uint8_t, 5>> prereqs{{0xff},    {0xff},    {0, 1, 0xff}, {2, 0xff},
                                                         {3, 0xff}, {3, 0xff}, {3, 0xff}};
  const std::vector<Vec3> pos = ResWnd::layout_tree(prereqs);
  REQUIRE(pos.size() == 7);
  // Level 0 from 240 degrees, then +180: (z sin a, 0 / 15, z cos a).
  CHECK_THAT(pos[0][0], WithinAbs(23 * -0.8660254, 1e-4));
  CHECK(pos[0][1] == 0.0F);
  CHECK_THAT(pos[0][2], WithinAbs(23 * -0.5, 1e-4));
  CHECK_THAT(pos[1][0], WithinAbs(23 * 0.8660254, 1e-4));
  CHECK(pos[1][1] == 15.0F);
  // Level 1 alone: radius 4 at 270 degrees, 30 lower.
  CHECK_THAT(pos[2][0], WithinAbs(-4.0, 1e-4));
  CHECK(pos[2][1] == 30.0F);
  CHECK_THAT(pos[2][2], WithinAbs(0.0, 1e-4));
  CHECK(pos[3][1] == 60.0F);
  // Level 3 from 330 degrees: the first on the axis (radius 4), then 23 at
  // 150 and 330 degrees, odd items 15 lower.
  CHECK_THAT(pos[4][0], WithinAbs(4 * -0.5, 1e-4));
  CHECK(pos[4][1] == 90.0F);
  CHECK_THAT(pos[5][0], WithinAbs(23 * 0.5, 1e-4));
  CHECK_THAT(pos[5][2], WithinAbs(23 * -0.8660254, 1e-4));
  CHECK(pos[5][1] == 105.0F);
  CHECK_THAT(pos[6][0], WithinAbs(23 * -0.5, 1e-4));
  CHECK(pos[6][1] == 90.0F);
}

TEST_CASE("the research camera projects with a 30 degree field of view", "[research]") {
  Camera3d camera;
  camera.set_projection(30.0F, 1.0F, 100000.0F, 320);
  CHECK_THAT(camera.focal, WithinAbs(1194.2562, 1e-3));
  camera.position = {0.0F, 0.0F, -150.0F};
  camera.update_view();
  const Vec3 v = camera.transform({10.0F, 20.0F, 0.0F});
  CHECK(v == Vec3{10.0F, 20.0F, 150.0F});
  const ScreenPoint p = project_point(v, camera.focal, ResWnd::kCenterX, ResWnd::kCenterY);
  // 10 * 1194.2562 / 150 = 79.6 -> chop(80.1) = 80.
  CHECK(p.x == 235 + 80);
  CHECK(p.y == 240 + 159);
  // A quarter turn of the forward vector turns the tree about its axis.
  camera.rotate_forward_y(90.0F);
  camera.update_view();
  const Vec3 r = camera.transform({0.0F, 0.0F, 10.0F});
  CHECK_THAT(r[0], WithinAbs(10.0, 1e-4));
  CHECK_THAT(r[2], WithinAbs(150.0, 1e-4));
}

TEST_CASE("the research screen shows the day-820 tree from the saved camera", "[research][content]") {
  OA_REQUIRE_GAME_FILES(files);
  ResearchRig r(files);
  test::write_shot(r.rig, "research_820");
  ResWnd& w = r.window();
  // The camera is the game's: 150 high, clamped to the visible items.
  CHECK(w.camera().position[1] == 150.0F);
  // The current project, Light Bending, has the orange ring.
  const auto project = static_cast<std::size_t>(r.research().project.at(0));
  CHECK(std::string(game::name_view(r.research().items.at(project).name)) == "Light Bending");
  const ScreenPoint p = w.screen_position(project);
  CHECK(p.x + 7 == 242);
  CHECK(p.y + 46 == 146);
}

TEST_CASE("research: hover, click to choose a project, M, PgDn, rotation and leaving", "[research][content]") {
  OA_REQUIRE_GAME_FILES(files);
  ResearchRig r(files);
  ResWnd& w = r.window();
  game::Research& research = r.research();
  // The empty ring right of centre: a visible, unknown item.
  const ScreenPoint p = w.screen_position(w.draw_order().back());
  std::uint16_t target = ResWnd::kNone;
  for (std::size_t i = 0; i < research.count; ++i) {
    const ScreenPoint q = w.screen_position(i);
    if ((research.items[i].known_mask & 1) == 0 && q.x + 7 > 200 && q.x + 7 < 300 && q.y + 46 > 250 && q.y + 46 < 320) {
      target = static_cast<std::uint16_t>(i);
    }
  }
  REQUIRE(target != ResWnd::kNone);
  (void)p;
  const ScreenPoint t = w.screen_position(target);
  r.rig.move(t.x + 7 + 25, t.y + 46);
  CHECK(w.hovered() == ResWnd::kNone);  // 25 px away is outside
  r.rig.move(t.x + 7 + 24, t.y + 46 - 24);
  CHECK(w.hovered() == target);
  const std::uint16_t progress = research.progress[0];
  r.rig.click(t.x + 7 + 24, t.y + 46 - 24);
  CHECK(research.project[0] == static_cast<std::int16_t>(target));
  CHECK(research.progress[0] == progress);  // the progress carries over
  test::write_shot(r.rig, "research_820_choice");

  // M toggles the self-managed research.
  r.rig.key(SDLK_M);
  CHECK(research.self_managed == -1);
  r.rig.pass();
  test::write_shot(r.rig, "research_820_managed");
  r.rig.key(SDLK_M);
  CHECK(research.self_managed == 0);

  // PgDn goes to the bottom of the visible tree, Home to the top.
  r.rig.key(SDLK_PAGEDOWN);
  float bottom = 0;
  for (std::size_t i = 0; i < research.count; ++i) {
    if (game::research_can_research(research, 0, static_cast<std::uint16_t>(i))) {
      bottom = std::max(bottom, w.positions()[i][1]);
    }
  }
  CHECK(w.camera().position[1] == bottom);
  test::write_shot(r.rig, "research_820_bottom");
  r.rig.key(SDLK_HOME);
  CHECK(w.camera().position[1] == 0.0F);

  // A right click on RESROTLEFT rotates at each pass until clicked again.
  r.rig.click(531, 457, MouseButton::kRight);
  CHECK(w.motion() == ResWnd::kRotating);
  r.rig.pass();
  r.rig.click(531, 457, MouseButton::kRight);
  CHECK(w.motion() == 0);
  CHECK(w.camera().orientation[6] != 0.0F);
  // A left click only rotates while held.
  r.rig.pass(InputEvent{.type = EventType::kMouseDown, .x = 547, .y = 457, .button = MouseButton::kLeft});
  CHECK(w.motion() == (ResWnd::kRotating | ResWnd::kRotatingRight));
  r.rig.pass(InputEvent{.type = EventType::kMouseUp, .x = 547, .y = 457, .button = MouseButton::kLeft});
  CHECK((w.motion() & ResWnd::kRotating) == 0);

  // Leaving clears the player's "newly discovered" marks.
  research.items[0].unseen_mask = 1;
  r.rig.wm.set_screen(0, true);
  CHECK(research.items[0].unseen_mask == 0);
}

TEST_CASE("research: the oracle's views of the day-820 game", "[research][content][.shots]") {
  OA_REQUIRE_GAME_FILES(files);
  // tests/oracle/research_screen.txt: SCR001 (pointer at 550, 230), SCR002
  // (240, 260), SCR003 PgDn, SCR004 two right clicks on RESROTLEFT 13 passes
  // apart after Home, SCR005 M.
  ResearchRig r(files);
  test::write_shot(r.rig, "oracle_research_001");
  r.rig.move(240, 260);
  r.rig.pass();
  test::write_shot(r.rig, "oracle_research_002");
  r.rig.key(SDLK_PAGEDOWN);
  r.rig.pass();
  test::write_shot(r.rig, "oracle_research_003");
  r.rig.key(SDLK_HOME);
  for (int i = 0; i < 10; ++i) {
    r.rig.pass();
  }
  // rclick = move, press, release; then wait 10, rclick again.
  r.rig.move(531, 457);
  r.rig.pass(InputEvent{.type = EventType::kMouseDown, .x = 531, .y = 457, .button = MouseButton::kRight});
  r.rig.pass(InputEvent{.type = EventType::kMouseUp, .x = 531, .y = 457, .button = MouseButton::kRight});
  for (int i = 0; i < 10; ++i) {
    r.rig.pass();
  }
  r.rig.move(531, 457);
  r.rig.pass(InputEvent{.type = EventType::kMouseDown, .x = 531, .y = 457, .button = MouseButton::kRight});
  r.rig.pass(InputEvent{.type = EventType::kMouseUp, .x = 531, .y = 457, .button = MouseButton::kRight});
  for (int i = 0; i < 5; ++i) {
    r.rig.pass();
  }
  test::write_shot(r.rig, "oracle_research_004");
  r.rig.key(SDLK_M);
  for (int i = 0; i < 5; ++i) {
    r.rig.pass();
  }
  test::write_shot(r.rig, "oracle_research_005");
}

}  // namespace
}  // namespace oa
