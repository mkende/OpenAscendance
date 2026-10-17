#include "screens/screens.h"

#include "screens/bat_control_wnd.h"
#include "screens/cos_wnd.h"
#include "screens/end_game_wnd.h"
#include "screens/fleet_wnd.h"
#include "screens/intel_wnd.h"
#include "screens/load_wnd.h"
#include "screens/neg_wnd.h"
#include "screens/new_game_wnd.h"
#include "screens/plan_wnd.h"
#include "screens/res_wnd.h"
#include "screens/ship_des_wnd.h"
#include "screens/start_game_wnd.h"
#include "screens/status_wnd.h"
#include "screens/system_display_stub.h"

namespace oa {

void add_screen_windows(WindowFactory& factory) {
  // First: the battle screen's classes replace it.
  add_system_display_stub(factory);
  add_star_map_windows(factory);
  add_star_system_windows(factory);
  add_load_windows(factory);
  add_new_game_windows(factory);
  add_start_game_windows(factory);
  add_planet_windows(factory);
  add_research_windows(factory);
  add_status_windows(factory);
  add_ship_design_windows(factory);
  add_fleet_windows(factory);
  add_negotiation_windows(factory);
  add_intel_windows(factory);
  add_end_game_windows(factory);
}

}  // namespace oa
