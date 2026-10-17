// The front-end screens built on the window toolkit (`docs/07-ui-framework.md`,
// "Screens"): the load/save and tutorial lists (screens 2, 4, 15), the new
// game set-up (3), the species history (12), the planet status list (10),
// the planet screen (17) and the research screen (20).
#pragma once

#include "ui/winmgr.h"

namespace oa {

// Adds the screens' window classes to `factory` (by `windows.txt` TYPE).
void add_screen_windows(WindowFactory& factory);

}  // namespace oa
