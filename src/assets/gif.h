// The GIF pictures (`logo.gif`, `0opening.gif`: GIF87a, 640x480, one
// non-interlaced image), decoded like `vfx_gif_decode` (0x5c201), which skips
// the colour tables: the picture shows with the palette already loaded.
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "core/image.h"

namespace oa {

// Returns the first image of the GIF `data` (its pixels only; pixels missing
// from a short stream are 0). Throws `DataError` naming `context` when it is
// not a GIF, is interlaced or its LZW stream is malformed.
Image8 decode_gif(std::span<const std::uint8_t> data, std::string_view context = "GIF");

// `screenshot_save_gif` (0x31338) with `gif_lzw_encode_screen` (0x31228):
// the GIF87a of `image` (one image, no local table, LZW minimum code size 8)
// with the global colour table `palette` (6-bit values scaled by 255 / 63).
std::vector<std::uint8_t> encode_gif(const Image8& image, const VgaPalette& palette);

}  // namespace oa
