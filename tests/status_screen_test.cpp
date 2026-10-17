#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "core_session.h"
#include "game_data.h"
#include "screen_shots.h"
#include "screens/status_wnd.h"
#include "ui_rig.h"

namespace oa::test {

namespace {

constexpr int kScreenStatus = 10;
constexpr int kScreenPlanet = 17;
constexpr int kScreenStarSystem = 16;

// The shipped day-820 game (`RESUME.GAM`) on the planet list.
struct StatusRig {
  explicit StatusRig(const GameFiles& files) : session(files, files.load("RESUME.GAM")), rig(files, &session) {
    rig.wm.set_screen(kScreenStatus, true);
    rig.pass();
    wnd = dynamic_cast<StatusWnd*>(rig.wm.find_window("STATUSWND"));
  }
  CoreGameSession session;
  UiRig rig;
  StatusWnd* wnd = nullptr;
};

std::string row_name(StatusRig& r, int row) {
  return std::string(game::name_view(r.session.session().game->cosmos.planets.at(
      static_cast<std::size_t>(r.wnd->row_planet(row))).name));
}

}  // namespace

TEST_CASE("The planet list holds the player's planets, oldest colony first", "[status]") {
  OA_REQUIRE_GAME_FILES(files);
  StatusRig r(files);
  REQUIRE(r.wnd != nullptr);
  // The original's list of the day-820 game (oracle m4_prs SCR001): 13
  // planets, Zoroaster III, Flammifer I and Liberty III on top.
  REQUIRE(r.wnd->row_count() == 13);
  CHECK(row_name(r, 0) == "Zoroaster III");
  CHECK(row_name(r, 1) == "Flammifer I");
  CHECK(row_name(r, 2) == "Liberty III");
  const game::Cosmos& cosmos = r.session.session().game->cosmos;
  for (int i = 1; i < r.wnd->row_count(); ++i) {
    CHECK(cosmos.planets.at(static_cast<std::size_t>(r.wnd->row_planet(i - 1))).acquired_day <=
          cosmos.planets.at(static_cast<std::size_t>(r.wnd->row_planet(i))).acquired_day);
  }
  // "Chamachies  (13 Planets Occupied, 13 Systems Controlled)".
  CHECK(StatusWnd::controlled_systems(cosmos, 0) == 13);
  // The pointer where the oracle's screenshot has it: the first row hovered.
  r.rig.move(555, 150);
  write_shot(r.rig, "status_list");
}

TEST_CASE("A click on a planet list row opens the planet or its star system", "[status]") {
  OA_REQUIRE_GAME_FILES(files);
  SECTION("right of the star picture: the planet screen") {
    StatusRig r(files);
    const game::PlanetIndex planet = r.wnd->row_planet(1);
    r.rig.move(300, 257);
    r.rig.click(300, 257);
    r.rig.pass();
    CHECK(r.session.session().game->cosmos.selected_planet == planet);
    CHECK(r.rig.wm.current_screen() == kScreenPlanet);
    CHECK(r.rig.wm.history().back() == kScreenStatus);
  }
  SECTION("on the star picture: the star system") {
    StatusRig r(files);
    const game::Cosmos& cosmos = r.session.session().game->cosmos;
    const game::StarIndex star = cosmos.planets.at(static_cast<std::size_t>(r.wnd->row_planet(0))).star;
    r.rig.move(80, 100);
    r.rig.click(80, 100);
    r.rig.pass();
    CHECK(cosmos.selected_star == star);
    CHECK(r.rig.wm.current_screen() == kScreenStarSystem);
  }
}

}  // namespace oa::test
