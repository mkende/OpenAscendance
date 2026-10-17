// The text tables: windows.txt, SUBTITLE.TXT, music.txt, soundfx.txt.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "assets/flic_script.h"
#include "assets/sound_tables.h"
#include "assets/windows_txt.h"
#include "core/error.h"
#include "game_data.h"

namespace oa {
namespace {

std::vector<std::uint8_t> bytes(std::string_view s) { return {s.begin(), s.end()}; }

TEST_CASE("windows.txt is read by position, its keyword names ignored", "[tables]") {
  const WindowsTxt w = parse_windows_txt(
      bytes("MEM_REQ 1\r\nHAZEBRBLES 200 NUMTABLES 1 TABLE DATA\\A.HAZ\r\nFONTS 300 SMALLFONT S.FNT LARGEFONT L.FNT\r\n"
            "TYPE 100 STATENUMBER 0 SHAPEFILE DATA\\0OPENING.SHP\r\n"
            "TYPE 0 NAME Exit PARENT NONE STATE 2 0 4 MOUSEFOCUS 1 X0 1 Y0 2 X1 3 Y1 4 SHAPEFRAME -1\r\n"
            "SOUNDCHANNEL 0 SENDMESSAGE 1 SENDPARAM1 999 SENDPARAM2 1 HELPINDEX exit WINITEMS 1\r\n"
            "TEXTITEM 0 Exit^to^DOS 2 0 4\r\n"
            "TYPE 13 NAME DBG X0 0 Y0 0 X1 9 Y1 9 cX0 1 cY0 2 cX1 3 typo 4 SHAPEFILE D.SHP\r\n"
            "END -1\r\n\x1a"));
  CHECK(w.haze_tables == std::vector<std::string>{"DATA\\A.HAZ"});
  CHECK(w.large_font == "L.FNT");
  CHECK(w.screen_shapes.at(0) == "DATA\\0OPENING.SHP");
  REQUIRE(w.windows.size() == 2);
  const WindowRecord& exit = w.windows[0];
  CHECK(exit.screens == std::vector<int>{0, 4});
  CHECK(exit.mouse_focus);
  CHECK(exit.x1 == 3);
  CHECK(exit.param1 == 999);
  REQUIRE(exit.items.size() == 1);
  CHECK(exit.items[0].text == "Exit^to^DOS");
  CHECK(exit.items[0].flags == 2);
  CHECK(exit.items[0].y == 4);
  CHECK(w.windows[1].extras.at("cY1") == "4");
  CHECK(w.windows[1].extras.at("SHAPEFILE") == "D.SHP");
  CHECK_THROWS_AS(parse_windows_txt(bytes("MEM_REQ 1 TYPE 27 NAME X")), DataError);
}

TEST_CASE("the install's windows.txt", "[tables][content]") {
  OA_REQUIRE_GAME_FILES(files);
  const WindowsTxt w = parse_windows_txt(files.load("windows.txt"));
  CHECK(w.haze_tables.size() == 23);
  CHECK(w.windows.size() == 124);
  const auto intro = std::ranges::find_if(w.windows, [](const WindowRecord& r) { return r.name == "INTROFLIC"; });
  REQUIRE(intro != w.windows.end());
  CHECK(intro->type == 1);
  CHECK(intro->screens == std::vector<int>{6});
  CHECK(intro->extras.at("FLICFILE") == "data\\intro65a.flc");
  CHECK(intro->extras.at("LOOPCOUNT") == "1");
  CHECK(intro->message == 3);
  const auto quit = std::ranges::find_if(w.windows, [](const WindowRecord& r) { return r.name == "EXITTODOS"; });
  REQUIRE(quit != w.windows.end());
  CHECK(quit->message == 2);
  CHECK(quit->y0 == 440);
}

TEST_CASE("SUBTITLE.TXT: subtitles 32 frames early, events sorted by frame", "[tables][content]") {
  OA_REQUIRE_GAME_FILES(files);
  const FlicScript s = parse_flic_script(files.load("SUBTITLE.TXT"));
  CHECK(s.title_y0 == 379);
  CHECK(s.title_y1 == 419);
  CHECK(s.font_file == "DATA\\INTRO.FNT");
  CHECK(s.palette_start == 240);
  CHECK(s.palette_count == 16);
  CHECK(s.subtitles.front() == "THIS GALAXY CONSISTS");
  REQUIRE(!s.events.empty());
  CHECK(s.events.front().frame == 1);
  CHECK(s.events.front().type == FlicEvent::kFrameRate);
  CHECK(s.events.front().a1 == 12);
  CHECK(std::ranges::is_sorted(s.events, {}, &FlicEvent::frame));
  // "THIS GALAXY CONSISTS" 55..60: shown at 23, hidden at 60.
  const auto show = std::ranges::find_if(
      s.events, [](const FlicEvent& e) { return e.type == FlicEvent::kShowSubtitle && e.a1 == 0; });
  REQUIRE(show != s.events.end());
  CHECK(show->frame == 23);
  const auto tune = std::ranges::find_if(s.events, [](const FlicEvent& e) { return e.type == FlicEvent::kPlayTune; });
  REQUIRE(tune != s.events.end());
  CHECK(tune->frame == 20);
  CHECK(tune->a1 == 6);
}

TEST_CASE("music.txt and soundfx.txt", "[tables][content]") {
  OA_REQUIRE_GAME_FILES(files);
  const std::vector<std::string> music = parse_music_table(files.load("music.txt"));
  REQUIRE(music.size() == 29);
  CHECK(music[0] == "data\\theme00.raw");
  CHECK(music[7] == "data\\intro01.raw");
  const SoundFxTable sfx = parse_soundfx_table(files.load("soundfx.txt"));
  CHECK(sfx.cache_classes[1].slots == 3);
  CHECK(sfx.cache_classes[1].slot_bytes == 45000);
  REQUIRE(sfx.names.size() == 86);
  CHECK(sfx.names[0] == "button");
  CHECK(sfx.names[11] == "shield");
  CHECK_THROWS_AS(parse_music_table(bytes("2 a.raw")), DataError);
}

}  // namespace
}  // namespace oa
