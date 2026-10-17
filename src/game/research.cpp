#include "game/research.h"

#include <array>
#include <cmath>
#include <vector>

#include "core/fixed_width.h"
#include "game/float_math.h"

namespace oa::game {

namespace {

// Returns `trunc((points + 1) ** 0.85)` for non-zero points, else 0 (pow
// 0x73f30, `__CHP`).
std::int32_t research_curve(std::int32_t points) {
  return points == 0 ? 0 : chop(std::pow(static_cast<double>(points + 1), 0.85));
}

bool knows(const Research& research, std::size_t item, RaceIndex race) {
  return ((research.items[item].known_mask >> (race & 0x1f)) & 1) != 0;
}

}  // namespace

void research_daily_totals(Session& session) {
  const Cosmos& cosmos = session.game->cosmos;
  Research& research = session.game->research;
  std::array<std::uint16_t, kMaxRaces> points{};
  for (std::int32_t p = 0; p < cosmos.planet_count; ++p) {
    const Planet& planet = cosmos.planets[static_cast<std::size_t>(p)];
    if (planet.owner < cosmos.race_count) {
      // Hostile: +1 for a weak AI planet (any race but the player).
      if (planet.owner != session.player_race && cosmos.atmosphere == Atmosphere::kHostile &&
          static_cast<std::uint16_t>(planet.research) < 15) {
        points[planet.owner] = wrap_u16(points[planet.owner] + 1);
      }
      points[planet.owner] = wrap_u16(points[planet.owner] + planet.research);
    }
  }
  for (std::int32_t r = 0; r < cosmos.race_count; ++r) {
    const auto race = static_cast<std::size_t>(r);
    if (research.project[race] == -1) {
      // A race without a project loses its points.
      research.progress[race] = 0;
      continue;
    }
    research.progress[race] = wrap_u16(research.progress[race] + research_curve(points[race]));
    const auto item = static_cast<std::uint16_t>(research.project[race]);
    if (research.items[item].cost <= research.progress[race]) {
      const auto bit = static_cast<std::uint8_t>(1u << (r & 0x1f));
      research.items[item].known_mask |= bit;
      research.items[item].unseen_mask |= bit;
      research.project[race] = -1;
      research.progress[race] = 0;
      if (r == session.player_race) {
        session.post_event(EventCode::kResearchDone, item);
        research_check_basics(session);
      }
    }
  }
}

void research_check_basics(Session& session) {
  Research& research = session.game->research;
  if (research.basics_done == 0 && research_knows_basics(research, session.player_race)) {
    session.post_event(EventCode::kBasicsKnown);
    research.basics_done = -1;
  }
}

bool research_knows_basics(const Research& research, RaceIndex race) {
  return knows(research, 0, race) && knows(research, 1, race) && knows(research, 2, race) && knows(research, 3, race);
}

bool research_can_research(const Research& research, RaceIndex race, std::uint16_t item) {
  if (knows(research, item, race)) {
    return true;  // the original returns -1 for a known item
  }
  for (const std::uint8_t prereq : research.items[item].prereqs) {
    if (prereq == 0xff) {
      return true;
    }
    if (!knows(research, prereq, race)) {
      return false;
    }
  }
  return true;
}

std::int32_t research_race_rate(const Session& session, RaceIndex race) {
  const Cosmos& cosmos = session.game->cosmos;
  std::int32_t points = 0;
  for (std::int32_t p = 0; p < cosmos.planet_count; ++p) {
    const Planet& planet = cosmos.planets[static_cast<std::size_t>(p)];
    if (planet.owner == race) {
      points += static_cast<std::uint16_t>(planet.research);
    }
  }
  return research_curve(points);
}

std::int32_t research_days_left(std::int32_t cost, std::int32_t progress, std::int32_t rate) {
  std::int32_t days = 0xffff;
  if (rate > 0) {
    days = (cost - progress - 1) / rate + 1;
  }
  return days < 1 ? 1 : days;
}

void research_grant_random(Session& session, RaceIndex race, const Planet& planet) {
  Research& research = session.game->research;
  std::vector<std::uint16_t> candidates;
  for (std::uint16_t i = 0; i < research.count; ++i) {
    if (static_cast<std::int16_t>(i) != research.project[race] && !knows(research, i, race)) {
      candidates.push_back(i);
    }
  }
  if (candidates.empty()) {
    return;
  }
  const std::uint16_t item =
      candidates[static_cast<std::size_t>(session.rng.rand() % static_cast<std::int32_t>(candidates.size()))];
  const auto bit = static_cast<std::uint8_t>(1u << (race & 0x1f));
  research.items[item].known_mask |= bit;
  research.items[item].unseen_mask |= bit;
  if (race == session.player_race) {
    session.post_event(EventCode::kXenoDigDiscovery, item, planet_index(session.game->cosmos, planet));
  }
}

}  // namespace oa::game
