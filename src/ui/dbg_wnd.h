// The developer's debug window (`winmgr.cpp`, class `DbgWnd`, `windows.txt`
// TYPE 13 `WMGRDEBUG`, ctor 0x559a4, vtable 0x8602c; `docs/07-ui-framework.md`
// "Hotkeys"): run modally by Alt+W in the developer mode, a list
// (`DBGWNDLIST`) of the current screen's windows (W), the memory blocks (M),
// the shape cache (C), the 256 palette entries (P) or the music tunes (N),
// and EXIT. Picking a tune plays it; a right click on a palette row makes it
// the colour of the WMGR_MAPFOCUS remap's entry 0xf2; an Alt + left click
// anywhere scrolls the palette to the colour under the pointer.
#pragma once

#include <cstdint>

#include "ui/gwindow.h"

namespace oa {

class ListBox;

class DbgWnd final : public GWindow {
 public:
  // The buttons' messages (W, M, C, P, N; EXIT sends `kEventOk`) and the
  // window manager's palette pick.
  enum Message : int {
    kMessageWindows = 1002,
    kMessageMemory = 1003,
    kMessageCache = 1004,
    kMessagePalette = 1005,
    kMessageTunes = 1006,
    kMessagePalettePick = 0x3f1,
  };
  // What the list shows (`+0xbf`).
  enum class Mode : std::uint8_t { kWindows = 0, kMemory = 1, kCache = 2, kPalette = 3, kTunes = 4 };

  DbgWnd(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes);

  Mode mode() const { return mode_; }

  // `DbgWnd_handle_event` (0x55a1c): the opening of the modal loop lists
  // the windows; the buttons switch the list (when it shows something
  // else); EXIT ends the loop.
  bool handle_event(int type, int a, int b) override;
  // `DbgWnd_draw` (0x55e18): the mode's title in the top of the client
  // rectangle, the children.
  void draw(int mode) override;
  void draw_hilite(int mode) override { draw(mode); }

 private:
  ListBox& list() const;
  // Clears the list and fills it for `mode` (`DbgWnd_list_windows` 0x56278,
  // `DbgWnd_list_memory` 0x55f3c, `DbgWnd_list_cache` 0x56340,
  // `DbgWnd_list_palette` 0x56610, `DbgWnd_list_tunes` 0x56654).
  void fill(Mode mode);
  void list_windows(ListBox& list) const;
  void list_memory(ListBox& list) const;
  void list_cache(ListBox& list) const;
  void list_tunes(ListBox& list) const;

  Mode mode_ = Mode::kWindows;
  // `cX0`..`cY1` of `windows.txt`: the title's rectangle is its top 50
  // rows.
  Pane client_;
};

}  // namespace oa
