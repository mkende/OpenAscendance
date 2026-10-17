#include "screens/screens.h"

#include "screens/cos_wnd.h"
#include "screens/load_wnd.h"
#include "screens/new_game_wnd.h"
#include "screens/plan_wnd.h"
#include "screens/res_wnd.h"
#include "screens/start_game_wnd.h"
#include "screens/status_wnd.h"

namespace oa {

void add_screen_windows(WindowFactory& factory) {
  add_star_map_windows(factory);
  add_load_windows(factory);
  add_new_game_windows(factory);
  add_start_game_windows(factory);
  add_planet_windows(factory);
  add_research_windows(factory);
  add_status_windows(factory);
}

}  // namespace oa
