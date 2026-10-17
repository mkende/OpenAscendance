#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "core_session.h"
#include "game/planet.h"
#include "game/research.h"
#include "game/ship.h"
#include "game/ship_design.h"
#include "game_data.h"
#include "screen_shots.h"
#include "screens/fleet_wnd.h"
#include "screens/plan_wnd.h"
#include "screens/ship_des_wnd.h"
#include "ui_rig.h"

namespace oa::test {

namespace {

using game::DesignCell;

constexpr int kScreenShipDesign = 8;
constexpr int kScreenPlanet = 17;
constexpr int kScreenFleet = 18;
constexpr std::uint8_t kOrbitalDocks = 24;

// Gizmos of `gizmos.txt` used below.
constexpr std::int8_t kMassBarrageGun = 0;   // weapon, industry 15
constexpr std::int8_t kFourierMissiles = 1;  // weapon, industry 25
constexpr std::int8_t kProtonShaver = 27;    // generator, industry 20

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

// The shipped day-820 game (`RESUME.GAM`) as `ship_design_save.py` edits
// it for the oracle run `ship_design.txt` (the player knows the Large and
// Enormous hulls, Ajax III has built Orbital Docks on square 74), on screen
// `screen` (with `planet` selected), the pointer at (x, y).
struct GameRig {
  GameRig(const GameFiles& files, int screen, std::string_view planet = {}, int x = 300, int y = 100)
      : session(files, files.load("RESUME.GAM")), rig(files, &session) {
    game::Research& research = s().game->research;
    research.items[19].known_mask |= 1;
    research.items[31].known_mask |= 1;
    game::Planet& ajax = cosmos().planets.at(static_cast<std::size_t>(find_planet(s(), "Ajax III")));
    game::Square& docks = game::planet_squares(cosmos(), ajax)[74];
    REQUIRE(docks.item == game::kNoItem);
    docks.item = kOrbitalDocks;
    docks.flags |= game::kSquareBuilt;
    if (!planet.empty()) {
      cosmos().selected_planet = find_planet(s(), planet);
    }
    rig.wm.restore_window_state();
    rig.move(x, y);
    rig.wm.set_screen(screen, true);
    rig.pass();
  }
  game::Session& s() { return session.session(); }
  game::Cosmos& cosmos() { return session.session().game->cosmos; }
  game::Planet& planet() { return cosmos().planets.at(static_cast<std::size_t>(cosmos().selected_planet)); }
  FleetWnd& fleet() { return dynamic_cast<FleetWnd&>(*rig.wm.find_window("FLEETWND")); }
  ShipDesWnd& design() { return dynamic_cast<ShipDesWnd&>(*rig.wm.find_window("SHIPDESSCREEN")); }
  PlanWnd& plan() { return dynamic_cast<PlanWnd&>(*rig.wm.find_window("PLANETALLOC")); }
  PlSquareWnd& dialog() { return dynamic_cast<PlSquareWnd&>(*rig.wm.find_window("PLSQUARE")); }
  // Presses `sdl_key` on the next pass and lets 10 passes go by.
  void key(std::uint32_t sdl_key) {
    rig.key(sdl_key);
    for (int i = 0; i < 10; ++i) {
      rig.pass();
    }
  }

