// The System Display (screen 16: `BatModeWnd`, `BatControlWnd`): the
// original's runs replayed (`replay_rig.h`) for the star-system view and the
// battles, and the view's state in the saves (OA_DATA install; the
// comparisons with the original need OA_ORACLE, `docs/12-oracle.md`).

#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL_keycode.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include "core_session.h"
#include "game/session.h"
#include "game_data.h"
#include "platform/touch.h"
#include "replay_rig.h"
#include "screen_shots.h"
#include "screens/bat_control_wnd.h"
#include "screens/bat_mode_wnd.h"
#include "screens/deselect_button.h"
#include "ui/help_wnd.h"
#include "ui/list_box.h"

namespace oa {

TEST_CASE("the star-system view of the original", "[star_system][screens][content][oracle]") {
  OA_REQUIRE_GAME_FILES(files);
  // main_screens: a new game's home star opened from the star map (SCR006),
  // back on the star map (SCR007), the view's camera in RESUME.GAM.
  // star_system_views: the camera buttons, a planet hovered, selected,
  // opened, the toggles, the camera kept for the star (`tests/oracle`).
  test::replay_oracle_runs(files, {{.run = "main_screens", .seed = 1234, .shots = {"SCR006.GIF", "SCR007.GIF"}},
                                   {.run = "m5/star_system_views", .seed = 1234, .shots = {}}});
}

TEST_CASE("a battle of the original played through the System Display", "[star_system][screens][content][oracle]") {
  OA_REQUIRE_GAME_FILES(files);
  // battle_ui: the day-820 battle at Feculon, the player's picks, targeting,
  // a move by the two clicks, Repeat, shields, Pass, captions, toggles,
  // leaving; its 03.SAV (`tests/oracle/battle_ui.txt`).
  // move_point, move_point2, move_planet, move_lane, repeat_pass: the moves
  // ordered by the view (to a grid point, a planet, a lane end) and by
  // Repeat Move, with the stack bytes the original leaves in their action
  // blocks; abandon_skip: a ship abandoned, Auto's effects skipped by Space,
  // Enter and a right click.
  test::replay_oracle_runs(files, {{.run = "m5/battle_ui", .keep_saves = true, .shots = {}},
                                   {.run = "m5/move_point", .keep_saves = true, .shots = {}},
                                   {.run = "m5/move_point2", .keep_saves = true, .shots = {}},
                                   {.run = "m5/move_planet", .keep_saves = true, .shots = {}},
                                   {.run = "m5/move_lane", .keep_saves = true, .shots = {}},
                                   {.run = "m5/repeat_pass", .keep_saves = true, .shots = {}},
                                   {.run = "m5/abandon_skip", .keep_saves = true, .shots = {}}});
}

TEST_CASE("the original's battle effects played frame by frame", "[star_system][screens][content][oracle]") {
  OA_REQUIRE_GAME_FILES(files);
  // `tests/oracle/m5fx`: the day-820 battle at Feculon with Frigate 1
  // given one gizmo (`fx_save.py`), used on a Capelon Medium, the lane end
  // or itself; screenshots every 4 passes through each of the effects 3..15,
  // and the save after the Capelons' turn (the action block of the use).
  std::vector<test::ReplayRun> runs;
  for (const int g : {52, 55, 32, 56, 74, 58, 46, 60, 59, 68, 45, 39, 40, 36, 72, 38}) {
    runs.push_back(
        {.run = std::format("m5fx/fx_{:02d}", g), .resume = std::format("m5fx/fx_{:02d}.GAM", g), .shots = {}});
  }
  // The stale target of an untargeted gizmo's action block: another row,
  // another view.
  for (const char* run : {"m5fx/fx_stale_a", "m5fx/fx_stale_b"}) {
    runs.push_back({.run = run, .resume = "m5fx/fx_stale.GAM", .shots = {}});
  }
  test::replay_oracle_runs(files, runs);
}

namespace {

constexpr int kScreenStarSystem = 16;
constexpr int kScreenPlanet = 17;

// Returns the index of the star named `name`.
game::StarIndex find_star(const game::Cosmos& c, std::string_view name) {
  for (game::StarIndex i = 0; i < c.star_count; ++i) {
    if (game::name_view(c.stars[static_cast<std::size_t>(i)].name) == name) {
      return i;
    }
  }
  FAIL("no star " << name);
  return game::kNone;
}

// The shipped day-820 game (`RESUME.GAM`) on the System Display of Feculon,
// where the player's two Frigates and two Capelon Mediums are; with `war`,
// the player at war with the Capelons (race 1) as in the oracle's
// `resume820_battle` case; with `deselect`, the remake's deselect button.
struct SystemRig {
  SystemRig(const GameFiles& files, bool war, bool deselect = false)
      : session(files, files.load("RESUME.GAM")), rig(files, &session) {
    if (deselect) {
      install_deselect_button(rig.wm);
    }
    game::Cosmos& c = cosmos();
    if (war) {
      c.races[s().player_race].relation[kCapelons] = game::kWar;
      c.races[kCapelons].relation[s().player_race] = game::kWar;
    }
    rig.wm.restore_window_state();
    c.selected_star = find_star(c, "Feculon");
    rig.move(400, 460);
    rig.wm.set_screen(kScreenStarSystem, true);
    rig.pass();
  }

