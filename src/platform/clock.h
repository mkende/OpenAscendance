// The original's timers (AIL timers on the PIT, `docs/05-subsystems.md`): the
// 100 Hz game tick (`WinMgr_timer_tick` 0x4f1f4 on `0x12202c`) and the
// vertical blank (60 Hz in 640x480), on the system clock or, for headless
// runs, on a virtual clock that only moves when the program waits.
#pragma once

#include <cstdint>

namespace oa {

class Clock {
 public:
  // The game tick rate (`WinMgr_start_timer` 0x4f9fc).
  static constexpr int kTickHz = 100;
  // The vertical blank rate of the VGA 640x480 mode.
  static constexpr int kVblankHz = 60;

  // A clock on the system's time, or a virtual one (`virtual_time`)
  // starting at 0.
  explicit Clock(bool virtual_time);

  // Returns the 100 Hz ticks since the clock's start (wrapping).
  std::uint32_t ticks() const;
  // Returns the microseconds since the clock's start.
  std::uint64_t micros() const;

  // Waits until the next vertical blank.
  void wait_vblank();
  // Waits about a millisecond (a virtual clock advances by exactly 1 ms).
  void idle();
  // Waits until `ticks()` is at least `tick` (as an unsigned difference).
  void wait_until_tick(std::uint32_t tick);

  bool is_virtual() const { return virtual_; }

  // Stops the clock while the program is in the background (a remake
  // addition): the time until `resume` does not count. Nothing for a
  // virtual clock or when already so.
  void suspend();
  void resume();

 private:
  // The system's time, in nanoseconds since the start, but the background.
  std::uint64_t system_ns() const;

  bool virtual_;
  std::uint64_t start_ns_ = 0;
  std::uint64_t virtual_us_ = 0;
  // Since when the clock is suspended (the system's ticks), 0 when it runs.
  std::uint64_t suspended_at_ns_ = 0;
};

}  // namespace oa
