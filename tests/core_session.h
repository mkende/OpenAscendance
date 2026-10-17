// A game session over the game core for the tests of the game screens: a
// save of the install (or of the oracle corpus) loaded into a
// `game::Session`, which `GameSession::core` hands to the screens.
#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string_view>
#include <vector>

#include "assets/cfile.h"
#include "game/save.h"
#include "game/session.h"
#include "game/tables.h"
#include "ui/game_session.h"

namespace oa::test {

class CoreGameSession : public StubGameSession {
 public:
  // Loads `save` (the bytes of a `.SAV` / `RESUME.GAM`) over the tables of
  // `files`; the player's race is the save header's.
  CoreGameSession(const GameFiles& files, const std::vector<std::uint8_t>& save)
      : tables_(std::make_unique<game::GameTables>(game::load_game_tables(files))), session_(*tables_) {
    const game::SaveImage image = game::parse_save(save);
    game::load_game(session_, image);
    session_.set_player_race(static_cast<std::uint8_t>(image.header.player_race));
  }

  bool has_game() const override { return true; }
  int day() const override { return session_.game->cosmos.day; }
  int race_count() const override { return session_.game->cosmos.race_count; }
  int player_race() const override { return session_.player_race; }
  int race_species(int race) const override { return session_.game->cosmos.races.at(race).species; }
  int race_color(int race) const override { return session_.game->cosmos.races.at(race).color; }
  game::Session* core() override { return &session_; }

  game::Session& session() { return session_; }

 private:
  std::unique_ptr<game::GameTables> tables_;
  game::Session session_;
};

// Returns the bytes of the file at `path`, empty when it cannot be read.
inline std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace oa::test
