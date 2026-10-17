// Galaxy generation (`cosmos.cpp`, `docs/09-simulation.md`, "New game and
// galaxy generation"): stars, backdrops, star lanes, races and home stars,
// planets and the hop matrix, drawing from the session's generator in the
// original order. The new-game flow calling them is in `game/new_game.h`.
#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "game/session.h"
#include "game/state.h"

namespace oa::game {

inline constexpr std::size_t kStarTypes = 13;
// Generation weights of the star types (0x86394), summing to 100.
inline constexpr std::array<std::int16_t, kStarTypes> kStarTypeWeights = {16, 4, 6, 4, 8, 13, 11, 8, 5, 6, 2, 9, 8};
// Star radius (0x8637a) and maximum lane count (0x863ae) by type.
inline constexpr std::array<std::int16_t, kStarTypes> kStarRadius = {30, 30, 30, 30, 20, 20, 20,
                                                                     20, 20, 20, 15, 15, 15};
inline constexpr std::array<std::int16_t, kStarTypes> kStarMaxLanes = {6, 6, 6, 6, 4, 4, 4, 4, 4, 4, 3, 3, 3};
// Planet orbit radii (0x86370).
inline constexpr std::array<std::int16_t, kPlanetsPerStar> kOrbitRadius = {10, 25, 40, 55, 70};
// Home world size (0x863c8) and type (0x8641c) by species.
inline constexpr std::array<std::uint16_t, kSpeciesCount> kHomeSize = {4, 4, 3, 4, 3, 4, 3, 3, 4, 3, 3,
                                                                       3, 4, 4, 4, 3, 4, 3, 2, 3, 3};
inline constexpr std::array<std::uint16_t, kSpeciesCount> kHomeType = {5, 1, 1, 6, 3, 2, 8, 6, 4, 4, 10,
                                                                       2, 7, 8, 9, 7, 3, 5, 3, 7, 9};

// Species with an ability applied at the start (`Race::ability`).
inline constexpr std::uint8_t kSpeciesKambuchka = 3;
inline constexpr std::uint8_t kSpeciesHanshaks = 4;
inline constexpr std::uint8_t kSpeciesOculons = 15;

// Builds stars, backdrops, lanes and races; planets and hops are the
// caller's: `Cosmos_generate` 0x1db74 (grid global = 32).
void cosmos_generate(Session& session, std::int16_t stars, std::int16_t races, std::uint8_t species);

// Places `count` (1..100) stars on concentric shells with random names and
// types (5 draws each): `Cosmos_generate_stars` 0x1dd08.
void cosmos_generate_stars(Session& session, std::int16_t count);

// Places the star-system backdrop sprites on a ring (2 draws each):
// `Cosmos_generate_backdrops` 0x1dbb8.
void cosmos_generate_backdrops(Session& session, std::int16_t count);

// Links each star to its nearest stars, then joins the components with red
// links until one remains (no draws): `Cosmos_generate_lanes` 0x1e934.
void cosmos_generate_lanes(Cosmos& cosmos);

// Adds a lane between stars `a` and `b` when neither is full, with its end
// offsets: `Cosmos_connect_stars` 0x1cf68. Returns the lane, or kNone.
LaneIndex cosmos_connect_stars(Cosmos& cosmos, StarIndex a, StarIndex b);

// Resets the races (`races` 2..7) with home stars spread at least 600 apart
// on average (450 below 25 stars), distinct species (race 0 gets `species`)
// and colours, the atmosphere's DIP shift, no ships, and the start
// abilities: `Cosmos_new_galaxy` 0x1e174.
void cosmos_new_galaxy(Session& session, std::int16_t races, std::uint8_t species);

// Stores (and returns) the mean distance between the home stars:
// `Cosmos_mean_home_distance` 0x217ec.
std::int16_t cosmos_mean_home_distance(Cosmos& cosmos);

// Gives race 0 `color` and the other races random distinct colours:
// `Cosmos_assign_colors` 0x1e77c.
void cosmos_assign_colors(Session& session, std::uint8_t color);

// Creates 1..5 planets per star, the home worlds with their species' size
// and type, a population of 2 and a built Colony Base:
// `Cosmos_generate_planets` 0x1ed78.
void cosmos_generate_planets(Session& session);

// Fills the hop matrix with the lane-graph distances (0 self, 0xff outside
// the stars): `Cosmos_compute_hops` 0x1e9ac. The original's level-by-level
// pair scan gives exactly the breadth-first distances computed here.
void cosmos_compute_hops(Cosmos& cosmos);

// Appends the roman numeral of `n` (0..3999) to the NUL-terminated `name`,
// as `append_roman_numeral` 0x31580 (thousands quirk included).
void append_roman_numeral(std::span<char> name, std::int32_t n);

}  // namespace oa::game
