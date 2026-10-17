#include "game/battle.h"

#include <array>

#include "core/fixed_width.h"
#include "game/battle_helpers.h"
#include "game/cosmos.h"
#include "game/float_math.h"
#include "game/planet.h"
#include "game/race.h"
#include "game/race_ai.h"
#include "game/raceneg.h"
#include "game/ship.h"
#include "game/tables.h"

namespace oa::game {

using namespace battle_detail;

namespace {

// The evasive gizmos (0x867ec) and their target kinds (0x867f8): 69
// Invulnerablizer, 63 Toroidal Blaster, 50 Backfirer, 46 Gravimetric
// Catapult, 45 Positron Bouncer, 38 Smart Bomb, 37 Disarmer, 36 Recaller,
// 35 Brunswik Dissipator, 33 Molecular Tie Down, then a 0 (no category-5
// gizmo is 0: never matches).
constexpr std::array<std::int8_t, 11> kEvasiveGizmos = {69, 63, 50, 46, 45, 38, 37, 36, 35, 33, 0};
constexpr std::array<std::uint8_t, 11> kEvasiveTargets = {kTargetNone, kTargetNone, kTargetShip, kTargetNone,
                                                          kTargetShip, kTargetNone, kTargetShip, kTargetNone,
                                                          kTargetShip, kTargetShip, kTargetShip};

constexpr std::uint8_t kGizmoCategorySpecial = 5;

bool race_knows(const Session& session, RaceIndex race, std::uint8_t item) {
  const Research& research = session.game->research;
  return item < research.count && ((research.items[item].known_mask >> (race & 0x1f)) & 1) != 0;
}

// Every ship takes part (PATCH).
bool any_ship(const Ship&) { return true; }

bool is_ai(const Session& session, const Race& race) { return race.index != session.player_race || session.autoplay; }

// The action block +0x62..+0x87, saved and restored around a threat probe.
struct ActionBlock {
  std::uint8_t action;
  RawAddress action_slot;
  std::uint8_t target_kind;
  RawAddress target;
  Vec3 target_point;
  Vec3 action_point;
  std::int32_t action_result;
};

ActionBlock save_action(const Ship& s) {
  return {s.action, s.action_slot, s.target_kind, s.target, s.target_point, s.action_point, s.action_result};
}

void restore_action(Ship& s, const ActionBlock& b) {
  s.action = b.action;
  s.action_slot = b.action_slot;
  s.target_kind = b.target_kind;
  s.target = b.target;
  s.target_point = b.target_point;
  s.action_point = b.action_point;
  s.action_result = b.action_result;
}

// Each built orbital weapon of each planet aims at the nearest hostile ship
// and the first one able to fire is chosen (the planet loops of 0x3d2ac).
bool planets_fire(Session& session, const BattleLists& lists, const std::vector<ShipIndex>& present,
                  std::uint8_t hostile_mask, BattleAction& action) {
  Cosmos& cosmos = session.game->cosmos;
  const PlanItemTable& items = session.tables->planitems;
  for (const PlanetIndex p : lists.planets) {
    Planet& planet = cosmos.planets[static_cast<std::size_t>(p)];
    for (std::uint16_t sq = planet.surface_squares; sq < planet.square_count; ++sq) {
      const std::int32_t square = planet.first_square + sq;
      const Square& s = cosmos.squares[static_cast<std::size_t>(square)];
      if (s.item == kNoItem || (s.flags & kSquareBuilt) == 0 || (items.items.at(s.item).flags & kItemWeapon) == 0) {
        continue;
      }
      float best = kFarAway;
      ShipIndex target = kNone;
      for (const ShipIndex i : present) {
        const Ship& ship = cosmos.ships[static_cast<std::size_t>(i)];
        if (!hostile_owner(hostile_mask, ship)) {
          continue;
        }
        const float d = target_distance(planet.position, ship.position, ship);
        if (d < best) {
          best = d;
          target = i;
        }
      }
      // The original asserts a target exists (some hostile ship is in space).
      if (target == kNone) {
        continue;
      }
      planet.fire_square = square_address(session, square);
      planet.fire_target = ship_address(session, target);
      planet_fire_weapon(session, planet, false);
      if (planet.fire_result & 1) {
        action = {BattleActor::kPlanet, p};
        return true;
      }
    }
  }
  return false;
}

// A move order to `base + (rand() % m + a, rand() % m + a, rand() % m + a)`
// (x, y, z drawn in that order).
void move_near(Session& session, Ship& ship, const Vec3& base, std::int32_t m, std::int32_t a) {
  Vec3 point = base;
  for (float& c : point) {
    c = f32(static_cast<double>(c) + static_cast<double>(static_cast<float>(session.rng.rand() % m + a)));
  }
  set_action(ship, 0, 0, kTargetPoint, 0);
  ship.target_point = point;
}

}  // namespace

namespace battle_detail {

bool carries(const Ship& ship, std::int8_t gizmo) {
  for (std::int32_t s = 0; s < ship.slot_count; ++s) {
    if (ship.slots[static_cast<std::size_t>(s)].gizmo == gizmo) {
      return true;
    }
  }
  return false;
}

bool hostile_owner(std::uint8_t hostile_mask, const Ship& ship) {
  return ((1u << (static_cast<std::uint32_t>(ship.owner) & 0x1f)) & hostile_mask) != 0;
}

void set_action(Ship& ship, std::uint8_t action, RawAddress slot, std::uint8_t target_kind, RawAddress target) {
  ship.action = action;
  ship.action_slot = slot;
  ship.target_kind = target_kind;
  ship.target = target;
}

BattleAction ship_action(const Cosmos& cosmos, const Ship& ship) {
  return {BattleActor::kShip, ship_index(cosmos, ship)};
}

float target_distance(const Vec3& a, const Vec3& b, const Ship& target) {
  const float d = f32(length(sub(a, b)));
  return target.location_kind == ShipLocation::kSystem ? d : f32(static_cast<double>(d) + kNotInSpacePenalty);
}

bool ships_fire(Session& session, const BattleLists& lists, const std::vector<ShipIndex>& present,
                std::uint8_t hostile_mask, BattleAction& action, Ship*& unarmed, Ship*& out_of_range,
                bool (*eligible)(const Ship&)) {
  Cosmos& cosmos = session.game->cosmos;
  for (const ShipIndex i : lists.ships) {
    Ship& ship = cosmos.ships[static_cast<std::size_t>(i)];
    if (!eligible(ship) || ship.location_kind != ShipLocation::kSystem || ship.power == 0) {
      continue;
    }
    if (!ship_has_ready_weapon(session, ship)) {
      unarmed = &ship;
      continue;
    }
    for (const ShipIndex j : present) {
      const Ship& enemy = cosmos.ships[static_cast<std::size_t>(j)];
      if (!hostile_owner(hostile_mask, enemy)) {
        continue;
      }
      if (ship_ai_select_weapon(session, ship, kSelectShip, ship_address(session, j)) != -1) {
        action = ship_action(cosmos, ship);
        return true;
      }
      out_of_range = &ship;
    }
  }
  return false;
}

bool use_support_gizmos(Session& session, Ship* out_of_range, Ship* unarmed, BattleAction& action) {
  Cosmos& cosmos = session.game->cosmos;
  const auto use = [&](Ship& ship, std::int32_t slot) {
    set_action(ship, 1, gizmo_slot_address(session, ship_index(cosmos, ship), slot), kTargetNone, ship.target);
    action = ship_action(cosmos, ship);
    ship_do_action(session, ship, false);
    return (ship.action_result & kActionPossible) != 0;
  };
  if (out_of_range != nullptr) {
    const std::int32_t slot = ship_find_gizmo(*out_of_range, kGizmoAccutron);
    if (slot != -1 && (out_of_range->battle_flags & kShipAccutron) == 0 && out_of_range->power > 12 &&
        use(*out_of_range, slot)) {
      return true;
    }
  }
  if (unarmed != nullptr) {
    const std::int32_t slot = ship_find_gizmo(*unarmed, kGizmoReplenisher);
    if (slot != -1 && unarmed->power > 13 && use(*unarmed, slot)) {
      return true;
    }
  }
  return false;
}

bool can_hit(Session& session, Ship& enemy, const Ship& ship) {
  const ActionBlock saved = save_action(enemy);
  const bool hit = ship_ai_select_weapon(session, enemy, kSelectShip,
                                         ship_address(session, ship_index(session.game->cosmos, ship))) != -1;
  restore_action(enemy, saved);
  return hit;
}

bool use_evasive_gizmo(Session& session, Ship& ship, Ship& threat, std::span<const std::int8_t> gizmos,
                       std::span<const std::uint8_t> kinds) {
  if (ship.power <= ship_battle_power(session, ship) / 2) {
    return false;
  }
  const Cosmos& cosmos = session.game->cosmos;
  const ShipIndex self = ship_index(cosmos, ship);
  const RawAddress threat_raw = ship_address(session, ship_index(cosmos, threat));
  ship.action = 1;
  for (std::int32_t s = 0; s < ship.slot_count; ++s) {
    const std::int8_t gizmo = ship.slots[static_cast<std::size_t>(s)].gizmo;
    if (gizmo < 0 || session.tables->gizmos.at(static_cast<std::size_t>(gizmo)).category != kGizmoCategorySpecial) {
      continue;
    }
    for (std::size_t k = 0; k < gizmos.size(); ++k) {
      if (gizmo != gizmos[k]) {
        continue;
      }
      ship.action_slot = gizmo_slot_address(session, self, s);
      ship.target_kind = kinds[k];
      ship.target = threat_raw;
      ship_do_action(session, ship, false);
      if (ship.action_result & kActionPossible) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace battle_detail

bool race_battle_turn(Session& session, Race& race, Star& star, BattleAction& action) {
  static constexpr BattleTurnRules kStandard = {
      .tactical = race_ai_tactical,
      .ship_orders = race_ai_ship_orders,
      .recall_at_any_threat = false,
      .tactical_zero_continues = false,
      .only_invasion_ships_wait = false,
  };
  return race_battle_turn_with(session, race, star, action, kStandard);
}

bool race_battle_turn_with(Session& session, Race& race, Star& star, BattleAction& action,
                           const BattleTurnRules& rules) {
  Cosmos& cosmos = session.game->cosmos;
  const std::uint8_t present_mask = static_cast<std::uint8_t>(star.ships_mask | star.owner_mask);
  const std::uint8_t self_bit = static_cast<std::uint8_t>(1u << (race.index & 0x1f));
  if ((present_mask & self_bit) == 0) {
    return false;
  }
  race_reset_ship_moves_at_star(session, race, star);
  BattleLists lists;
  bool has_orders = false;
  for (const ShipIndex i : cosmos_ships_at_star(cosmos, star)) {
    const Ship& ship = cosmos.ships[static_cast<std::size_t>(i)];
    if (ship.owner == race.index && ship.power > 0 && ship.location_kind != ShipLocation::kBuilding) {
      lists.power = static_cast<std::int16_t>(lists.power + static_cast<std::int16_t>(ship.power));
      lists.moves = static_cast<std::int16_t>(lists.moves + static_cast<std::int16_t>(ship.moves));
      lists.ships.push_back(i);
      if (ship.destination_kind != ShipOrder::kNothing) {
        has_orders = true;
      }
    }
  }
  for (std::int16_t k = 0; k < star.planet_count; ++k) {
    const PlanetIndex p = star.planets[static_cast<std::size_t>(k)];
    if (cosmos.planets[static_cast<std::size_t>(p)].owner == race.index) {
      lists.planets.push_back(p);
    }
  }
  if (lists.ships.empty() && lists.planets.empty()) {
    return false;
  }

  // Recall home: an AI away from home that knows the Recaller, when the
  // home has no colony left, or holds an enemy and the forces here (sic:
  // counted at this star) are too weak.
  const StarIndex here = star_index(cosmos, star);
  const std::uint8_t recaller_req = session.tables->gizmos.at(kGizmoRecaller).research_requirement;
  if (here != race.home && !lists.ships.empty() && race.index != session.player_race &&
      race_knows(session, race.index, recaller_req)) {
    const std::uint8_t war = race_at_war_mask(session, race);
    const Star& home = cosmos.stars[static_cast<std::size_t>(race.home)];
    bool recall = (home.owner_mask & 0x7f) == 0;
    if (!recall && ((home.ships_mask | home.owner_mask) & war) != 0 && rules.recall_at_any_threat) {
      recall = true;
    } else if (!recall && ((home.ships_mask | home.owner_mask) & war) != 0) {
      std::int32_t enemy = 0;
      std::int32_t own = 0;
      const std::int32_t enemies = race_count_ships_at_star(session, race, kWar, star, &enemy);
      const std::int32_t friends = race_count_ships_at_star(session, race, kAlliance, star, &own);
      enemy = enemy < 1 ? 1 : enemy > 99999 ? 99999 : enemy;
      if (own < enemy * 2 || friends < enemies) {
        recall = true;
      }
    }
    if (recall) {
      for (const ShipIndex i : lists.ships) {
        Ship& ship = cosmos.ships[static_cast<std::size_t>(i)];
        if (ship.location_kind != ShipLocation::kSystem || ship.power == 0) {
          continue;
        }
        const std::int32_t slot = ship_find_gizmo(ship, kGizmoRecaller);
        if (slot == -1) {
          continue;
        }
        set_action(ship, 1, gizmo_slot_address(session, i, slot), kTargetNone, ship.target);
        if (ship_do_action(session, ship, false) & kActionPossible) {
          action = ship_action(cosmos, ship);
          return true;
        }
      }
    }
  }

  // Hostility of the present races (slots 0..6, not `race_count`).
  bool hostile = false;
  for (std::uint8_t r = 0; r < kMaxRaces; ++r) {
    const std::uint8_t bit = static_cast<std::uint8_t>(1u << r);
    if ((present_mask & bit) == 0) {
      continue;
    }
    std::uint8_t entry = r;
    if (star.ships_mask & bit) {
      entry |= kBattleRaceShips;
    }
    if (star.owner_mask & bit) {
      entry |= kBattleRaceColony;
    }
    if (race.relation[r] == kWar) {
      // The player fights a race at war only where that race has ships.
      if (race.index != session.player_race || session.autoplay || (star.ships_mask & bit) != 0) {
        hostile = true;
        entry |= kBattleRaceHostile;
      }
    } else if (race.index != session.player_race && r == session.player_race &&
               (session.star_day_flags[static_cast<std::size_t>(here)] & self_bit) != 0) {
      // The player offended this AI here today.
      hostile = true;
      entry |= kBattleRaceHostile;
    }
    lists.races.push_back(entry);
  }
  // An AI also fights the enemies of its allies present (that it is not
  // allied with), when nothing else is hostile.
  if (lists.races.size() != 1 && !hostile && is_ai(session, race)) {
    for (const std::uint8_t ally : lists.races) {
      if (race.relation[ally & 7] != kAlliance) {
        continue;
      }
      const Race& friend_race = cosmos.races[ally & 7];
      for (std::uint8_t& other : lists.races) {
        if (friend_race.relation[other & 7] == kWar && race.relation[other & 7] != kAlliance) {
          hostile = true;
          other |= kBattleRaceHostile;
        }
      }
    }
  }
  if (hostile && (is_ai(session, race) || session.screen == 16)) {
    const std::int32_t r = rules.tactical(session, race, star, lists, action);
    if (r == -1) {
      return true;
    }
    if (r == 0 && !rules.tactical_zero_continues) {
      return false;
    }
  }

  // Standing orders: idle AI ships get orders, the ship with an order and
  // the most power acts.
  if (race.index == session.player_race && !has_orders && !session.autoplay) {
    return false;
  }
  std::int16_t power = lists.power;
  Ship* actor = nullptr;
  std::int16_t best = 0;
  for (const ShipIndex i : lists.ships) {
    Ship& ship = cosmos.ships[static_cast<std::size_t>(i)];
    if (ship.destination_kind == ShipOrder::kNothing && is_ai(session, race)) {
      rules.ship_orders(session, race, ship);
    }
    if (best < ship.power && ship.destination_kind != ShipOrder::kNothing) {
      best = static_cast<std::int16_t>(ship.power);
      actor = &ship;
    }
  }
  if (actor == nullptr) {
    return false;
  }
  Ship& ship = *actor;
  switch (ship.destination_kind) {
    case ShipOrder::kGoStar: {
      if (ship.location_kind == ShipLocation::kOrbit) {
        planet_undock_ship(session, cosmos.planets[static_cast<std::size_t>(ship.location)], ship);
      }
      if (race.index != session.player_race && star_lane_end_guarded(session, star, race.index) &&
          (!rules.only_invasion_ships_wait || (ship.has_colonizer != -1 && carries(ship, kGizmoInvasionModule)))) {
        return false;
      }
      const StarIndex dest = ship.destination;
      if (dest == here) {
        ship_clear_order_code(session, ship);
        if (ship.owner != session.player_race || session.autoplay) {
          rules.ship_orders(session, race, ship);
        }
        return true;
      }
      // The first lane whose far end is one hop nearer (the last lane when
      // none is: the original asserts).
      const auto hops = [&](StarIndex a, StarIndex b) {
        return static_cast<std::int16_t>(cosmos.hops[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)]);
      };
      LaneIndex lane = kNone;
      for (std::int16_t k = 0; k < star.lane_count; ++k) {
        lane = star.lanes[static_cast<std::size_t>(k)];
        const Lane& l = cosmos.lanes[static_cast<std::size_t>(lane)];
        const StarIndex far = l.star_a == here ? l.star_b : l.star_a;
        if (hops(far, dest) == hops(here, dest) - 1) {
          break;
        }
      }
      if (lane == kNone) {
        break;
      }
      const RawAddress lane_raw = lane_address(session, lane);
      if ((cosmos.lanes[static_cast<std::size_t>(lane)].flags & 2) == 0 ||
          ship_ai_select_weapon(session, ship, kSelectLane, lane_raw) == -1) {
        set_action(ship, 0, 0, kTargetLane, lane_raw);
      }
      action = ship_action(cosmos, ship);
      break;
    }
    case ShipOrder::kColonize: {
      Planet& planet = cosmos.planets[static_cast<std::size_t>(ship.destination)];
      if (planet.owner != kNoOwner) {
        ship_clear_order_code(session, ship);
      } else if (ship.location_kind == ShipLocation::kOrbit) {
        if (ship.location == ship.destination) {
          // Colonises at once (the destination index is kept).
          const std::int32_t colonizer = ship_find_gizmo(ship, kGizmoColonizer);
          if (colonizer != -1) {
            ship_remove_gizmo(session, ship, colonizer);
          }
          planet.population = 2;
          planet.owner = race.index;
          planet.acquired_day = cosmos.day;
          planet_auto_place_item(session, planet, kItemColonyBase, -1);
          ship.power -= 1;
          power = static_cast<std::int16_t>(power - 1);
          star.owner_mask |= self_bit;
          ship_clear_order_code(session, ship);
        } else {
          planet_undock_ship(session, cosmos.planets[static_cast<std::size_t>(ship.location)], ship);
        }
      } else {
        set_action(ship, 0, 0, kTargetPlanet, planet_address(session, ship.destination));
        action = ship_action(cosmos, ship);
      }
      break;
    }
    case ShipOrder::kOrbitPlanet:
      if (ship.location_kind == ShipLocation::kOrbit) {
        ship_clear_order_code(session, ship);
        if (session.screen != 16) {
          session.post_event(EventCode::kEnteredOrbit, ship_index(cosmos, ship), ship.destination);
        }
      } else if (ship.location_kind == ShipLocation::kSystem) {
        set_action(ship, 0, 0, kTargetPlanet, planet_address(session, ship.destination));
        action = ship_action(cosmos, ship);
      } else {
        ship_clear_order_code(session, ship);
      }
      break;
    case ShipOrder::kGoSpace:
      if ((ship.action_result & kActionReached) == 0 && ship.action == 0) {
        ship.target_kind = kTargetPoint;
        action = ship_action(cosmos, ship);
      } else {
        ship_clear_order_code(session, ship);
      }
      break;
    default:
      // Battle-only orders: the ship stops for the day.
      ship.moves = 0;
      power = static_cast<std::int16_t>(power - static_cast<std::int16_t>(ship.power));
      ship.power = 0;
      break;
  }
  return power > 0;
}

std::int32_t race_ai_tactical(Session& session, Race& race, Star& star, const BattleLists& lists,
                              BattleAction& action) {
  Cosmos& cosmos = session.game->cosmos;
  const StarIndex here = star_index(cosmos, star);
  // Fight or flee: home bonus, +20 per own planet.
  const std::uint32_t h = cosmos.hops[static_cast<std::size_t>(race.home)][static_cast<std::size_t>(here)];
  std::int32_t bonus = h < 2 ? 99999 : h < 4 ? static_cast<std::int32_t>(h) * 75 : 0;
  bonus += static_cast<std::int32_t>(lists.planets.size()) * 20;
  std::int32_t enemy = 0;
  std::int32_t own = 0;
  race_count_ships_at_star(session, race, kWar, star, &enemy);
  race_count_ships_at_star(session, race, kAlliance, star, &own);
  bool fight = wrap_i32(static_cast<std::int64_t>(own) + bonus) > enemy;
  const std::uint8_t player_bit = static_cast<std::uint8_t>(1u << (session.player_race & 0x1f));
  if (((star.ships_mask | star.owner_mask) & player_bit) == 0) {
    fight = true;
  }
  if (race.index == session.player_race && !session.autoplay) {
    fight = true;
  }
  std::uint8_t hostile_mask = 0;
  for (const std::uint8_t entry : lists.races) {
    if (entry & kBattleRaceHostile) {
      hostile_mask = static_cast<std::uint8_t>(hostile_mask | (1u << (entry & 7)));
    }
  }
  const std::vector<ShipIndex> present = cosmos_ships_at_star(cosmos, star);
  bool hostile_in_space = false;
  for (const ShipIndex i : present) {
    const Ship& ship = cosmos.ships[static_cast<std::size_t>(i)];
    if (ship.location_kind == ShipLocation::kSystem && hostile_owner(hostile_mask, ship)) {
      hostile_in_space = true;
      break;
    }
  }
  if (!hostile_in_space && race.index == session.player_race && !session.autoplay) {
    return 2;
  }

  if (lists.ships.empty()) {
    // Planets only: their orbital weapons fire at the nearest hostile ship.
    if (hostile_in_space && planets_fire(session, lists, present, hostile_mask, action)) {
      return -1;
    }
    return 0;
  }

  Ship* unarmed = nullptr;
  Ship* out_of_range = nullptr;
  if (fight) {
    if (!hostile_in_space) {
      // Attack the first hostile planet of the star.
      Planet* target = nullptr;
      for (std::int16_t k = 0; k < star.planet_count && target == nullptr; ++k) {
        Planet& planet = cosmos.planets[static_cast<std::size_t>(star.planets[static_cast<std::size_t>(k)])];
        const std::uint8_t owner = planet.owner;
        if (owner != kNoOwner && owner != race.index &&
            (race.relation[owner] == kWar ||
             (owner == session.player_race &&
              (session.star_day_flags[static_cast<std::size_t>(here)] & (1u << (race.index & 0x1f))) != 0))) {
          target = &planet;
        }
      }
      if (target == nullptr) {
        return 0;
      }
      Planet& planet = *target;
      // Invade with an own ship docked in its orbit.
      for (std::uint16_t sq = planet.surface_squares; sq < planet.square_count; ++sq) {
        if (cosmos.squares[static_cast<std::size_t>(planet.first_square + sq)].item != kItemShip) {
          continue;
        }
        Ship& docked = cosmos.ships[static_cast<std::size_t>(planet_square_ship(session, planet, sq))];
        if (docked.owner == race.index) {
          std::int32_t used = 0;
          planet_invade(session, planet, sq, used);
          planet_undock_ship(session, planet, docked);
          return -1;
        }
      }
      const RawAddress planet_raw = planet_address(session, planet_index(cosmos, planet));
      if (!planet_has_orbital_guard(session, planet)) {
        // Unguarded: the first ship in space with an Invasion Module goes
        // into its orbit.
        for (const ShipIndex i : lists.ships) {
          Ship& ship = cosmos.ships[static_cast<std::size_t>(i)];
          if (ship.location_kind == ShipLocation::kSystem && carries(ship, kGizmoInvasionModule)) {
            action = ship_action(cosmos, ship);
            set_action(ship, 0, 0, kTargetPlanet, planet_raw);
            return -1;
          }
        }
        return (star.ships_mask & player_bit) == 0 ? 2 : 0;
      }
      // Guarded: the first armed ship able to bombard it fires, else the
      // last armed one moves next to it.
      Ship* armed = nullptr;
      for (const ShipIndex i : lists.ships) {
        Ship& ship = cosmos.ships[static_cast<std::size_t>(i)];
        if (ship.location_kind != ShipLocation::kSystem || !ship_has_ready_weapon(session, ship)) {
          continue;
        }
        armed = &ship;
        if (ship_ai_select_weapon(session, ship, kSelectPlanet, planet_raw) != -1) {
          action = ship_action(cosmos, ship);
          return -1;
        }
      }
      if (armed == nullptr) {
        return 0;
      }
      action = ship_action(cosmos, *armed);
      move_near(session, *armed, planet.position, 100, 20);
      return -1;
    }

    // Shields up on the own ships in space a hostile ship could hit, down on
    // the others.
    Ship* threatened = nullptr;
    Ship* threat = nullptr;
    for (const ShipIndex i : lists.ships) {
      Ship& ship = cosmos.ships[static_cast<std::size_t>(i)];
      if (ship.location_kind != ShipLocation::kSystem) {
        continue;
      }
      for (const ShipIndex j : present) {
        Ship& enemy_ship = cosmos.ships[static_cast<std::size_t>(j)];
        if (!hostile_owner(hostile_mask, enemy_ship)) {
          continue;
        }
        if (can_hit(session, enemy_ship, ship)) {
          ship_set_all_shields(session, ship, true);
          threatened = &ship;
          threat = &enemy_ship;
        }
      }
      if (&ship != threatened) {
        ship_set_all_shields(session, ship, false);
      }
    }
    if (ships_fire(session, lists, present, hostile_mask, action, unarmed, out_of_range, any_ship)) {
      return -1;
    }
    if (use_support_gizmos(session, out_of_range, unarmed, action)) {
      return -1;
    }
    if (threatened != nullptr && ship_ai_use_evasive_gizmo(session, *threatened, *threat)) {
      action = ship_action(cosmos, *threatened);
      return -1;
    }
    // The strongest ship (hit points x damage x power) closes in; original
    // quirk: without a positive value the threatened ship is kept.
    Ship* mover = threatened;
    std::int32_t best = 0;
    for (const ShipIndex i : lists.ships) {
      Ship& ship = cosmos.ships[static_cast<std::size_t>(i)];
      const std::int32_t v = wrap_i32(static_cast<std::int64_t>(ship.hit_points) * ship.weapon_damage * ship.power);
      if (ship.location_kind == ShipLocation::kSystem && v > best) {
        best = v;
        mover = &ship;
      }
    }
    if (mover == nullptr) {
      return 0;
    }
    if (!lists.planets.empty() && planets_fire(session, lists, present, hostile_mask, action)) {
      return -1;
    }
    float nearest = kFarAway;
    const Ship* goal = nullptr;
    for (const ShipIndex j : present) {
      const Ship& enemy_ship = cosmos.ships[static_cast<std::size_t>(j)];
      if (!hostile_owner(hostile_mask, enemy_ship)) {
        continue;
      }
      const float d = target_distance(enemy_ship.position, mover->position, enemy_ship);
      if (d < nearest && kApproachMinimum < d) {
        nearest = d;
        goal = &enemy_ship;
      }
    }
    if (goal == nullptr) {
      return 0;
    }
    action = ship_action(cosmos, *mover);
    move_near(session, *mover, goal->position, 200, 100);
    return -1;
  }

  // Fleeing: the first threatened ship raises its shields and tries an
  // evasive gizmo.
  Ship* threatened = nullptr;
  Ship* threat = nullptr;
  for (const ShipIndex i : lists.ships) {
    Ship& ship = cosmos.ships[static_cast<std::size_t>(i)];
    if (ship.location_kind != ShipLocation::kSystem) {
      continue;
    }
    for (const ShipIndex j : present) {
      Ship& enemy_ship = cosmos.ships[static_cast<std::size_t>(j)];
      if (hostile_owner(hostile_mask, enemy_ship) && can_hit(session, enemy_ship, ship)) {
        threatened = &ship;
        threat = &enemy_ship;
        break;
      }
    }
    if (threatened != nullptr) {
      break;
    }
  }
  if (threatened != nullptr) {
    ship_set_all_shields(session, *threatened, true);
    if (ship_ai_use_evasive_gizmo(session, *threatened, *threat)) {
      action = ship_action(cosmos, *threatened);
      return -1;
    }
  } else {
    // Original quirk: the firing step only runs when no ship is threatened.
    if (ships_fire(session, lists, present, hostile_mask, action, unarmed, out_of_range, any_ship)) {
      return -1;
    }
  }
  if (use_support_gizmos(session, out_of_range, unarmed, action)) {
    return -1;
  }
  // The first ship in orbit (undocked) or in space heads for the star at the
  // other end of the system's first lane.
  const Lane& first_lane = cosmos.lanes[static_cast<std::size_t>(star.lanes[0])];
  const StarIndex refuge = first_lane.star_a == here ? first_lane.star_b : first_lane.star_a;
  for (const ShipIndex i : lists.ships) {
    Ship& ship = cosmos.ships[static_cast<std::size_t>(i)];
    if (ship.location_kind == ShipLocation::kOrbit) {
      planet_undock_ship(session, cosmos.planets[static_cast<std::size_t>(ship.location)], ship);
    } else if (ship.location_kind != ShipLocation::kSystem) {
      continue;
    }
    ship.destination_kind = ShipOrder::kGoStar;
    ship.destination = refuge;
    return 2;
  }
  return 0;
}

bool ship_ai_use_evasive_gizmo(Session& session, Ship& ship, Ship& threat) {
  return use_evasive_gizmo(session, ship, threat, kEvasiveGizmos, kEvasiveTargets);
}

}  // namespace oa::game
