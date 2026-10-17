// The negotiation screen's dialogue (`NegWnd`, screen 13, the `negwnd` module,
// `docs/11-screens.md` "Diplomacy"): the `.dip` sentences, the
// third parties, the player's options and the alien's words, and the
// player's choices applied through the diplomacy rules (`game/raceneg.h`).
// The window (`screens/neg_wnd.h`) shows this state and forwards the clicks.
//
// Usage: `open` when the screen is entered (the target's pending action
// decides between proposing and answering), then `pick_third_party`,
// `star_selected` and `choose` as the player acts; after each call the
// window shows `options()` and `alien_text()` again.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "game/address.h"
#include "game/session.h"

namespace oa::game {

// The dialogue of a `.dip` file (`NegWnd_parse_dip` 0x326f8): the sentences
// of the 15 actions and the 30 responses, in the files' order (the labels
// are ignored), each as `fgets` read it (its line end kept) cut to 127
// characters.
struct DipSentences {
  static constexpr std::size_t kActions = 15;
  static constexpr std::size_t kResponses = 30;
  static constexpr std::size_t kMaxLength = 127;

  std::array<std::string, kActions> actions;
  std::array<std::string, kResponses> responses;
};

// Parses the sentences of a `RACExx.DIP` or `PLAYER.DIP` file. Throws
// `DataError` when a section is missing or holds too many sentences.
DipSentences parse_dip_sentences(std::span<const std::uint8_t> data);

class Negotiation {
 public:
  // The most third parties (the races but the player and the target).
  static constexpr std::size_t kMaxThirdParties = 5;
  // The alien's opening line when the player speaks first (response 29,
  // `DR_NoDipResponse`).
  static constexpr std::uint8_t kNoResponse = 29;

  // What a choice asks of the window.
  enum class Outcome : std::uint8_t {
    kGoodbye,   // leave the screen (message 3)
    kGoToStar,  // the selected star is set: open the star system (screen 16)
    kApplied,   // the galaxy may have changed: refresh the map (event 0xd)
  };

  // A row of the player's list: the sentence and its action or response.
  struct Option {
    std::uint8_t code = 0;
    std::string text;
  };

  // `species_names` are the 21 species' names (`0xf4578`); both must
  // outlive the negotiation.
  Negotiation(Session& session, std::span<const std::string> species_names);

  // Entering the screen (`NegWnd_handle_event` 0x317d8, event 1) with
  // `target` (`0x8675d`) and the sentences of its species and of the
  // player: the third parties listed (the first one selected), the used
  // options cleared; with no pending action of the target the player's
  // actions (`Race_list_actions`) and the alien's response 29, else the
  // player's replies (`Race_player_reply_options`) to the alien's pending
  // action, which the alien says.
  void open(RaceIndex target, const DipSentences& alien, const DipSentences& player);

  RaceIndex target() const { return target_; }
  // Whether the player answers the alien's proposal (`+0xb7` -1).
  bool replying() const { return replying_; }
  // The races known to the player and the target (`NegWnd_list_third_parties`
  // 0x31d78, `0x866c2`), and the index of the selected one (`+0xbf`).
  const std::vector<RaceIndex>& third_parties() const { return third_parties_; }
  int selected_third_party() const { return selected_; }
  // The selected third race, -1 when there is none (the list's 0xffff).
  std::int16_t third_race() const;
  // The rows of the player's list and whether their last refresh keeps the
  // list's scroll position.
  const std::vector<Option>& options() const { return options_; }
  bool keep_scroll() const { return keep_scroll_; }
  // What the alien says (`ALIENDIP`).
  const std::string& alien_text() const { return alien_text_; }

  // `NegWnd_pick_third_party` (0x31e34) after its hit test: selects third
  // party `index`; proposing, the actions are listed again (scroll kept).
  // Returns whether the selection changed (the window redraws).
  bool pick_third_party(int index);
  // `NegWnd_on_star_selected` (0x3213c): proposing, the last actions are
  // formatted again for the new selected star (scroll kept).
  void star_selected();
  // `NegWnd_choose_option` (0x31f20) with the row's `code`. Answering:
  // response 1 (accept `DiscoverAskHelp`) selects the alien's star;
  // another response applies the alien's action
  // (`Race_apply_diplomatic_action`), clears it and lists the player's
  // actions. Proposing: Goodbye (14) leaves; another action is marked used,
  // answered (`Race_respond_to_diplomatic_action`, which the alien says),
  // applied and the actions listed again. Either way the window then
  // proposes.
  Outcome choose(std::uint8_t code);

  // `NegWnd_format_sentence` (0x32368): sentence `code` of the alien's
  // (`player` false) or the player's dialogue, an action or a response,
  // with `%m` (the speaker's species), `%y` (the listener's), `%s` (the
  // alien's pending star, or the selected star when the player speaks) and
  // `%t` (the selected third party for the player's actions, else the race
  // of the target's pending argument) expanded.
  std::string format_sentence(std::uint8_t code, bool player, bool action) const;

 private:
  // `NegWnd_list_third_parties` (0x31d78).
  void list_third_parties();
  // `Race_list_actions` for the target and the selected third party, kept
  // as the last list (`0xf01cc`).
  void list_actions();
  // `NegWnd_show_player_actions` (0x32168): the last list's actions not yet
  // used.
  void show_actions(bool keep_scroll);
  const std::string& species_name(std::size_t race) const;

  Session& session_;
  std::span<const std::string> species_names_;
  const DipSentences* alien_ = nullptr;
  const DipSentences* player_ = nullptr;
  RaceIndex target_ = 0;
  bool replying_ = false;                            // +0xb7
  std::array<bool, DipSentences::kActions> used_{};  // 0xf1c50
  std::vector<RaceIndex> third_parties_;             // 0x866c2, count +0xc1
  int selected_ = 0;                                 // +0xbf
  std::vector<std::uint8_t> actions_;                // 0xf01cc, count 0xf01dc
  std::vector<Option> options_;
  bool keep_scroll_ = false;
  std::string alien_text_;
};

}  // namespace oa::game
