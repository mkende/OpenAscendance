// The research of the day (`research.cpp`, `docs/09-simulation.md`
// "Research"). The research state is `Game::research`; `research_load` (the
// reset) is in `game/race.h`.
#pragma once

#include <cstdint>

#include "game/address.h"
#include "game/session.h"
#include "game/state.h"

namespace oa::game {

// Adds each race's research of the day to its project and completes it
// (event 0 and the basics check for the player); a race without a project
// loses its points (`Research_daily_totals` 0x41f5c).
void research_daily_totals(Session& session);

// Posts event 21 once the player knows items 0..3 (`Research_check_basics`
// 0x4211c, flag `Research::basics_done`).
void research_check_basics(Session& session);
// Tells whether `race` knows items 0..3 (`Research_knows_basics` 0x42160).
bool research_knows_basics(const Research& research, RaceIndex race);

// Tells whether `race` may research `item`: every prerequisite known; also
// true for an item the race already knows (callers test that first)
// (`Research_can_research` 0x421ac).
bool research_can_research(const Research& research, RaceIndex race, std::uint16_t item);

// Returns the race's research points per day, without the Hostile bonus
// (`Research_race_rate` 0x42250).
std::int32_t research_race_rate(const Session& session, RaceIndex race);
// Returns `(cost - progress - 1) / rate + 1`, 0xffff without research,
// at least 1 (`Research_days_left` 0x42480).
std::int32_t research_days_left(std::int32_t cost, std::int32_t progress, std::int32_t rate);

// Grants `race` a random unknown item other than its project (Xeno dig;
// one draw when any is left; event 22 for the player) (`Research_grant_random`
// 0x422f4).
void research_grant_random(Session& session, RaceIndex race, const Planet& planet);

}  // namespace oa::game
