// The new-game flow: the program start (`main` 0x1c424, corn.cpp) and the
// game side of the new-game window (`NewGameWnd_handle_event` 0x4dd5c).
// Every regeneration draws from the session's generator, so a galaxy is a
// function of the seed and of the window actions, in order.
//
// Example, the default new game of a session seeded through `cosmos.txt`
// (the oracle's runs, `docs/12-oracle.md`):
//
//     Session session(tables);
//     const CosmosTxt txt{.stars = 50, .races = 7, .seed = 1234};
//     program_start(session, time_seed, &txt, /*resume_exists=*/false);
//     NewGameSetup setup;
//     setup.activate(session);
//     setup.begin(session);
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "game/session.h"
#include "game/state.h"

namespace oa::game {

// The debug galaxy file `cosmos.txt` (`STARS n RACES n SEED s`, then
// optional species numbers per race slot).
struct CosmosTxt {
  std::int16_t stars = 50;
  std::int16_t races = 7;
  std::int32_t seed = -1;  // -1: keep the time seed
  std::vector<std::int32_t> species;
};

// Parses `cosmos.txt` as `main` reads it (`%s %d %s %d %s %d`, then `%d`
// species until the end or `races` of them). Throws `DataError` when the
// first six tokens are malformed.
CosmosTxt parse_cosmos_txt(std::span<const std::uint8_t> data);

// Runs `main`'s set-up of the start-up galaxy (after `logo.gif` loaded into
// `Session::scratch`): `srand(time_seed)`, the
// player in slot 0, then with `cosmos_txt` `srand(seed)` (unless -1) and its
// counts and species, else 50 stars and 7 races; the planets, the colours,
// and the hops when no `resume.gam` exists.
void program_start(Session& session, std::uint32_t time_seed, const CosmosTxt* cosmos_txt, bool resume_exists);

// The settings of the new-game window (shorts at window +0xb3..+0xbb) and
// the regenerations its controls trigger.
class NewGameSetup {
 public:
  // Star counts of the five densities (0x86a00).
  static constexpr std::array<std::int16_t, 5> kDensityStars = {15, 25, 50, 75, 100};

  // Opening the window (message 1; its `resume.gam` save is the caller's):
  // density 2, 5 races, Neutral, species 0, the atmosphere byte set to
  // Neutral, a new galaxy, then colour 0 for the player in slot 0.
  void activate(Session& session);
  // Star density button (message 501): the next density, a new galaxy.
  void next_density(Session& session);
  // Race count button (message 503): 3..7 races, new races only.
  void next_race_count(Session& session);
  // Atmosphere button (message 505): the next atmosphere, kept in the window
  // until `begin`.
  void next_atmosphere();
  // Species list pick (message 0x1c01): new races when the species changes.
  void pick_species(Session& session, std::uint8_t species);
  // Colour bar click: the player's colour, new colours for the others.
  void pick_color(Session& session, std::uint8_t color);
  // "Begin New Game": the planets, the hops, then the chosen atmosphere
  // (only now: the races were set up with the Neutral thresholds).
  void begin(Session& session);

  std::int16_t species() const { return species_; }
  std::int16_t color() const { return color_; }
  std::int16_t density() const { return density_; }
  std::int16_t race_count() const { return race_count_; }
  Atmosphere atmosphere() const { return atmosphere_; }

 private:
  void generate(Session& session) const;

  std::int16_t species_ = 0;                      // +0xb3
  std::int16_t color_ = 0;                        // +0xb5
  std::int16_t density_ = 2;                      // +0xb7
  std::int16_t race_count_ = 5;                   // +0xb9
  Atmosphere atmosphere_ = Atmosphere::kNeutral;  // +0xbb
};

}  // namespace oa::game
