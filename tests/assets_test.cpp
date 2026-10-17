// The COB archives and the asset decoders, against the original files
// (reference values from the Python decoders of tools/ and from ffmpeg).

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "assets/cfile.h"
#include "assets/flc.h"
#include "assets/fnt.h"
#include "assets/gif.h"
#include "assets/pal.h"
#include "assets/pcm.h"
#include "assets/shp.h"
#include "core/error.h"
#include "game_data.h"

namespace oa {
namespace {

// CRC-32 (the zlib one), to compare pictures with the reference decoders'.
std::uint32_t crc32(std::span<const std::uint8_t> data) {
  std::uint32_t crc = 0xffffffffu;
  for (const std::uint8_t b : data) {
    crc ^= b;
    for (int k = 0; k < 8; ++k) {
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
  }
  return ~crc;
}

TEST_CASE("a COB directory gives each member up to the next one", "[assets]") {
  // 2 members: "a" (3 bytes) and "b\\c" (2 bytes).
  std::vector<std::uint8_t> data(4 + 2 * 54);
  data[0] = 2;
  data[4] = 'a';
  data[4 + 50] = 'b';
  data[4 + 51] = '\\';
  data[4 + 52] = 'c';
  const std::uint32_t start = 4 + 2 * 54;
  data[4 + 100] = static_cast<std::uint8_t>(start);
  data[4 + 104] = static_cast<std::uint8_t>(start + 3);
  const std::vector<CobMember> m = parse_cob_directory(data, start + 5, 0, "test");
  REQUIRE(m.size() == 2);
  CHECK(m[0].name == "a");
  CHECK(m[0].size == 3);
  CHECK(m[1].name == "b\\c");
  CHECK(m[1].offset == start + 3);
  CHECK(m[1].size == 2);
  CHECK_THROWS_AS(parse_cob_directory(data, start + 2, 0, "test"), DataError);
}

TEST_CASE("the install's archive members are found case-insensitively", "[assets][content]") {
  OA_REQUIRE_GAME_FILES(files);
  CHECK(files.directory().archives().size() == 3);
  CHECK(files.directory().find("DATA/GAME.PAL") != nullptr);
  CHECK(files.load("data\\button.voc").size() == 3340);
  // A loose file of the install.
  CHECK(files.load("logic.flc").size() == 3835618);
  CHECK_FALSE(files.try_load("data\\nothing.shp"));
}

TEST_CASE("palettes and haze tables", "[assets][content]") {
  OA_REQUIRE_GAME_FILES(files);
  VgaPalette p;
  CHECK(load_palette(p, files.load("DATA\\game.pal")) == 256);
  // 0xf3 is the white of the UI text, 0xf2 / 0x96 the button backgrounds.
  CHECK(p.dac[0xf3 * 3] == 63);
  CHECK(p.dac[0xf2 * 3 + 2] == 12);
  CHECK(p.dac[0x96 * 3 + 1] == 17);
  VgaPalette sub;
  CHECK(load_palette(sub, files.load("DATA\\SUBFONT.PAL"), 240) == 16);
  CHECK(sub.dac[0] == 0);
  const HazeTable black00 = parse_haze_table(files.load("DATA\\BLACK00.HAZ"));
  // Not quite the identity (docs/02-graphics-formats.md).
  CHECK(black00[0x78] == 0x78);
  CHECK(black00[0x79] == 0x80);
  CHECK(black00[0x7f] == 0x57);
}

TEST_CASE("shape tables decode as the Python reference", "[assets][content]") {
  OA_REQUIRE_GAME_FILES(files);
  const ShapeTable opening(files.load("DATA\\0OPENING.SHP"));
  REQUIRE(opening.size() == 7);
  const Shape& s = opening.at(0);
  CHECK(s.x0 == 0);
  CHECK(s.width() == 319);
  CHECK(s.height() == 31);
  const DecodedShape d = decode_shape(s);
  std::vector<std::uint8_t> pixels(d.pixels.pixels().begin(), d.pixels.pixels().end());
  for (std::size_t i = 0; i < pixels.size(); ++i) {
    if (!d.opaque[i]) {
      pixels[i] = 255;
    }
  }
  CHECK(crc32(pixels) == 0x1109f00e);
  const ShapeTable mouse(files.load("DATA\\MOUSE.SHP"));
  CHECK(mouse.size() == 12);
  CHECK(mouse.at(0).x0 == -1);
  CHECK(mouse.at(0).width() == 12);
}

TEST_CASE("fonts", "[assets][content]") {
  OA_REQUIRE_GAME_FILES(files);
  const Font big = parse_font(files.load("DATA\\BIGFONT.FNT"));
  CHECK(big.height == 11);
  CHECK(big.background == 255);
  CHECK(big.widths.size() == 128);
  CHECK(big.char_width('A') == 8);
  // `^` stands for a space in windows.txt: as wide, and blank.
  CHECK(big.char_width('^') == big.char_width(' '));
  const Font intro = parse_font(files.load("DATA\\INTRO.FNT"));
  CHECK(intro.height == 39);
  CHECK(intro.char_width('T') == 21);
}

TEST_CASE("GIF pictures decode as the Python reference", "[assets][content]") {
  OA_REQUIRE_GAME_FILES(files);
  const Image8 opening = decode_gif(files.load("DATA\\0OPENING.GIF"));
  CHECK(opening.width() == 640);
  CHECK(opening.height() == 480);
  CHECK(crc32(opening.pixels()) == 0x51b50c73);
  CHECK(crc32(decode_gif(files.load("DATA\\LOGO.GIF")).pixels()) == 0x6d087e24);
}

TEST_CASE("FLC frames decode as ffmpeg's", "[assets][content]") {
  OA_REQUIRE_GAME_FILES(files);
  const auto ignore = [](int, std::uint8_t, std::uint8_t, std::uint8_t) {};
  {
    const FlcFile intro(files.load("data\\intro65a.flc"));
    CHECK(intro.header().frames == 1143);
    CHECK(intro.header().speed_ms == 71);
    Image8 screen(640, 480);
    int entries = 0;
    const FlcFrameChunks first =
        intro.decode_frame(0, screen, 0, 0, [&](int, std::uint8_t, std::uint8_t, std::uint8_t) { ++entries; }, 240, 16);
    CHECK(first.color256);
    CHECK(first.brun);
    // The subtitle colours 240..255 are protected.
    CHECK(entries == 240);
    CHECK(crc32(screen.pixels()) == 0xe39df4e8);
    intro.decode_frame(1, screen, 0, 0, ignore);
    CHECK(crc32(screen.pixels()) == 0xf449bd42);
    for (int f = 2; f < 30; ++f) {
      intro.decode_frame(f, screen, 0, 0, ignore);
    }
    CHECK(crc32(screen.pixels()) == 0x20daeb8b);
  }
  {
    const FlcFile logic(files.load("LOGIC.FLC"));
    CHECK(logic.header().frames == 225);
    Image8 screen(640, 480);
    for (int f = 0; f <= 150; ++f) {
      logic.decode_frame(f, screen, 0, 0, ignore);
    }
    CHECK(crc32(screen.pixels()) == 0x613eed5c);
  }
}

TEST_CASE("sound effects drop the 42-byte header and the last byte", "[assets][content]") {
  OA_REQUIRE_GAME_FILES(files);
  const std::vector<std::uint8_t> button = sfx_samples(files.load("data\\button.voc"));
  CHECK(button.size() == 3340 - 43);
  CHECK(button.front() == 128);
  CHECK_THROWS_AS(sfx_samples(std::vector<std::uint8_t>(43)), DataError);
}

}  // namespace
}  // namespace oa
