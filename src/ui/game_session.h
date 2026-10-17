// The game core as the user interface sees it: the narrow interface the
// window manager and the front-end screens call for the game state (the
// `Cosmos` global, `Game_save` 0x4f458 / `Game_load` 0x4f3c0, the new-game
// galaxy set-up), implemented by `src/game`. `StubGameSession` stands in for
// it until then: no game, no saves.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace oa {

// The 0x74-byte header of a save (`docs/02-data-formats.md`, "Saves"),
// what the load/save list shows.
struct SaveHeader {
  int day = 0;
  int race_count = 0;
  int player_race = 0;
  std::array<int, 7> species{};    // per race slot
  std::array<int, 7> colors{};     // per race slot
  std::array<int, 7> relations{};  // the player's: 0 not met, 1 peace, 2 war, 3 alliance
  int fleet = 0;                   // the player's ships
  int colonies = 0;
  int discoveries = 0;  // researched items
};

// A slot of the load/save list (`LoadWnd_read_headers` 0x30dc8).
struct SaveSlot {
  int slot = 0;                      // 1..20 (`%02d.sav`)
  std::optional<SaveHeader> header;  // nothing for an empty slot
  // The file's modification time (DOS file date and time), local time.
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
};

// The new-game settings of screen 3 (`NewGameWnd`, `docs/07-ui-framework.md`).
struct NewGameSettings {
  int species = 0;        // the player's species, 0..20
  int color = 0;          // the player's colour, 0..6
  int star_density = 2;   // 0..4
  int species_count = 5;  // 3..7
  int atmosphere = 1;     // 0 peaceful, 1 neutral, 2 hostile
};

class GameSession {
 public:
  static constexpr int kSaveSlots = 20;

  virtual ~GameSession() = default;

  // Returns whether a game is loaded (a new one begun, a save or
  // `resume.gam` loaded).
  virtual bool has_game() const = 0;
  // The current day (cosmos +0, the turn counter).
  virtual int day() const = 0;
  // The races of the game: their number, the player's slot, and per slot the
  // species (race +0x01) and colour (race +0x02).
  virtual int race_count() const = 0;
  virtual int player_race() const = 0;
  virtual int race_species(int race) const = 0;
  virtual int race_color(int race) const = 0;

  // The 20 slots of the load/save list, in order.
  virtual std::vector<SaveSlot> list_saves() const = 0;
  // `Game_save` into slot 1..20; returns false when it could not be written.
  virtual bool save(int slot) = 0;
  // `Game_load` of slot 1..20; returns false when the slot cannot be read.
  virtual bool load(int slot) = 0;
  // `resume.gam`: the game saved at exit and when the new-game screen opens,
  // loaded on the first opening menu.
  virtual bool resume_exists() const = 0;
  virtual bool save_resume() = 0;
  virtual bool load_resume() = 0;

  // The new-game screen (`NewGameWnd_handle_event` 0x4dd5c) calls these in
  // the original's order, each one a step of the galaxy set-up:
  // `Cosmos_generate` (on entry and on a star density change): a new
  // galaxy of `stars` stars and `races` races, the player's (race 0) of
  // `species`;
  virtual void generate_galaxy(int stars, int races, int species) = 0;
  // `Cosmos_new_galaxy` (on a species count or player species change): new
  // races for the same stars;
  virtual void new_galaxy(int races, int species) = 0;
  // `Cosmos_assign_colors` (on entry and on a colour pick): the races'
  // colours, the player's (race 0) being `color`;
  virtual void assign_colors(int color) = 0;
  // "Begin New Game": `Cosmos_generate_planets`, `Cosmos_compute_hops`, the
  // atmosphere (`0x93bac`) set to `settings.atmosphere`,
  // `Cosmos_setup_race_palettes` (the race slots' shapes): the game starts.
  virtual void begin_game(const NewGameSettings& settings) = 0;
  // The atmosphere byte (`0x93bac`: 0 peaceful, 1 neutral, 2 hostile), set
  // by "Begin" (the species history picks its paragraph by it).
  virtual int atmosphere() const = 0;
};

// The stand-in game core: no game, every save slot empty, nothing written.
class StubGameSession : public GameSession {
 public:
  bool has_game() const override { return game_; }
  int day() const override { return game_ ? 1 : 0; }
  int race_count() const override { return races_; }
  int player_race() const override { return 0; }
  int race_species(int race) const override { return race == 0 ? species_ : race; }
  int race_color(int race) const override { return race == 0 ? color_ : race; }
  std::vector<SaveSlot> list_saves() const override {
    std::vector<SaveSlot> slots(kSaveSlots);
    for (int i = 0; i < kSaveSlots; ++i) {
      slots[static_cast<std::size_t>(i)].slot = i + 1;
    }
    return slots;
  }
  bool save(int /*slot*/) override { return false; }
  bool load(int /*slot*/) override { return false; }
  bool resume_exists() const override { return false; }
  bool save_resume() override { return false; }
  bool load_resume() override { return false; }
  void generate_galaxy(int /*stars*/, int races, int species) override {
    races_ = races;
    species_ = species;
  }
  void new_galaxy(int races, int species) override {
    races_ = races;
    species_ = species;
  }
  void assign_colors(int color) override { color_ = color; }
  void begin_game(const NewGameSettings& settings) override {
    atmosphere_ = settings.atmosphere;
    game_ = true;
  }
  int atmosphere() const override { return atmosphere_; }

 private:
  bool game_ = false;
  int races_ = 0;
  int species_ = 0;
  int color_ = 0;
  int atmosphere_ = 1;
};

}  // namespace oa
