// The graphics system (`gsystem.cpp`, global `GSystem` 0xc84e0): the
// 640x480 8-bit screen, the VGA DAC, the palette shadow, the palette fades
// and the haze tables (`docs/07-ui-framework.md`, "Graphics system").
//
// The original draws into an off-screen buffer whose dirty panes are copied
// to video memory once per main-loop pass; a few paths write to the screen
// directly. Here there is one picture, shown whole at each presentation, so
// the dirty-pane bookkeeping is left out.
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

#include "assets/cfile.h"
#include "assets/pal.h"
#include "core/image.h"
#include "gfx/vfx.h"

namespace oa {

class GSystem {
 public:
  // Waits for the next vertical blank: the platform presents the screen
  // with the current DAC and waits for the next 1/60 s.
  using VblankWait = std::function<void()>;

  // `wait_vblank` must be set before the fades that wait.
  explicit GSystem(VblankWait wait_vblank);

  Image8& screen() { return screen_; }
  const Image8& screen() const { return screen_; }
  // The pane covering the whole screen (`GSystem+0x14`).
  static constexpr Pane kScreenPane{};

  // The DAC as programmed: what the screen shows.
  const VgaPalette& dac() const { return dac_; }
  // The palette shadow (`+0x433`), the target of the fades.
  const VgaPalette& shadow() const { return shadow_; }

  // Waits for a vertical blank (VFX driver slot 5).
  void wait_vblank() const;

  // `GSystem_load_palette` (0x2b878): loads `file` into the shadow at
  // `first..first+count-1` and programs the DAC with it. Throws `DataError`.
  void load_palette(const GameFiles& files, std::string_view file, int first, int count);
  // Sets shadow entries `first..first+count-1` from the 6-bit triplets `rgb`
  // (`GSystem_set_palette_entries` 0x2b900, without touching the DAC).
  void set_palette_entries(int first, int count, std::span<const std::uint8_t> rgb);
  // `GSystem_set_dac_range` (0x2b944): programs DAC entries
  // `first..first+count-1` from `rgb`, black when empty.
  void set_dac_range(int first, int count, std::span<const std::uint8_t> rgb = {});
  // Programs one DAC entry (VFX driver slot 9).
  void set_dac(int index, std::uint8_t r, std::uint8_t g, std::uint8_t b);
  // `GSystem_read_dac_range` (0x2b9d0) to its default destination: reads DAC
  // entries `first..first+count-1` into the shadow.
  void read_dac_into_shadow(int first, int count);

  // `GSystem_fade_to_palette_step` (0x2bd04): step `k` (0..64) of the fade
  // from the DAC (read at step 0) to the shadow: a component moves 1 toward
  // its target while farther than `64 - k`.
  void fade_to_palette_step(int k);
  // `GSystem_fade_to_color_step` (0x2b424): the same toward the single colour
  // `rgb` (3 bytes; black when empty), with `max(0, 64 - k)`.
  void fade_to_color_step(int k, std::span<const std::uint8_t> rgb = {});
  // The subtitle fades (`GSystem_fade_in_range_step` 0x2bbe0 /
  // `GSystem_fade_out_range_step` 0x2bb38): every component of DAC entries
  // `first..first+count-1` moves 2 toward the shadow (in), or 2 toward 0
  // (out), one at a time and clamped.
  void fade_in_range_step(int first, int count);
  void fade_out_range_step(int first, int count);

  // `palette_fade_window_colors` (0x5c9fd): fades the colours the screen
  // uses from the DAC to `target` in Bresenham steps over about `vblanks`
  // vertical blanks (blocking); the other entries are left as they are.
  void fade_window_colors(const VgaPalette& target, int vblanks);

  // `GSystem_alloc_haze_tables` / `GSystem_load_haze_table` (0x2b7ac,
  // 0x2b7e0): the haze tables of `windows.txt`, by index.
  std::vector<HazeTable>& haze_tables() { return haze_tables_; }

 private:
  VblankWait wait_vblank_;
  Image8 screen_{kScreenWidth, kScreenHeight};
  VgaPalette dac_;
  VgaPalette dac_copy_;  // `+0x133`, the source of the stepped fades
  VgaPalette shadow_;
  std::vector<HazeTable> haze_tables_;
};

}  // namespace oa
