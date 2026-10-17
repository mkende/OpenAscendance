// Planet surfaces and outputs (`planet.cpp`): the parts the galaxy
// generation uses. Squares live in `Cosmos::squares`; a planet owns
// `square_count` consecutive ones from `first_square` (surface squares
// first, then the 10 orbital ones).
#pragma once

#include <cstdint>
#include <span>

#include "game/session.h"
#include "game/state.h"
#include "game/tables.h"

namespace oa::game {

// No square (the original's 0xffff square index).
inline constexpr std::uint16_t kNoSquare = 0xffff;

// Returns the squares of `planet`.
std::span<Square> planet_squares(Cosmos& cosmos, const Planet& planet);
std::span<const Square> planet_squares(const Cosmos& cosmos, const Planet& planet);

// Lays out and colours a new surface at `first` (size clamped to 4, type
// set to 2 when out of range) and resets the planet's economy, owner and
// project: `Planet_init_surface` 0x33210 (one draw per surface square, then
// the ruins draws). Returns the planet's square count.
std::uint16_t planet_init_surface(Session& session, Planet& planet, std::uint16_t size, std::uint16_t type,
                                  SquareIndex first);

// Recomputes the free-square counts, the outputs, the population limits and
// the weapon/shield counts from the squares: `Planet_recalc_output` 0x34034.
// Antagonizer (ANTAG 0x34e70): outside autoplay, a planet not owned by the
// player gets industry x1.3 (Neutral, Hostile) and research x1.3 (Hostile),
// and the Tractor Beam counts as a shield.
void planet_recalc_output(const Session& session, Planet& planet);

// Returns the first square holding `item`, `kNoSquare` if none
// (`Planet_find_item` 0x34a1c).
std::uint16_t planet_find_item(const Cosmos& cosmos, const Planet& planet, std::uint8_t item);
// Returns the surface square at (`row`, `column`) of the planet's shape,
// `kNoSquare` outside it (`Planet_square_at` 0x34a54).
std::uint16_t planet_square_at(const PlanItemTable& items, const Planet& planet, std::int32_t row, std::int32_t column);
// Returns `row << 16 | column` of surface square `square`, -1 for an
// orbital square (`Planet_square_rowcol` 0x34aec).
std::int32_t planet_square_rowcol(const PlanItemTable& items, const Planet& planet, std::uint16_t square);

// Tells whether `item` may be placed on square `square` (the race's
// research, the free population, the square's kind, colour and ruins, and the
// adjacency of surface buildings): `Planet_can_place_item` 0x3379c.
bool planet_can_place_item(const Session& session, const Planet& planet, std::uint8_t item, std::uint16_t square);

// Returns whether the planet's owner can start a ship here: a built Shipyard
// and a positive ship allowance (`Planet_can_build_ship` 0x34f2c).
bool planet_can_build_ship(const Session& session, const Planet& planet);

// Places `item` on empty square `square`, built at once, when it is a
// surface item on a surface square (an original bug rejects orbital items)
// and the free population suffices; sets the weapon shots; then recomputes
// the outputs. Returns whether it was placed (`Planet_set_square_item`
// 0x33cd0, mode 0).
bool planet_build_item_now(Session& session, Planet& planet, std::uint16_t square, std::uint8_t item);

// Returns a random empty square accepting `item` (`rand() % candidates`,
// no draw without candidates), `kNoSquare` if none
// (`Planet_pick_random_free_square` 0x33bec). Antagonizer: the first best
// scored candidate, no draw (`Planet_pick_scored_free_square` ANTAG 0x347cc).
std::uint16_t planet_pick_random_free_square(Session& session, const Planet& planet, std::uint8_t item);

// Picks a random free square for `item` and builds it there at once:
// `Planet_auto_place_item` 0x33ca8 with mode -1. Returns the square.
std::uint16_t planet_auto_build_item(Session& session, Planet& planet, std::uint8_t item);

}  // namespace oa::game
