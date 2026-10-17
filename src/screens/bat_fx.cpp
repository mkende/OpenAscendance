// The effect animations of the System Display (`batfx.cpp`, `batfxa.cpp`;
// `docs/08-combat.md` "Effects and sounds"): `BatModeWnd`'s effect timer and
// one player per kind of action, moving the effect sprites (display item 1)
// and the items concerned through the effect positions (`+0x4f80`), with
// the `batfx.txt` shapes and sounds. They change nothing of the game: the
// action is applied when its effect ends (`BatControlWnd_finish_action`).
#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "game/address.h"
#include "game/session.h"
#include "game/ship.h"
#include "gfx/vfx_poly.h"
#include "screens/bat_control_wnd.h"
#include "screens/bat_mode_wnd.h"

namespace oa {

namespace {

using game::f32;

double d(float v) { return static_cast<double>(v); }

// The gizmos with an effect of their own (`BatModeWnd_effect_tick`).
constexpr int kGizmoFleetDisperser = 40;
constexpr int kGizmoGravityDistorter = 39;
constexpr int kGizmoLaneBlocker = 32;
constexpr int kGizmoRecaller = 36;
constexpr int kGizmoSmartBomb = 38;
constexpr int kGizmoPositronBouncer = 45;
constexpr int kGizmoGravCatapult = 46;
constexpr int kGizmoTractorBeam = 52;
constexpr int kGizmoHyperswapper = 55;
constexpr int kGizmoGravCondensor = 56;
constexpr int kGizmoRemoteRepair = 58;
constexpr int kGizmoSacrificialOrb = 59;
constexpr int kGizmoLaneMagnetron = 60;
constexpr int kGizmoDisintegrator = 61;
constexpr int kGizmoPlasmaCoupler = 68;
constexpr int kGizmoSelfDestruct = 72;
constexpr int kGizmoMassCondensor = 74;
// The planet items with an effect: the Tractor Beam; the batfx.txt rows of
// the Orbital Missile Base and the Short and Long Range Whoppers.
constexpr std::uint8_t kItemTractorBeam = 0x11;
constexpr std::uint8_t kItemMissileBase = 0x1c;
constexpr std::uint8_t kItemShortWhopper = 0x1d;
constexpr std::uint8_t kItemLongWhopper = 0x1e;
constexpr int kRowMissileBase = 0x4c;

// The GZ_NOTARGET flag.
constexpr std::uint16_t kGizmoNoTarget = 0x40;

// `batfx.txt` shape files: the shield hit, the explosions, the lane exits.
constexpr int kFileShield = 4;
constexpr int kFileDestroy = 5;
constexpr int kFileBlueExit = 7;
constexpr int kFileRedExit = 8;
// Sounds: the shield hit, the explosion, the thrust.
constexpr int kSfxShield = 1;
constexpr int kSfxExplosion = 5;
constexpr int kSfxThrust = 7;

// The effect sprite's display item (the scene's free item).
constexpr int kSpriteItem = 1;

// The effects (`+0x4f37`).
enum Effect : std::uint8_t {
  kFxMove = 0,
  kFxSelf = 1,
  kFxProjectile = 2,
  kFxTractor = 3,
  kFxHyperswap = 4,
  kFxLaneBlocker = 5,
  kFxCondensor = 6,
  kFxRemoteRepair = 7,
  kFxGravCatapult = 8,
  kFxLaneMagnetron = 9,
  kFxTransfer = 10,
  kFxPositronBouncer = 11,
  kFxDisperser = 12,
  kFxRecaller = 13,
  kFxSelfDestruct = 14,
  kFxSmartBomb = 15,
  kFxNone = 0xff,
};

}  // namespace

bool BatModeWnd::step_toward(Vec3& p, const Vec3& target, float step) {
  const Vec3 dv = game::sub(target, p);
  const float len = f32(game::length(dv));
  if (step >= len) {
    p = target;
    return true;
  }
  const Vec3 a = {f32(d(dv[0]) * step), f32(d(dv[1]) * step), f32(d(step) * dv[2])};
  const double inv = 1.0 / d(len);
  const Vec3 t = {f32(d(a[0]) * inv), f32(d(a[1]) * inv), f32(d(a[2]) * inv)};
  p = {f32(d(p[0]) + t[0]), f32(d(p[1]) + t[1]), f32(d(p[2]) + t[2])};
  return false;
}

BatModeWnd::Item& BatModeWnd::effect_sprite() { return items_[kSpriteItem]; }

void BatModeWnd::start_effect() {
  effect_ = kFxNone;
  selected_ = -1;
  fx_count_ = 0;
  state_ = State::kAnimating;
  wm_.set_timer(id_, kMessageEffectTick, 0, 0, 0);
}

void BatModeWnd::effect_tick() {
  game::Session* s = core();
  BatControlWnd* ctrl = control();
  if (s == nullptr || ctrl == nullptr) {
    return;
  }
  const game::Cosmos& c = s->game->cosmos;
  bool done = false;
  if (effect_ == kFxNone) {
    build_scene();
    refresh();
    tick_ = 0;
    const BatControlWnd::Selection sel = ctrl->selection();
    if (sel.kind == BatControlWnd::kObjectShip) {
      const game::Ship& ship = c.ships[static_cast<std::size_t>(sel.object)];
      if (ship.action == 0) {
        effect_ = kFxMove;
      } else if (ship.action == 1) {
        const auto slot = game::gizmo_slot_at(*s, static_cast<game::ShipIndex>(sel.object), ship.action_slot);
        const int g = slot ? ship.slots[static_cast<std::size_t>(*slot)].gizmo : -1;
        switch (g) {
          case kGizmoTractorBeam:
            effect_ = kFxTractor;
            break;
          case kGizmoHyperswapper:
            effect_ = kFxHyperswap;
            break;
          case kGizmoLaneBlocker:
            effect_ = kFxLaneBlocker;
            break;
          case kGizmoGravCondensor:
          case kGizmoMassCondensor:
            effect_ = kFxCondensor;
            break;
          case kGizmoRemoteRepair:
            effect_ = kFxRemoteRepair;
            break;
          case kGizmoGravCatapult:
            effect_ = kFxGravCatapult;
            break;
          case kGizmoLaneMagnetron:
            effect_ = kFxLaneMagnetron;
            break;
          case kGizmoSacrificialOrb:
          case kGizmoPlasmaCoupler:
            effect_ = kFxTransfer;
            break;
          case kGizmoPositronBouncer:
            effect_ = kFxPositronBouncer;
            break;
          case kGizmoFleetDisperser:
          case kGizmoGravityDistorter:
            effect_ = kFxDisperser;
            break;
          case kGizmoRecaller:
            effect_ = kFxRecaller;
            break;
          case kGizmoSelfDestruct:
            effect_ = kFxSelfDestruct;
            break;
          case kGizmoSmartBomb:
            effect_ = kFxSmartBomb;
            break;
          default:
            effect_ = g >= 0 && (s->tables->gizmos.at(static_cast<std::size_t>(g)).flags & kGizmoNoTarget) != 0
                          ? kFxSelf
                          : kFxProjectile;
            break;
        }
      }
    } else if (sel.kind == BatControlWnd::kObjectPlanet) {
      const game::Planet& planet = c.planets[static_cast<std::size_t>(sel.object)];
      const auto square = game::square_at(*s, planet.fire_square);
      const std::uint8_t item = square ? c.squares[static_cast<std::size_t>(*square)].item : 0;
      effect_ = item == kItemTractorBeam ? kFxTractor : kFxProjectile;
    }
  }
  switch (effect_) {
    case kFxMove:
      done = fx_move();
      break;
    case kFxSelf:
      done = fx_self();
      break;
    case kFxProjectile:
      done = fx_projectile();
      break;
    case kFxTractor:
      done = fx_tractor();
      break;
    case kFxHyperswap:
      done = fx_hyperswap();
      break;
    case kFxLaneBlocker:
      done = fx_lane_blocker();
      break;
    case kFxCondensor:
      done = fx_condensor();
      break;
    case kFxRemoteRepair:
      done = fx_remote_repair();
      break;
    case kFxGravCatapult:
      done = fx_grav_catapult();
      break;
    case kFxLaneMagnetron:
      done = fx_lane_magnetron();
      break;
    case kFxTransfer:
      done = fx_transfer();
      break;
    case kFxPositronBouncer:
      done = fx_positron_bouncer();
      break;
    case kFxDisperser:
      done = fx_disperser();
      break;
    case kFxRecaller:
      done = fx_recaller();
      break;
    case kFxSelfDestruct:
      done = fx_self_destruct();
      break;
    case kFxSmartBomb:
      done = fx_smart_bomb();
      break;
    default:
      done = true;
      break;
  }
  if (skip_) {
    skip_ = false;
    done = true;
  }
  refresh(false);
  draw(1);
  ++tick_;
  if (done) {
    state_ = State::kIdle;
    wm_.kill_timer(id_, kMessageEffectTick);
    ctrl->finish_action(true);
    build_scene();
    refresh();
    effect_ = kFxNone;
    for (int i = 0; i < fx_count_; ++i) {
      items_[static_cast<std::size_t>(fx_items_[static_cast<std::size_t>(i)])].flags &= kFlagStarSideA;
    }
  }
}

bool BatModeWnd::fx_move() {
  game::Session* s = core();
  const BatControlWnd::Selection sel = control()->selection();
  if (sel.kind != BatControlWnd::kObjectShip) {
    throw std::logic_error("star system: a move effect without a ship");
  }
  const game::Ship& ship = s->game->cosmos.ships[static_cast<std::size_t>(sel.object)];
  if (ship.target_kind != game::kTargetPoint && ship.target_kind != game::kTargetPlanet &&
      ship.target_kind != game::kTargetLane) {
    throw std::logic_error("star system: a move effect without a target");
  }
  bool done = false;
  if (tick_ == 0) {
    fx_count_ = 1;
    fx_items_[0] = find_item(ItemKind::kShip, sel.object);
    Item& item = items_[static_cast<std::size_t>(fx_items_[0])];
    item.flags = kFlagNoLine | kFlagFxPosition;
    item.fx_position = 0;
    fx_pos_[0] = ship.position;
    phase_ = 0;
    if (ship.moves != 0 && ship.power != 0) {
      wm_.services().sound.play_sfx(kSfxThrust);
    }
  }
  if (phase_ == 0) {
    if (step_toward(fx_pos_[0], ship.action_point, 20.0F)) {
      if ((ship.action_result & game::kActionReached) != 0 && ship.target_kind == game::kTargetLane) {
        const auto lane = game::lane_at(*s, ship.target);
        if (lane && game::ship_can_enter_lane(*s, ship, s->game->cosmos.lanes[static_cast<std::size_t>(*lane)])) {
          items_[static_cast<std::size_t>(fx_items_[0])].flags |= kFlagHidden;
          fx_count_ = 2;
          fx_items_[1] = find_item(ItemKind::kLaneEnd, *lane);
          phase_ = 1;
          return false;
        }
      }
      return true;
    }
  } else {
    const auto lane = game::lane_at(*s, ship.target);
    const int file =
        lane && (s->game->cosmos.lanes[static_cast<std::size_t>(*lane)].flags & 1) != 0 ? kFileRedExit : kFileBlueExit;
    int frame = phase_ >> 2;
    const int last = static_cast<int>(file_shapes(file).size()) - 1;
    if (last < frame && frame <= last * 2) {
      frame = last * 2 - frame;
    } else if (last * 2 < frame) {
      done = true;
      frame = 0;
    }
    set_item_file(items_[static_cast<std::size_t>(fx_items_[1])], file, frame);
    ++phase_;
  }
  return done;
}

bool BatModeWnd::fx_self() {
  game::Session* s = core();
  const BatControlWnd::Selection sel = control()->selection();
  if (sel.kind != BatControlWnd::kObjectShip) {
    throw std::logic_error("star system: a gizmo effect without a ship");
  }
  const game::Ship& ship = s->game->cosmos.ships[static_cast<std::size_t>(sel.object)];
  const auto slot = game::gizmo_slot_at(*s, static_cast<game::ShipIndex>(sel.object), ship.action_slot);
  const int g = slot ? ship.slots[static_cast<std::size_t>(*slot)].gizmo : 0;
  if (tick_ == 0) {
    phase_ = 0;
    fx_count_ = 1;
    fx_items_[0] = kSpriteItem;
    Item& sprite = effect_sprite();
    sprite.kind = ItemKind::kEffect;
    sprite.flags = kFlagNoLine | kFlagFxPosition;
    sprite.fx_position = 0;
    fx_pos_[0] = ship.position;
    const EffectRow& row = effect_rows_.at(static_cast<std::size_t>(g));
    set_item_file(sprite, row.shape1, 0);
    wm_.services().sound.play_sfx(row.sfx1);
  }
  return (tick_ & 3) == 0 && next_frame(effect_sprite());
}

bool BatModeWnd::fx_projectile() {
  game::Session* s = core();
  const BatControlWnd::Selection sel = control()->selection();
  const game::Cosmos& c = s->game->cosmos;
  Vec3 start{};
  Vec3 end{};
  int row = 0;
  std::uint32_t result = 0;
  std::uint8_t target_kind = game::kTargetShip;
  // The display item hit and, for a ship, its hull.
  int target_item = -1;
  int target_hull = 0;
  const auto find_target = [&](game::RawAddress raw) {
    if (const auto ship = game::ship_at(*s, raw)) {
      target_item = find_item(ItemKind::kShip, *ship);
      target_hull = static_cast<std::int8_t>(c.ships[static_cast<std::size_t>(*ship)].hull_size);
    } else if (const auto planet = game::planet_at(*s, raw)) {
      target_item = find_item(ItemKind::kPlanet, *planet);
    } else if (const auto lane = game::lane_at(*s, raw)) {
      target_item = find_item(ItemKind::kLaneEnd, *lane);
    }
  };
  if (sel.kind == BatControlWnd::kObjectShip) {
    const game::Ship& ship = c.ships[static_cast<std::size_t>(sel.object)];
    start = ship.position;
    end = ship.action_point;
    const auto slot = game::gizmo_slot_at(*s, static_cast<game::ShipIndex>(sel.object), ship.action_slot);
    row = slot ? ship.slots[static_cast<std::size_t>(*slot)].gizmo : 0;
    result = static_cast<std::uint32_t>(ship.action_result);
    target_kind = ship.target_kind;
    find_target(ship.target);
  } else if (sel.kind == BatControlWnd::kObjectPlanet) {
    const game::Planet& planet = c.planets[static_cast<std::size_t>(sel.object)];
    start = planet.position;
    const auto target = game::ship_at(*s, planet.fire_target);
    if (target) {
      end = c.ships[static_cast<std::size_t>(*target)].position;
    }
    result = planet.fire_result;
    const auto square = game::square_at(*s, planet.fire_square);
    const std::uint8_t item = square ? c.squares[static_cast<std::size_t>(*square)].item : 0;
    row = item == kItemMissileBase    ? kRowMissileBase
          : item == kItemShortWhopper ? kRowMissileBase + 1
          : item == kItemLongWhopper  ? kRowMissileBase + 2
                                      : 0;
    find_target(planet.fire_target);
  } else {
    throw std::logic_error("star system: a projectile effect without a shooter");
  }
  const EffectRow& rows = effect_rows_.at(static_cast<std::size_t>(row));
  bool done = false;
  if (tick_ == 0) {
    fx_count_ = 2;
    phase_ = 0;
    fx_items_[0] = kSpriteItem;
    Item& sprite = effect_sprite();
    sprite.kind = ItemKind::kEffect;
    sprite.flags = kFlagNoLine | kFlagFxPosition;
    sprite.fx_position = 0;
    fx_pos_[0] = start;
    set_item_file(sprite, rows.shape1, 0);
    fx_items_[1] = target_item;
    wm_.services().sound.play_sfx(rows.sfx1);
  }
  if (phase_ == 0) {
    const Vec3 dv = game::sub(end, start);
    const float step = f32(std::trunc(game::length(dv) * d(0.015625F) + d(20.0F)));
    if (step_toward(fx_pos_[0], end, step)) {
      phase_ = 1;
      return false;
    }
    next_frame(effect_sprite());
    return false;
  }
  float scale = 1.0F;
  int sfx = kSfxShield;
  int shape = kFileShield;
  if ((result & game::kActionDestroyed) != 0 || row == kGizmoDisintegrator) {
    if (target_item >= 0 && items_[static_cast<std::size_t>(target_item)].kind == ItemKind::kShip) {
      scale = f32(static_cast<double>(target_hull) * (1.0 / 3.0) + 1.0);
    }
    sfx = kSfxExplosion;
    if (target_item >= 0) {
      items_[static_cast<std::size_t>(target_item)].flags |= kFlagHidden;
    }
    shape = (row & 1) == 0 ? kFileDestroy + 1 : kFileDestroy;
    if (row == kGizmoDisintegrator) {
      sfx = rows.sfx2;
      shape = rows.shape2;
    }
  } else if ((result & game::kActionPenetrated) != 0 || (result & game::kActionNoDamage) != 0 ||
             target_kind == game::kTargetPlanet || target_kind == game::kTargetLane) {
    sfx = rows.sfx2;
    shape = rows.shape2;
  }
  Item& sprite = effect_sprite();
  if (phase_ == 1) {
    set_item_file(sprite, shape, 0);
    wm_.services().sound.play_sfx(sfx);
    phase_ = 2;
  }
  if ((tick_ & 3) == 0 && next_frame(sprite)) {
    sprite.flags |= kFlagHidden;
    done = true;
  }
  sprite.scale = scale;
  return done;
}

// The effects still to port: they end at once (the action is applied
// without its animation).
bool BatModeWnd::fx_tractor() { return true; }
bool BatModeWnd::fx_hyperswap() { return true; }
bool BatModeWnd::fx_lane_blocker() { return true; }
bool BatModeWnd::fx_condensor() { return true; }
bool BatModeWnd::fx_remote_repair() { return true; }
bool BatModeWnd::fx_grav_catapult() { return true; }
bool BatModeWnd::fx_lane_magnetron() { return true; }
bool BatModeWnd::fx_transfer() { return true; }
bool BatModeWnd::fx_positron_bouncer() { return true; }
bool BatModeWnd::fx_disperser() { return true; }
bool BatModeWnd::fx_recaller() { return true; }
bool BatModeWnd::fx_self_destruct() { return true; }
bool BatModeWnd::fx_smart_bomb() { return true; }

void BatModeWnd::fx_draw_item(const Item& item) {
  // The translucent sprites (`BatModeWnd_fx_draw_translucent` 0x15630):
  // WHITE00..09 by the fade level.
  int level = fade_;
  if (level > 9) {
    level = 18 - level;
  }
  level = std::clamp(level, 0, 9);
  if (item.shapes != nullptr) {
    vfx_shape_transform_draw(wm_.screen(), pane_, item.shapes->at(static_cast<std::size_t>(item.frame)), item.x,
                             item.y, item.draw_scale, item.draw_scale,
                             &wm_.services().gsystem.haze_tables().at(static_cast<std::size_t>(11 + level)));
  }
}

}  // namespace oa
