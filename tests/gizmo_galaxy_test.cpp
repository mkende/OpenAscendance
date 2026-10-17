// (placeholder: filled by the coverage work; see docs/12-oracle.md)

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "core_session.h"
#include "game/session.h"
#include "game_data.h"
#include "replay_rig.h"
#include "screens/bat_mode_wnd.h"

namespace oa {

TEST_CASE("scratch system items", "[scratch]") {
  OA_REQUIRE_GAME_FILES(files);
  const char* save = std::getenv("SCRATCH_SAVE");
  const int star = std::atoi(std::getenv("SCRATCH_STAR"));
  test::CoreGameSession session(files, test::read_file(save));
  test::UiRig rig(files, &session);
  rig.wm.restore_window_state();
  game::Cosmos& c = session.session().game->cosmos;
  c.selected_star = static_cast<game::StarIndex>(star);
  rig.move(400, 460);
  rig.wm.set_screen(16, true);
  for (int i = 0; i < 5; ++i) rig.pass();
  BatModeWnd& v = dynamic_cast<BatModeWnd&>(*rig.wm.find_window("BatModeWnd"));
  for (const BatModeWnd::Item& it : v.items()) {
    std::printf("kind %d obj %d at %d %d\n", static_cast<int>(it.kind), it.object, it.x + v.pane().x0, it.y + v.pane().y0);
  }
}

}  // namespace oa
