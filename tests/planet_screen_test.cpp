#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL_keycode.h>

#include <format>
#include <string_view>

#include "core_session.h"
#include "game/planet.h"
#include "game/player_actions.h"
#include "game/ship.h"
#include "game_data.h"
#include "screen_shots.h"
#include "screens/plan_wnd.h"
#include "ui/help_wnd.h"
#include "ui_rig.h"

namespace oa::test {
namespace {

constexpr int kScreenPlanet = 17;

// Returns the index of the planet named `name` in the session's cosmos.
game::PlanetIndex find_planet(const game::Session& s, std::string_view name) {
  const game::Cosmos& c = s.game->cosmos;
  for (game::PlanetIndex i = 0; i < c.planet_count; ++i) {
    if (game::name_view(c.planets[static_cast<std::size_t>(i)].name) == name) {
      return i;
    }
  }
  FAIL("no planet " << name);
  return game::kNone;
}

// The shipped day-820 game (`RESUME.GAM`) on the planet screen of `name`,
// the pointer at (x, y).
struct PlanetRig {
  PlanetRig(const GameFiles& files, std::string_view name, int x = 300, int y = 100)
      : session(files, files.load("RESUME.GAM")), rig(files, &session) {
    planet = find_planet(session.session(), name);
    session.session().game->cosmos.selected_planet = planet;
    // What loading the game does: the race slots' shapes and the saved
    // window state.
    rig.wm.restore_window_state();
    rig.move(x, y);
    rig.wm.set_screen(kScreenPlanet, true);
    rig.pass();
  }

  PlanWnd& plan() { return dynamic_cast<PlanWnd&>(*rig.wm.find_window("PLANETALLOC")); }
  PlSquareWnd& dialog() { return dynamic_cast<PlSquareWnd&>(*rig.wm.find_window("PLSQUARE")); }
  game::Planet& p() { return session.session().game->cosmos.planets.at(static_cast<std::size_t>(planet)); }

