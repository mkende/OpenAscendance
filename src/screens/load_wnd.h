// The load/save and tutorial lists (`loadwin.cpp`, class `LoadWnd`,
// `windows.txt` TYPE 11 LOADSCREEN, ctor 0x30920, vtable 0x85d08): the title
// bar of screens 2 (save), 4 (load) and 15 (tutorials) over its LOADLIST
// list box: the 20 save slots `01.sav`..`20.sav`, or the tutorials of
// `tutindex.txt` (`recon/notes/ui.md`, "LoadWnd").
#pragma once

#include <string>
#include <vector>

#include "ui/game_session.h"
#include "ui/gwindow.h"
#include "ui/list_box.h"
#include "ui/winmgr.h"

namespace oa {

class LoadWnd final : public GWindow {
 public:
  // The mode (`+0xaf`), from the screen it is activated on.
  enum class Mode { kLoad = 0, kSave = 1, kTutorial = 2 };

  // The rows' heights (`+0x8c9` of the list).
  static constexpr int kSaveRowHeight = 0x55;
  static constexpr int kTutorialRowHeight = 0x3c;
  // The most tutorials of `tutindex.txt`.
  static constexpr int kMaxTutorials = 7;

  // A tutorial of `tutindex.txt` (0xe0 bytes at `+0x9c0`).
  struct Tutorial {
    std::string name;         // `<name>.tsv` / `<name>.bin`
    std::string description;  // the list's text
    int number = 0;           // 1-based
  };

  LoadWnd(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes) : GWindow(wm, record, shapes) {}

  Mode mode() const { return mode_; }
  const std::vector<SaveSlot>& slots() const { return slots_; }
  const std::vector<Tutorial>& tutorials() const { return tutorials_; }

  // `LoadWnd_handle_event` (0x30988): 1 fills the list for the screen's
  // mode, the keys `1`..`9` pick the row of that index (`1` is the second
  // row; the save list takes 1..9, the others 1..8), 0x1c01 (a row picked)
  // saves into the slot and goes back, loads a used slot and goes to the
  // star map, or starts a tutorial (`WinMgr::start_tutorial`).
  bool handle_event(int type, int a, int b) override;
  // `LoadWnd_draw` (0x30d2c): the title bar (wiped with 0x10 to load, 0x73
  // to save, left as the background for the tutorials) and the list.
  void draw(int mode) override;
  void draw_hilite(int mode) override { draw(mode); }

 private:
  // `LoadWnd_read_headers` (0x30dc8) / `LoadWnd_load_tutindex` (0x30ef0).
  void read_headers();
  void load_tutindex();
  void pick(int row);
  // `draw_save_game_entry` (0x301b0): a save slot's row.
  void draw_save_entry(const Pane& row, int index, bool hilite);
  // `LoadWnd_draw_tutorial_item` (0x30878).
  void draw_tutorial_entry(const Pane& row, int index, bool hilite);

  Mode mode_ = Mode::kLoad;
  ListBox* list_ = nullptr;  // +0xab
  std::vector<SaveSlot> slots_;
  std::vector<Tutorial> tutorials_;
};

// Adds `LoadWnd` (TYPE 11) to `factory`.
void add_load_windows(WindowFactory& factory);

}  // namespace oa
