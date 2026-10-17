#include "screens/screens.h"

#include "screens/load_wnd.h"
#include "screens/new_game_wnd.h"
#include "screens/start_game_wnd.h"

namespace oa {

void add_screen_windows(WindowFactory& factory) {
  add_load_windows(factory);
  add_new_game_windows(factory);
  add_start_game_windows(factory);
}

}  // namespace oa
