// The negotiation screen (screen 13, `docs/11-screens.md` "Diplomacy"):
// `NegWnd` (`windows.txt` TYPE 21 NEGSCREEN, ctor 0x3173c, vtable 0x85d9c)
// over the dialogue of `game/negotiation.h`: the alien's words in ALIENDIP
// (a `TextBox`), the player's options in PLAYERDIP (a `ListBox`), the
// selected system and the third parties' portraits in the left boxes. The
// mini map, the alien's portrait and its theme are `CosWnd`'s.
#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "game/negotiation.h"
#include "ui/gwindow.h"
#include "ui/list_box.h"
#include "ui/text_box.h"
#include "ui/winmgr.h"

namespace oa {

class NegWnd final : public GWindow {
 public:
  // The third-party box ((174,325)-(327,472), its click area strictly inside
  // (173,324)-(328,473)) and its portraits' centres (`0x866ae` / `0x866b8`).
  static constexpr Pane kThirdPartyBox{0xae, 0x145, 0x147, 0x1d8};
  static constexpr std::array<int, 5> kThirdPartyX{207, 297, 252, 207, 297};
  static constexpr std::array<int, 5> kThirdPartyY{358, 358, 403, 442, 442};
  // The selected system box.
  static constexpr Pane kSystemBox{7, 0x145, 0xa5, 0x163};

  NegWnd(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes) : GWindow(wm, record, shapes) {}

  // `NegWnd_handle_event` (0x317d8): 1 sets up the lists and opens the
  // dialogue; 4/5 in the third-party box pick a portrait
  // (`NegWnd_pick_third_party` 0x31e34); 0xc there gives help `tinyrace`;
  // 0x1c01 chooses the picked row (`NegWnd_choose_option` 0x31f20).
  bool handle_event(int type, int a, int b) override;
  // The window and the third parties' box (outside its rectangle), not
  // targets.
  void press_areas(std::vector<PressArea>& out) const override;
  // `NegWnd_draw` (0x31b34): the third parties (when there are some), the
  // selected system, the children.
  void draw(int mode) override;
  void draw_hilite(int mode) override { draw(mode); }

  // `NegWnd_on_star_selected` (0x3213c): the map's star click on screen 13.
  void on_star_selected();

  // The dialogue, once the screen was entered with a game.
  const game::Negotiation* negotiation() const { return negotiation_ ? &*negotiation_ : nullptr; }

 private:
  // `NegWnd_load_dialogue` (0x325a4): the sentences of the target's species
  // and of the player (`PLAYER.DIP`), kept by species (the original keeps
  // them by race slot, `docs/11-screens.md`).
  void load_dialogue(int species);
  // The dialogue's rows into PLAYERDIP and its words into ALIENDIP.
  void show_options();
  void show_alien_text();
  // `NegWnd_draw_option_row` (0x316d0).
  void draw_option_row(const Pane& row, const ListBox::Item& item, bool hilite);

  ListBox* list_ = nullptr;   // +0xab PLAYERDIP
  TextBox* alien_ = nullptr;  // +0xaf ALIENDIP
  GWindow* map_ = nullptr;    // +0xb3 COSMOSWnd
  std::vector<std::string> species_names_;
  std::array<std::optional<game::DipSentences>, 21> alien_sentences_;
  std::optional<game::DipSentences> player_sentences_;
  std::optional<game::Negotiation> negotiation_;
};

// Adds `NegWnd` (TYPE 21) to `factory`.
void add_negotiation_windows(WindowFactory& factory);

}  // namespace oa
