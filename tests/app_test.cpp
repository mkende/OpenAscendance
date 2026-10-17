// The command line and the choice of the install.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include "app/command_line.h"
#include "app/install_source.h"
#include "core/error.h"
#include "game_data.h"

namespace oa {
namespace {

CommandLine parse(std::vector<std::string_view> args) { return parse_command_line(args); }

TEST_CASE("command line: data folder, screenshots and scripted input", "[app]") {
  const CommandLine o =
      parse({"--data", "/x", "--screenshot", "s.bmp", "--frames", "40", "--keys", "-2,click:318:215,esc", "--sharp"});
  CHECK(o.data_dir == "/x");
  CHECK(o.screenshot == "s.bmp");
  CHECK(o.frames == 40);
  CHECK(o.scaling == Scaling::kSharp);
  REQUIRE(o.keys.size() == 5);
  CHECK(o.keys[0].type == EventType::kKeyUp);
  CHECK(o.keys[2].type == EventType::kMouseDown);
  CHECK(o.keys[2].x == 318);
  CHECK(o.keys[3].type == EventType::kMouseUp);
  CHECK(o.keys[4].type == EventType::kKeyDown);
  CHECK_THROWS(parse({"--frames", "0"}));
  CHECK_THROWS(parse({"--data"}));
  CHECK_THROWS(parse({"--bogus", "1"}));
}

TEST_CASE("the remembered install folder", "[app]") {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "oa_app_test";
  std::filesystem::remove_all(dir);
  const std::filesystem::path cfg = dir / "sub" / "INSTALL.CFG";
  CHECK_FALSE(load_install_config(cfg));
  save_install_config(cfg, "/games/ascend");
  CHECK(load_install_config(cfg) == std::filesystem::path("/games/ascend"));
  {
    std::ofstream bad(cfg);
    bad << "something else\n";
  }
  CHECK_THROWS_AS(load_install_config(cfg), DataError);
  std::filesystem::remove_all(dir);
}

TEST_CASE("start-up: --data, then the remembered folder, else the setup", "[app]") {
  const StartupInstall none = choose_startup_install(std::filesystem::path("/no/such/dir"), false, std::nullopt);
  CHECK_FALSE(none.install);
  CHECK(none.problems.size() == 1);
  CHECK(choose_startup_install(std::nullopt, false, std::nullopt).problems.empty());
  const auto install = test::game_install();
  if (!install) {
    SKIP("OA_DATA is not set to an Ascendancy install");
  }
  const StartupInstall remembered =
      choose_startup_install(std::filesystem::path("/no/such/dir"), false, install->root());
  CHECK(remembered.install);
  CHECK(remembered.problems.size() == 1);
  CHECK_FALSE(choose_startup_install(install->root(), true, std::nullopt).install);
}

}  // namespace
}  // namespace oa