  CoreGameSession session;
  UiRig rig;
};

// The oracle script's first visit of the design screen: Frigate 1 viewed
// from the Ships list, the weapons shown (the design screen keeps its
// category and selection), back on the list.
void view_frigate(GameRig& r) {
  r.rig.click(300, 300, MouseButton::kRight);
  r.rig.pass();
  REQUIRE(r.rig.wm.current_screen() == kScreenShipDesign);
  r.rig.click(500, 22);  // WEAPONS
  r.key(SDLK_ESCAPE);
  REQUIRE(r.rig.wm.current_screen() == kScreenFleet);
}

// Opens the planet screen of `name`, the pointer at (x, y).
void open_planet(GameRig& r, std::string_view name, int x, int y) {
  r.cosmos().selected_planet = find_planet(r.s(), name);
  r.rig.move(x, y);
  r.rig.wm.set_screen(kScreenPlanet, true);
  r.rig.pass();
}

// Starts a new design on Flammifer I as the oracle script does after
// `view_frigate`: the Ship from the build list, then orbital square column
// 0, row 2.
void start_new_design(GameRig& r) {
  view_frigate(r);
  open_planet(r, "Flammifer I", 300, 257);
  r.rig.click(540, 50);  // PLBUILD
  const auto& items = r.plan().build_items();
  const auto it = std::find(items.begin(), items.end(), game::kItemShip);
  REQUIRE(it != items.end());
  for (int i = 0; i < 12; ++i) {
    r.rig.click(507, 405);  // the scroll bar's down arrow
    for (int k = 0; k < 5; ++k) {
      r.rig.pass();
    }
  }
  // The Metroplex that research 31 adds puts the Ship on the third row.
  r.rig.click(300, 310);
  REQUIRE(r.plan().selection() == game::kItemShip);
  r.rig.click(48, 300);
  for (int i = 0; i < 10; ++i) {
    r.rig.pass();
  }
  REQUIRE(r.rig.wm.current_screen() == kScreenShipDesign);
}

}  // namespace

TEST_CASE("Ship design slot grid of each hull", "[shipdes]") {
  // `Ship_init`'s spreading of the 5 / 10 / 15 / 25 slots over the 48 cells.
  CHECK(std::vector(game::ship_design_cells(0).begin(), game::ship_design_cells(0).end()) ==
        std::vector<DesignCell>{{0, 0}, {4, 3}, {7, 8}, {10, 3}, {13, 3}});
  CHECK(std::vector(game::ship_design_cells(1).begin(), game::ship_design_cells(1).end()) ==
        std::vector<DesignCell>{{0, 0}, {2, 1}, {3, 4}, {5, 6}, {7, 6}, {8, 6}, {9, 6}, {10, 5}, {11, 4}, {13, 3}});
  CHECK(std::vector(game::ship_design_cells(2).begin(), game::ship_design_cells(2).end()) ==
        std::vector<DesignCell>{{0, 0},
                                {1, 1},
                                {2, 2},
                                {3, 4},
                                {4, 5},
                                {6, 5},
                                {7, 5},
                                {8, 4},
                                {8, 7},
                                {9, 4},
                                {10, 2},
                                {10, 5},
                                {11, 3},
                                {12, 2},
                                {13, 3}});
  CHECK(std::vector(game::ship_design_cells(3).begin(), game::ship_design_cells(3).end()) ==
        std::vector<DesignCell>{{0, 0},  {0, 1},  {1, 1},  {2, 1},  {2, 3},  {3, 3},  {4, 3}, {4, 5}, {5, 5},
                                {6, 5},  {6, 7},  {7, 6},  {7, 8},  {8, 5},  {8, 7},  {9, 3}, {9, 5}, {9, 7},
                                {10, 3}, {10, 5}, {11, 1}, {11, 3}, {11, 5}, {12, 3}, {13, 3}});
  // The hit test finds each slot at its cell's centre, and only the hull's.
  for (std::uint8_t hull = 0; hull < 4; ++hull) {
    const auto cells = game::ship_design_cells(hull);
    for (std::size_t i = 0; i < cells.size(); ++i) {
      const game::DesignPoint p = game::design_cell_point(cells[i]);
      CHECK(game::ship_design_slot_at(p.x, p.y, hull) == static_cast<std::int32_t>(i));
      // The diamond is 72x36: 17 pixels off-centre is still the cell.
      CHECK(game::ship_design_slot_at(p.x + 34, p.y, hull) == static_cast<std::int32_t>(i));
      CHECK(game::ship_design_slot_at(p.x, p.y - 17, hull) == static_cast<std::int32_t>(i));
    }
  }
  // (0, 1) is an Enormous slot only; above the grid there is none.
  const game::DesignPoint p = game::design_cell_point({0, 1});
  CHECK(game::ship_design_slot_at(p.x, p.y, 3) == 1);
  CHECK(game::ship_design_slot_at(p.x, p.y, 0) == -1);
  CHECK(game::ship_design_slot_at(403, 148, 3) == -1);
}

TEST_CASE("Ship design costs, hulls and gizmo lists", "[shipdes]") {
  OA_REQUIRE_GAME_FILES(files);
  CoreGameSession core(files, files.load("RESUME.GAM"));
  game::Session& s = core.session();
  const std::uint8_t player = s.player_race;
  game::Ship ship;
  game::ship_init(s, ship, 0);
  ship.owner = player;

  // Industry: the empty hull, then its gizmos.
  CHECK(game::ship_industry_cost(s, ship) == 70);
  REQUIRE(game::ship_install_gizmo(s, ship, kFourierMissiles, 0));
  CHECK(game::ship_industry_cost(s, ship) == 70 + s.tables->gizmos[kFourierMissiles].industry);
  // Build days at Flammifer I (industry 15 per day, no progress):
  // ceil(cost / 15).
  const game::Planet& flammifer = s.game->cosmos.planets.at(static_cast<std::size_t>(find_planet(s, "Flammifer I")));
  CHECK(flammifer.industry == 15);
  CHECK(game::research_days_left(game::ship_industry_cost(s, ship), 0, 15) ==
        (game::ship_industry_cost(s, ship) + 14) / 15);

  // Refit: 30 plus the gizmo industry added; removals are free.
  const game::Ship original = ship;
  CHECK(game::ship_design_refit_cost(s, ship, original) == 30);
  REQUIRE(game::ship_install_gizmo(s, ship, kProtonShaver, 1));
  CHECK(game::ship_design_refit_cost(s, ship, original) == 30 + s.tables->gizmos[kProtonShaver].industry);
  REQUIRE(game::ship_remove_gizmo(s, ship, 0));
  REQUIRE(game::ship_remove_gizmo(s, ship, 1));
  CHECK(game::ship_design_refit_cost(s, ship, original) == 30);

  // The hull cycle: Small and Medium only without the research, wrapping.
  game::Research& research = s.game->research;
  REQUIRE_FALSE(game::ship_design_hull_known(research, player, 2));
  REQUIRE_FALSE(game::ship_design_hull_known(research, player, 3));
  CHECK(game::ship_design_cycle_hull(s, ship, player));
  CHECK(ship.hull_size == 1);
  CHECK(ship.slot_count == 10);
  CHECK(game::ship_design_cycle_hull(s, ship, player));
  CHECK(ship.hull_size == 0);
  research.items[19].known_mask |= static_cast<std::uint8_t>(1u << player);
  research.items[31].known_mask |= static_cast<std::uint8_t>(1u << player);
  for (const int hull : {1, 2, 3, 0}) {
    CHECK(game::ship_design_cycle_hull(s, ship, player));
    CHECK(ship.hull_size == hull);
  }
  // Six gizmos do not fit a Small hull: Enormous goes back to Medium,
  // repacked in the first slots.
  REQUIRE(game::ship_design_cycle_hull(s, ship, player));
  REQUIRE(game::ship_design_cycle_hull(s, ship, player));
  REQUIRE(game::ship_design_cycle_hull(s, ship, player));
  REQUIRE(ship.hull_size == 3);
  for (const std::int32_t slot : {2, 5, 9, 12, 20, 24}) {
    REQUIRE(game::ship_install_gizmo(s, ship, kProtonShaver, slot));
  }
  CHECK(game::ship_design_cycle_hull(s, ship, player));
  CHECK(ship.hull_size == 1);
  for (std::size_t i = 0; i < 10; ++i) {
    CHECK(ship.slots[i].gizmo == (i < 6 ? kProtonShaver : -1));
  }

  // The list of a category: its gizmos without research or known.
  const std::vector<std::int8_t> weapons = game::ship_design_gizmo_list(s, 0, player);
  REQUIRE_FALSE(weapons.empty());
  for (const std::int8_t g : weapons) {
    const game::Gizmo& gizmo = s.tables->gizmos.at(static_cast<std::size_t>(g));
    CHECK(gizmo.category == 0);
    CHECK((gizmo.research_requirement == 0xff ||
           (research.items.at(gizmo.research_requirement).known_mask >> player & 1) != 0));
  }
  CHECK(weapons.front() == kMassBarrageGun);
}

TEST_CASE("Ships list rows, title and picks", "[shipdes]") {
  // The oracle run `ship_design.txt` SCR001..SCR005.
  OA_REQUIRE_GAME_FILES(files);
  GameRig r(files, kScreenFleet, {}, 553, 184);
  FleetWnd& fleet = r.fleet();
  REQUIRE(fleet.row_count() == 5);
  const std::vector<std::string> names{"Explorator 1", "Explorator 2", "Frigate 1", "Frigate 2", "Invader 1"};
  for (int i = 0; i < 5; ++i) {
    CHECK(game::name_view(r.cosmos().ships.at(static_cast<std::size_t>(fleet.row_ship(i))).name) ==
          names[static_cast<std::size_t>(i)]);
  }
  const auto location = [&](int row) {
    return FleetWnd::location_text(r.rig.wm, r.s(), r.cosmos().ships.at(static_cast<std::size_t>(fleet.row_ship(row))));
  };
  CHECK(location(0) == "Orbiting Ajax III");
  CHECK(location(1) == "In Starlane to Vernunft System\nETA - 8 Days");
  CHECK(location(2) == "in Feculon System");
  CHECK(location(4) == "Under Construction at Zoroaster III");
  write_shot(r.rig, "fleet_list");
  r.rig.move(555, 300);
  write_shot(r.rig, "fleet_hover");

  // A right click shows the ship's design, read-only.
  const game::ShipIndex frigate = fleet.row_ship(2);
  r.rig.click(300, 300, MouseButton::kRight);
  r.rig.pass();
  REQUIRE(r.rig.wm.current_screen() == kScreenShipDesign);
  ShipDesWnd& design = r.design();
  CHECK(design.read_only());
  CHECK(design.ship() == &r.cosmos().ships.at(static_cast<std::size_t>(frigate)));
  CHECK(design.category() == ShipDesWnd::kGenerator);
  write_shot(r.rig, "design_view");
  const game::Ship before = *design.ship();
  r.rig.click(403, 166);
  r.rig.click(403, 166, MouseButton::kRight);
  CHECK(design.ship()->slots[0].gizmo == before.slots[0].gizmo);
  CHECK_FALSE(design.modified());
  r.rig.click(500, 22);  // WEAPONS
  CHECK(design.category() == ShipDesWnd::kWeapon);
  CHECK(design.selected_gizmo() == kMassBarrageGun);
  write_shot(r.rig, "design_view_weapons");
  r.key(SDLK_ESCAPE);
  CHECK(r.rig.wm.current_screen() == kScreenFleet);
  CHECK(design.ship() == nullptr);
  write_shot(r.rig, "fleet_back");

  // A left click goes where the ship is: Explorator 1's planet.
  r.rig.click(300, 100);
  r.rig.pass();
  CHECK(r.rig.wm.current_screen() == kScreenPlanet);
  CHECK(game::name_view(r.planet().name) == "Ajax III");
}

TEST_CASE("Ship design: a new design named and built", "[shipdes]") {
  // The oracle run `ship_design.txt` SCR006..SCR018 on Flammifer I.
  OA_REQUIRE_GAME_FILES(files);
  GameRig r(files, kScreenFleet);
  const std::int16_t ships = r.cosmos().ship_count;
  start_new_design(r);
  ShipDesWnd& design = r.design();
  REQUIRE(design.ship() != nullptr);
  game::Ship& ship = *design.ship();
  CHECK(design.mode() == ShipDesWnd::kNewDesign);
  CHECK_FALSE(design.read_only());
  CHECK(r.cosmos().ship_count == ships + 1);
  CHECK(ship.hull_size == 0);
  CHECK(design.category() == ShipDesWnd::kWeapon);
  CHECK(design.selected_gizmo() == kMassBarrageGun);
  write_shot(r.rig, "design_new");
  r.rig.move(403, 166);
  CHECK(design.hovered_slot() == 0);
  write_shot(r.rig, "design_preview");
  for (const int hull : {1, 2, 3}) {
    r.rig.click(558, 22);  // HULL
    CHECK(ship.hull_size == hull);
    write_shot(r.rig, std::string("design_hull") + std::to_string(hull));
  }
  CHECK_FALSE(design.modified());
  r.rig.click(403, 166);  // slot 0: the Mass Barrage Gun
  CHECK(design.modified());
  r.rig.click(500, 22);   // WEAPONS
  r.rig.click(557, 189);  // the second weapon
  CHECK(design.selected_gizmo() == kFourierMissiles);
  r.rig.click(439, 184);  // slot 1
  r.rig.click(403, 202);  // slot 2
  r.rig.click(439, 184, MouseButton::kRight);
  CHECK(ship.slots[0].gizmo == kMassBarrageGun);
  CHECK(ship.slots[1].gizmo == -1);
  CHECK(ship.slots[2].gizmo == kFourierMissiles);
  CHECK(ship.gizmo_count == 2);
  // 410 + 15 + 25 industry at 15 per day: " (30 Days)".
  CHECK(game::ship_industry_cost(r.s(), ship) == 450);
  write_shot(r.rig, "design_fitted");
  for (const auto& [x, y, category, name] : {std::tuple{617, 22, ShipDesWnd::kShield, "design_defenses"},
                                             std::tuple{500, 61, ShipDesWnd::kDrive, "design_engines"},
                                             std::tuple{539, 61, ShipDesWnd::kScanner, "design_sensors"},
                                             std::tuple{617, 61, ShipDesWnd::kSpecial, "design_special"}}) {
    r.rig.click(x, y);
    CHECK(design.category() == category);
    write_shot(r.rig, name);
  }
  r.rig.click(558, 22);  // HULL: back to Small, the gizmos repacked
  CHECK(ship.hull_size == 0);
  CHECK(ship.slots[0].gizmo == kMassBarrageGun);
  CHECK(ship.slots[1].gizmo == kFourierMissiles);
  write_shot(r.rig, "design_small");

  // Esc, then Enter in "Name Your Ship": the ship is kept, its name the
  // hull's.
  r.rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_ESCAPE, .mods = 0});
  r.rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_RETURN, .mods = 0});
  r.rig.drain();
  for (int i = 0; i < 10; ++i) {
    r.rig.pass();
  }
  CHECK(r.rig.wm.current_screen() == kScreenPlanet);
  CHECK(r.cosmos().ship_count == ships + 1);
  // As the oracle's save: the ship as designed, built on day 825 (the day
  // plus the empty hull's days at its creation, `Race_ai_new_ship`).
  CHECK(game::name_view(ship.name) == "Small");
  CHECK(ship.location_kind == game::ShipLocation::kBuilding);
  CHECK(ship.built_day == 825);
  CHECK(ship.gizmo_count == 2);
  CHECK(ship.weapon_damage == 3);
  CHECK(ship.max_hit_points == 10);
  game::Planet& p = r.planet();
  CHECK(p.project_item == game::kItemShip);
  CHECK(game::planet_square_ship(r.s(), p, p.project_square) == game::ship_index(r.cosmos(), ship));
  write_shot(r.rig, "design_built");
}

