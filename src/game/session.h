// One run of the original program: the game state plus the session globals
// the game code reads besides it (the random generator, the player's race,
// the position grid, the research-tree flag). Galaxies generated in the same
// session reuse the same records, so their stale fields carry over, as in
// the original.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "core/watcom_random.h"
#include "game/state.h"
#include "game/tables.h"

namespace oa::game {

struct Session {
  // `tables` must outlive the session.
  explicit Session(const GameTables& tables) : tables(&tables), game(std::make_unique<Game>()) {}

  // Sets the player's race slot and its bit mask (`Cosmos_set_player_race`
  // 0x1cf14); 0xff selects slot 0 with every bit (autoplay).
  void set_player_race(std::uint8_t race) {
    player_mask = race == 0xff ? 0xff : static_cast<std::uint8_t>(1u << race);
    player_race = race == 0xff ? 0 : race;
  }

  const GameTables* tables;
  std::unique_ptr<Game> game;         // the global objects (cosmos, research, ...)
  WatcomRandom rng;                   // the CRT generator (state 0x89718)
  std::uint8_t player_race = 0;       // 0xf4576
  std::uint8_t player_mask = 1;       // 0x8675c
  std::int32_t grid = 5;              // 0x86a14, position snap grid
  bool autoplay = false;              // 0x90b8c, the `flash.pop` demo mode
  bool research_tree_loaded = false;  // 0xf4728
  // What the next save writes besides the game: the stale header bytes
  // (entries past the race count, `stale`) and the window state, owned by
  // the user interface (`WinMgr_serialize_windows` 0x556a4).
  SaveHeader stale_header;
  std::vector<std::uint8_t> window_state;
};

}  // namespace oa::game
