// The rules behind the ship design screen (`shipwin.cpp`,
// `docs/11-screens.md` "Ship design screen"): the isometric slot grid of each
// hull (`Ship_init` 0x4470c builds its masks, `ship_design_slot_at`
// 0x48bb8), the gizmo list of a category, the hull cycle, the refit cost,
// and how the planet screen ends a design (`PlanWnd_finish_ship_design`
// 0x37dec).
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "game/address.h"
#include "game/session.h"
#include "game/state.h"

namespace oa::game {

// The grid: 14 rows, row `r` holding the columns `kDesignRowStart[r]` ..
// `kDesignRowStart[r] + kDesignRowLength[r] - 1` (0x86834, 0x86844), 48
// cells.
inline constexpr int kDesignRows = 14;
inline constexpr std::array<std::uint8_t, kDesignRows> kDesignRowStart{0, 0, 1, 2, 3, 4, 5, 5, 4, 3, 2, 1, 2, 3};
inline constexpr std::array<std::uint8_t, kDesignRows> kDesignRowLength{2, 3, 3, 3, 3, 3, 3, 4, 5, 5, 5, 5, 3, 1};

// A cell of the grid.
struct DesignCell {
  int row = 0;
  int column = 0;

  bool operator==(const DesignCell&) const = default;
};

// A screen point.
struct DesignPoint {
  int x = 0;
  int y = 0;
};

// The screen point of a cell's shape hotspot (`ShipDesWnd_draw` 0x49b68),
// also the centre of its diamond for the hit test.
constexpr DesignPoint design_cell_point(const DesignCell& cell) {
  return {403 + (cell.column - cell.row) * 36, 166 + (cell.column + cell.row) * 18};
}

// Returns the cells of `hull`'s slots in slot order (row by row): the
// `slot_count` slots spread over the 48 cells by `Ship_init`'s
// single-precision loop (the masks 0xf64b4).
std::span<const DesignCell> ship_design_cells(std::uint8_t hull);

// Returns the slot of `hull` under the screen point (x, y), -1 for none
// (`ship_design_slot_at` 0x48bb8).
std::int32_t ship_design_slot_at(int x, int y, std::uint8_t hull);

// Tells whether `race` may build hull `hull`: Large needs research 19
// (Advanced Exploration), Enormous research 31.
bool ship_design_hull_known(const Research& research, RaceIndex race, std::uint8_t hull);

// The HULL button (`ShipDesWnd_cycle_hull` 0x49aac): moves `ship` to the
// next hull `race` knows (wrapping, skipping the hulls its gizmos do not
// fit); returns whether the hull changed (false when it came back to its
// own).
bool ship_design_cycle_hull(Session& session, Ship& ship, RaceIndex race);

// Returns the gizmos of `category` whose research `race` knows (or that
// need none), in table order (`ShipDesWnd_fill_gizmo_list` 0x49124).
std::vector<std::int8_t> ship_design_gizmo_list(const Session& session, std::uint8_t category, RaceIndex race);

// Returns the cost of refitting `original` into `edited`: 30 plus the
// gizmo industry added, removals free (`ShipDesWnd_refit_cost` 0x4a210).
std::int32_t ship_design_refit_cost(const Session& session, const Ship& edited, const Ship& original);

// Copies `name` into the ship's name like `strcpy` (at most 29
// characters; the bytes after the NUL keep their values), as the input box
// edits it.
void ship_set_name(Ship& ship, std::string_view name);

// A new design left unmodified: destroys the ship ordered on `square` of
// `planet` and empties the square (`PlanWnd_finish_ship_design`).
void ship_design_abandon(Session& session, Planet& planet, std::uint16_t square);

// An accepted refit of the ship on `square` of `planet`: the square becomes
// an unbuilt Ship project (forced), the planet's ship cost `cost` and the
// ship is refitted at the planet (`PlanWnd_finish_ship_design`).
void ship_design_accept_refit(Session& session, Planet& planet, std::uint16_t square, Ship& ship, std::int32_t cost);

}  // namespace oa::game