  game::Session& s() { return session.session(); }
  game::Cosmos& cosmos() { return s().game->cosmos; }
  BatModeWnd& view() { return dynamic_cast<BatModeWnd&>(*rig.wm.find_window("BatModeWnd")); }
  BatControlWnd& panel() { return dynamic_cast<BatControlWnd&>(*rig.wm.find_window("BatControlWnd")); }
  ListBox& list() { return dynamic_cast<ListBox&>(*rig.wm.find_window("BatListWnd")); }
  // The screen position of the view's item of ship `ship`.
  std::pair<int, int> ship_at(int ship) {
    BatModeWnd& v = view();
    const int i = v.find_item(BatModeWnd::ItemKind::kShip, ship);
    REQUIRE(i >= 0);
    const BatModeWnd::Item& item = v.items()[static_cast<std::size_t>(i)];
    return {item.x + v.pane().x0, item.y + v.pane().y0};
  }
  // Runs passes until the effects end (at most `max`).
  void settle(int max = 2000) {
    for (int i = 0; i < max && view().state() == BatModeWnd::State::kAnimating; ++i) {
      rig.pass();
    }
  }

  static constexpr std::size_t kCapelons = 1;
  test::CoreGameSession session;
  test::UiRig rig;
};

// The Frigates of the player and the Capelon Mediums at Feculon.
constexpr int kFrigate1 = 14;
constexpr int kMedium = 6;

// The list rows (84 pixels from y 143) and the move clicks of the view.
constexpr int kListX = 520;
int list_row_y(int row) { return 143 + 84 * row + 40; }

}  // namespace

TEST_CASE("the star-system view's state of a new game", "[star_system]") {
  OA_REQUIRE_GAME_FILES(files);
  test::UiRig rig(files);
  const auto& view = dynamic_cast<const BatModeWnd&>(*rig.wm.find_window("BatModeWnd"));
  std::vector<std::uint8_t> state(BatModeWnd::kStateSize);
  REQUIRE(view.save_state(state));
  const auto f32_at = [&](std::size_t at) {
    float v = 0;
    std::memcpy(&v, state.data() + at, 4);
    return v;
  };
  const auto i32_at = [&](std::size_t at) {
    std::int32_t v = 0;
    std::memcpy(&v, state.data() + at, 4);
    return v;
  };
  // 100 star cameras: no position, identity rotations, not set, no target.
  for (std::size_t star = 0; star < 100; ++star) {
    CHECK(f32_at(12 * star) == 0.0F);
    for (std::size_t k = 0; k < 9; ++k) {
      CHECK(f32_at(0x4b0 + 36 * star + 4 * k) == (k % 4 == 0 ? 1.0F : 0.0F));
    }
    CHECK(i32_at(0x12c0 + 4 * star) == 0);
  }
  // The projection centre of the (7,7)-(465,472) view, then the field of
  // view and grid radius (unset), the toggles: grid, -, elevation lines,
  // foot quads, backdrops.
  CHECK(i32_at(0x19e8) == (229 | (232 << 16)));
  CHECK(i32_at(0x19ec) == 0);
  CHECK(i32_at(0x19f4) == -1);
  CHECK(i32_at(0x19f8) == 0);
  CHECK(i32_at(0x19fc) == -1);
  CHECK(i32_at(0x1a00) == -1);
  CHECK(i32_at(0x1a04) == -1);
}

TEST_CASE("the star-system view builds the scene of the selected star", "[star_system]") {
  OA_REQUIRE_GAME_FILES(files);
  SystemRig r(files, false);
  REQUIRE(r.rig.wm.current_screen() == kScreenStarSystem);
  const game::Cosmos& c = r.cosmos();
  const game::Star& star = c.stars[static_cast<std::size_t>(c.selected_star)];
  int stars = 0;
  int planets = 0;
  int lanes = 0;
  int ships = 0;
  int backdrops = 0;
  for (const BatModeWnd::Item& item : r.view().items()) {
    switch (item.kind) {
      case BatModeWnd::ItemKind::kStar:
        ++stars;
        CHECK(item.frame == star.type);
        break;
      case BatModeWnd::ItemKind::kPlanet:
        ++planets;
        break;
      case BatModeWnd::ItemKind::kLaneEnd:
        ++lanes;
        break;
      case BatModeWnd::ItemKind::kShip:
        ++ships;
        CHECK(c.ships[static_cast<std::size_t>(item.object)].location_kind == game::ShipLocation::kSystem);
        break;
      case BatModeWnd::ItemKind::kBackdrop:
        ++backdrops;
        break;
      default:
        break;
    }
  }
  CHECK(stars == 1);
  CHECK(planets == star.planet_count);
  CHECK(lanes == star.lane_count);
  CHECK(ships == 4);
  CHECK(backdrops == c.backdrop_count);
  // The first visit's camera: 35 degrees, the whole scene in view, the grid
  // as wide as the view at the target's distance.
  const BatModeWnd& v = r.view();
  CHECK(v.camera().fov == 35.0F);
  CHECK(v.target() == Vec3{0, 0, 0});
  CHECK(v.distance() > 0.0F);
  CHECK(v.grid_radius() == game::f32(static_cast<double>(v.distance()) *
                                     game::f32(std::tan(35.0 * static_cast<double>(0.017453292F) * 0.5)) * 2.0));
  // The panel lists the player's planet first (bit 31 of its data), then the
  // Frigates.
  ListBox& list = r.list();
  REQUIRE(list.count() == 3);
  CHECK(list.item(0).data < 0);
  CHECK(list.item(1).object == kFrigate1);
  // Leaving keeps the star's camera in the saved state.
  r.rig.key(SDLK_ESCAPE);
  std::vector<std::uint8_t> state(BatModeWnd::kStateSize);
  REQUIRE(r.view().save_state(state));
  std::int32_t visited = 0;
  std::memcpy(&visited, state.data() + 0x12c0 + 4 * static_cast<std::size_t>(c.selected_star), 4);
  CHECK(visited == -1);
}

TEST_CASE("a move ordered with the two clicks of the view", "[star_system]") {
  OA_REQUIRE_GAME_FILES(files);
  SystemRig r(files, true);
  game::Ship& frigate = r.cosmos().ships[kFrigate1];
  const Vec3 start = frigate.position;
  const std::int32_t power = frigate.power;
  // The Frigate picked in the list: its gizmos, the move cursor.
  r.rig.click(kListX, list_row_y(1));
  CHECK(r.panel().list_mode() == BatControlWnd::kListGizmos);
  CHECK(r.panel().selection().object == kFrigate1);
  CHECK(r.view().state() == BatModeWnd::State::kMovePoint);
  // A grid point, then its height.
  r.rig.move(150, 300);
  const Vec3 point = r.view().move_point();
  r.rig.click(150, 300);
  CHECK(r.view().state() == BatModeWnd::State::kMoveElevation);
  r.rig.move(150, 200);
  CHECK(r.view().move_point()[0] == point[0]);
  CHECK(r.view().move_point()[1] < point[1]);
  r.rig.click(150, 200);
  // Animated, then applied: the ship moved toward the point, at the cost of
  // power; the order is kept for Repeat Move.
  CHECK(r.view().state() == BatModeWnd::State::kAnimating);
  CHECK(frigate.destination_kind == game::ShipOrder::kGoSpace);
  r.settle();
  CHECK(r.view().state() == BatModeWnd::State::kIdle);
  CHECK(frigate.position != start);
  CHECK(frigate.power < power);
  CHECK(r.panel().battle().player_turn());
}

TEST_CASE("the panel's planet picture opens the planet as a second click on it in the view does", "[star_system]") {
  OA_REQUIRE_GAME_FILES(files);
  // A point of the panel's top: the selected object's picture (474, 7)-(632,
  // 92), else the star's title.
  constexpr int kPictureX = 553;
  constexpr int kPictureY = 30;
  // The planet screen opened from the System Display, its planet selected,
  // and the galaxy's selected planet before.
  struct Opened {
    int screen;
    std::vector<int> history;
    game::PlanetIndex planet;
    bool operator==(const Opened&) const = default;
  };
  // Picks the player's planet in the list (row 0), then opens it with
  // `open`.
  const auto open_with = [&](const auto& open) {
    SystemRig r(files, false);
    r.cosmos().selected_planet = game::kNone;
    r.rig.click(kListX, list_row_y(0));
    REQUIRE(r.panel().list_mode() == BatControlWnd::kListOrbital);
    REQUIRE(r.panel().selection().kind == BatControlWnd::kObjectPlanet);
    const auto planet = static_cast<game::PlanetIndex>(r.panel().selection().object);
    open(r, planet);
    r.rig.pass();
    CHECK(r.cosmos().selected_planet == planet);
    return Opened{r.rig.wm.current_screen(), r.rig.wm.history(), r.cosmos().selected_planet};
  };
  // The original's way: the selected planet clicked in the view.
  const Opened by_view = open_with([](SystemRig& r, game::PlanetIndex planet) {
    BatModeWnd& v = r.view();
    const int i = v.find_item(BatModeWnd::ItemKind::kPlanet, planet);
    REQUIRE(i == v.selected());
    const BatModeWnd::Item& item = v.items()[static_cast<std::size_t>(i)];
    r.rig.move(item.x + v.pane().x0, item.y + v.pane().y0);
    REQUIRE(v.hovered() == i);
    r.rig.click(item.x + v.pane().x0, item.y + v.pane().y0);
  });
  CHECK(by_view.screen == kScreenPlanet);
  CHECK(by_view.history.back() == kScreenStarSystem);
  // The picture: the same, as do 5 pixels around it.
  CHECK(open_with([](SystemRig& r, game::PlanetIndex) { r.rig.click(kPictureX, kPictureY); }) == by_view);
  CHECK(open_with([](SystemRig& r, game::PlanetIndex) { r.rig.click(kPictureX, 2); }) == by_view);

  // No planet selected: the panel's top does nothing (the original's).
  SystemRig r(files, false);
  r.rig.click(kPictureX, kPictureY);
  r.rig.pass();
  CHECK(r.rig.wm.current_screen() == kScreenStarSystem);
  // A ship selected (its picture there): nothing either.
  r.rig.click(kListX, list_row_y(1));
  REQUIRE(r.panel().selection().kind == BatControlWnd::kObjectShip);
  r.rig.click(kPictureX, kPictureY);
  r.rig.pass();
  CHECK(r.rig.wm.current_screen() == kScreenStarSystem);
  // The planet selected but a click in the view would target it: nothing.
  SystemRig targeting(files, false);
  targeting.rig.click(kListX, list_row_y(0));
  REQUIRE(targeting.panel().selection().kind == BatControlWnd::kObjectPlanet);
  targeting.view().set_state(BatModeWnd::State::kTargeting);
  targeting.rig.click(kPictureX, kPictureY);
  targeting.rig.pass();
  CHECK(targeting.rig.wm.current_screen() == kScreenStarSystem);
}

TEST_CASE("the deselect button drops the selection; it and ABANDON are greyed when they would do nothing",
          "[star_system]") {
  OA_REQUIRE_GAME_FILES(files);
  SystemRig r(files, false, true);
  WinMgr& wm = r.rig.wm;
  GWindow& button = *wm.find_window(DeselectButton::kName);
  GWindow& abandon = *wm.find_window("ABANDON");
  const Pane p = button.pane();
  CHECK(p == Pane{557, 442, 587, 472});
  REQUIRE(button.shown());
  const Image8& screen = r.rig.gs.screen();
  // Its stone recess: the top row's, a side or corner from each slot, the
  // sides mirrored along the slot.
  const Pane rotation = wm.find_window("BATROTLEFT")->pane();
  const Pane grid = wm.find_window("TOGGRID")->pane();
  const int w = p.x1 - p.x0;
  for (int d = 1; d <= 4; ++d) {
    for (int x = 0; x <= w; ++x) {
      CHECK(screen.at(p.x0 + x, p.y0 - d) == screen.at(rotation.x0 + w - x, rotation.y0 - d));
    }
    CHECK(screen.at(p.x1 + d, p.y1 + d) == screen.at(grid.x0 + w + d, grid.y0 + w + d));
  }
  test::write_shot(r.rig, "system_deselect");
  // The pixels of a pane.
  const auto pixels = [&](const Pane& pane) {
    std::vector<std::uint8_t> out;
    for (int y = pane.y0; y <= pane.y1; ++y) {
      for (int x = pane.x0; x <= pane.x1; ++x) {
        out.push_back(screen.at(x, y));
      }
    }
    return out;
  };
  constexpr std::uint8_t kWhite = 15;
  // The selection marker's corner (white when not greyed).
  const auto marker = [&] { return screen.at(p.x0 + 7, p.y0 + 1); };

  // Nothing selected: both greyed, the background kept; a click does
  // nothing.
  CHECK(button.greyed());
  CHECK(abandon.greyed());
  CHECK(marker() != kWhite);
  CHECK(screen.at(p.x0, p.y0) == GWindow::kBackColor);
  const std::vector<std::uint8_t> greyed_abandon = pixels(abandon.pane());
  r.rig.click(p.x0 + 15, p.y0 + 15);
  CHECK(screen.at(p.x0, p.y0) == GWindow::kBackColor);  // never hot

  // A Frigate picked (a move): both in colour, the button hot under the
  // pointer as ABANDON is.
  r.rig.click(kListX, list_row_y(1));
  REQUIRE(r.view().state() == BatModeWnd::State::kMovePoint);
  CHECK_FALSE(button.greyed());
  CHECK_FALSE(abandon.greyed());
  CHECK(pixels(abandon.pane()) != greyed_abandon);
  r.rig.move(p.x0 + 15, p.y0 + 15);
  CHECK(marker() == kWhite);
  CHECK(screen.at(p.x0, p.y0) == GWindow::kHotBackColor);
  // Its click: the move and the selection dropped, both greyed again.
  r.rig.click(p.x0 + 15, p.y0 + 15);
  CHECK(r.view().state() == BatModeWnd::State::kIdle);
  CHECK(r.view().selected() == -1);
  CHECK(r.panel().selection().kind == BatControlWnd::kObjectNone);
  CHECK(button.greyed());
  CHECK(abandon.greyed());
  CHECK(pixels(abandon.pane()) == greyed_abandon);

  // A Capelon ship selected: nothing to abandon, something to deselect.
  BatModeWnd& v = r.view();
  const int capelon = v.find_item(BatModeWnd::ItemKind::kShip, kMedium);
  REQUIRE(capelon >= 0);
  v.set_selected(capelon);
  v.refresh();
  CHECK_FALSE(button.greyed());
  CHECK(abandon.greyed());
  r.rig.click(p.x0 + 15, p.y0 + 15);
  CHECK(v.selected() == -1);

  // Shift and a click: its help, greyed too.
  REQUIRE(button.greyed());
  r.rig.queue_help_click(p.x0 + 15, p.y0 + 15);
  r.rig.queue_click(320, 350);  // the help window's OK
  r.rig.drain();
  CHECK(dynamic_cast<HelpWnd&>(*wm.find_window("HELPWINDOW")).topic() == "oa_deselect");
  CHECK_FALSE(wm.in_modal());
}

TEST_CASE("leaving the System Display during an effect applies the action", "[star_system]") {
  OA_REQUIRE_GAME_FILES(files);
  SystemRig r(files, true);
  game::Ship& frigate = r.cosmos().ships[kFrigate1];
  const Vec3 start = frigate.position;
  r.rig.click(kListX, list_row_y(1));
  r.rig.move(150, 300);
  r.rig.click(150, 300);
  r.rig.move(150, 200);
  r.rig.click(150, 200);
  REQUIRE(r.view().state() == BatModeWnd::State::kAnimating);
  r.rig.key(SDLK_ESCAPE);
  CHECK(r.rig.wm.current_screen() != kScreenStarSystem);
  CHECK(frigate.position != start);
  CHECK(r.s().screen != kScreenStarSystem);
}

TEST_CASE("a finger put down during an effect skips it as Space does, and clicks nothing", "[star_system]") {
  OA_REQUIRE_GAME_FILES(files);
  // The same move in two games: skipped by Space in one, by a tap on a row
  // of the list (a click there picks the row) in the other.
  SystemRig by_key(files, true);
  SystemRig by_tap(files, true);
  TouchMapper touch(by_tap.rig.input);
  touch.set_skip_query([&] { return by_tap.rig.wm.skippable_animation(); });
  for (SystemRig* r : {&by_key, &by_tap}) {
    r->rig.click(kListX, list_row_y(1));
    r->rig.move(150, 300);
    r->rig.click(150, 300);
    r->rig.move(150, 200);
    r->rig.click(150, 200);
    REQUIRE(r->view().state() == BatModeWnd::State::kAnimating);
    r->rig.pass();
    REQUIRE(r->rig.wm.skippable_animation());
  }
  by_key.rig.key(SDLK_SPACE);
  touch.feed(InputEvent{.type = EventType::kFingerDown, .x = kListX, .y = list_row_y(0), .finger = 1});
  by_tap.rig.pass();
  touch.feed(InputEvent{.type = EventType::kFingerUp, .x = kListX, .y = list_row_y(0), .finger = 1, .time_ms = 50});
  for (int i = 0; i < 3; ++i) {
    by_key.rig.pass();
    by_tap.rig.pass();
    CHECK(!by_tap.rig.input.state().left_changed);
    CHECK(!by_tap.rig.input.state().right_changed);
  }
  CHECK(by_key.view().state() == BatModeWnd::State::kIdle);
  CHECK(by_tap.view().state() == BatModeWnd::State::kIdle);
  CHECK(!by_tap.rig.wm.skippable_animation());
  CHECK(by_tap.cosmos().ships[kFrigate1].position == by_key.cosmos().ships[kFrigate1].position);
  CHECK(by_tap.panel().list_mode() == by_key.panel().list_mode());
  CHECK(by_tap.panel().selection().kind == by_key.panel().selection().kind);
  CHECK(by_tap.panel().selection().object == by_key.panel().selection().object);
  CHECK(by_tap.rig.input.state().x != kListX);  // the pointer stayed

  // Without an effect, a tap clicks again.
  touch.feed(InputEvent{.type = EventType::kFingerDown, .x = kListX, .y = list_row_y(1), .finger = 2});
  by_tap.rig.pass();
  touch.feed(InputEvent{.type = EventType::kFingerUp, .x = kListX, .y = list_row_y(1), .finger = 2, .time_ms = 50});
  by_tap.rig.pass();
  CHECK(by_tap.rig.input.state().left_changed);
}

TEST_CASE("a weapon out of range cannot be fired", "[star_system]") {
  OA_REQUIRE_GAME_FILES(files);
  SystemRig r(files, true);
  r.rig.click(kListX, list_row_y(1));
  // The first weapon (the list shows the weapons first): targeting.
  r.rig.click(kListX, list_row_y(0));
  CHECK(r.view().state() == BatModeWnd::State::kTargeting);
  const auto [x, y] = r.ship_at(kMedium);
  r.rig.move(x, y);
  CHECK(r.view().hovered() >= 0);
  CHECK_FALSE(r.panel().check_target(BatControlWnd::kObjectShip, kMedium));
  r.rig.click(x, y);
  // Nothing fired: still targeting, the Medium untouched.
  CHECK(r.view().state() == BatModeWnd::State::kTargeting);
  CHECK(r.cosmos().ships[kMedium].hit_points == r.cosmos().ships[kMedium].max_hit_points);
  // A right click on nothing drops the selection.
  r.rig.click(400, 460, MouseButton::kRight);
  CHECK(r.view().state() == BatModeWnd::State::kIdle);
  CHECK(r.panel().list_mode() == BatControlWnd::kListMine);
}

TEST_CASE("Pass gives the turn to the other races", "[star_system]") {
  OA_REQUIRE_GAME_FILES(files);
  SystemRig r(files, true);
  const std::uint32_t rounds = r.panel().battle().rounds();
  r.rig.click(500, 70);  // Pass
  r.settle();
  CHECK(r.panel().battle().rounds() == rounds + 1);
  CHECK(r.panel().battle().player_turn());
}

// Development aid (hidden): replays OA_DEV_TRACE (seed OA_DEV_SEED, the
// install's saves with OA_DEV_KEEP, OA_DEV_RESUME as `RESUME.GAM`) for
// OA_DEV_PASSES passes and prints the star-system view's items and the
// panel's list.
TEST_CASE("star-system dev dump", "[.dev]") {
  OA_REQUIRE_GAME_FILES(files);
  const char* trace = std::getenv("OA_DEV_TRACE");
  if (trace == nullptr) {
    SKIP("no OA_DEV_TRACE");
  }
  test::ReplayDir dir;
  if (const char* resume = std::getenv("OA_DEV_RESUME"); resume != nullptr) {
    std::filesystem::copy_file(resume, dir.path / "RESUME.GAM");
  }
  const char* seed = std::getenv("OA_DEV_SEED");
  test::ReplayProgram program(files, seed != nullptr ? std::atoi(seed) : 1, dir.path,
                              std::getenv("OA_DEV_KEEP") != nullptr);
  REQUIRE(program.wm.start_replay(test::read_file(trace), "trace.bin", false));
  const int passes = std::atoi(std::getenv("OA_DEV_PASSES"));
  for (int i = 0; i < passes && program.wm.running(); ++i) {
    program.wm.run_pass();
  }
  auto* view = dynamic_cast<BatModeWnd*>(program.wm.find_window("BatModeWnd"));
  REQUIRE(view != nullptr);
  const Pane& p = view->pane();
  std::printf("screen %d state %d hovered %d selected %d\n", program.wm.current_screen(),
              static_cast<int>(view->state()), view->hovered(), view->selected());
  int i = 0;
  for (const BatModeWnd::Item& item : view->items()) {
    std::printf("item %d kind %d object %d at (%d, %d) depth %f unprojected %d box %d,%d..%d,%d scale %x\n", i++,
                static_cast<int>(item.kind), item.object, item.x + p.x0, item.y + p.y0, static_cast<double>(item.depth),
                item.unprojected, item.bx0, item.by0, item.bx1, item.by1, item.draw_scale);
  }
  if (auto* list = dynamic_cast<ListBox*>(program.wm.find_window("BatListWnd"))) {
    for (int r = 0; r < list->count(); ++r) {
      std::printf("row %d object %d data %d\n", r, list->item(r).object, list->item(r).data);
    }
  }
  if (const char* dump = std::getenv("OA_DUMP_DIR"); dump != nullptr) {
    std::filesystem::create_directories(dump);
    std::filesystem::copy(dir.path, dump,
                          std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
  }
}

}  // namespace oa
