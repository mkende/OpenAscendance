#include "game/battle_control.h"

#include "game/ai.h"
#include "game/cosmos.h"
#include "game/planet.h"
#include "game/race.h"
#include "game/ship.h"

namespace oa::game {

namespace {

// Checks (`apply` false) or applies a chosen action.
void do_action(Session& session, const BattleAction& action, bool apply) {
  Cosmos& cosmos = session.game->cosmos;
  if (action.kind == BattleActor::kShip) {
    ship_do_action(session, cosmos.ships[static_cast<std::size_t>(action.index)], apply);
  } else if (action.kind == BattleActor::kPlanet) {
    planet_fire_weapon(session, cosmos.planets[static_cast<std::size_t>(action.index)], apply);
  }
}

bool player_present(const Session& session, const Star& star) {
  return ((star.owner_mask | star.ships_mask) & session.player_mask) != 0;
}

}  // namespace

void BattleControl::list_races(const Session& session) {
  const Cosmos& cosmos = session.game->cosmos;
  const auto present = star_list_present_races(cosmos, cosmos.stars[static_cast<std::size_t>(star_)]);
  race_count_ = static_cast<std::int32_t>(present.size());
  for (std::size_t i = 0; i < present.size(); ++i) {
    races_[i] = present[i];
  }
}

void BattleControl::open(Session& session, Star& star) {
  star_ = star_index(session.game->cosmos, star);
  session.game->cosmos.selected_star = star_;
  list_races(session);
  cursor_ = 0;
  auto_ = false;
  pending_.reset();
  results_.fill(true);
  previous_screen_ = session.screen;
  session.screen = 16;
}

void BattleControl::close(Session& session) { session.screen = previous_screen_; }

void BattleControl::act(Session& session, BattleAction action) {
  do_action(session, action, false);
  pending_ = action;
}

bool BattleControl::pass(Session& session) {
  const Star& star = session.game->cosmos.stars[static_cast<std::size_t>(star_)];
  if (pending_ || !player_present(session, star) || auto_ || cursor_ != 0) {
    return false;
  }
  run_turns(session);
  return true;
}

bool BattleControl::start_auto(Session& session) {
  const Star& star = session.game->cosmos.stars[static_cast<std::size_t>(star_)];
  if (pending_ || !player_present(session, star) || auto_ || cursor_ != 0) {
    return false;
  }
  cursor_ = -1;
  auto_ = true;
  run_turns(session);
  return true;
}

void BattleControl::finish(Session& session) {
  if (!pending_) {
    return;
  }
  const BattleAction action = *pending_;
  pending_.reset();
  do_action(session, action, true);
  run_turns(session);
}

void BattleControl::finish_all(Session& session) {
  while (pending_) {
    finish(session);
  }
}

void BattleControl::run_turns(Session& session) {
  Cosmos& cosmos = session.game->cosmos;
  const StrategicAi& ai = strategic_ai(session.game->variant);
  for (;;) {
    ++cursor_;
    if (cursor_ >= race_count_ && !new_round(session)) {
      return;
    }
    const auto c = static_cast<std::size_t>(cursor_);
    BattleAction action;
    results_[c] = ai.battle_turn(session, cosmos.races[static_cast<std::size_t>(races_[c])],
                                 cosmos.stars[static_cast<std::size_t>(star_)], action);
    if (action.kind != BattleActor::kNone) {
      do_action(session, action, false);
      pending_ = action;
      return;
    }
  }
}

bool BattleControl::new_round(Session& session) {
  Cosmos& cosmos = session.game->cosmos;
  bool again = false;
  if (auto_) {
    for (std::int32_t i = 0; i < race_count_; ++i) {
      if (results_[static_cast<std::size_t>(i)]) {
        again = true;
        break;
      }
    }
  }
  auto_ = again;
  list_races(session);
  cursor_ = 0;
  const Star& star = cosmos.stars[static_cast<std::size_t>(star_)];
  for (std::int32_t i = 0; i < race_count_; ++i) {
    race_reset_ship_moves_at_star(session, cosmos.races[static_cast<std::size_t>(races_[static_cast<std::size_t>(i)])],
                                  star);
  }
  return again;
}

}  // namespace oa::game
