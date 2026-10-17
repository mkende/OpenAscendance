// The variant's AI (`docs/10-antagonizer-ai.md`, "Plugging the variant into
// the shared code"): the two entry points the day calls, each variant
// providing its own `Race_turn_update` (with its AI turn) and
// `Race_battle_turn` (with its tactical AI). The rule changes shared by the
// player and the AI branch on `Game::variant` where they occur.
#pragma once

#include "game/battle.h"
#include "game/session.h"
#include "game/state.h"

namespace oa::game {

class StrategicAi {
 public:
  virtual ~StrategicAi() = default;

  // The race's day (`Race_turn_update`), for AI races and the player alike.
  virtual void race_turn_update(Session& session, Race& race) const = 0;
  // One battle turn of `race` at `star` (`Race_battle_turn`); returns
  // whether the race is still active.
  virtual bool battle_turn(Session& session, Race& race, Star& star, BattleAction& action) const = 0;
};

// Returns the AI of `variant`.
const StrategicAi& strategic_ai(Variant variant);

}  // namespace oa::game
