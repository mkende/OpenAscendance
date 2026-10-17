#include "platform/touch.h"

#include <cmath>
#include <cstdlib>

namespace oa {

void TouchMapper::mouse(EventType type, int x, int y, MouseButton button) {
  input_.feed(InputEvent{.type = type, .x = x, .y = y, .button = button, .touch = true});
}

std::int64_t TouchMapper::spread() const {
  if (!second_) {
    return 0;
  }
  const std::int64_t dx = first_.x - second_->x;
  const std::int64_t dy = first_.y - second_->y;
  return dx * dx + dy * dy;
}

void TouchMapper::finish(bool tap) {
  switch (phase_) {
    case Phase::kPending:
      if (tap) {
        mouse(EventType::kMouseDown, start_x_, start_y_, MouseButton::kLeft);
        mouse(EventType::kMouseUp, start_x_, start_y_, MouseButton::kLeft);
      }
      break;
    case Phase::kLongPress:
      mouse(EventType::kMouseUp, first_.x, first_.y, MouseButton::kRight);
      break;
    case Phase::kDrag:
    case Phase::kPinch:
      input_.end_gesture();
      break;
    case Phase::kIdle:
    case Phase::kDone:
      break;
  }
}

void TouchMapper::feed(const InputEvent& event) {
  const Finger finger{.id = event.finger, .x = event.x, .y = event.y};
  switch (event.type) {
    case EventType::kFingerDown:
      if (phase_ == Phase::kIdle) {
        fingers_ = 1;
        first_ = finger;
        second_.reset();
        start_x_ = finger.x;
        start_y_ = finger.y;
        down_ms_ = event.time_ms;
        phase_ = Phase::kPending;
        mouse(EventType::kMouseMove, finger.x, finger.y);
        return;
      }
      ++fingers_;
      if (phase_ == Phase::kPending || phase_ == Phase::kDrag) {
        finish(false);
        second_ = finger;
        phase_ = Phase::kPinch;
        input_.begin_gesture((first_.x + second_->x) / 2, (first_.y + second_->y) / 2);
      }
      return;
    case EventType::kFingerMove: {
      const bool is_first = finger.id == first_.id;
      const bool is_second = second_ && finger.id == second_->id;
      if (phase_ == Phase::kPinch && (is_first || is_second)) {
        const std::int64_t before = spread();
        (is_first ? first_ : *second_) = finger;
        const std::int64_t after = spread();
        if (before > 0 && after > 0) {
          input_.zoom_gesture(static_cast<float>(std::sqrt(static_cast<double>(after) / static_cast<double>(before))));
        }
        return;
      }
      if (!is_first) {
        return;
      }
      if (phase_ == Phase::kPending &&
          (std::abs(finger.x - start_x_) > kSlop || std::abs(finger.y - start_y_) > kSlop)) {
        phase_ = Phase::kDrag;
        input_.begin_gesture(start_x_, start_y_);
        input_.drag_gesture(finger.x - start_x_, finger.y - start_y_);
      } else if (phase_ == Phase::kDrag) {
        input_.drag_gesture(finger.x - first_.x, finger.y - first_.y);
      } else if (phase_ == Phase::kLongPress) {
        mouse(EventType::kMouseMove, finger.x, finger.y);
      } else if (phase_ != Phase::kPending) {
        return;
      }
      first_ = finger;
      return;
    }
    case EventType::kFingerUp:
    case EventType::kFingerCancel:
      if (phase_ == Phase::kIdle) {
        return;
      }
      fingers_ = fingers_ > 0 ? fingers_ - 1 : 0;
      if (phase_ == Phase::kPinch || finger.id == first_.id) {
        finish(event.type == EventType::kFingerUp);
        phase_ = Phase::kDone;
      }
      if (fingers_ == 0) {
        phase_ = Phase::kIdle;
      }
      return;
    default:
      return;
  }
}

void TouchMapper::update(std::uint64_t now_ms) {
  if (phase_ == Phase::kPending && now_ms - down_ms_ >= kLongPress) {
    phase_ = Phase::kLongPress;
    mouse(EventType::kMouseDown, start_x_, start_y_, MouseButton::kRight);
  }
}

}  // namespace oa
