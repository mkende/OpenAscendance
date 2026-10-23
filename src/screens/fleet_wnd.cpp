#include "screens/fleet_wnd.h"

#include <algorithm>
#include <array>
#include <memory>

#include "core/c_format.h"
#include "core/error.h"
#include "game/float_math.h"
#include "game/race.h"
#include "game/session.h"
#include "game/ship.h"
#include "screens/ship_des_wnd.h"

namespace oa {

namespace {

constexpr int kScreenShipDesign = 8;
constexpr int kScreenStarSystem = 16;
constexpr int kScreenPlanet = 17;

// The static strings of the screen.
constexpr int kStringNoEnding = 0x1c;   // ""
constexpr int kStringPlural = 0x1d;     // "s"
constexpr int kStringOrbiting = 0x8f;   // "Orbiting %s"
constexpr int kStringInSystem = 0x90;   // "in %s System"
constexpr int kStringUnknown = 0x91;    // "Unknown"
constexpr int kStringInLane = 0x92;     // "In Starlane to %s System\nETA - %d Day%s"
constexpr int kStringBuilding = 0x93;   // "Under Construction at %s"
constexpr int kStringRefitting = 0x94;  // "Being Refitted at %s"
constexpr int kStringToPlanet = 0x95;   // "\nMoving to Planet"
constexpr int kStringToLane = 0x96;     // "\nMoving to Starlane"
constexpr int kStringTitle = 0x98;      // "%s  (%d Ship%s, %d more allowed)"
constexpr int kStringSpecies = 101;     // the species names, 101..121
constexpr const char* kBadLocation = "Bad m_LocationType";

// `gwshare.txt` tables.
constexpr int kSmallShips = 14;  // smship0N.shp of race slot N, frame = hull
constexpr int kRaceFlags = 32;
constexpr int kShipIcons = 35;
// `shipicon.shp` frames: experience, hull, power, the strength bars from 7.
constexpr int kExperienceIcon = 6;
constexpr int kHullIcon = 8;
constexpr int kPowerIcon = 11;
constexpr int kFirstStatIcon = 7;

// The row layout.
constexpr int kPictureX = 0x24e;
constexpr int kPictureY = 0x31;
constexpr int kMaxExperiencePips = 9;
constexpr int kBarIcons = 14;
constexpr int kBarStep = 14;
constexpr int kGaugeIcons = 7;
constexpr int kGaugeTop = 4;
constexpr int kGaugeBottom = 0x66;
constexpr int kGaugeHeight = 0x62;

// The lane speed factors: a red link a third, Chronomyst ships twice.
constexpr float kRedLinkFactor = 0.33333334f;
constexpr std::uint8_t kAbilityChronomyst = 18;

int race_text_color(int color) { return color * 4 + 0x13; }

}  // namespace

std::int32_t FleetWnd::lane_eta(const game::Session& session, const game::Ship& ship) {
  const game::Cosmos& cosmos = session.game->cosmos;
  const game::Lane& lane = cosmos.lanes.at(static_cast<std::size_t>(ship.location));
  const game::Star& a = cosmos.stars.at(static_cast<std::size_t>(lane.star_a));
  const game::Star& b = cosmos.stars.at(static_cast<std::size_t>(lane.star_b));
  const float length = game::f32(game::length(game::sub(a.position, b.position)));
  float speed = static_cast<float>(ship.lane_speed + ship.hyper_speed);
  if ((lane.flags & 1) != 0) {
    speed = game::f32(static_cast<double>(speed) * static_cast<double>(kRedLinkFactor));
  }
  if (cosmos.races.at(static_cast<std::size_t>(ship.owner)).ability == kAbilityChronomyst) {
    speed = std::max(speed, 1.0f);
    speed = game::f32(static_cast<double>(speed) * 2.0);
  }
  return game::chop((static_cast<double>(length) - static_cast<double>(ship.position[0])) /
                    static_cast<double>(speed)) +
         1;
}

std::string FleetWnd::location_text(WinMgr& wm, const game::Session& session, const game::Ship& ship) {
  const game::Cosmos& cosmos = session.game->cosmos;
  const auto planet_name = [&]() {
    return std::string(game::name_view(cosmos.planets.at(static_cast<std::size_t>(ship.location)).name));
  };
  std::string text;
  switch (ship.location_kind) {
    case game::ShipLocation::kOrbit:
      text = c_format(wm.static_string(kStringOrbiting), planet_name().c_str());
      break;
    case game::ShipLocation::kSystem:
      text =
          c_format(wm.static_string(kStringInSystem),
                   std::string(game::name_view(cosmos.stars.at(static_cast<std::size_t>(ship.location)).name)).c_str());
      break;
    case game::ShipLocation::kLane: {
      const game::Lane& lane = cosmos.lanes.at(static_cast<std::size_t>(ship.location));
      // Bit 31 aside, a non-zero direction heads to star B.
      const game::Star& to =
          cosmos.stars.at(static_cast<std::size_t>(ship.position[1] == 0.0f ? lane.star_a : lane.star_b));
      const std::string name = (to.explored_mask >> session.player_race & 1) != 0
                                   ? std::string(game::name_view(to.name))
                                   : wm.static_string(kStringUnknown);
      const std::int32_t days = lane_eta(session, ship);
      text = c_format(wm.static_string(kStringInLane), name.c_str(), static_cast<int>(days),
                      wm.static_string(days == 1 ? kStringNoEnding : kStringPlural).c_str());
      break;
    }
    case game::ShipLocation::kBuilding:
      text = c_format(wm.static_string(kStringBuilding), planet_name().c_str());
      break;
    case game::ShipLocation::kRefit:
      text = c_format(wm.static_string(kStringRefitting), planet_name().c_str());
      break;
    default:
      text = kBadLocation;
      break;
  }
  if (ship.destination_kind != game::ShipOrder::kNothing && ship.destination_kind != game::ShipOrder::kGoSpace &&
      ship.location_kind == game::ShipLocation::kSystem) {
    text += wm.static_string(ship.destination_kind == game::ShipOrder::kGoStar ? kStringToLane : kStringToPlanet);
  }
  return text;
}

void FleetWnd::fill() {
  list_->clear();
  game::Session* session = wm_.services().game.core();
  if (session == nullptr) {
    return;
  }
  const game::Cosmos& cosmos = session->game->cosmos;
  for (const game::ShipIndex ship : game::race_list_ships(cosmos, cosmos.races.at(session->player_race))) {
    const int row = list_->add_item({}, kRowHeight);
    list_->set_item_data(row, ship);
  }
  // `ListItem_compare_value` (0x10a14) on the rows' data, the built day.
  list_->sort([&cosmos](const ListBox::Item& a, const ListBox::Item& b) {
    const std::int32_t da = cosmos.ships.at(static_cast<std::size_t>(a.data)).built_day;
    const std::int32_t db = cosmos.ships.at(static_cast<std::size_t>(b.data)).built_day;
    return da > db ? 1 : da == db ? 0 : -1;
  });
}

LongPress FleetWnd::child_long_press(const GWindow& child, int x, int y) const {
  return &child == list_ && list_->row_at(x, y) >= 0 ? LongPress::kRight : LongPress::kHelp;
}

bool FleetWnd::handle_event(int type, int a, int b) {
  switch (type) {
    case kEventActivate:
      if (list_ == nullptr) {
        list_ = dynamic_cast<ListBox*>(wm_.find_window("STATSHIPLST"));
        if (list_ == nullptr) {
          throw DataError("windows.txt: no STATSHIPLST list box");
        }
        list_->set_line_height(kLineHeight);
        list_->set_draw_item([this](const Pane& row, const ListBox::Item& item, bool hilite) {
          const game::Session* session = wm_.services().game.core();
          if (session != nullptr) {
            draw_row(row, session->game->cosmos.ships.at(static_cast<std::size_t>(item.data)), hilite);
          }
        });
        list_->set_colors(0x31, 0, 0);
      }
      fill();
      list_->scroll_to(top_);
      return GWindow::handle_event(type, a, b);
    case kEventDeactivate:
      top_ = list_ != nullptr ? list_->top() : 0;
      return GWindow::handle_event(type, a, b);
    case kEventListPick: {
      game::Session* session = wm_.services().game.core();
      if (session == nullptr || list_ == nullptr || a < 0 || a >= list_->count()) {
        return false;
      }
      game::Cosmos& cosmos = session->game->cosmos;
      game::Ship& ship = cosmos.ships.at(static_cast<std::size_t>(row_ship(a)));
      if (b == kEventRightClick) {
        auto* design = dynamic_cast<ShipDesWnd*>(wm_.find_window("SHIPDESSCREEN"));
        if (design == nullptr) {
          throw DataError("windows.txt: no SHIPDESSCREEN");
        }
        design->open(&ship, ShipDesWnd::kNewDesign, true);
        wm_.post_message(kMessageChangeScreen, kScreenShipDesign, 1);
        top_ = list_->top();
        return true;
      }
      switch (ship.location_kind) {
        case game::ShipLocation::kBuilding:
        case game::ShipLocation::kRefit:
        case game::ShipLocation::kOrbit:
          cosmos.selected_planet = ship.location;
          wm_.post_message(kMessageChangeScreen, kScreenPlanet, 1);
          top_ = list_->top();
          return true;
        case game::ShipLocation::kSystem:
          cosmos.selected_star = ship.location;
          wm_.post_message(kMessageChangeScreen, kScreenStarSystem, 1);
          top_ = list_->top();
          return true;
        case game::ShipLocation::kLane:
          wm_.post_message(kMessageBack, 0, 0);
          return true;
        default:
          return false;
      }
    }
    default:
      return GWindow::handle_event(type, a, b);
  }
}

void FleetWnd::draw(int /*mode*/) {
  const game::Session* session = wm_.services().game.core();
  if (session != nullptr && list_ != nullptr) {
    const game::Cosmos& cosmos = session->game->cosmos;
    const game::Race& race = cosmos.races.at(session->player_race);
    const int allowed = std::clamp(game::race_ship_allowance(cosmos, race), 0, 100);
    const int ships = list_->count();
    const std::string title =
        c_format(wm_.static_string(kStringTitle), wm_.static_string(kStringSpecies + race.species).c_str(), ships,
                 ships == 1 ? "" : "s", allowed);
    GameFont& font = wm_.large_font();
    font.pane() = pane_;
    font.draw_wrapped(wm_.screen(), 0x19, 9, title, kFontCenterH, race_text_color(race.color), 0xff, 0);
    wm_.draw_colored(pane_, wm_.shared_shapes(kRaceFlags).at(race.species), 3, 3, race.color);
  }
  draw_children();
}

void FleetWnd::draw_row(const Pane& row, const game::Ship& ship, bool hilite) {
  const game::Session* session = wm_.services().game.core();
  if (session == nullptr) {
    return;
  }
  const game::Cosmos& cosmos = session->game->cosmos;
  const game::Race& player = cosmos.races.at(session->player_race);
  Image8& screen = wm_.screen();

  vfx_shape_draw(screen, row, wm_.shared_shapes(kSmallShips + session->player_race).at(ship.hull_size), 0x41, 0x32);
  const int color = hilite ? kTextColor : race_text_color(player.color);
  switch (ship.location_kind) {
    case game::ShipLocation::kBuilding:
    case game::ShipLocation::kRefit:
    case game::ShipLocation::kOrbit: {
      const game::Planet& planet = cosmos.planets.at(static_cast<std::size_t>(ship.location));
      vfx_shape_draw(screen, row, wm_.shapes().get("DATA\\planets.shp").at(planet.type * 5u + planet.size), kPictureX,
                     kPictureY);
      break;
    }
    case game::ShipLocation::kSystem: {
      const game::Star& star = cosmos.stars.at(static_cast<std::size_t>(ship.location));
      vfx_shape_draw(screen, row, wm_.shapes().get("DATA\\suns.shp").at(static_cast<std::size_t>(star.type)), kPictureX,
                     kPictureY);
      break;
    }
    default:
      break;
  }
  GameFont& small = wm_.small_font();
  small.pane() = row;
  small.draw_wrapped(screen, 0x17c, 5, location_text(wm_, *session, ship), 0, color, 0xff, 0x8e);
  GameFont& large = wm_.large_font();
  large.pane() = row;
  large.draw_wrapped(screen, 0x96, 5, game::name_view(ship.name), 0, color, 0xff, 0);

  const ShapeTable& icons = wm_.shared_shapes(kShipIcons);
  const game::Race& owner = cosmos.races.at(static_cast<std::size_t>(ship.owner));
  const int pips = std::min(game::ship_experience(*session, ship), kMaxExperiencePips);
  for (int i = 0; i < pips; ++i) {
    wm_.draw_colored(row, icons.at(kExperienceIcon), 0x103 + i * 9, 6, owner.color);
  }

  // The strength bars: weapons, shields, battle speed, scanners, power.
  const std::array<std::int32_t, 5> stats{ship.weapon_damage, ship.shield_max, ship.drive_speed, ship.scanner_level,
                                          ship.generator_power};
  for (std::size_t r = 0; r < stats.size(); ++r) {
    Pane clip = row;
    clip.x1 = clip.x0 + 0x95;
    if (stats[r] > 0) {
      clip.x1 += stats[r] * kBarStep / 4;
    }
    const Shape& icon = icons.at(kFirstStatIcon + r);
    for (int i = 0; i < kBarIcons; ++i) {
      vfx_shape_draw(screen, clip, icon, 0x96 + i * kBarStep, 0x19 + static_cast<int>(r) * kBarStep);
    }
  }

  // The hull and power gauges: 7 icons, the missing part blacked out from
  // the top (`draw_rect_frame` 0x2b294 mode -1, a fill).
  const std::array<std::int32_t, 2> maxima{ship.max_hit_points, game::ship_battle_power(*session, ship)};
  const std::array<std::int32_t, 2> values{ship.hit_points, ship.power};
  const std::array<int, 2> frames{kHullIcon, kPowerIcon};
  for (std::size_t g = 0; g < maxima.size(); ++g) {
    if (maxima[g] <= 0) {
      continue;
    }
    const std::int32_t value = std::clamp(values[g], 0, maxima[g]);
    const int x = 0x15e + static_cast<int>(g) * kBarStep;
    for (int i = 0; i < kGaugeIcons; ++i) {
      vfx_shape_draw(screen, row, icons.at(static_cast<std::size_t>(frames[g])), x, i * kBarStep + 5);
    }
    const int y1 = kGaugeBottom - value * kGaugeHeight / maxima[g];
    const Pane fill{row.x0 + x, row.y0 + kGaugeTop, std::min(row.x0 + x + 0xd, row.x1), std::min(row.y0 + y1, row.y1)};
    if (fill.x0 <= fill.x1 && fill.y0 <= fill.y1) {
      vfx_pane_wipe(screen, fill, 0);
    }
  }
}

void add_fleet_windows(WindowFactory& factory) {
  factory.add(17, [](WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes) -> std::unique_ptr<GWindow> {
    return std::make_unique<FleetWnd>(wm, record, shapes);
  });
}

}  // namespace oa