  CoreGameSession session;
  UiRig rig;
  game::PlanetIndex planet = game::kNone;
};

}  // namespace

TEST_CASE("Planet screen geometry", "[planet]") {
  using namespace planet_geometry;
  // The top vertex of the first square of a row and the diamond around it.
  CHECK(diamond_top(0, 0).x == 410);
  CHECK(diamond_top(0, 0).y == 110);
  CHECK(diamond_top(2, 5).x == 410 - 68 + 170);
  CHECK(diamond_top(2, 5).y == 110 + 34 + 85);
  const auto v = diamond(diamond_top(1, 1));
  CHECK(v[1].x - v[3].x == 68);
  CHECK(v[2].y - v[0].y == 34);
  // The hit test inverts the drawing at the centre of every diamond.
  for (int r = 0; r < 15; ++r) {
    for (int c = 0; c < 15; ++c) {
      const PolyPoint t = diamond_top(r, c);
      const RowColumn rc = screen_to_rowcol(t.x, t.y + 17);
      CHECK(rc.row == r);
      CHECK(rc.column == c);
    }
  }
  // Two columns of five 70x70 orbital cells.
  CHECK(orbital_cell(0).x == 13);
  CHECK(orbital_cell(0).y == 119);
  CHECK(orbital_cell(7).x == 83);
  CHECK(orbital_cell(7).y == 259);
  CHECK(in_orbit_area(14, 120));
  CHECK_FALSE(in_orbit_area(153, 120));
}

TEST_CASE("Planet screen squares and hover", "[planet]") {
  OA_REQUIRE_GAME_FILES(files);
  PlanetRig r(files, "Zoroaster III");
  PlanWnd& plan = r.plan();
  const game::Planet& p = r.p();
  // The orbital columns.
  CHECK(plan.square_at_point(48, 154) == p.surface_squares);
  CHECK(plan.square_at_point(120, 154) == p.surface_squares + 5);
  CHECK(plan.square_at_point(48, 440) == p.surface_squares + 4);
  // Off the planet.
  CHECK(plan.square_at_point(630, 470) == PlanWnd::kNoSquare);
  // Hovering a square of the player's planet.
  r.rig.move(410, 280);
  const std::uint16_t sq = plan.square_at_point(410, 280);
  REQUIRE(sq != PlanWnd::kNoSquare);
  CHECK(plan.hovered() == sq);
  write_shot(r.rig, "planet_hover");
}

TEST_CASE("Planet screen dialogs and keys", "[planet]") {
  OA_REQUIRE_GAME_FILES(files);
  PlanetRig r(files, "Zoroaster III");
  write_shot(r.rig, "planet");
  PlanWnd& plan = r.plan();
  CHECK(plan.flags() == (PlanWnd::kColoredSquares | PlanWnd::kPlanetPicture));
  r.rig.key(SDLK_S);
  CHECK(plan.flags() == PlanWnd::kPlanetPicture);
  write_shot(r.rig, "planet_plain");
  r.rig.key(SDLK_S);

  // The panels open the dialog in their mode, Esc closes it.
  r.rig.click(60, 50);  // PLRES
  CHECK(r.dialog().shown());
  CHECK(r.dialog().mode() == PlSquareWnd::kResearch);
  CHECK(r.dialog().commands() == std::vector<int>{PlSquareWnd::kCancel});
  write_shot(r.rig, "planet_research");
  r.rig.key(SDLK_ESCAPE);
  CHECK_FALSE(r.dialog().shown());
  for (const auto& [x, mode, name] : {std::tuple{170, PlSquareWnd::kIndustry, "planet_industry"},
                                      std::tuple{290, PlSquareWnd::kProsperity, "planet_prosperity"},
                                      std::tuple{400, PlSquareWnd::kPlanet, "planet_info"}}) {
    r.rig.click(x, 50);
    CHECK(r.dialog().mode() == mode);
    write_shot(r.rig, name);
    r.rig.key(SDLK_ESCAPE);
  }

  // A built square: Abandon and Cancel.
  r.rig.move(410, 280);
  r.rig.click(410, 280);
  REQUIRE(r.dialog().shown());
  CHECK(r.dialog().mode() == PlSquareWnd::kSquare);
  CHECK(r.dialog().commands() == std::vector<int>{PlSquareWnd::kAbandon, PlSquareWnd::kCancel});
  write_shot(r.rig, "planet_square");
  r.rig.key(SDLK_ESCAPE);

  // The build list of the whole planet.
  r.rig.click(540, 50);  // PLBUILD
  CHECK(r.rig.wm.find_window("PLANLIST")->shown());
  CHECK_FALSE(plan.build_items().empty());
  write_shot(r.rig, "planet_build");
  r.rig.move(300, 200);
  write_shot(r.rig, "planet_build_hover");
  r.rig.key(SDLK_ESCAPE);
  CHECK_FALSE(r.rig.wm.find_window("PLANLIST")->shown());

  // M toggles the planet's self-management.
  const std::int32_t managed = r.p().self_managed;
  r.rig.key(SDLK_M);
  CHECK(r.p().self_managed == ~managed);
  r.rig.click(400, 50);
  write_shot(r.rig, "planet_info_managed");
  r.rig.key(SDLK_ESCAPE);
  r.rig.key(SDLK_M);
  CHECK(r.p().self_managed == managed);

  // Esc with nothing open leaves the screen.
  r.rig.key(SDLK_ESCAPE);
  CHECK(r.rig.wm.current_screen() != kScreenPlanet);
}

TEST_CASE("Planet screen orbital square dialog", "[planet]") {
  OA_REQUIRE_GAME_FILES(files);
  PlanetRig r(files, "Zoroaster III");
  r.rig.move(48, 154);
  write_shot(r.rig, "planet_orbit_hover");
  r.rig.click(48, 154);
  REQUIRE(r.dialog().shown());
  write_shot(r.rig, "planet_orbit_square");
}

TEST_CASE("Planet screen build list picks a project", "[planet]") {
  OA_REQUIRE_GAME_FILES(files);
  PlanetRig r(files, "Zoroaster III");
  game::Session& s = r.session.session();
  game::Planet& p = r.p();
  PlanWnd& plan = r.plan();
  // Every listed item can be placed somewhere on the planet; Colony Base
  // never is.
  r.rig.click(540, 50);
  for (const std::uint8_t item : plan.build_items()) {
    CHECK(item != game::kItemColonyBase);
    CHECK(game::planet_can_place_item(s, p, item, PlanWnd::kNoSquare));
  }
  // Picking a surface building selects it ("Pick a Location").
  const auto& items = plan.build_items();
  const auto it = std::find_if(items.begin(), items.end(), [&](std::uint8_t i) {
    return i < s.tables->planitems.surface_count && (s.tables->planitems.items[i].flags & game::kItemWholePlanet) == 0;
  });
  REQUIRE(it != items.end());
  const std::uint8_t item = *it;
  const int row = static_cast<int>(it - items.begin());
  plan.handle_event(kEventListPick, row, kEventLeftClick);
  CHECK(plan.selection() == item);
  CHECK_FALSE(r.rig.wm.find_window("PLANLIST")->shown());
  // A click on a square that accepts it makes it the project there.
  std::uint16_t target = PlanWnd::kNoSquare;
  for (std::uint16_t sq = 0; sq < p.surface_squares && target == PlanWnd::kNoSquare; ++sq) {
    if (game::planet_squares(s.game->cosmos, p)[sq].item == game::kNoItem &&
        game::planet_can_place_item(s, p, item, sq)) {
      target = sq;
    }
  }
  REQUIRE(target != PlanWnd::kNoSquare);
  // Find the screen point of the target square.
  const game::PlanItemTable& t = s.tables->planitems;
  const std::int32_t rc = game::planet_square_rowcol(t, p, target);
  const PolyPoint top = planet_geometry::diamond_top(rc >> 16, rc & 0xffff);
  r.rig.move(top.x, top.y + 17);
  CHECK(plan.hovered() == target);
  write_shot(r.rig, "planet_pick_location");
  // A long press would drop the item (the right button).
  CHECK(r.rig.wm.long_press(top.x, top.y + 17) == LongPress::kRight);
  r.rig.click(top.x, top.y + 17);
  CHECK(p.project_square == target);
  CHECK(p.project_item == item);
  CHECK(plan.selection() == PlanWnd::kNoSelection);
  CHECK(r.rig.wm.long_press(top.x, top.y + 17) == LongPress::kHelp);
  // Abandon on the unbuilt project square removes it.
  r.rig.click(top.x, top.y + 17);
  REQUIRE(r.dialog().shown());
  CHECK(r.dialog().commands() == std::vector<int>{PlSquareWnd::kAbandon, PlSquareWnd::kCancel});
  r.dialog().handle_event(PlSquareWnd::kAbandon, 0, 0);
  CHECK(game::planet_squares(s.game->cosmos, p)[target].item == game::kNoItem);
  CHECK(p.project_square == game::kNoSquare);
  CHECK_FALSE(r.dialog().shown());
}

TEST_CASE("Planet screen scrolled build list sets a planet-wide project", "[planet]") {
  // The oracle run `corpus/resume820_endlessparty`: Flammifer I switches to
  // Endless Party from the scrolled build list.
  OA_REQUIRE_GAME_FILES(files);
  PlanetRig r(files, "Flammifer I", 300, 257);
  r.rig.click(540, 50);  // PLBUILD
  REQUIRE(r.rig.wm.find_window("PLANLIST")->shown());
  for (int i = 0; i < 18; ++i) {
    r.rig.click(507, 405);  // the scroll bar's down arrow
    for (int k = 0; k < 5; ++k) {
      r.rig.pass();
    }
  }
  r.rig.click(300, 233);
  r.rig.drain();
  for (int k = 0; k < 20; ++k) {
    r.rig.pass();
  }
  CHECK(r.p().project_item == game::kItemEndlessParty);
  CHECK(r.p().project_square == PlanWnd::kNoSquare);
  write_shot(r.rig, "planet_endless_party");
}

TEST_CASE("Planet screen placement and square dialogs", "[planet]") {
  // The oracle script `planet_actions.txt` on Zoroaster III (a Ship in
  // production on orbital square 46).
  OA_REQUIRE_GAME_FILES(files);
  PlanetRig r(files, "Zoroaster III");
  game::Session& s = r.session.session();
  game::Planet& p = r.p();
  const auto square_item = [&](std::uint16_t sq) { return game::planet_squares(s.game->cosmos, p)[sq].item; };
  REQUIRE(p.project_item == game::kItemShip);
  const std::int16_t ships = s.game->cosmos.ship_count;
  r.rig.click(540, 50);   // PLBUILD
  r.rig.click(300, 156);  // row 0: Factory
  CHECK(r.plan().selection() == 0);
  r.rig.move(376, 382);
  CHECK(r.plan().hovered() == 40);
  write_shot(r.rig, "act_preview");
  r.rig.click(376, 382);
  // The Factory replaces the Ship in production, which is destroyed.
  CHECK(p.project_square == 40);
  CHECK(p.project_item == 0);
  CHECK(square_item(40) == 0);
  CHECK(square_item(46) == game::kNoItem);
  CHECK(s.game->cosmos.ship_count == ships - 1);
  write_shot(r.rig, "act_project");
  r.rig.click(376, 382);
  CHECK(r.dialog().commands() == std::vector<int>{PlSquareWnd::kAbandon, PlSquareWnd::kCancel});
  write_shot(r.rig, "act_project_dialog");
  r.rig.key(SDLK_ESCAPE);
  r.rig.click(342, 365);
  CHECK(r.dialog().square() == 39);
  CHECK(r.dialog().commands() == std::vector<int>{PlSquareWnd::kBuild, PlSquareWnd::kCancel});
  write_shot(r.rig, "act_empty_dialog");
  r.rig.click(240, 352);  // Build
  CHECK(r.rig.wm.find_window("PLANLIST")->shown());
  write_shot(r.rig, "act_square_list");
  r.rig.click(300, 156);  // Factory on square 39
  CHECK(p.project_square == 39);
  CHECK(square_item(39) == 0);
  CHECK(square_item(40) == game::kNoItem);
  write_shot(r.rig, "act_project_moved");
  r.rig.click(48, 294);
  CHECK(r.dialog().square() == 47);
  write_shot(r.rig, "act_orbital_dialog");
}

TEST_CASE("Planet item help: the item's name and picture", "[planet]") {
  OA_REQUIRE_GAME_FILES(files);
  PlanetRig r(files, "Zoroaster III");
  auto& help = dynamic_cast<HelpWnd&>(*r.rig.wm.find_window("HELPWINDOW"));
  for (const int item : {0, static_cast<int>(game::kItemShip)}) {
    // The help runs modally until the right click that closes it.
    // (A move for the pass that posts it, the click for the modal loop.)
    r.rig.wm.post_message(kMessageHelp, item, kHelpPlanet);
    r.rig.pending.push_back(InputEvent{.type = EventType::kMouseMove, .x = 320, .y = 240});
    r.rig.queue_click(320, 240, MouseButton::kRight);
    r.rig.drain();
    CHECK(help.topic() == std::format("plitem{:02d}", item));
    const std::string& name = r.session.session().tables->planitems.items[static_cast<std::size_t>(item)].name;
    CHECK(help.content().elements[0].text.find(name) != std::string::npos);
    CHECK(help.content().elements[1].shape_index == (item == game::kItemShip ? 14 : 30));
    CHECK(help.content().elements[1].frame == (item == game::kItemShip ? 0 : item));
  }
}

TEST_CASE("Planet screen ship in orbit: dialog and Leave Orbit", "[planet]") {
  // The oracle script `planet_ship.txt`: Explorator 1 (slot 3) in orbit of
  // the player's Ajax III, on orbital square 73.
  OA_REQUIRE_GAME_FILES(files);
  PlanetRig r(files, "Ajax III", 300, 100);
  game::Session& s = r.session.session();
  game::Planet& p = r.p();
  const auto square_item = [&](std::uint16_t sq) { return game::planet_squares(s.game->cosmos, p)[sq].item; };
  write_shot(r.rig, "ship_planet");
  REQUIRE(square_item(73) == game::kItemShip);
  r.rig.move(48, 154);
  CHECK(r.plan().hovered() == 73);
  write_shot(r.rig, "ship_hover");
  r.rig.click(48, 154);
  CHECK(r.dialog().commands() ==
        std::vector<int>{PlSquareWnd::kLeaveOrbit, PlSquareWnd::kAbandon, PlSquareWnd::kCancel});
  write_shot(r.rig, "ship_dialog");
  r.rig.click(213, 352);  // Leave Orbit
  CHECK_FALSE(r.dialog().shown());
  CHECK(square_item(73) == game::kNoItem);
  const game::Ship& ship = s.game->cosmos.ships[3];
  CHECK(ship.location_kind == game::ShipLocation::kSystem);
  CHECK(ship.location == p.star);
  write_shot(r.rig, "ship_left");
}

TEST_CASE("Planet screen Create Colony from a colony ship", "[planet]") {
  // Explorator 1 (slot 3, two Colonizers) moved from Ajax III to the orbit
  // of an unowned planet of the same star.
  OA_REQUIRE_GAME_FILES(files);
  CoreGameSession session(files, files.load("RESUME.GAM"));
  game::Session& s = session.session();
  game::Cosmos& c = s.game->cosmos;
  game::Ship& ship = c.ships[3];
  game::Planet& ajax = c.planets.at(static_cast<std::size_t>(ship.location));
  game::PlanetIndex target = game::kNone;
  for (game::PlanetIndex i = 0; i < c.planet_count && target == game::kNone; ++i) {
    if (c.planets[static_cast<std::size_t>(i)].star == ajax.star &&
        c.planets[static_cast<std::size_t>(i)].owner == game::kNoOwner) {
      target = i;
    }
  }
  REQUIRE(target != game::kNone);
  game::Planet& p = c.planets[static_cast<std::size_t>(target)];
  REQUIRE(game::planet_undock_ship(s, ajax, ship));
  REQUIRE(game::planet_dock_ship(s, p, ship));
  const std::uint16_t ship_square = p.surface_squares;
  REQUIRE(game::planet_squares(c, p)[ship_square].item == game::kItemShip);
  c.selected_planet = target;
  UiRig rig(files, &session);
  rig.wm.restore_window_state();
  rig.move(300, 100);
  rig.wm.set_screen(kScreenPlanet, true);
  rig.pass();
  auto& dialog = dynamic_cast<PlSquareWnd&>(*rig.wm.find_window("PLSQUARE"));
  auto& plan = dynamic_cast<PlanWnd&>(*rig.wm.find_window("PLANETALLOC"));
  // Only the ship can be hovered on an unowned planet.
  rig.move(410, 280);
  CHECK(plan.hovered() == PlanWnd::kNoSquare);
  rig.move(48, 154);
  CHECK(plan.hovered() == ship_square);
  rig.click(48, 154);
  REQUIRE(dialog.commands() == std::vector<int>{PlSquareWnd::kCreateColony, PlSquareWnd::kLeaveOrbit,
                                                PlSquareWnd::kAbandon, PlSquareWnd::kCancel});
  write_shot(rig, "colony_dialog");
  rig.click(200, 352);  // Create Colony
  CHECK(plan.selection() == game::kItemColonyBase);
  // In placement mode every square can be hovered; the first one that takes
  // the Colony Base.
  std::uint16_t square = PlanWnd::kNoSquare;
  for (std::uint16_t sq = 0; sq < p.surface_squares && square == PlanWnd::kNoSquare; ++sq) {
    if (game::planet_can_place_item(s, p, game::kItemColonyBase, sq)) {
      square = sq;
    }
  }
  REQUIRE(square != PlanWnd::kNoSquare);
  const std::int32_t rc = game::planet_square_rowcol(s.tables->planitems, p, square);
  const PolyPoint top = planet_geometry::diamond_top(rc >> 16, rc & 0xffff);
  rig.move(top.x, top.y + 17);
  CHECK(plan.hovered() == square);
  write_shot(rig, "colony_place");
  // The click, then Enter in the "Name Your Colony" box, then a right click
  // closing the colony report.
  rig.queue_click(top.x, top.y + 17);
  rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_RETURN, .mods = 0});
  rig.pending.push_back(InputEvent{.type = EventType::kMouseMove, .x = 320, .y = 240});
  rig.queue_click(320, 240, MouseButton::kRight);
  rig.drain();
  CHECK(p.owner == s.player_race);
  CHECK(p.population == 2);
  CHECK(game::planet_squares(c, p)[square].item == game::kItemColonyBase);
  CHECK(game::ship_find_gizmo(ship, game::kGizmoColonizer) != -1);  // the second one
  auto& help = dynamic_cast<HelpWnd&>(*rig.wm.find_window("HELPWINDOW"));
  CHECK((help.topic() == "colonize" || help.topic() == "maxcolonize"));
  CHECK(help.content().elements[2].text.starts_with(std::string(game::name_view(p.name))));
  CHECK(plan.selection() == PlanWnd::kNoSelection);
}

TEST_CASE("Planet actions of the game core", "[planet]") {
  game::Planet p{};
  p.self_managed = 0;
  game::planet_toggle_self_managed(p);
  CHECK(p.self_managed == -1);
  game::planet_toggle_self_managed(p);
  CHECK(p.self_managed == 0);
}

}  // namespace oa::test
