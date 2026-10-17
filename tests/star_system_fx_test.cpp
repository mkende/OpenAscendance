// The effect animations of the System Display (`BatModeWnd` effects,
// `batfx.cpp` / `batfxa.cpp`): the filled ellipse of the Lane Blocker's
// field, and the effects' lengths in ticks, played on the shipped day-820
// game's battle at Feculon (`Source/RESUME.GAM`, the Capelons at war after
// the oracle's `resume820_battle` script; here the player's Frigate is
// given the gizmo).

#include <catch2/catch_test_macros.hpp>

#include <string_view>

#include "core_session.h"
#include "game/session.h"
#include "game/ship.h"
#include "game_data.h"
#include "gfx/vfx.h"
#include "screens/bat_control_wnd.h"
#include "screens/bat_mode_wnd.h"
#include "ui/list_box.h"
#include "ui_rig.h"

namespace oa {
namespace {

constexpr int kScreenStarSystem = 16;

TEST_CASE("vfx_ellipse_fill fills between the outline's columns", "[gfx]") {
  Image8 outline(64, 64);
  Image8 filled(64, 64);
  const Pane pane{0, 0, 63, 63};
  vfx_ellipse_draw(outline, pane, 30, 30, 12, 7, 1);
  vfx_ellipse_fill(filled, pane, 30, 30, 12, 7, 1);
  for (int y = 0; y < 64; ++y) {
    int first = -1;
    int last = -1;
    for (int x = 0; x < 64; ++x) {
      if (outline.at(x, y) != 0) {
        first = first == -1 ? x : first;
        last = x;
      }
    }
    for (int x = 0; x < 64; ++x) {
      INFO("x " << x << " y " << y);
      CHECK((filled.at(x, y) != 0) == (first != -1 && x >= first && x <= last));
    }
  }
  // Clipped to the pane; a zero radius is a line.
  Image8 clipped(64, 64);
  vfx_ellipse_fill(clipped, Pane{0, 0, 29, 63}, 30, 30, 12, 7, 1);
  CHECK(clipped.at(30, 30) == 0);
  CHECK(clipped.at(29, 30) == 1);
  Image8 line(64, 64);
  vfx_ellipse_fill(line, pane, 10, 10, 0, 5, 2);
  CHECK(line.at(10, 5) == 2);
  CHECK(line.at(10, 15) == 2);
  CHECK(line.at(11, 10) == 0);
}

// The day-820 game with the Capelons at war, on the System Display of
// Feculon.
struct BattleRig {
  explicit BattleRig(const GameFiles& files) : session(files, files.load("RESUME.GAM")), rig(files, &session) {
    rig.wm.restore_window_state();
    game::Cosmos& c = session.session().game->cosmos;
    for (game::StarIndex i = 0; i < c.star_count; ++i) {
      if (game::name_view(c.stars[static_cast<std::size_t>(i)].name) == "Feculon") {
        c.selected_star = i;
      }
    }
    REQUIRE(c.selected_star >= 0);
    // The war of the oracle's script (both sides).
    const std::uint8_t player = session.session().player_race;
    for (std::size_t r = 0; r < static_cast<std::size_t>(c.race_count); ++r) {
      if (r != player && c.races[r].relation[player] != 0) {
        c.races[r].relation[player] = game::kWar;
        c.races[player].relation[r] = game::kWar;
      }
    }
    rig.move(600, 20);
    rig.wm.set_screen(kScreenStarSystem, true);
    rig.pass();
  }

  BatModeWnd& view() { return *dynamic_cast<BatModeWnd*>(rig.wm.find_window("BatModeWnd")); }
  BatControlWnd& control() { return *dynamic_cast<BatControlWnd*>(rig.wm.find_window("BatControlWnd")); }
  game::Cosmos& cosmos() { return session.session().game->cosmos; }