TEST_CASE("Ship design: unchanged and hull-only designs are discarded", "[shipdes]") {
  // `exp/e10`: only the hull changed, Esc: no ship, no project.
  OA_REQUIRE_GAME_FILES(files);
  for (const bool change_hull : {false, true}) {
    GameRig r(files, kScreenFleet);
    const std::int16_t ships = r.cosmos().ship_count;
    start_new_design(r);
    if (change_hull) {
      r.rig.click(558, 22);
      CHECK(r.design().ship()->hull_size == 1);
    }
    r.key(SDLK_ESCAPE);
    CHECK(r.rig.wm.current_screen() == kScreenPlanet);
    CHECK(r.cosmos().ship_count == ships);
    CHECK(game::planet_squares(r.cosmos(), r.planet())[r.planet().surface_squares + 2].item == game::kNoItem);
    CHECK(r.planet().project_item == game::kNoItem);
  }
}

TEST_CASE("Ship design: a refit at the Orbital Docks", "[shipdes]") {
  // The oracle run `ship_design.txt` SCR019..: Explorator 1 in orbit of
  // Ajax III (square 73), next to the edited Orbital Docks.
  OA_REQUIRE_GAME_FILES(files);
  GameRig r(files, kScreenPlanet, "Ajax III");
  game::Planet& p = r.planet();
  const std::uint16_t square = 73;
  game::Ship& ship = r.cosmos().ships.at(static_cast<std::size_t>(game::planet_square_ship(r.s(), p, square)));
  REQUIRE(game::name_view(ship.name) == "Explorator 1");
  r.rig.move(48, 154);
  r.rig.click(48, 154);
  REQUIRE(r.dialog().shown());
  REQUIRE(r.dialog().commands() ==
          std::vector<int>{PlSquareWnd::kLeaveOrbit, PlSquareWnd::kRefit, PlSquareWnd::kAbandon, PlSquareWnd::kCancel});
  write_shot(r.rig, "refit_dialog");
  r.rig.click(280, 352);  // Refit
  r.rig.pass();
  REQUIRE(r.rig.wm.current_screen() == kScreenShipDesign);
  ShipDesWnd& design = r.design();
  CHECK(design.mode() == ShipDesWnd::kRefit);
  CHECK(design.ship() == &ship);
  const std::uint8_t hull = ship.hull_size;
  r.rig.click(558, 22);  // HULL: no change in a refit
  CHECK(ship.hull_size == hull);
  CHECK(design.refit_cost(r.s()) == 30);
  // A generator in the first empty slot, 4 (Medium cell (7, 6)).
  REQUIRE(hull == 1);
  REQUIRE(game::ship_find_gizmo(ship, -1) == 4);
  r.rig.click(578, 61);  // ENERGY
  r.rig.click(367, 400);
  CHECK(ship.slots[4].gizmo == kProtonShaver);
  const std::int32_t cost = 30 + r.s().tables->gizmos[kProtonShaver].industry;
  CHECK(design.refit_cost(r.s()) == cost);
  write_shot(r.rig, "refit_days");
  r.rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_ESCAPE, .mods = 0});
  r.rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_RETURN, .mods = 0});
  r.rig.drain();
  for (int i = 0; i < 10; ++i) {
    r.rig.pass();
  }
  CHECK(r.rig.wm.current_screen() == kScreenPlanet);
  const game::Square& sq = game::planet_squares(r.cosmos(), p)[square];
  CHECK(sq.item == game::kItemShip);
  CHECK((sq.flags & game::kSquareBuilt) == 0);
  CHECK(p.project_item == game::kItemShip);
  CHECK(p.project_square == square);
  CHECK(p.ship_cost == cost);
  CHECK(ship.location_kind == game::ShipLocation::kRefit);
  CHECK(ship.location == game::planet_index(r.cosmos(), p));
  CHECK(game::name_view(ship.name) == "Explorator 1");
  CHECK(ship.gizmo_count == 9);
  CHECK(ship.generator_power == 6);
  write_shot(r.rig, "refit_done");
}

}  // namespace oa::test
