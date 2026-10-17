// The SDL3 window showing the 640x480 indexed picture (the original's VESA
// screen and VFX driver, `docs/05-subsystems.md`), and the input events.
// Simplified from SilentHeritage's platform/display.h.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "core/image.h"
#include "platform/input_event.h"

namespace oa {

// How the 640x480 picture is scaled to the window.
enum class Scaling : std::uint8_t {
  kSmooth,  // bilinear (the default)
  kSharp,   // nearest neighbour: the original pixels
};

// The game window. The indexed frame is converted through the palette and
// scaled to the window keeping its 4:3 aspect ratio (black bars). Only one
// display may exist; it initializes and shuts down SDL video. The system
// pointer is hidden over the window: the game draws its own.
class Display {
 public:
  // Opens a resizable window `scale` times the original size. Throws
  // `std::runtime_error` if SDL fails.
  Display(std::string_view title, int scale, Scaling scaling);
  ~Display();
  Display(const Display&) = delete;
  Display& operator=(const Display&) = delete;

  // Shows the 640x480 `frame` with `palette`.
  void present(const Image8& frame, const Palette& palette);

  // Returns the next pending input event, if any. Alt+Enter (either Enter
  // key) toggles fullscreen, as in DOSBox: its key down, repeats and release
  // never reach the caller.
  std::optional<InputEvent> poll();

  bool fullscreen() const;
  void set_fullscreen(bool fullscreen);
  Scaling scaling() const;
  void set_scaling(Scaling scaling);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace oa
