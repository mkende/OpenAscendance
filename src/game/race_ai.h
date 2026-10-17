// The standard strategic AI (`race.cpp`, `docs/09-simulation.md` "Standard
// strategic AI"), also the player's auto-management (self-managed planets,
// auto-research). "AI race" means a race other than the player, or any race
// in autoplay.
#pragma once

#include <cstdint>

#include "game/address.h"
#include "game/session.h"
#include "game/state.h"

namespace oa::game {

// Tells whether `race` is run by the AI (not the player, or autoplay).
inline bool race_is_ai(const Session& session, const Race& race) {
  return race.index != session.player_race || session.autoplay;
}

// The AI part of the race's day (`Race_ai_turn` 0x39ec4): drift, mood,
// diplomacy, targets, planet management, research choice, ability, ship
// orders.
void race_ai_turn(Session& session, Race& race);

// Rebuilds the target stars (`Race_build_targets` 0x39ff8).
void race_build_targets(Session& session, Race& race);
// Drifts the attitudes toward the other races (`Race_update_attitude_drift`
// 0x3a128).
void race_update_attitude_drift(Session& session, Race& race);
// The mood swing every 10 days (`Race_bio_rhythm` 0x3a39c).
void race_bio_rhythm(Session& session, Race& race);

// Chooses the project of an idle planet (`Race_ai_manage_planet` 0x3a5a0).
void race_ai_manage_planet(Session& session, Race& race, Planet& planet);
// The Shipyard / ship / orbital part of it: returns 1 (done), 2 or 0
// (`Race_ai_choose_orbital_project` 0x3abc8).
std::int32_t race_ai_choose_orbital_project(Session& session, Race& race, Planet& planet);
// Starts a special building; returns whether it did
// (`Race_ai_planet_specials` 0x3b0bc).
bool race_ai_planet_specials(Session& session, Race& race, Planet& planet);
// Returns 0 industry, 1 research, 2 prosperity, 3 housing
// (`Race_ai_planet_focus` 0x3b334).
std::int32_t race_ai_planet_focus(const Session& session, const Race& race, const Planet& planet);

// Chooses the research project (`Race_ai_choose_research` 0x3b488).
void race_ai_choose_research(Session& session, Race& race);
// Gives an idle ship its orders (`Race_ai_ship_orders` 0x3b5e8).
void race_ai_ship_orders(Session& session, Race& race, Ship& ship);

// Designs the new ship `ship` built at `planet` (`Race_ai_design_ship`
// 0x3b7c8 and its hull fillers 0x3ba70..0x3c194).
void race_ai_design_ship(Session& session, Race& race, Ship& ship, Planet& planet);
// Allocates the ship ordered at `planet` (`Race_ai_new_ship` 0x3c258);
// nothing when no slot is free.
Ship* race_ai_new_ship(Session& session, Race& race, Planet& planet);
// Tells whether the next design is a colony ship (one draw unless decided
// by the ranking or the day) (`Race_ai_wants_colony_ship` 0x3c428).
bool race_ai_wants_colony_ship(Session& session, const Race& race);
// Returns the planet of `star` to colonise, `kNone` if none
// (`Race_ai_choose_colony_site` 0x3c4d4).
PlanetIndex race_ai_choose_colony_site(const Session& session, const Race& race, const Star& star);

}  // namespace oa::game
