#include "game/ship_design.h"

#include <algorithm>
#include <cstring>

#include "game/address.h"
#include "game/float_math.h"
#include "game/planet.h"
#include "game/ship.h"
#include "game/tables.h"

namespace oa::game {

namespace {

// The cells every hull may use (0x86854): all of them.
constexpr std::array<std::uint8_t, kDesignRows> kDesignRowCells{3,    7,    7,    7,    7,    7, 7,
                                                                0x0f, 0x1f, 0x1f, 0x1f, 0x1f, 7, 1};
constexpr std::array<std::int32_t, kHullCount> kHullSlots{5, 10, 15, 25};

// The research items of the Large and Enormous hulls.
constexpr std::uint8_t kResearchLargeHull = 19;
constexpr std::uint8_t kResearchEnormousHull = 31;
constexpr std::uint8_t kHullLarge = 2;
constexpr std::uint8_t kHullEnormous = 3;

// The refit's base cost.
constexpr std::int32_t kRefitBaseCost = 30;

using HullCells = std::array<std::vector<DesignCell>, kHullCount>;

// `Ship_init`'s one-time loop (0x4470c): per hull, the cells at every
// `48 / (n - 1)` step of a float accumulator. The sums compare unrounded
// (53-bit x87) and are stored as floats.
HullCells build_hull_cells() {
  int cell_count = 0;
  for (int r = 0; r < kDesignRows; ++r) {
    for (int k = 0; k < kDesignRowLength[static_cast<std::size_t>(r)]; ++k) {
      if ((kDesignRowCells[static_cast<std::size_t>(r)] >> k & 1) != 0) {
        ++cell_count;
      }
    }
  }
  HullCells cells;
  for (std::size_t hull = 0; hull < kHullCount; ++hull) {
    std::int32_t left = kHullSlots[hull];
    const float step = f32(static_cast<double>(cell_count) / static_cast<double>(left - 1));
    float acc = step;
    for (int r = 0; r < kDesignRows && left >= 1; ++r) {
      const auto row = static_cast<std::size_t>(r);
      for (int k = 0; k < kDesignRowLength[row]; ++k) {
        if ((kDesignRowCells[row] >> k & 1) == 0) {
          continue;
        }
        const double sum = static_cast<double>(acc) + 1.0;
        acc = f32(sum);
        bool take = false;
        if (sum < static_cast<double>(step)) {
          // The last slot goes to the last cell the accumulator nearly
          // fills.
          take = left == 1 && static_cast<double>(step) + -1.0 < static_cast<double>(acc);
        } else {
          acc = f32(static_cast<double>(acc) - static_cast<double>(step));
          take = true;
        }
        if (take) {
          cells[hull].push_back({r, kDesignRowStart[row] + k});
          --left;
        }
      }
    }
  }
  return cells;
}

const HullCells& hull_cells() {
  static const HullCells cells = build_hull_cells();
  return cells;
}

bool race_knows(const Research& research, std::uint8_t item, RaceIndex race) {
  return (research.items.at(item).known_mask >> (race & 0x1f) & 1) != 0;
}

}  // namespace

std::span<const DesignCell> ship_design_cells(std::uint8_t hull) { return hull_cells().at(hull); }

// 0x48bb8. The divisions truncate toward zero; a point above the grid
// (y <= 148) is no slot. Rows below 0 cannot occur inside the window
// (x <= 476).
std::int32_t ship_design_slot_at(int x, int y, std::uint8_t hull) {
  const int dy = y - 0x94;
  if (dy <= 0) {
    return -1;
  }
  const int half = (x - 0x16f) / 2;
  const int row = (dy - half + 0x12) / 2 / 0x12;
  const int column = (dy + half - 0x12) / 2 / 0x12;
  if (row < 0 || row >= kDesignRows) {
    return -1;
  }
  const std::span<const DesignCell> cells = ship_design_cells(hull);
  const auto it = std::find(cells.begin(), cells.end(), DesignCell{row, column});
  return it == cells.end() ? -1 : static_cast<std::int32_t>(it - cells.begin());
}

bool ship_design_hull_known(const Research& research, RaceIndex race, std::uint8_t hull) {
  if (hull == kHullLarge) {
    return race_knows(research, kResearchLargeHull, race);
  }
  if (hull == kHullEnormous) {
    return race_knows(research, kResearchEnormousHull, race);
  }
  return true;
}

// 0x49aac (the window's new-design test is the caller's).
bool ship_design_cycle_hull(Session& session, Ship& ship, RaceIndex race) {
  std::uint8_t hull = ship.hull_size;
  for (;;) {
    hull = hull >= kHullCount - 1 ? 0 : static_cast<std::uint8_t>(hull + 1);
    if (!ship_design_hull_known(session.game->research, race, hull)) {
      continue;
    }
    if (hull == ship.hull_size) {
      return false;
    }
    if (ship_change_hull_size(session, ship, hull)) {
      return true;
    }
  }
}

// 0x49124.
std::vector<std::int8_t> ship_design_gizmo_list(const Session& session, std::uint8_t category, RaceIndex race) {
  std::vector<std::int8_t> list;
  for (std::size_t i = 0; i < kGizmoCount; ++i) {
    const Gizmo& g = session.tables->gizmos.at(i);
    if (g.category == category &&
        (g.research_requirement == 0xff || race_knows(session.game->research, g.research_requirement, race))) {
      list.push_back(static_cast<std::int8_t>(i));
    }
  }
  return list;
}

// 0x4a210.
std::int32_t ship_design_refit_cost(const Session& session, const Ship& edited, const Ship& original) {
  const std::int32_t added = ship_gizmos_industry(session, edited) - ship_gizmos_industry(session, original);
  return added > 0 ? added + kRefitBaseCost : kRefitBaseCost;
}

void ship_set_name(Ship& ship, std::string_view name) {
  const std::size_t n = std::min(name.size(), ship.name.size() - 1);
  std::memcpy(ship.name.data(), name.data(), n);
  ship.name[n] = '\0';
}

// 0x37dec, the cancelled new design.
void ship_design_abandon(Session& session, Planet& planet, std::uint16_t square) {
  Cosmos& cosmos = session.game->cosmos;
  const ShipIndex ship = planet_square_ship(session, planet, square);
  ship_destroy(session, cosmos.ships.at(static_cast<std::size_t>(ship)));
  planet_set_square_item(session, planet, square, kNoItem, SquareMode::kRemove);
}

// 0x37dec, the accepted refit.
void ship_design_accept_refit(Session& session, Planet& planet, std::uint16_t square, Ship& ship, std::int32_t cost) {
  Cosmos& cosmos = session.game->cosmos;
  planet_squares(cosmos, planet)[square].flags &= static_cast<std::uint16_t>(~kSquareBuilt);
  planet_set_square_item(session, planet, square, kItemShip, SquareMode::kForceProject);
  planet.ship_cost = cost;
  ship.location_kind = ShipLocation::kRefit;
  ship.location = planet_index(cosmos, planet);
}

}  // namespace oa::game
