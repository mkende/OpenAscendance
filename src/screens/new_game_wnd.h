// The new-game set-up (`status.cpp`, `NewGameWnd`, `windows.txt` TYPE 18,
// ctor 0x4d4fc, vtable 0x85f78: screen 3): the player's species (RACELIST,
// its portrait, name and blurb), the star density, species count and
// atmosphere buttons (messages 501, 503, 505), the colour bar and the "Begin
// New Game" box (`docs/07-ui-framework.md`, "Screens").
#pragma once

#include <array>
#include <string>

#include "ui/game_session.h"
#include "ui/winmgr.h"

namespace oa {

class ListBox;

class NewGameWnd final : public GWindow {
 public:
  static constexpr int kSpecies = 21;
  static constexpr int kColors = 7;
  // The messages of the setting buttons (GALAXY_MORE, PLAYERS_MORE,
  // DIFFICULTYMORE).
  static constexpr int kMessageDensity = 501;
  static constexpr int kMessageSpeciesCount = 503;
  static constexpr int kMessageAtmosphere = 505;
  // The colour bar and the "Begin New Game" box, inclusive screen pixels
  // (the hit tests are strict inside them).
  static constexpr Pane kColorBar{0x17b, 0x1a4, 0x1ed, 0x1d8};
  static constexpr Pane kBeginBox{0x1f6, 0x17d, 0x278, 0x1d8};

  NewGameWnd(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes);

  // The settings as chosen so far (`+0xb3..+0xbb`).
  const NewGameSettings& settings() const { return settings_; }

  // `NewGameWnd_handle_event` (0x4dd5c): 1 saves `resume.gam`, fills
  // RACELIST and resets the settings and the galaxy; 4/5 pick a colour or
  // begin the game (screen 12); 0xc gives the colour bar's and the box's
  // help; 501/503/505 cycle a setting; 0x1c01 picks the species.
  bool handle_event(int type, int a, int b) override;
  // The window, not a target, and its colour bar and Begin box (outside
  // its rectangle), targets.
  void press_areas(std::vector<PressArea>& out) const override;
  // `NewGameWnd_draw` (0x4d5d0): the "Begin New Game" box and the list's
  // title, and in mode 0 the species panel, the settings' summary and the
  // colour bar.
  void draw(int mode) override;
  void draw_hilite(int mode) override { draw(mode); }

 private:
  // `CosWnd_build_items` of the star map after a new galaxy (the star
  // density and species count changes; not the species pick).
  void rebuild_map();
  // `NewGameWnd_draw_galaxy_preview` (0x4db24).
  void draw_begin_box();
  // `NewGameWnd_draw_species_item` (0x4ca20): a RACELIST row.
  void draw_species_item(const Pane& row, int species, bool hilite);
  // Sets the shape frame of setting button `index` (its first WINITEM) and
  // draws it.
  void set_button_frame(int index, int frame, bool redraw);
  bool in_begin_box(int x, int y) const;

  ListBox* list_ = nullptr;
  std::array<std::string, kSpecies> blurbs_;
  NewGameSettings settings_;
};

// Adds the class of this screen to `factory`.
void add_new_game_windows(WindowFactory& factory);

}  // namespace oa
