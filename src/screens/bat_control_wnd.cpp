#include "screens/bat_control_wnd.h"

#include <stdexcept>
#include <string>

#include "game/address.h"
#include "game/cosmos.h"
#include "game/planet.h"
#include "game/ship.h"
#include "gfx/vfx_poly.h"
#include "screens/bat_mode_wnd.h"

namespace oa {

namespace {

// `gwshare.txt` tables.
constexpr int kShipShapes = 14;
constexpr int kGizmoShapes = 28;
constexpr int kPlanetShapes = 29;
constexpr int kPlanItemShapes = 30;
constexpr int kRaceFlags = 32;
constexpr int kRaceStatShapes = 35;

// The panel's top (`BatControlWnd_draw`): the wiped area, the frames of
// `16BATTLE.SHP` laid on it (the system, a ship, a planet), the title box.
constexpr Pane kPanelPane{0x1d2, 0, 0x27f, 0x8d};
constexpr int kFrameSystem = 10;
constexpr int kFrameShip = 11;
constexpr int kFramePlanet = 19;
constexpr Pane kTitlePane{0x1da, 7, 0x278, 0x25};
// The sun's icon in the title box (scale 0.2) and the texts.
constexpr std::int32_t kSunScale = 0x3333;
constexpr int kTitleTextX = 0x28;
constexpr int kTitleWidth = 0x6c;
// The list's rows.
constexpr int kRowHeight = 0x54;

// The static strings and colours of the rows.
constexpr int kStringDepleted = 0;
constexpr int kStringNoPower = 1;
constexpr int kStringStarTypes = 0xb2;
constexpr std::uint8_t kHotBack = 0x96;
constexpr std::uint8_t kShipNameColor = 0x0f;
// The haze tables of the ship sprites (`windows.txt` HAZEBRBLES): no power,
// damaged, the species' shades; the unusable gizmos and weapons.
constexpr std::size_t kHazeGrey = 22;
constexpr std::size_t kHazeRed = 8;
constexpr std::size_t kHazeDark3 = 3;
constexpr std::size_t kHazeDark6 = 6;
constexpr std::size_t kHazeUnusable = 7;

// The gizmo flags of the list.
constexpr std::uint16_t kGizmoToggleable = 0x20;
constexpr std::uint16_t kGizmoNoTarget = 0x40;
constexpr std::uint16_t kGizmoPassive = 0x80;
// The planet item flag of the weapons.
constexpr std::uint16_t kItemWeaponFlag = 4;

// The list rows' data: bit 31 a planet; the statuses of the gizmos.
constexpr int kDataPlanet = static_cast<int>(0x80000000U);
constexpr int kStatusForeign = 1;
constexpr int kStatusDepleted = 2;
constexpr int kStatusNoPower = 3;

std::uint8_t color_base(const game::Cosmos& c, int race) {
  return static_cast<std::uint8_t>(c.races[static_cast<std::size_t>(race)].color * 4 + 0x13);
}

bool player_present(const game::Session& s) {
  const game::Cosmos& c = s.game->cosmos;
  const game::Star& star = c.stars[static_cast<std::size_t>(c.selected_star)];
  return ((star.owner_mask | star.ships_mask) & s.player_mask) != 0;
}

// `draw_rect_frame` (0x2b294) outline mode.
void rect_frame(Image8& screen, const Pane& pane, int x0, int y0, int x1, int y1, std::uint8_t color) {
  vfx_line_draw(screen, pane, x0, y0, x1, y0, color);
  vfx_line_draw(screen, pane, x0, y1, x1, y1, color);
  vfx_line_draw(screen, pane, x0, y0, x0, y1, color);
  vfx_line_draw(screen, pane, x1, y0, x1, y1, color);
}

}  // namespace

BatControlWnd::BatControlWnd(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes)
    : GWindow(wm, record, shapes) {}

BatModeWnd* BatControlWnd::mode_wnd() const { return dynamic_cast<BatModeWnd*>(wm_.find_window("BatModeWnd")); }

ListBox* BatControlWnd::list() const { return dynamic_cast<ListBox*>(wm_.find_window("BatListWnd")); }

void BatControlWnd::activate(int a, int b) {
  game::Session* s = core();
  ListBox* rows = list();
  if (s == nullptr || rows == nullptr || mode_wnd() == nullptr) {
    return;
  }
  rows->set_style(0);
  rows->set_colors(0x31, 0, 0);
  rows->clear();
  active_ = true;
  shown_ = true;
  repeat_ship_ = -1;
  game::Cosmos& c = s->game->cosmos;
  battle_.open(*s, c.stars.at(static_cast<std::size_t>(c.selected_star)));
  list_mode_ = player_present(*s) ? kListMine : kListOthers;
  category_ = 0;
  broadcast_to_children(kEventActivate, a, b);
  update_category_buttons();
  draw(kDrawNormal);
}

void BatControlWnd::update_category_buttons() {
  std::size_t first = 0;
  while (first < children_.size() && children_[first]->name() != "My^Items") {
    ++first;
  }
  if (first >= children_.size()) {
    throw std::logic_error("star system: no My^Items button");
  }
  for (std::size_t i = 0; i < 9 && first + i < children_.size(); ++i) {
    bool on = false;
    if (list_mode_ == kListGizmos) {
      on = i >= 5;
    } else if (list_mode_ != kListOrbital) {
      on = i < 5;
    }
    children_[first + i]->set_enabled(on);
  }
}

void BatControlWnd::select(int kind, int object) {
  action_ = kActionMove;
  action_slot_ = 0;
  selection_ = {kind, object};
  switch (kind) {
    case kObjectNone:
      list_mode_ = kListMine;
      break;
    case kObjectPlanet:
      list_mode_ = kListOrbital;
      break;
    case kObjectShip:
      list_mode_ = kListGizmos;
      break;
    default:
      throw std::logic_error("star system: selection of a bad kind");
  }
  update_category_buttons();
  draw(kDrawNormal);
}

bool BatControlWnd::check_target(int kind, int object) {
  game::Session* s = core();
  repeat_ship_ = -1;
  if (s == nullptr) {
    return false;
  }
  game::Cosmos& c = s->game->cosmos;
  if (selection_.kind == kObjectShip && kind != kObjectNone) {
    game::Ship& ship = c.ships[static_cast<std::size_t>(selection_.object)];
    std::uint8_t target_kind = game::kTargetNone;
    game::RawAddress target = ship.target;  // a stack slot left as it was
    if (kind == kObjectPlanet) {
      target_kind = game::kTargetPlanet;
      target = game::planet_address(*s, static_cast<game::PlanetIndex>(object));
    } else if (kind == kObjectShip) {
      target_kind = game::kTargetShip;
      target = game::ship_address(*s, static_cast<game::ShipIndex>(object));
    } else if (kind == kObjectLane && action_ == kActionGizmo) {
      target_kind = game::kTargetLane;
      target = game::lane_address(*s, static_cast<game::LaneIndex>(object));
    }
    ship.action = 1;
    ship.action_slot =
        game::gizmo_slot_address(*s, static_cast<game::ShipIndex>(selection_.object), action_slot_);
    ship.target_kind = target_kind;
    ship.target = target;
    ship.target_point = {};
    ship.action_point = {};
    game::ship_do_action(*s, ship, false);
    return (ship.action_result & 1) != 0;
  }
  if (selection_.kind == kObjectPlanet && kind == kObjectShip) {
    game::Planet& planet = c.planets[static_cast<std::size_t>(selection_.object)];
    planet.fire_square = game::square_address(*s, planet.first_square + action_slot_);
    planet.fire_target = game::ship_address(*s, static_cast<game::ShipIndex>(object));
    game::planet_fire_weapon(*s, planet, false);
    return (planet.fire_result & 1) != 0;
  }
  return false;
}

void BatControlWnd::execute(int kind, int object, const game::Vec3* point) {
  game::Session* s = core();
  BatModeWnd* view = mode_wnd();
  repeat_ship_ = -1;
  if (s == nullptr || view == nullptr) {
    return;
  }
  if (battle_.cursor() != 0) {
    throw std::logic_error("star system: an action out of the player's turn");
  }
  game::Cosmos& c = s->game->cosmos;
  if (selection_.kind == kObjectShip) {
    game::Ship& ship = c.ships[static_cast<std::size_t>(selection_.object)];
    ship.destination_kind = game::ShipOrder::kNothing;
    if (action_ == kActionMove) {
      game::Vec3 target_point{};
      game::RawAddress target = ship.target;
      std::uint8_t target_kind = ship.target_kind;
      if (kind == kObjectPoint) {
        target_kind = game::kTargetPoint;
        target_point = point != nullptr ? *point : view->move_point();
        repeat_point_ = target_point;
        ship.destination_kind = game::ShipOrder::kGoSpace;
        ship.destination = 0;
      } else if (kind == kObjectPlanet) {
        target_kind = game::kTargetPlanet;
        ship.destination_kind = game::ShipOrder::kOrbitPlanet;
        ship.destination = static_cast<std::int16_t>(object);
        target = game::planet_address(*s, static_cast<game::PlanetIndex>(object));
      } else if (kind == kObjectLane) {
        target_kind = game::kTargetLane;
        ship.destination_kind = game::ShipOrder::kGoStar;
        const game::Lane& lane = c.lanes[static_cast<std::size_t>(object)];
        ship.destination = lane.star_a == c.selected_star ? lane.star_b : lane.star_a;
        target = game::lane_address(*s, static_cast<game::LaneIndex>(object));
      }
      ship.action = 0;
      ship.target_kind = target_kind;
      ship.target = target;
      ship.target_point = target_point;
      ship.action_point = {};
      game::ship_do_action(*s, ship, false);
      if ((ship.action_result & game::kActionReached) == 0) {
        repeat_ship_ = selection_.object;
        repeat_object_ = object;
        repeat_kind_ = kind;
      }
    }
  }
  list_mode_ = kListMine;
  update_category_buttons();
  action_ = kActionMove;
  action_slot_ = 0;
  draw(kDrawNormal);
  const game::BattleActor actor = selection_.kind == kObjectShip ? game::BattleActor::kShip : game::BattleActor::kPlanet;
  battle_.act(*s, {actor, static_cast<std::int16_t>(selection_.object)}, false);
  view->start_effect();
}

template <typename Turns>
void BatControlWnd::run_turns(Turns turns) {
  const std::uint32_t rounds = battle_.rounds();
  const Selection before = selection_;
  turns();
  if (battle_.rounds() != rounds) {
    // BatControlWnd_new_round 0x12368 redraws the panel within the turns.
    const Selection now = selection_;
    selection_ = before;
    draw(kDrawNormal);
    selection_ = now;
  }
  if (const auto& pending = battle_.pending()) {
    selection_ = {pending->kind == game::BattleActor::kShip ? kObjectShip : kObjectPlanet, pending->index};
    if (BatModeWnd* view = mode_wnd()) {
      view->start_effect();
    }
  }
}

void BatControlWnd::finish_action(bool interactive) {
  game::Session* s = core();
  if (s == nullptr) {
    return;
  }
  if (!interactive) {
    battle_.finish_all(*s);
    selection_ = {};
    return;
  }
  battle_.apply_pending(*s);
  selection_ = {};
  fill_list();
  run_turns([&] { battle_.run_turns(*s); });
}

void BatControlWnd::fill_list() {
  game::Session* s = core();
  ListBox* rows = list();
  if (s == nullptr || rows == nullptr) {
    return;
  }
  const game::Cosmos& c = s->game->cosmos;
  const game::Star& star = c.stars[static_cast<std::size_t>(c.selected_star)];
  rows->clear();
  bool sort = true;
  const auto add = [&](int object, int data) {
    const int row = rows->add_item(std::string(), -1, false);
    rows->set_item_object(row, object);
    rows->set_item_data(row, data);
  };
  switch (list_mode_) {
    case kListMine:
    case kListOthers: {
      const bool mine = list_mode_ == kListMine;
      rows->set_line_height(kRowHeight);
      rows->set_draw_item([this](const Pane& row, const ListBox::Item& item, bool hilite) {
        draw_object_item(row, item.object, (item.data & kDataPlanet) != 0, hilite);
      });
      const std::vector<game::ShipIndex> ships = game::cosmos_ships_at_star(c, star);
      for (std::size_t i = 0; i < ships.size(); ++i) {
        const game::Ship& ship = c.ships[static_cast<std::size_t>(ships[i])];
        if (ship.location_kind == game::ShipLocation::kSystem && (ship.owner == s->player_race) == mine) {
          add(ships[i], static_cast<std::int16_t>(i));
        }
      }
      for (std::int16_t i = 0; i < star.planet_count; ++i) {
        const game::PlanetIndex p = star.planets[static_cast<std::size_t>(i)];
        if ((c.planets[static_cast<std::size_t>(p)].owner == s->player_race) == mine) {
          add(p, i | kDataPlanet);
        }
      }
      break;
    }
    case kListGizmos: {
      if (selection_.kind != kObjectShip) {
        sort = false;
        break;
      }
      rows->set_line_height(kRowHeight);
      rows->set_draw_item([this](const Pane& row, const ListBox::Item& item, bool hilite) {
        draw_gizmo_item(row, item.object, item.data, hilite);
      });
      const game::Ship& ship = c.ships[static_cast<std::size_t>(selection_.object)];
      if (ship.owner == s->player_race || game::ship_detected_by_race(*s, ship, s->player_race) == 2) {
        for (std::int32_t i = 0; i < ship.slot_count; ++i) {
          const game::GizmoSlot& slot = ship.slots[static_cast<std::size_t>(i)];
          if (slot.gizmo == -1) {
            continue;
          }
          const game::Gizmo& g = s->tables->gizmos.at(static_cast<std::size_t>(slot.gizmo));
          const bool passive = (g.flags & kGizmoPassive) != 0;
          if (!((g.category == category_ && !passive) || (category_ == 4 && passive))) {
            continue;
          }
          int status = 0;
          if (ship.owner == s->player_race) {
            if ((g.flags & kGizmoToggleable) == 0) {
              if (g.uses == 0 || slot.uses_left != 0) {
                if (ship.power < g.power) {
                  status = kStatusNoPower;
                }
              } else {
                status = kStatusDepleted;
              }
            }
          } else {
            status = kStatusForeign;
          }
          if (passive) {
            status = slot.gizmo + 500;
          }
          if (g.category == 2) {
            status = slot.gizmo + 600;
          } else if (g.category == 4) {
            status = slot.gizmo + 700;
          }
          add(i, status);
        }
      }
      break;
    }
    case kListOrbital: {
      if (selection_.kind != kObjectPlanet) {
        sort = false;
        break;
      }
      rows->set_line_height(kRowHeight);
      rows->set_draw_item([this](const Pane& row, const ListBox::Item& item, bool hilite) {
        draw_orbital_item(row, item.object, item.data, hilite);
      });
      const game::Planet& planet = c.planets[static_cast<std::size_t>(selection_.object)];
      if (planet.owner == s->player_race) {
        for (std::uint16_t sq = 0; sq < planet.square_count; ++sq) {
          const game::Square& square = c.squares[static_cast<std::size_t>(planet.first_square + sq)];
          if ((square.flags & game::kSquareBuilt) == 0 ||
              (s->tables->planitems.items.at(square.item).flags & kItemWeaponFlag) == 0) {
            continue;
          }
          game::PlanetWeaponStats stats;
          game::planitem_weapon_stats(square.item, stats);
          add(sq, stats.shots != 0 && (square.flags >> 8) == 0 ? 1 : 0);
        }
      }
      break;
    }
    default:
      sort = false;
      break;
  }
  if (sort) {
    // ListItem_compare_value 0x10a14.
    rows->sort([](const ListBox::Item& a, const ListBox::Item& b) {
      if (b.data < a.data) {
        return 1;
      }
      return a.data == b.data ? 0 : -1;
    });
  }
  rows->draw(kDrawNormal);
}

void BatControlWnd::draw_object_item(const Pane& row, int object, bool planet, bool hilite) {
  const game::Session* s = core();
  if (s == nullptr) {
    return;
  }
  const game::Cosmos& c = s->game->cosmos;
  Image8& screen = wm_.screen();
  std::uint8_t back = 0;
  if (hilite) {
    vfx_pane_wipe(screen, row, kHotBack);
    back = kHotBack;
  }
  GameFont& font = wm_.small_font();
  if (planet) {
    const game::Planet& p = c.planets[static_cast<std::size_t>(object)];
    const Shape& shape = wm_.shared_shapes(kPlanetShapes).at(static_cast<std::size_t>(p.size + p.type * 5));
    const PolyPoint at = center_shape(row, shape);
    vfx_shape_draw(screen, row, shape, at.x, at.y);
    int color = kTextColor;
    if (p.owner != game::kNoOwner) {
      const game::Race& owner = c.races[p.owner];
      wm_.draw_colored(row, wm_.shared_shapes(kRaceFlags).at(owner.species), 2, 4, owner.color);
      color = color_base(c, p.owner);
    }
    font.pane() = row;
    font.draw_wrapped(screen, 0x12, 7, game::name_view(p.name), 0, color, 0xff, 0);
    return;
  }
  const game::Ship& ship = c.ships[static_cast<std::size_t>(object)];
  const game::Race& owner = c.races[static_cast<std::size_t>(ship.owner)];
  const Shape& shape = wm_.shared_shapes(kShipShapes + ship.owner).at(ship.hull_size);
  const bool visible = ship.owner == s->player_race || game::ship_detected_by_race(*s, ship, s->player_race) > 0;
  const PolyPoint at = center_shape(row, shape);
  const std::vector<HazeTable>& haze = wm_.services().gsystem.haze_tables();
  const HazeTable* table = nullptr;
  if (visible) {
    if (ship.power < 1) {
      table = &haze.at(kHazeGrey);
    } else if (ship.hit_points * 4 < ship.max_hit_points || ship.hit_points < 5) {
      table = &haze.at(kHazeRed);
    } else if (owner.species == 0) {
      table = &haze.at(kHazeDark3);
    } else if (owner.species != 13) {
      table = &haze.at(kHazeDark6);
    }
  }
  if (table != nullptr) {
    vfx_shape_translate_draw(screen, row, shape, at.x, at.y, *table);
  } else {
    vfx_shape_draw(screen, row, shape, at.x, at.y);
  }
  wm_.draw_colored(row, wm_.shared_shapes(kRaceFlags).at(owner.species), 2, 4, owner.color);
  font.pane() = row;
  font.draw_wrapped(screen, 0x12, 7, game::name_view(ship.name), 0, kShipNameColor, 0xff, 0);
  const ShapeTable& stats_shapes = wm_.shared_shapes(kRaceStatShapes);
  if (!visible) {
    return;
  }
  const std::int32_t experience = game::ship_experience(*s, ship);
  for (int i = 0; i < experience && i < 10; ++i) {
    wm_.draw_colored(row, stats_shapes.at(0), 0x12 + 5 * i, 0x11, owner.color);
  }
  // The five strengths: icons clipped to twice the value.
  const std::array<std::int32_t, 5> values = {ship.weapon_damage, ship.shield_max, ship.drive_speed,
                                              ship.scanner_level, ship.generator_power};
  for (std::size_t r = 0; r < values.size(); ++r) {
    Pane clip = row;
    clip.x1 = row.x0 + 1;
    if (values[r] > 0) {
      clip.x1 += values[r] * 8 / 4;
    }
    for (int i = 0; i < 15; ++i) {
      vfx_shape_draw(screen, clip, stats_shapes.at(r + 1), 2 + 8 * i, 0x1f + 9 * static_cast<int>(r));
    }
  }
  // The hit points and power columns, then their unfilled parts wiped: at
  // the screen's (124, 8) (pane-relative coordinates used as absolute
  // ones: a bug of the original).
  const std::array<std::int32_t, 2> value = {ship.hit_points, ship.power};
  const std::array<std::int32_t, 2> maximum = {ship.max_hit_points, game::ship_battle_power(*s, ship)};
  const std::array<int, 2> frames = {2, 5};
  for (std::size_t k = 0; k < 2; ++k) {
    if (maximum[k] <= 0) {
      continue;
    }
    std::int32_t v = value[k];
    if (v < 0) {
      v = 0;
    } else if (v > maximum[k]) {
      v = maximum[k];
    }
    const int x = 0x7c + 8 * static_cast<int>(k);
    for (int i = 0; i < 9; ++i) {
      vfx_shape_draw(screen, row, stats_shapes.at(static_cast<std::size_t>(frames[k])), x, 9 + 8 * i);
    }
    vfx_pane_wipe(screen, Pane{x, 8, x + 6, 0x50 - v * 0x48 / maximum[k]}, back);
  }
}

void BatControlWnd::draw_gizmo_item(const Pane& row, int slot_index, int status, bool hilite) {
  const game::Session* s = core();
  if (s == nullptr || selection_.kind != kObjectShip) {
    return;
  }
  const game::Cosmos& c = s->game->cosmos;
  const game::Ship& ship = c.ships[static_cast<std::size_t>(selection_.object)];
  const game::GizmoSlot& slot = ship.slots[static_cast<std::size_t>(slot_index)];
  if (slot.gizmo == -1) {
    throw std::logic_error("star system: an empty gizmo slot listed");
  }
  const game::Gizmo& g = s->tables->gizmos.at(static_cast<std::size_t>(slot.gizmo));
  Image8& screen = wm_.screen();
  if (hilite && status == 0 && (g.flags & kGizmoPassive) == 0) {
    vfx_pane_wipe(screen, row, kHotBack);
  }
  const Shape& shape = wm_.shared_shapes(kGizmoShapes).at(static_cast<std::size_t>(slot.gizmo));
  const PolyPoint at = center_shape(row, shape);
  if (status == kStatusDepleted || status == kStatusNoPower) {
    vfx_shape_translate_draw(screen, row, shape, at.x, at.y, wm_.services().gsystem.haze_tables().at(kHazeUnusable));
  } else {
    vfx_shape_draw(screen, row, shape, at.x, at.y);
  }
  const std::uint8_t player_color = color_base(c, s->player_race);
  if (slot.toggled != 0 && (g.flags & kGizmoToggleable) != 0) {
    rect_frame(screen, row, 1, 1, row.x1 - row.x0 - 1, row.y1 - row.y0 - 1, player_color);
  }
  GameFont& font = wm_.small_font();
  font.pane() = row;
  font.draw_wrapped(screen, 5, 2, g.name, 0, player_color, 0xff, 0);
  if (status == kStatusDepleted || status == kStatusNoPower) {
    font.pane() = row;
    font.draw_wrapped(screen, 5, 0xc, wm_.static_string(status == kStatusDepleted ? kStringDepleted : kStringNoPower),
                      0, kShipNameColor, 0xff, 0);
  }
}

void BatControlWnd::draw_orbital_item(const Pane& row, int square, int status, bool hilite) {
  const game::Session* s = core();
  if (s == nullptr || selection_.kind != kObjectPlanet) {
    return;
  }
  const game::Cosmos& c = s->game->cosmos;
  const game::Planet& planet = c.planets[static_cast<std::size_t>(selection_.object)];
  const std::uint8_t item = c.squares[static_cast<std::size_t>(planet.first_square + square)].item;
  Image8& screen = wm_.screen();
  if (hilite && status == 0) {
    vfx_pane_wipe(screen, row, kHotBack);
  }
  const Shape& shape = wm_.shared_shapes(kPlanItemShapes).at(item);
  const PolyPoint at = center_shape(row, shape);
  if (status == 1) {
    vfx_shape_translate_draw(screen, row, shape, at.x, at.y, wm_.services().gsystem.haze_tables().at(kHazeUnusable));
  } else {
    vfx_shape_draw(screen, row, shape, at.x, at.y);
  }
  const std::uint8_t player_color = color_base(c, s->player_race);
  GameFont& font = wm_.small_font();
  font.pane() = row;
  font.draw_wrapped(screen, 5, 2, s->tables->planitems.items.at(item).name, 0, player_color, 0xff, 0);
  if (status == 1) {
    font.pane() = row;
    font.draw_wrapped(screen, 5, 0xc, wm_.static_string(kStringDepleted), 0, player_color, 0xff, 0);
  }
}

void BatControlWnd::draw(int mode) {
  const game::Session* s = core();
  if (s == nullptr) {
    return;
  }
  if (mode != kEventListPick) {
    fill_list();
  }
  Image8& screen = wm_.screen();
  vfx_pane_wipe(screen, kPanelPane, 0);
  int frame = kFrameSystem;
  if (list_mode_ == kListGizmos || list_mode_ == kListOrbital) {
    if (ListBox* rows = list()) {
      const Pane rect{rows->pane().x0, 7, rows->pane().x1, rows->line_height() + 8};
      if (selection_.kind == kObjectShip || selection_.kind == kObjectPlanet) {
        draw_object_item(rect, selection_.object, list_mode_ == kListOrbital, false);
      }
    }
    frame = list_mode_ == kListGizmos ? kFrameShip : kFramePlanet;
  }
  if (shapes_ != nullptr) {
    vfx_shape_draw(screen, kPanelPane, shapes_->at(static_cast<std::size_t>(frame)), 0, 0);
  }
  draw_children();
  if (list_mode_ != kListMine && list_mode_ != kListOthers) {
    return;
  }
  const game::Cosmos& c = s->game->cosmos;
  const game::Star& star = c.stars[static_cast<std::size_t>(c.selected_star)];
  const ShapeTable& suns = wm_.shapes().get("data\\suns.shp");
  vfx_shape_transform_draw(screen, kTitlePane, suns.at(static_cast<std::size_t>(star.type)), 15, 15, kSunScale,
                           kSunScale);
  int color = kTextColor;
  int controller = -1;
  for (int r = 0; r < c.race_count; ++r) {
    const game::Race& race = c.races[static_cast<std::size_t>(r)];
    if (race.extinct == -1 || star.owner_mask == 0) {
      continue;
    }
    const unsigned bit = 1U << r;
    if ((star.owner_mask | bit) == bit || (race.home == c.selected_star && (star.owner_mask & bit) != 0)) {
      controller = r;
      break;
    }
  }
  if (controller != -1) {
    const game::Race& race = c.races[static_cast<std::size_t>(controller)];
    color = color_base(c, controller);
    wm_.draw_colored(kTitlePane, wm_.shared_shapes(kRaceFlags).at(race.species), 0x8d, 3, race.color);
  }
  GameFont& large = wm_.large_font();
  large.pane() = kTitlePane;
  large.draw_wrapped(screen, kTitleTextX, 5, game::name_view(star.name), 0, color, 0xff, kTitleWidth);
  GameFont& small = wm_.small_font();
  small.pane() = kTitlePane;
  small.draw_wrapped(screen, kTitleTextX, 0x12, wm_.static_string(kStringStarTypes + star.type), 0, kTextColor, 0xff,
                     kTitleWidth);
}

void BatControlWnd::list_pick(int row, int button) {
  game::Session* s = core();
  BatModeWnd* view = mode_wnd();
  ListBox* rows = list();
  if (s == nullptr || view == nullptr || rows == nullptr || row < 0 || row >= rows->count()) {
    return;
  }
  if (button == kEventHelpClick) {
    // Message 0x1c02: the gizmo's or the planet item's help.
    const ListBox::Item& item = rows->item(row);
    if (list_mode_ == kListGizmos && selection_.kind == kObjectShip) {
      const game::Ship& ship = s->game->cosmos.ships[static_cast<std::size_t>(selection_.object)];
      wm_.post_message(kMessageHelp, ship.slots[static_cast<std::size_t>(item.object)].gizmo, kHelpGizmo);
    } else if (list_mode_ == kListOrbital && selection_.kind == kObjectPlanet) {
      const game::Cosmos& c = s->game->cosmos;
      const game::Planet& planet = c.planets[static_cast<std::size_t>(selection_.object)];
      wm_.post_message(kMessageHelp, c.squares[static_cast<std::size_t>(planet.first_square + item.object)].item,
                       kHelpPlanet);
    }
    return;
  }
  if (view->state() == BatModeWnd::State::kAnimating) {
    return;
  }
  repeat_ship_ = -1;
  const ListBox::Item item = rows->item(row);
  game::Cosmos& c = s->game->cosmos;
  switch (list_mode_) {
    case kListMine:
    case kListOthers:
      if (button == kEventLeftClick) {
        if (item.data >= 0) {
          selection_.kind = kObjectShip;
          list_mode_ = kListGizmos;
        } else {
          selection_.kind = kObjectPlanet;
          list_mode_ = kListOrbital;
        }
        update_category_buttons();
        selection_.object = item.object;
        view->select_from_control();
        draw(kDrawNormal);
      } else {
        const int i = view->find_item(
            item.data >= 0 ? BatModeWnd::ItemKind::kShip : BatModeWnd::ItemKind::kPlanet, item.object);
        if (i >= 0) {
          view->center_on_item(i);
          view->refresh();
        }
      }
      break;
    case kListGizmos: {
      if (item.data != 0 || selection_.kind != kObjectShip) {
        return;
      }
      game::Ship& ship = c.ships[static_cast<std::size_t>(selection_.object)];
      const game::GizmoSlot& slot = ship.slots[static_cast<std::size_t>(item.object)];
      const game::Gizmo& g = s->tables->gizmos.at(static_cast<std::size_t>(slot.gizmo));
      if ((g.flags & kGizmoPassive) != 0) {
        return;
      }
      if ((g.flags & kGizmoToggleable) != 0) {
        game::ship_toggle_gizmo(*s, ship, item.object);
      } else {
        action_ = kActionGizmo;
        action_slot_ = item.object;
        if ((g.flags & kGizmoNoTarget) != 0) {
          ship.action = 1;
          ship.action_slot = game::gizmo_slot_address(*s, static_cast<game::ShipIndex>(selection_.object), item.object);
          ship.target_kind = game::kTargetNone;
          ship.target_point = {};
          ship.action_point = {};
          action_ = kActionMove;
          action_slot_ = 0;
          list_mode_ = kListMine;
          update_category_buttons();
          battle_.act(*s, {game::BattleActor::kShip, static_cast<std::int16_t>(selection_.object)}, false);
          view->start_effect();
        } else {
          view->set_state(BatModeWnd::State::kTargeting);
        }
      }
      draw(kEventListPick);
      break;
    }
    case kListOrbital:
      if (item.data != 0) {
        return;
      }
      action_ = kActionPlanetWeapon;
      action_slot_ = item.object;
      view->set_state(BatModeWnd::State::kTargeting);
      break;
    default:
      break;
  }
}

bool BatControlWnd::handle_event(int type, int a, int b) {
  game::Session* s = core();
  BatModeWnd* view = mode_wnd();
  const bool animating = view != nullptr && view->state() == BatModeWnd::State::kAnimating;
  switch (type) {
    case kEventActivate:
      activate(a, b);
      return false;
    case kEventDeactivate:
      if (s != nullptr) {
        battle_.close(*s);
      }
      return GWindow::handle_event(type, a, b);
    case kEventRefresh:
      draw(kDrawNormal);
      return false;
    case kMessageMyItems:
    case kMessageOtherItems: {
      const ListMode mode = type == kMessageMyItems ? kListMine : kListOthers;
      if (list_mode_ != mode) {
        list_mode_ = mode;
        update_category_buttons();
        draw(kDrawNormal);
      }
      return true;
    }
    case kMessagePass:
      if (s != nullptr && !animating && player_present(*s) && !battle_.auto_on() && battle_.cursor() == 0) {
        run_turns([&] { battle_.pass(*s); });
      }
      return false;
    case kMessageAuto:
      if (s != nullptr && !animating && player_present(*s) && !battle_.auto_on() && battle_.cursor() == 0) {
        run_turns([&] { battle_.start_auto(*s); });
      }
      return false;
    case kMessageRepeat: {
      if (s == nullptr || animating || repeat_ship_ == -1) {
        return false;
      }
      const game::Cosmos& c = s->game->cosmos;
      const game::Ship& ship = c.ships[static_cast<std::size_t>(repeat_ship_)];
      if (ship.location_kind != game::ShipLocation::kSystem || ship.location != c.selected_star) {
        return false;
      }
      selection_ = {kObjectShip, repeat_ship_};
      action_ = kActionMove;
      action_slot_ = 0;
      const game::Vec3 point = repeat_point_;
      execute(repeat_kind_, repeat_object_, &point);
      return false;
    }
    case kMessageWeapons:
    case kMessageShields:
    case kMessagePassive:
    case kMessageSpecials: {
      const std::uint8_t category = type == kMessageWeapons   ? 0
                                    : type == kMessageShields ? 1
                                    : type == kMessagePassive ? 4
                                                              : 5;
      if (category_ != category) {
        category_ = category;
        fill_list();
      }
      return true;
    }
    case kEventListPick:
      list_pick(a, b);
      return true;
    default:
      return GWindow::handle_event(type, a, b);
  }
}

void FlagButton::draw(int mode) {
  RaceColorButton::draw(mode);
  const GameSession& game = wm_.services().game;
  wm_.draw_colored(pane_, wm_.shared_shapes(kRaceFlags).at(static_cast<std::size_t>(game.race_species(game.player_race()))),
                   3, 3, game.race_color(game.player_race()));
}

void add_star_system_windows(WindowFactory& factory) {
  factory.add(4, [](WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes) -> std::unique_ptr<GWindow> {
    return std::make_unique<BatModeWnd>(wm, record, shapes);
  });
  factory.add(19, [](WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes) -> std::unique_ptr<GWindow> {
    return std::make_unique<BatControlWnd>(wm, record, shapes);
  });
  factory.add(32, [](WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes) -> std::unique_ptr<GWindow> {
    return std::make_unique<FlagButton>(wm, record, shapes);
  });
}

}  // namespace oa
