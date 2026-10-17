// The battle turn of a race at a star (`race.cpp`, `docs/08-combat.md` "AI
// in battle"): its standing orders and the standard tactical AI. One call
// chooses at most one action; the caller (`star_resolve_combat`, or the
// System Display's turn loop) checks and applies it with `ship_do_action` /
// `planet_fire_weapon`. Everything here is SDL-free; the System Display
// (`BatControlWnd`) drives the same functions.
#pragma once

#include <cstdint>
#include <vector>

#include "game/address.h"
#include "game/session.h"
#include "game/state.h"

namespace oa::game {

// The action a race chose (`Race_battle_turn`'s out parameter).
enum class BattleActor : std::uint8_t { kNone = 0, kPlanet = 2, kShip = 3 };
struct BattleAction {
  BattleActor kind = BattleActor::kNone;
  std::int16_t index = kNone;  // planet index or ship slot
};

// One turn of `race` at `star` (Standard rules, `Race_battle_turn`
// 0x3c6d8): resets its ships' moves, recall, hostility, the tactical AI or
// the standing orders. Returns whether the race is still active (-1).
bool race_battle_turn(Session& session, Race& race, Star& star, BattleAction& action);

struct BattleLists;
// The variant-specific parts of `Race_battle_turn` (PATCH 0x3c6d8, ANTAG
// 0x406c4: same structure).
struct BattleTurnRules {
  // The tactical AI, returning -1 (acted), 0 or 2.
  std::int32_t (*tactical)(Session&, Race&, Star&, const BattleLists&, BattleAction&);
  // The orders of an idle AI ship.
  void (*ship_orders)(Session&, Race&, Ship&);
  // ANTAG: an AI away from home recalls its ships at any threat at home
  // (PATCH also weighs the forces here).
  bool recall_at_any_threat;
  // ANTAG: only the tactical AI's -1 ends the turn; 0 and 2 go on with the
  // standing orders (PATCH: 0 ends it).
  bool tactical_zero_continues;
  // ANTAG: at a guarded lane end, only the ships that are not colony ships
  // and carry an Invasion Module wait (PATCH: every AI ship).
  bool only_invasion_ships_wait;
};

// `Race_battle_turn` with the variant's `rules`.
bool race_battle_turn_with(Session& session, Race& race, Star& star, BattleAction& action,
                           const BattleTurnRules& rules);

// `BattleLists::races` entry bits besides the race index (bits 0..2).
enum BattleRaceFlag : std::uint8_t {
  kBattleRaceShips = 8,      // has ships at the star
  kBattleRaceColony = 0x10,  // has a colony at the star
  kBattleRaceHostile = 0x20,
};

// What `race_battle_turn` gathers for the tactical AI (its stack arrays).
struct BattleLists {
  // The races present at the star (ships or colony, slots 0..6 in order):
  // index | `BattleRaceFlag`s.
  std::vector<std::uint8_t> races;
  // The race's ships at the star with power that are not being built,
  // in `cosmos_ships_at_star` order.
  std::vector<ShipIndex> ships;
  // The race's planets of the star, in orbit order.
  std::vector<PlanetIndex> planets;
  // Sums over `ships` of the power and moves (low 16 bits, as the
  // original's shorts).
  std::int16_t power = 0;
  std::int16_t moves = 0;
};

// The standard tactical AI (`Race_ai_tactical` 0x3d2ac); returns -1 (an
// action, or an invasion, was done), 0 (pass) or 2 (go on with the standing
// orders). `action` may be set even when 0 is returned (original
// behaviour: a failed Accutron/Replenisher check leaves it set).
std::int32_t race_ai_tactical(Session& session, Race& race, Star& star, const BattleLists& lists, BattleAction& action);

// Tries the evasive gizmos of the table 0x867ec on `ship` threatened by
// `threat` when the ship has more than half its day's power
// (`Ship_ai_use_evasive_gizmo` 0x3ec78); returns whether one passed its
// check (the action block is then set to it).
bool ship_ai_use_evasive_gizmo(Session& session, Ship& ship, Ship& threat);

}  // namespace oa::game
