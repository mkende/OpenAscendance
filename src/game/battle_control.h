// The turn loop of the System Display (`BatControlWnd`, `docs/08-combat.md`
// "Turn order in the System Display"): the races present take turns, the
// player first; each race's turn is `Race_battle_turn` (the variant's AI),
// whose chosen action is checked, animated by the user interface, then
// applied. Without the player the same rules run in `star_resolve_combat`.
//
// Use (the user interface animates between `run`/`act` and `finish`):
//
//     BattleControl control;
//     control.open(session, star);           // the player's turn
//     control.act(session, {BattleActor::kShip, ship});  // a filled action block
//     while (control.pending()) {            // animate control.pending(), then
//       control.finish(session);             // apply it and let the others play
//     }
//     control.start_auto(session);           // or control.pass(session)
//     ...
//     control.close(session);
#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "game/battle.h"
#include "game/session.h"
#include "game/state.h"

namespace oa::game {

class BattleControl {
 public:
  // Opens the display on `star` (message 1 of `BatControlWnd_handle`
  // 0x10b24): the present races, cursor 0, every result -1, Auto off; the
  // star becomes the selected one (`Cosmos::selected_star`, as the star map
  // does before opening it); the
  // screen becomes the System Display (16, where the tactical AI also plays
  // the player's ships).
  void open(Session& session, Star& star);
  // Leaves the display: restores the previous screen.
  void close(Session& session);

  // The player's action (move, gizmo, planet weapon) whose action block is
  // filled: checked (`Ship_do_action` / `Planet_fire_weapon` with apply 0)
  // unless `check` is false (the untargeted gizmos of the list,
  // `BatControlWnd_handle` 0x1c01), then pending until `finish`.
  void act(Session& session, BattleAction action, bool check = true);
  // Pass (message 0x327) and Auto (0x328): allowed on the player's turn
  // (cursor 0, Auto off, the player present, no action pending). Return
  // whether they ran.
  bool pass(Session& session);
  bool start_auto(Session& session);

  // The checked action waiting for its animation, if any.
  const std::optional<BattleAction>& pending() const { return pending_; }
  // Applies the pending action and runs the next turns
  // (`BatControlWnd_finish_action` 0x12238).
  void finish(Session& session);
  // Applies every action at once, as closing the display during an effect
  // does.
  void finish_all(Session& session);
  // The two halves of `finish`, for the display, which redraws its list in
  // between: the pending action applied, then `BatControlWnd_run_turns`
  // 0x1229c (the next races' turns until one chooses an action, left
  // pending, or the round ends without Auto).
  void apply_pending(Session& session);
  void run_turns(Session& session);
  // The rounds ended so far (`BatControlWnd_new_round` 0x12368 calls), which
  // the display redraws after.
  std::uint32_t rounds() const { return rounds_; }

  bool auto_on() const { return auto_; }
  // The race whose turn it is (`+0xd4`, an index into the present races).
  std::int16_t cursor() const { return cursor_; }
  // Tells whether it is the player's turn (cursor 0, nothing pending).
  bool player_turn() const { return cursor_ == 0 && !pending_ && !auto_; }

 private:
  // `BatControlWnd_new_round` 0x12368: Auto stays on while a race acted, the
  // races listed again, cursor 0, every race's moves refilled; returns
  // whether the turns go on.
  bool new_round(Session& session);
  void list_races(const Session& session);

  StarIndex star_ = kNone;
  std::int16_t cursor_ = 0;                      // +0xd4
  std::int32_t race_count_ = 0;                  // +0xd6
  std::array<std::int16_t, kMaxRaces> races_{};  // +0xda
  std::array<bool, kMaxRaces> results_{};        // +0xe8 (-1: still active)
  bool auto_ = false;                            // +0x104
  std::optional<BattleAction> pending_;          // +0xab / +0xac
  std::int16_t previous_screen_ = 12;
  std::uint32_t rounds_ = 0;
};

}  // namespace oa::game
