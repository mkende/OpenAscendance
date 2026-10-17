// The touch screen as the original's mouse (a remake addition): the fingers'
// events turned into the pointer's moves and button changes that the
// original reads, and into the view gestures (`ViewGesture`).
//
// - A finger put down moves the pointer there at once (the hover).
// - Lifted without moving past `kSlop` before `kLongPress`: a left click
//   there (a press and a release).
// - Held still for `kLongPress`: the right button pressed there, released
//   with the finger (the original's continuous actions and its cancels).
// - Moved past `kSlop` first: a view drag, without any click.
// - A second finger: a pinch, without any click, until every finger is up.
#pragma once

#include <cstdint>
#include <optional>

#include "platform/input.h"
#include "platform/input_event.h"

namespace oa {

class TouchMapper {
 public:
  // The distance a finger may move and still tap, in screen pixels.
  static constexpr int kSlop = 10;
  // The hold of a long press, in milliseconds.
  static constexpr std::uint64_t kLongPress = 500;

  // `input` gets the mouse events and gestures; it must outlive the mapper.
  explicit TouchMapper(Input& input) : input_(input) {}

  // Takes a finger event (`kFingerDown` .. `kFingerCancel`); others are
  // ignored.
  void feed(const InputEvent& event);
  // Presses the right button once a finger has been held `kLongPress`
  // (`now_ms` on the clock of the events' `time_ms`).
  void update(std::uint64_t now_ms);

 private:
  enum class Phase : std::uint8_t {
    kIdle,
    kPending,    // one finger down, not moved nor held yet
    kLongPress,  // the right button held
    kDrag,       // a view drag
    kPinch,      // two fingers
    kDone,       // the gesture is over; waiting for every finger to lift
  };
  struct Finger {
    std::uint64_t id = 0;
    int x = 0;
    int y = 0;
  };

  // Feeds a mouse event made from the touch.
  void mouse(EventType type, int x, int y, MouseButton button = MouseButton::kNone);
  // The squared distance between the two fingers.
  std::int64_t spread() const;
  // Ends the phase's gesture or button (`tap`: a pending touch clicks).
  void finish(bool tap);

  Input& input_;
  Phase phase_ = Phase::kIdle;
  Finger first_;
  std::optional<Finger> second_;
  int start_x_ = 0;
  int start_y_ = 0;
  std::uint64_t down_ms_ = 0;
  // The fingers down.
  int fingers_ = 0;
};

}  // namespace oa
