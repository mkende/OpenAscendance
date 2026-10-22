// The SDL3 window showing the 640x480 indexed picture (the original's VESA
// screen and VFX driver, `docs/05-subsystems.md`), and the input events.
// Simplified from SilentHeritage's platform/display.h.
#pragma once

#include <array>
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

// Where the 640x480 picture goes in the window's output, in pixels.
struct PictureRect {
  float x = 0;
  float y = 0;
  float w = 0;
  float h = 0;

  bool operator==(const PictureRect&) const = default;
};

// Returns the place of the picture in an output of `width` x `height`
// pixels whose edges (left, top, right, bottom) lose `cutout` pixels to the
// display cutout: the largest 4:3 rectangle in what is left, centred there
// (the whole output when the cutout leaves less than a quarter of the
// picture's size).
PictureRect letterbox(int width, int height, const std::array<int, 4>& cutout);

// A rectangle of the window, in its coordinates (the mouse's).
struct WindowRect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  bool operator==(const WindowRect&) const = default;
};

// Returns the window rectangle covering the picture `picture` (output
// pixels) of a window of `density` output pixels per window unit: its
// partly covered units included, so that the mouse confined to it reaches
// the picture's every edge pixel, and never more than one unit past them.
WindowRect pointer_confinement(const PictureRect& picture, float density);

// The game window. The indexed frame is converted through the palette and
// scaled to the window keeping its 4:3 aspect ratio (black bars), clear of
// the display cutout on Android, where the window is always the full screen
// without the system bars. Only one display may exist; it initializes and
// shuts down SDL video. The system pointer is hidden over the window: the
// game draws its own. In the full screen, the system pointer is confined to
// the picture (`pointer_confinement`), so that it turns back at once at the
// picture's edges, where the game's stops. Fingers come as finger events (no mouse events made
// of them, `TouchMapper` does), Android's Back as `kBack`.
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

  // The picture's place in the output now (`letterbox`), recomputed as soon
  // as the window or its display changes (resized, folded, rotated, moved to
  // another display or scale): the last frame is then shown again at once,
  // and the pointer and fingers mapped through the new place.
  PictureRect picture() const;
  // Resizes the window (a window, not the full screen).
  void set_window_size(int width, int height);

  bool fullscreen() const;
  void set_fullscreen(bool fullscreen);
  Scaling scaling() const;
  void set_scaling(Scaling scaling);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace oa