  // The ships in space of the player (`mine`) or of the others.
  game::ShipIndex ship(bool mine) {
    const game::Session& s = session.session();
    for (game::ShipIndex i = 0; i < static_cast<game::ShipIndex>(game::kMaxShips); ++i) {
      const game::Ship& sh = cosmos().ships[static_cast<std::size_t>(i)];
      if (sh.owner >= 0 && sh.location_kind == game::ShipLocation::kSystem && sh.location == cosmos().selected_star &&
          (sh.owner == s.player_race) == mine) {
        return i;
      }
    }
    FAIL("no ship");
    return game::kNone;
  }

  // Gives the player's ship gizmo `g` in its first slot, selects it, picks
  // the gizmo in the list (and its target), then plays the effect: returns
  // its length in ticks.
  int play(std::int8_t g, bool targeted) {
    game::Session& s = session.session();
    const game::ShipIndex mine = ship(true);
    game::Ship& user = cosmos().ships[static_cast<std::size_t>(mine)];
    user.slots[0] = {g, 1, 0};
    user.power = 1000;
    control().select(BatControlWnd::kObjectShip, mine);
    const game::Gizmo& gizmo = s.tables->gizmos.at(static_cast<std::size_t>(g));
    const int category = (gizmo.flags & 0x80) != 0 ? 4 : gizmo.category;
    control().handle_event(category == 0   ? BatControlWnd::kMessageWeapons
                           : category == 1 ? BatControlWnd::kMessageShields
                           : category == 4 ? BatControlWnd::kMessagePassive
                                           : BatControlWnd::kMessageSpecials,
                           0, 0);
    auto& list = *dynamic_cast<ListBox*>(rig.wm.find_window("BatListWnd"));
    int row = -1;
    for (int i = 0; i < list.count(); ++i) {
      if (list.item(i).object == 0) {
        row = i;
      }
    }
    REQUIRE(row >= 0);
    control().handle_event(kEventListPick, row, kEventLeftClick);
    if (targeted) {
      REQUIRE(view().state() == BatModeWnd::State::kTargeting);
      const game::ShipIndex other = ship(false);
      control().check_target(BatControlWnd::kObjectShip, other);
      view().set_state(BatModeWnd::State::kIdle);
      control().execute(BatControlWnd::kObjectShip, other);
    }
    REQUIRE(view().state() == BatModeWnd::State::kAnimating);
    int ticks = 0;
    bool started = false;
    while (ticks < 1000) {
      view().effect_tick();
      ++ticks;
      if (view().effect() != 0xff) {
        started = true;
      } else if (started) {
        break;
      }
    }
    return ticks;
  }

  test::CoreGameSession session;
  test::UiRig rig;
};

TEST_CASE("System Display effects last as many ticks as the original's", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  SECTION("Hyperswapper: the swap at tick 20, the end at tick 41") {
    BattleRig r(files);
    CHECK(r.play(55, true) == 42);
  }
  SECTION("Remote Repair Facility: 36 ticks of glow") {
    BattleRig r(files);
    CHECK(r.play(58, false) == 37);
  }
  SECTION("every effect on a ship, or on its own, comes to an end") {
    for (const int g : {52, 59, 68, 45, 40, 74}) {
      INFO("gizmo " << g);
      BattleRig r(files);
      CHECK(r.play(static_cast<std::int8_t>(g), true) < 1000);
    }
    for (const int g : {56, 39, 46, 38, 72}) {
      INFO("gizmo " << g);
      BattleRig r(files);
      CHECK(r.play(static_cast<std::int8_t>(g), false) < 1000);
    }
  }
  SECTION("Recaller: fades in 9 steps of 4 ticks, then shrinks by 0.04 a tick") {
    BattleRig r(files);
    // Fade 9 at tick 32 (tick 0 counts), then the float scale from 1.0
    // below 0.04 in 24 steps of -0.04f (0.03999982 after the rounding).
    CHECK(r.play(36, false) == 33 + 24);
  }
}

}  // namespace
}  // namespace oa
