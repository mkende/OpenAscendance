#include "ui/button_group.h"

#include <format>
#include <sstream>

#include "core/error.h"
#include "core/log.h"
#include "ui/winmgr.h"

namespace oa {

ButtonGroup::ButtonGroup(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes)
    : GWindow(wm, record, shapes) {
  // The loader sets no MOUSEFOCUS and sound 0 (the init's 7 overwritten).
  sound_ = 0;
  const auto count = record.extras.find("NUMCONTROLS");
  const int n = count == record.extras.end() ? 0 : std::stoi(count->second);
  for (int i = 0; i < n; ++i) {
    const auto it = record.extras.find(std::format("RACE{}CONTROL", i + 1));
    if (it == record.extras.end()) {
      throw DataError(std::format("windows.txt: {} lacks control {}", record.name, i + 1));
    }
    std::istringstream in(it->second);
    Control c;
    in >> c.id >> c.x0 >> c.y0 >> c.x1 >> c.y1 >> c.flags >> c.frame;
    controls_.push_back(c);
  }
  for (int r = 0; r < kStartupRaces; ++r) {
    set_flags(1 << r, kEnabled);
  }
}

void ButtonGroup::update_race_frames() {
  const GameSession& game = wm_.services().game;
  for (int r = 0; r < game.race_count() && r < static_cast<int>(controls_.size()); ++r) {
    controls_[static_cast<std::size_t>(r)].frame = game.race_color(r) * 5 + 20;
  }
}

bool ButtonGroup::save_state(std::span<std::uint8_t> out) const {
  std::size_t at = 0;
  for (const Control& c : controls_) {
    for (const int v : {c.id, c.x0, c.x1, c.y0, c.y1, c.flags, c.frame}) {
      if (at + 2 <= out.size()) {
        out[at] = static_cast<std::uint8_t>(v);
        out[at + 1] = static_cast<std::uint8_t>(static_cast<std::uint32_t>(v) >> 8);
      }
      at += 2;
    }
  }
  return true;
}

void ButtonGroup::load_state(std::span<const std::uint8_t> in) {
  std::size_t at = 0;
  const auto next = [&]() {
    const auto v = static_cast<std::int16_t>(in[at] | (in[at + 1] << 8));
    at += 2;
    return static_cast<int>(v);
  };
  for (Control& c : controls_) {
    if (at + kControlRecordSize > in.size()) {
      return;
    }
    c.id = next();
    c.x0 = next();
    c.x1 = next();
    c.y0 = next();
    c.y1 = next();
    c.flags = next();
    c.frame = next();
  }
}

void ButtonGroup::set_flags(int id, int flags) {
  for (Control& c : controls_) {
    if (c.id == id) {
      c.flags = flags;
      return;
    }
  }
}

void ButtonGroup::move(int dx, int dy) {
  GWindow::move(dx, dy);
  for (Control& c : controls_) {
    c.x0 += dx;
    c.x1 += dx;
    c.y0 += dy;
    c.y1 += dy;
  }
}

int ButtonGroup::hit_test(int x, int y) const {
  const auto inside = [&](const Control& c) { return x <= c.x1 && x >= c.x0 && y <= c.y1 && y >= c.y0; };
  if (hover_ != -1 && inside(controls_[static_cast<std::size_t>(hover_)])) {
    return hover_;
  }
  for (std::size_t i = 0; i < controls_.size(); ++i) {
    if (inside(controls_[i])) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void ButtonGroup::set_hover(int index) {
  if (index == hover_) {
    return;
  }
  if (hover_ != -1) {
    controls_[static_cast<std::size_t>(hover_)].flags &= ~kHover;
  }
  if (index != -1 && (controls_[static_cast<std::size_t>(index)].flags & kEnabled) != 0) {
    controls_[static_cast<std::size_t>(index)].flags |= kHover;
  }
  hover_ = index;
}

bool ButtonGroup::toggle(int index) {
  Control& c = controls_[static_cast<std::size_t>(index)];
  if ((c.flags & kEnabled) == 0) {
    return false;
  }
  c.flags ^= kOn;
  return true;
}

bool ButtonGroup::handle_event(int type, int a, int b) {
  switch (type) {
    case kEventActivate: {
      update_race_frames();
      GWindow::handle_event(type, a, b);
      return false;
    }
    case kEventLeftClick:
    case kEventRightClick: {
      if (!contains(a, b)) {
        return false;
      }
      const int hit = hit_test(a, b);
      if (hit != -1) {
        if (sound_ != -1) {
          wm_.services().sound.play_sfx(sound_);
        }
        set_hover(hit);
        const bool toggled = toggle(hit);
        draw(kDrawNormal);
        if (toggled && parent_ != 0) {
          wm_.send_event(parent_, message_, controls_[static_cast<std::size_t>(hit)].id, 0);
        }
      }
      return true;
    }
    case kEventMouseLeave:
      wm_.cancel_notify(id_, -1);
      return true;
    case kEventMouseEnter:
      wm_.request_notify(id_, WinMgr::kNotifyMove, kEventMouseMove, 0, 0);
      return true;
    case kEventMouseMove: {
      const int hit = hit_test(a, b);
      if (hit != hover_) {
        set_hover(hit);
        draw(kDrawNormal);
      }
      return true;
    }
    case kEventSetControl:
      if (a == -1) {
        for (std::size_t i = 0; i < controls_.size(); ++i) {
          set_flags(1 << i, b);
        }
      } else {
        set_flags(a, b);
      }
      return true;
    default:
      return GWindow::handle_event(type, a, b);
  }
}

void ButtonGroup::draw(int /*mode*/) {
  Image8& screen = wm_.screen();
  if (const Shape* s = shape(frame_)) {
    vfx_shape_draw(screen, pane_, *s, 0, 0);
  }
  for (const Control& c : controls_) {
    int offset = 0;
    switch (c.flags) {
      case 0:
        offset = 4;
        break;
      case kEnabled:
        offset = 0;
        break;
      case kEnabled | kHover:
        offset = 1;
        break;
      case kEnabled | kOn:
        offset = 2;
        break;
      case kEnabled | kHover | kOn:
        offset = 3;
        break;
      default:
        log_warning("{}: control {} has flags {}", name_, c.id, c.flags);
        break;
    }
    if (const Shape* s = shape(c.frame + offset)) {
      vfx_shape_draw(screen, pane_, *s, c.x0 - pane_.x0, c.y0 - pane_.y0);
    }
  }
}

}  // namespace oa
