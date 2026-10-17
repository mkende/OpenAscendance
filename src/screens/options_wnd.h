// The remake's options (`docs/11-screens.md`, "Remake additions"): an
// "Options" button on the opening menu, between "Save Game" and "Exit to
// DOS", the seven original buttons made shorter to make room, and the
// options screen it opens (the settings of `ui/options.h` and the sound
// mute flags of `ascend.cfg`).
//
// While a trace of the original is replayed (a tutorial), the opening menu
// takes its original layout back, the one the trace's clicks were recorded
// on.
#pragma once

#include <array>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

#include "gfx/vfx.h"
#include "ui/options.h"
#include "ui/winmgr.h"

namespace oa {

// The options screen's number (one the original does not use).
inline constexpr int kScreenOptions = 30;

// The opening menu's buttons of `windows.txt`, top to bottom, and the place
// of the Options button among them.
inline constexpr std::array<std::string_view, 7> kMenuButtons{"INTRO", "PLAYTUTORIAL", "FRESHGAME", "RESUME",
                                                              "LOAD",  "SAVE",         "EXITTODOS"};
inline constexpr std::size_t kOptionsButtonIndex = 6;

// Returns `count` panes spread over the vertical span of `original` (the
// first pane's top to the last one's bottom, at the first pane's x), the
// gap between them that between the first two of `original`; the rows left
// over by the division go to the first panes. `original` has at least two
// panes.
std::vector<Pane> spread_menu_buttons(std::span<const Pane> original, int count);

struct OptionsContext {
  // The settings the screen shows and its OK changes; must outlive the
  // window manager.
  RemakeOptions* options = nullptr;
  // `options.cfg`, written by OK; empty: not written.
  std::filesystem::path file;
  // The program's window; null for none (headless runs): the display
  // settings then only change `options`.
  DisplayControl* display = nullptr;
};

// Adds the Options button to the opening menu (and its layout to the
// menu's entry) and the options screen's windows. Throws `DataError` when
// a menu button or the help window is missing from `windows.txt`.
void install_options(WinMgr& wm, OptionsContext context);

}  // namespace oa
