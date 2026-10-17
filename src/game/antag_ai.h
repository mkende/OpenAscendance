// The Antagonizer's strategic AI (`ANTAG.EXE` v1.6.10,
// `docs/10-antagonizer-ai.md` "Strategic AI turn and production planner",
// "Ship orders and fleet maintenance", "Ship build start and ship design").
#pragma once

#include "game/address.h"
#include "game/session.h"
#include "game/state.h"

namespace oa::game {

// The race's day with the Antagonizer AI (`Race_turn_update` ANTAG 0x3b220).
void antag_race_turn_update(Session& session, Race& race);

// Gives an AI ship its orders from the last star evaluation
// (`Race_ai_ship_orders` ANTAG 0x3ebdc): ships in a system heading for a
// star are re-planned every day, then a colony site, else the best scored
// star for the ship's role (colony, invasion, war). Also called by the
// battle turn, which then reads the work arrays of the last AI race turn.
void antag_race_ai_ship_orders(Session& session, Race& race, Ship& ship);

// Gives an idle ship its orders (`Race_ai_ship_orders` ANTAG 0x3ebdc); also
// called by the Antagonizer's battle turn.
void antag_race_ai_ship_orders(Session& session, Race& race, Ship& ship);

}  // namespace oa::game
