#include "platform/input.h"

#include <algorithm>
#include <cstdlib>

#include "core/image.h"
#include "platform/bios_keys.h"

namespace oa {

void Input::feed(const InputEvent& event) {
  switch (event.type) {
    case EventType::kKeyDown:
    case EventType::kText:
      if (event.type == EventType::kKeyDown) {
        mods_ = event.mods;
      }
      if (const std::optional<std::uint16_t> word = bios_key_word(event); word && keys_.size() < kKeyBufferSize) {
        keys_.push_back(*word);
      }
      break;
    case EventType::kKeyUp:
      mods_ = event.mods;
      break;
    case EventType::kMouseMove:
    case EventType::kMouseDown:
    case EventType::kMouseUp: {
      const int from_x = mouse_x_;
      const int from_y = mouse_y_;
      // The MOUSE module keeps the pointer on the screen (MOUSE_init(640, 480)).
      mouse_x_ = std::clamp(event.x, 0, kScreenWidth - 1);
      mouse_y_ = std::clamp(event.y, 0, kScreenHeight - 1);
      if (event.type != EventType::kMouseMove &&
          (event.button == MouseButton::kLeft || event.button == MouseButton::kRight)) {
        buttons_.push_back({event.button, event.type == EventType::kMouseDown});
      }
      if (!event.touch) {
        track_mouse_drag(event, from_x, from_y);
      }
      break;
    }
    case EventType::kBack:
      back_ = true;
      break;
    case EventType::kQuit:
    case EventType::kFingerDown:
    case EventType::kFingerMove:
    case EventType::kFingerUp:
    case EventType::kFingerCancel:
    case EventType::kBackground:
      break;
  }
}

void Input::track_mouse_drag(const InputEvent& event, int from_x, int from_y) {
  if (event.type == EventType::kMouseDown && event.button == MouseButton::kLeft) {
    mouse_left_ = true;
    press_x_ = mouse_x_;
    press_y_ = mouse_y_;
  } else if (event.type == EventType::kMouseUp && event.button == MouseButton::kLeft) {
    mouse_left_ = false;
    if (mouse_dragging_) {
      mouse_dragging_ = false;
      end_gesture();
    }
  } else if (event.type == EventType::kMouseMove && mouse_left_) {
    if (mouse_dragging_) {
      drag_gesture(mouse_x_ - from_x, mouse_y_ - from_y);
    } else if (std::abs(mouse_x_ - press_x_) > kMouseDragSlop || std::abs(mouse_y_ - press_y_) > kMouseDragSlop) {
      mouse_dragging_ = true;
      begin_gesture(press_x_, press_y_);
      drag_gesture(mouse_x_ - press_x_, mouse_y_ - press_y_);
    }
  }
}

void Input::begin_gesture(int x, int y) { gesture_ = ViewGesture{.started = true, .active = true, .x = x, .y = y}; }

void Input::drag_gesture(int dx, int dy) {
  gesture_.dx += dx;
  gesture_.dy += dy;
}

void Input::zoom_gesture(float ratio) { gesture_.zoom *= ratio; }

void Input::end_gesture() { gesture_.active = false; }

const InputState& Input::poll() {
  InputState& s = state_;
  s.left_changed = false;
  s.right_changed = false;
  s.moved = false;
  s.key_event = false;

  // input_poll_keyboard 0x25a80.
  s.shift_flags = bios_shift_state(mods_);
  s.shift = (s.shift_flags & 3) != 0;
  s.ctrl = (s.shift_flags & 4) != 0;
  s.alt = (s.shift_flags & 8) != 0;
  if (!keys_.empty()) {
    const std::uint16_t word = keys_.front();
    keys_.pop_front();
    s.key_event = true;
    s.ascii = word & 0xff;
    s.scan_code = word >> 8;
  }

  // input_poll_mouse 0x25bd4: MOUSE_status, then the edges.
  bool left = s.left_down;
  bool right = s.right_down;
  bool left_taken = false;
  bool right_taken = false;
  while (!buttons_.empty()) {
    const ButtonChange c = buttons_.front();
    bool& taken = c.button == MouseButton::kLeft ? left_taken : right_taken;
    bool& down = c.button == MouseButton::kLeft ? left : right;
    if (taken && c.down != down) {
      break;
    }
    taken = taken || c.down != down;
    down = c.down;
    buttons_.pop_front();
  }
  if (mouse_x_ != s.x || mouse_y_ != s.y) {
    s.x = mouse_x_;
    s.y = mouse_y_;
    s.moved = true;
  }
  if (left != s.left_down) {
    s.left_down = left;
    s.left_changed = true;
  }
  if (right != s.right_down) {
    s.right_down = right;
    s.right_changed = true;
  }

  s.back = back_;
  back_ = false;
  s.gesture = gesture_;
  gesture_.started = false;
  gesture_.dx = 0;
  gesture_.dy = 0;
  gesture_.zoom = 1.0F;
  return s;
}

}  // namespace oa
