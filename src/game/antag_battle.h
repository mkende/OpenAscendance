// The Antagonizer's battle turn and tactical AI (`docs/10-antagonizer-ai.md`
// "Tactical combat AI and combat-side rules"): `Race_battle_turn` with the
// Antagonizer's rules (`game/battle.h`), its tactical AI and helpers.
#pragma once

#include <cstdint>

#include "game/battle.h"
#include "game/session.h"
#include "game/state.h"

namespace oa::game {

// One battle turn of `race` at `star` (`Race_battle_turn` ANTAG 0x406c4);
// returns whether the race is still active.
bool antag_race_battle_turn(Session& session, Race& race, Star& star, BattleAction& action);

// The Antagonizer's tactical AI (`Race_ai_tactical` ANTAG 0x41268): never
// flees; colony ships stay out of fights; Phase Bombs; every hostile planet
// whose orbit holds anything is attacked; surface Tractor Beams; ships stand
// off at their weapon reach. Returns -1 (acted), 0 or 2.
std::int32_t antag_race_ai_tactical(Session& session, Race& race, Star& star, const BattleLists& lists,
                                    BattleAction& action);

// Returns the largest preferred firing distance of the ship's usable
// gizmos against ships (`mode` 1) or planets (2), -1 when none
// (`Ship_ai_engage_distance` ANTAG 0x49b68).
std::int32_t ship_ai_engage_distance(const Session& session, const Ship& ship, std::int32_t mode);

// Returns 95% of the longest range of the planet's built orbital weapons
// with shots left, -1 when none (`Planet_ai_orbital_weapon_reach` ANTAG
// 0x35ed8).
std::int32_t planet_ai_orbital_weapon_reach(const Session& session, const Planet& planet);

// The Antagonizer's evasive gizmos (`Ship_ai_use_evasive_gizmo` ANTAG
// 0x43184): the standard list without the Recaller, whose target kind
// stays in the parallel list (so the Brunswik Dissipator is used untargeted).
bool antag_ship_ai_use_evasive_gizmo(Session& session, Ship& ship, Ship& threat);

}  // namespace oa::game
