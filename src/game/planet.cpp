#include "game/planet.h"

#include <cmath>
#include <utility>
#include <vector>

#include "core/fixed_width.h"
#include "game/float_math.h"
#include "game/race.h"

namespace oa::game {

namespace {

const PlanItem& item_of(const GameTables& tables, std::uint8_t item) { return tables.planitems.items.at(item); }

// `PlanItem_weapon_stats` 0x34ffc: shots per day of a planetary weapon (the
// range and damage outputs are not needed here); 0 for other items.
std::int32_t weapon_shots(std::uint8_t item) {
  switch (item) {
    case kItemMissileBase:
      return 1;
    case kItemWhopper:
    case kItemMegaWhopper:
      return 3;
    case kItemTractorBeam:
      return 2;
    default:
      return 0;
  }
}

// Returns `trunc((sum + 1) ** 0.85)` for a non-zero `sum`, else 0: the
// output curve of industry and prosperity (pow 0x73f30, `__CHP`).
std::int32_t curve(std::int32_t sum) { return sum == 0 ? 0 : chop(std::pow(static_cast<double>(sum + 1), 0.85)); }

bool square_built(std::span<const Square> squares, std::uint16_t square) {
  return square != kNoSquare && (squares[square].flags & kSquareBuilt) != 0;
}

// The Antagonizer's square choice (ANTAG 0x347cc): a red, green or blue
// square scores 3 for an item whose matching output (industry, prosperity,
// research) is positive and the largest, 2 when positive but not the
// largest, 0 when not positive; blue scores 1 once the owner knows every
// item; other colour bytes (ruins included) 1. The first best candidate wins.
std::uint16_t pick_scored_square(const Session& session, const Planet& planet, std::uint8_t item,
                                 const std::vector<std::uint16_t>& candidates) {
  const Cosmos& cosmos = session.game->cosmos;
  const Research& research = session.game->research;
  const Race& owner = cosmos.races.at(planet.owner);
  const bool all_known = research.count > 0 &&
                         ((research.items[research.count - 1u].known_mask >> (planet.owner & 0x1f)) & 1) != 0 &&
                         race_count_discoveries(research, owner) == research.count;
  const PlanItem& it = item_of(*session.tables, item);
  const auto score = [](std::int8_t mine, std::int8_t other1, std::int8_t other2) {
    return mine <= 0 ? 0 : (mine < other1 || mine < other2) ? 2 : 3;
  };
  const std::span<const Square> squares = planet_squares(cosmos, planet);
  std::int32_t best = -1;
  std::uint16_t chosen = kNoSquare;
  for (const std::uint16_t square : candidates) {
    std::int32_t s = 1;
    switch (squares[square].color) {
      case 2:
        s = score(it.industry, it.research, it.prosperity);
        break;
      case 3:
        s = score(it.prosperity, it.industry, it.research);
        break;
      case 4:
        if (!all_known) {
          s = score(it.research, it.industry, it.prosperity);
        }
        break;
      default:
        break;
    }
    if (best < s) {
      best = s;
      chosen = square;
    }
  }
  return chosen;
}

}  // namespace

std::span<Square> planet_squares(Cosmos& cosmos, const Planet& planet) {
  return std::span(cosmos.squares).subspan(static_cast<std::size_t>(planet.first_square), planet.square_count);
}

std::span<const Square> planet_squares(const Cosmos& cosmos, const Planet& planet) {
  return std::span(cosmos.squares).subspan(static_cast<std::size_t>(planet.first_square), planet.square_count);
}

std::uint16_t planet_init_surface(Session& session, Planet& planet, std::uint16_t size, std::uint16_t type,
                                  SquareIndex first) {
  const PlanItemTable& items = session.tables->planitems;
  Cosmos& cosmos = session.game->cosmos;
  planet.first_square = first;
  planet.size = size < 5 ? size : 4;
  planet.surface_squares = 0;
  for (const std::uint8_t height : items.column_height[planet.size]) {
    planet.surface_squares = static_cast<std::uint16_t>(planet.surface_squares + height);
  }
  planet.square_count = static_cast<std::uint16_t>(planet.surface_squares + kOrbitalSquares);
  planet.type = type < PlanItemTable::kTypes ? type : 2;
  // Cumulative thresholds of the raw rand() (0..0x7fff): black, red, green, blue.
  const auto& weights = items.type_weights[planet.type];
  const auto share = [](std::uint8_t pct) { return static_cast<std::int32_t>(pct * 0x7fffu / 100); };
  const std::int32_t black = share(weights[0]);
  const std::int32_t red = black + share(weights[1]);
  const std::int32_t green = red + share(weights[2]);
  const std::int32_t blue = green + share(weights[3]);
  std::int32_t habitable = 0;
  const std::span<Square> squares = planet_squares(cosmos, planet);
  for (std::size_t i = 0; i < squares.size(); ++i) {
    Square& sq = squares[i];
    sq.item = kNoItem;
    sq.flags = 0;
    sq.color = 1;
    if (i < planet.surface_squares) {
      const int r = session.rng.rand();
      if (r < black) {
        sq.color = 0;
      } else {
        ++habitable;
        sq.color = r < red ? 2 : r < green ? 3 : r < blue ? 4 : 1;
      }
    }
  }
  planet.flags = 0;
  planet.progress = 0;
  planet.growth = 0;
  planet.used_population = 0;
  planet.prosperity = 0;
  planet.research = 0;
  planet.industry = 0;
  planet.population = 0;
  if (session.rng.rand() % 7 == 0) {
    squares[static_cast<std::size_t>(session.rng.rand() % planet.surface_squares)].color |= kSquareRuins;
    planet.flags |= kPlanetRuins;
  }
  planet.project_square = kNoSquare;
  planet.project_item = kNoItem;
  planet.base_population = static_cast<std::uint16_t>(habitable / 3 + 5);
  planet.max_population = planet.base_population;
  planet.owner = kNoOwner;
  planet_recalc_output(session, planet);
  planet.unknown_5e = 0;
  return planet.square_count;
}

void planet_recalc_output(const Session& session, Planet& planet) {
  const GameTables& tables = *session.tables;
  const Cosmos& cosmos = session.game->cosmos;
  const bool antagonizer = session.game->variant == Variant::kAntagonizer;
  const std::span<const Square> squares = planet_squares(cosmos, planet);
  std::int32_t industry = 0;
  std::int32_t research = 0;
  std::int32_t prosperity = 0;
  std::int32_t used = 0;
  std::uint32_t max_population = planet.base_population;
  std::uint16_t free_surface = 0;
  std::uint16_t free_black = 0;
  std::uint16_t free_orbital = 0;
  std::uint8_t weapons = 0;
  std::uint8_t shields = 0;
  for (std::size_t i = 0; i < squares.size(); ++i) {
    const Square& sq = squares[i];
    if (sq.item == kNoItem) {
      if (i < planet.surface_squares) {
        if ((sq.color & kSquareRuins) == 0) {
          ++free_surface;
          if (sq.color == 0) {
            ++free_black;
          }
        }
      } else {
        ++free_orbital;
      }
      continue;
    }
    const PlanItem& item = item_of(tables, sq.item);
    if (sq.flags & kSquareBuilt) {
      // The colour bonus compares the whole colour byte (not on ruins).
      if (item.industry != 0) {
        industry += item.industry + (sq.color == 2 ? 1 : 0);
      }
      if (item.prosperity != 0) {
        prosperity += item.prosperity + (sq.color == 3 ? 1 : 0);
      }
      if (item.research != 0) {
        research += item.research + (sq.color == 4 ? 1 : 0);
      }
      const bool tractor_shield = antagonizer && sq.item == kItemTractorBeam;
      if ((item.flags & kItemWeapon) && !tractor_shield) {
        ++weapons;
      } else if ((item.flags & kItemShield) || tractor_shield) {
        ++shields;
      }
      max_population += static_cast<std::uint32_t>(static_cast<std::int32_t>(item.max_population));
    }
    if (sq.item != kItemShip && (sq.flags & kSquareAutomated) == 0 && (item.flags & kItemNoPopulation) == 0) {
      used += item.used_population;
    }
  }
  if (square_built(squares, planet_find_item(cosmos, planet, kItemHyperpowerPlant))) {
    industry += industry >> 1;
  }
  if (square_built(squares, planet_find_item(cosmos, planet, kItemFertilizationPlant))) {
    prosperity += prosperity >> 1;
  }
  if (square_built(squares, planet_find_item(cosmos, planet, kItemInternet))) {
    research += research >> 1;
  }
  switch (planet.project_item) {
    case kItemEndlessParty:
      prosperity += industry >> 2;
      break;
    case kItemScientistTakeover:
      research += industry >> 2;
      break;
    case kItemAutomation:
      used += item_of(tables, kItemAutomation).used_population;
      break;
    default:
      break;
  }
  industry = curve(industry);
  prosperity = curve(prosperity) - (planet.population >> 2);
  if (prosperity < 0) {
    prosperity = 0;
  }
  planet.free_surface = free_surface;
  planet.free_black = free_black;
  planet.free_orbital = free_orbital;
  planet.industry = wrap_i16(industry);
  planet.research = wrap_i16(research);
  if (antagonizer && planet.owner != session.player_race && !session.autoplay &&
      cosmos.atmosphere != Atmosphere::kPeaceful) {
    // The stored words, read back unsigned, times the double 1.3 (0x91db5).
    const auto boost = [](std::int16_t& v) {
      v = wrap_i16(chop(static_cast<double>(static_cast<std::uint16_t>(v)) * 1.3));
    };
    boost(planet.industry);
    if (cosmos.atmosphere == Atmosphere::kHostile) {
      boost(planet.research);
    }
  }
  planet.prosperity = wrap_i16(prosperity);
  planet.max_population = wrap_u16(max_population);
  planet.used_population = wrap_u16(used);
  planet.weapon_count = weapons;
  planet.shield_count = shields;
  if (planet.max_population > 100) {
    planet.max_population = 100;
  }
  if (planet.max_population < planet.population) {
    planet.population = planet.max_population;
  }
  if (planet.population < planet.used_population) {
    planet.used_population = planet.population;
  }
}

std::uint16_t planet_find_item(const Cosmos& cosmos, const Planet& planet, std::uint8_t item) {
  const std::span<const Square> squares = planet_squares(cosmos, planet);
  for (std::size_t i = 0; i < squares.size(); ++i) {
    if (squares[i].item == item) {
      return static_cast<std::uint16_t>(i);
    }
  }
  return kNoSquare;
}

std::uint16_t planet_square_at(const PlanItemTable& items, const Planet& planet, std::int32_t row,
                               std::int32_t column) {
  if (row < 0 || row >= static_cast<std::int32_t>(PlanItemTable::kColumns) || column < 0) {
    return kNoSquare;
  }
  const auto& heights = items.column_height[planet.size];
  std::int32_t first = 0;
  for (std::int32_t r = 0; r < row; ++r) {
    first += heights[static_cast<std::size_t>(r)];
  }
  const std::int32_t start = items.column_start[planet.size][static_cast<std::size_t>(row)];
  if (start <= column && column < start + heights[static_cast<std::size_t>(row)]) {
    return static_cast<std::uint16_t>(first + column - start);
  }
  return kNoSquare;
}

std::int32_t planet_square_rowcol(const PlanItemTable& items, const Planet& planet, std::uint16_t square) {
  if (square >= planet.surface_squares) {
    return -1;
  }
  std::int32_t column = 0;
  std::int32_t row = 0;
  std::uint16_t end = 0;
  const auto& heights = items.column_height[planet.size];
  const auto& starts = items.column_start[planet.size];
  while (end < planet.surface_squares) {
    column = square + starts[static_cast<std::size_t>(row)] - end;
    end = static_cast<std::uint16_t>(end + heights[static_cast<std::size_t>(row)]);
    if (square < end) {
      break;
    }
    ++row;
  }
  return column + row * 0x10000;
}

bool planet_can_build_ship(const Session& session, const Planet& planet) {
  const Cosmos& cosmos = session.game->cosmos;
  const std::uint16_t yard = planet_find_item(cosmos, planet, kItemShipyard);
  return square_built(planet_squares(cosmos, planet), yard) &&
         race_ship_allowance(cosmos, cosmos.races.at(planet.owner)) > 0;
}

bool planet_can_place_item(const Session& session, const Planet& planet, std::uint8_t item, std::uint16_t square) {
  const GameTables& tables = *session.tables;
  const Cosmos& cosmos = session.game->cosmos;
  const Research& research = session.game->research;
  if (planet.owner == kNoOwner && item != kItemColonyBase) {
    return false;
  }
  // The item itself: researched by the owner and enough free population (no
  // check for ids past the table).
  bool item_ok = true;
  if (item < PlanItemTable::kItems) {
    item_ok = false;
    const PlanItem& it = item_of(tables, item);
    const std::uint8_t req = it.research_requirement;
    if (req == 0xff || (req < research.count && ((research.items[req].known_mask >> (planet.owner & 0x1f)) & 1))) {
      std::uint32_t free = static_cast<std::uint32_t>(planet.population) - planet.used_population;
      if (planet.project_item != kNoItem) {
        free +=
            static_cast<std::uint16_t>(static_cast<std::int16_t>(item_of(tables, planet.project_item).used_population));
      }
      if (it.used_population <= static_cast<std::int32_t>(free & 0xffff)) {
        item_ok = true;
      }
      if (item_ok && item == kItemShip) {
        item_ok = planet_can_build_ship(session, planet);
      }
    }
    if (item == kItemColonyBase) {
      item_ok = true;
    }
  }
  // The square (no check past the planet's squares).
  bool square_ok = true;
  if (square < planet.square_count) {
    square_ok = false;
    const std::span<const Square> squares = planet_squares(cosmos, planet);
    const Square& sq = squares[square];
    const std::uint16_t flags = item < PlanItemTable::kItems ? item_of(tables, item).flags : 0;
    if (item < PlanItemTable::kItems && ((flags & kItemAnySquare) || cosmos.races.at(planet.owner).ability == 2)) {
      square_ok = true;
    }
    if (sq.color != 0 || item == kNoItem || (flags & kItemWholePlanet)) {
      square_ok = true;
    }
    if (item != kNoItem && item < 31 && (square < planet.surface_squares) != (item < tables.planitems.surface_count)) {
      square_ok = false;
    }
    if (item != kNoItem && ((sq.color & kSquareRuins) != 0) != (item == kItemXenoDig)) {
      square_ok = false;
    }
    if (item == kItemTerraforming && (sq.color != 0 || sq.item != kNoItem)) {
      square_ok = false;
    }
    if (item == kItemAutomation &&
        ((sq.flags & kSquareBuilt) == 0 || sq.item == kItemShip || (sq.flags & kSquareAutomated) != 0)) {
      square_ok = false;
    }
    // A new surface building must touch a built square.
    if (square < planet.surface_squares && (sq.flags & kSquareBuilt) == 0 && item != kItemColonyBase &&
        item != kItemAutomation) {
      const std::int32_t rc = planet_square_rowcol(tables.planitems, planet, square);
      const std::int32_t column = rc & 0xffff;
      const std::int32_t row = (rc >> 16) & 0xffff;
      const auto built = [&](std::int32_t r, std::int32_t c) {
        return square_built(squares, planet_square_at(tables.planitems, planet, r, c));
      };
      if (!built(row - 1, column) && !built(row + 1, column) && !built(row, column - 1) && !built(row, column + 1)) {
        square_ok = false;
      }
    }
  }
  return item_ok && square_ok;
}

bool planet_build_item_now(Session& session, Planet& planet, std::uint16_t square, std::uint8_t item) {
  const GameTables& tables = *session.tables;
  Cosmos& cosmos = session.game->cosmos;
  bool placed = false;
  if (square != kNoSquare) {
    Square& sq = planet_squares(cosmos, planet)[square];
    // Original bug kept: the kind test compares `item >= surface_count` (0 or
    // 1) with `(square < surface_squares) - 1` (0 or -1), so only surface
    // items on surface squares pass.
    const bool surface_square = square < planet.surface_squares;
    if (sq.item == kNoItem && item < PlanItemTable::kItems &&
        item_of(tables, item).used_population <=
            static_cast<std::int32_t>(planet.population) - static_cast<std::int32_t>(planet.used_population) &&
        item < tables.planitems.surface_count && surface_square) {
      sq.item = item;
      sq.flags = kSquareBuilt;
      placed = true;
      if (item_of(tables, item).flags & kItemWeapon) {
        const std::int32_t shots = weapon_shots(item);
        if (shots != 0) {
          sq.flags = static_cast<std::uint16_t>(sq.flags | (shots << 8));
        }
      }
    }
  }
  planet_recalc_output(session, planet);
  return placed;
}

std::uint16_t planet_pick_random_free_square(Session& session, const Planet& planet, std::uint8_t item) {
  const std::span<const Square> squares = planet_squares(std::as_const(session.game->cosmos), planet);
  std::vector<std::uint16_t> candidates;
  for (std::uint16_t i = 0; i < planet.square_count && candidates.size() < 100; ++i) {
    if (squares[i].item == kNoItem && planet_can_place_item(session, planet, item, i)) {
      candidates.push_back(i);
    }
  }
  if (candidates.empty()) {
    return kNoSquare;
  }
  if (session.game->variant != Variant::kAntagonizer) {
    return candidates[static_cast<std::size_t>(session.rng.rand()) % candidates.size()];
  }
  return pick_scored_square(session, planet, item, candidates);
}

std::uint16_t planet_auto_build_item(Session& session, Planet& planet, std::uint8_t item) {
  const std::uint16_t square = planet_pick_random_free_square(session, planet, item);
  if (square != kNoSquare && planet_squares(session.game->cosmos, planet)[square].item == kNoItem) {
    planet_build_item_now(session, planet, square, item);
  }
  return square;
}

}  // namespace oa::game
