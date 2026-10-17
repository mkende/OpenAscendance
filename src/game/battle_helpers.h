// Helpers shared by the battle turns and tactical AIs of the variants
// (`game/battle.cpp`, `game/antag_battle.cpp`): pieces of `Race_ai_tactical`
// (PATCH 0x3d2ac, ANTAG 0x41268) that both builds share.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "game/battle.h"
#include "game/session.h"
#include "game/state.h"

namespace oa::game::battle_detail {

// Distance added for ships not in space (PATCH 0x821d4, ANTAG 0x92338), the
// minimum approach distance (0x821d8 / 0x9233c) and the start of the
// nearest-target searches.
inline constexpr float kNotInSpacePenalty = 10000.0f;
inline constexpr float kApproachMinimum = 600.0f;
inline constexpr float kFarAway = 99999.0f;

// `ship_ai_select_weapon` target kinds.
inline constexpr std::uint16_t kSelectLane = 0;
inline constexpr std::uint16_t kSelectPlanet = 1;
inline constexpr std::uint16_t kSelectShip = 3;

// Tells whether a slot of `ship` holds `gizmo` (ANTAG `Ship_has_gizmo`
// 0x4a534, inlined in PATCH).
bool carries(const Ship& ship, std::int8_t gizmo);
// Tells whether the owner of `ship` is in `hostile_mask`.
bool hostile_owner(std::uint8_t hostile_mask, const Ship& ship);
// Sets the action block to a move (0) or gizmo use (1).
void set_action(Ship& ship, std::uint8_t action, RawAddress slot, std::uint8_t target_kind, RawAddress target);
BattleAction ship_action(const Cosmos& cosmos, const Ship& ship);
// The distance the tactical AIs rank targets by: `|a - b|` stored as a
// float, plus 10000 for a ship not in space.
float target_distance(const Vec3& a, const Vec3& b, const Ship& target);
// Tells whether the hostile ship `enemy` could hit `ship` (its action block
// is probed then restored).
bool can_hit(Session& session, Ship& enemy, const Ship& ship);
// The firing loop: the first own ship (of `lists.ships` accepted by
// `eligible`) in space with power and a ready weapon able to hit a hostile
// ship fires. `unarmed` gets the last such ship without a ready weapon,
// `out_of_range` the last armed one that could not fire.
bool ships_fire(Session& session, const BattleLists& lists, const std::vector<ShipIndex>& present,
                std::uint8_t hostile_mask, BattleAction& action, Ship*& unarmed, Ship*& out_of_range,
                bool (*eligible)(const Ship&));
// An out-of-range armed ship uses its Accutron (power > 12, not yet
// active); an unarmed one its Replenisher (power > 13). The action is
// reported even when the check fails; returns whether one passed.
bool use_support_gizmos(Session& session, Ship* out_of_range, Ship* unarmed, BattleAction& action);
// Tries the evasive gizmos `gizmos` (with their target kinds `kinds`) on
// `ship` threatened by `threat` (`Ship_ai_use_evasive_gizmo`).
bool use_evasive_gizmo(Session& session, Ship& ship, Ship& threat, std::span<const std::int8_t> gizmos,
                       std::span<const std::uint8_t> kinds);

}  // namespace oa::game::battle_detail
