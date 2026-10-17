// The original's input layer (`eventwnd`, 0x25a10-0x25cc8): the keyboard
// read through the BIOS (INT 16h, one key per main-loop pass, typematic
// repeat from the buffer) and the mouse polled with edge detection, kept in
// globals the window manager reads (`docs/07-ui-framework.md`, "Input
// layer"). Here they are fed by the platform's input events.
#pragma once

#include <cstdint>
#include <deque>

#include "core/image.h"
#include "platform/input_event.h"

namespace oa {

// A gesture turning a 3D view (a remake addition, not in the original's
// globals): a drag with the left button or one finger past a few pixels, a
// pinch of two fingers. `WinMgr` hands it to the window whose view it
// started on.
struct ViewGesture {
  bool started = false;  // a gesture started since the last poll, at (x, y)
  bool active = false;   // the gesture goes on after this poll
  int x = 0;
  int y = 0;
  // The drag since the last poll, in screen pixels.
  int dx = 0;
  int dy = 0;
  // The pinch since the last poll: the ratio of the fingers' distances
  // (above 1 apart), 1 for none.
  float zoom = 1.0F;
};

// The input globals (`0xc8484..0xc84d8`) after a poll. Booleans are plain
// `bool` here (0 / -1 in the original).
struct InputState {
  bool key_event = false;         // 0xc8488: a key was read this pass
  bool alt = false;               // 0xc848c
  bool ctrl = false;              // 0xc8490
  bool shift = false;             // 0xc8494: either Shift
  std::uint16_t scan_code = 0;    // 0xc84ac
  std::uint16_t ascii = 0;        // 0xc84ae
  std::uint16_t shift_flags = 0;  // 0xc84b0: BIOS shift state (INT 16h AH=2)
  bool mouse_present = true;      // 0xc84b4
  bool left_down = false;         // 0xc84b8
  bool right_down = false;        // 0xc84bc
  bool left_changed = false;      // 0xc84c4: pressed or released this pass
  bool right_changed = false;     // 0xc84c8
  bool moved = false;             // 0xc84d0
  int x = 0;                      // 0xc84d4
  int y = 0;                      // 0xc84d8
  // The remake's additions: the system's Back this pass, and the view
  // gesture.
  bool back = false;
  ViewGesture gesture;
};

// Turns the platform's input events into the original's polled state.
class Input {
 public:
  // The BIOS keyboard buffer's capacity (keys typed beyond it are lost).
  static constexpr std::size_t kKeyBufferSize = 15;

  // The distance a mouse may move with its left button held before it
  // drags a view, in screen pixels.
  static constexpr int kMouseDragSlop = 4;

  // Takes one input event: key words go to the keyboard buffer, the mouse
  // updates the pointer and queues its button changes, Back is reported by
  // the next poll. A mouse (not a finger) moved past `kMouseDragSlop` with
  // the left button held drags a view, until the button is released (the
  // press itself stays a click). Finger events go through `TouchMapper`.
  void feed(const InputEvent& event);

  // The view gestures (`ViewGesture`), reported by the next poll: one
  // starting at (x, y), its drags and pinches, its end.
  void begin_gesture(int x, int y);
  void drag_gesture(int dx, int dy);
  void zoom_gesture(float ratio);
  void end_gesture();

  // `input_poll` (0x25c8c): clears this pass's event flags, reads the shift
  // state and at most one key, and the mouse with edge detection against the
  // previous poll. Button changes queued since are applied one per button
  // per poll, so that a press and its release always show on two passes
  // (the original's mouse status could miss a click shorter than a pass).
  const InputState& poll();

  const InputState& state() const { return state_; }

  // `MOUSE_force_move`: the pointer moved to (x, y) without a move event
  // (a replayed trace's).
  void warp(int x, int y) {
    mouse_x_ = x;
    mouse_y_ = y;
    state_.x = x;
    state_.y = y;
  }

 private:
  struct ButtonChange {
    MouseButton button;
    bool down;
  };

  // A mouse event's part in the view drags; the pointer was at
  // (`from_x`, `from_y`) before it.
  void track_mouse_drag(const InputEvent& event, int from_x, int from_y);

  InputState state_;
  std::deque<std::uint16_t> keys_;
  std::deque<ButtonChange> buttons_;
  std::uint16_t mods_ = 0;
  bool back_ = false;
  ViewGesture gesture_;
  // The mouse's left button as fed (not as polled), where it was pressed,
  // and whether its moves drag a view.
  bool mouse_left_ = false;
  int press_x_ = 0;
  int press_y_ = 0;
  bool mouse_dragging_ = false;
  // `MOUSE_init` (0x612b4) puts the pointer at the screen's centre.
  int mouse_x_ = kScreenWidth / 2;
  int mouse_y_ = kScreenHeight / 2;
};

}  // namespace oa
