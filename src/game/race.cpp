#include "game/race.h"

namespace oa::game {

void race_reset(Race& race) {
  race.ai_build_threshold_a = 18;
  race.ai_build_threshold_b = 22;
  race.ai_unknown_1ae = 0;
  race.target_count = 0;
  race.bio_max_positive = 0;
  race.bio_max_negative = 0;
  race.pending_action = kNoDipAction;
  race.pending_action_arg = 0;
  race.relation.fill(kNotMet);
  race.attitude.fill(0);
  race.extinct = 0;
}

void race_init(Session& session, Race& race, std::uint8_t index, std::uint8_t species) {
  race_reset(race);
  race.index = index;
  if (race.species != species) {
    race.species = species;
    race_load_dip_values(session, race);
  }
}

void race_load_dip_values(Session& session, Race& race) {
  session.game->last_trade_days.fill(-10);
  const DipValues& d = session.tables->dip.at(race.species);
  race.declare_war = d.declare_war;
  race.accept_peace = d.accept_peace;
  race.propose_peace = d.propose_peace;
  race.break_alliance = d.break_alliance;
  race.accept_alliance = d.accept_alliance;
  race.propose_alliance = d.propose_alliance;
  race.start_attitude = d.start_attitude;
  race.ship_patience = d.ship_patience;
  race.bio_period = d.bio_period;
  race.bio_max_positive = d.bio_max_positive;
  race.bio_max_negative = d.bio_max_negative;
  race.bio_sin_x = d.bio_sin_x;
  race.bio_sin_half_x = d.bio_sin_half_x;
  race.bio_sin_twice_x = d.bio_sin_twice_x;
  race.neg_player_turns = d.neg_player_turns;
  race.ability = race.species;
  race.ability_charge = 0;
}

std::int32_t race_ship_count(const Cosmos& cosmos, const Race& race) {
  std::int32_t used = 0;
  std::int32_t count = 0;
  for (std::size_t i = 0; i < kMaxShips && used < cosmos.ship_count; ++i) {
    const Ship& ship = cosmos.ships[i];
    if (ship.owner != -1) {
      ++used;
      if (ship.owner == race.index) {
        ++count;
      }
    }
  }
  return count;
}

std::int32_t race_count_planets(const Cosmos& cosmos, const Race& race) {
  std::int32_t count = 0;
  for (std::int16_t i = 0; i < cosmos.planet_count; ++i) {
    if (cosmos.planets[static_cast<std::size_t>(i)].owner == race.index) {
      ++count;
    }
  }
  return count;
}

std::int32_t race_count_discoveries(const Research& research, const Race& race) {
  std::int32_t count = 0;
  for (std::size_t i = 0; i < research.count; ++i) {
    if ((research.items[i].known_mask >> (race.index & 0x1f)) & 1) {
      ++count;
    }
  }
  return count;
}

std::int32_t race_ship_allowance(const Cosmos& cosmos, const Race& race) {
  const std::uint32_t bit = 1u << (race.index & 0x1f);
  std::int32_t allowance = 1;
  for (std::int16_t i = 0; i < cosmos.star_count; ++i) {
    if (cosmos.stars[static_cast<std::size_t>(i)].owner_mask == bit) {
      ++allowance;
    }
  }
  allowance -= race_ship_count(cosmos, race);
  if (cosmos.stars.at(static_cast<std::size_t>(race.home)).owner_mask & bit) {
    ++allowance;
  }
  return allowance;
}

void research_load(Session& session) {
  Research& r = session.game->research;
  r.self_managed = 0;
  r.basics_done = 0;
  for (ResearchItem& item : r.items) {
    item.known_mask = 0;
    item.unseen_mask = 0;
  }
  if (!session.research_tree_loaded) {
    // The global is still the zeroed BSS here (a save load sets the flag
    // first), so the parsed tree is exactly what the loader leaves there.
    r.count = session.tables->research.count;
    r.items = session.tables->research.items;
    session.research_tree_loaded = true;
  }
  r.project.fill(-1);
  r.progress.fill(0);
}

}  // namespace oa::game
