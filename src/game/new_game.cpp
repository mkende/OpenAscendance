#include "game/new_game.h"

#include <algorithm>

#include "assets/tokens.h"
#include "core/fixed_width.h"
#include "game/galaxy.h"

namespace oa::game {

CosmosTxt parse_cosmos_txt(std::span<const std::uint8_t> data) {
  TokenReader in(data, "cosmos.txt");
  CosmosTxt txt;
  txt.stars = wrap_i16(in.keyword_integer());
  txt.races = wrap_i16(in.keyword_integer());
  txt.seed = in.keyword_integer();
  while (!in.at_end() && txt.species.size() < static_cast<std::size_t>(std::max<std::int16_t>(txt.races, 0))) {
    txt.species.push_back(in.integer());
  }
  return txt;
}

void program_start(Session& session, std::uint32_t time_seed, const CosmosTxt* cosmos_txt, bool resume_exists) {
  session.rng.srand(time_seed);
  session.set_player_race(0);
  if (cosmos_txt == nullptr) {
    cosmos_generate(session, 50, 7, 0);
    cosmos_generate_planets(session);
  } else {
    if (cosmos_txt->seed != -1) {
      session.rng.srand(static_cast<std::uint32_t>(cosmos_txt->seed));
    }
    cosmos_generate(session, cosmos_txt->stars, cosmos_txt->races, 0);
    cosmos_generate_planets(session);
    // Only the species byte: no `.dip` reload, no ability update.
    for (std::size_t i = 0; i < cosmos_txt->species.size() && i < kMaxRaces; ++i) {
      session.game->cosmos.races[i].species = wrap_u8(cosmos_txt->species[i]);
    }
  }
  cosmos_assign_colors(session, 0);
  if (!resume_exists) {
    cosmos_compute_hops(session.game->cosmos);
  }
}

void NewGameSetup::generate(Session& session) const {
  cosmos_generate(session, kDensityStars.at(static_cast<std::size_t>(density_)), race_count_,
                  static_cast<std::uint8_t>(species_));
}

void NewGameSetup::activate(Session& session) {
  density_ = 2;
  race_count_ = 5;
  atmosphere_ = Atmosphere::kNeutral;
  session.game->cosmos.atmosphere = Atmosphere::kNeutral;
  species_ = 0;
  generate(session);
  color_ = 0;
  session.player_race = 0;
  session.game->cosmos.races[0].species = 0;
  cosmos_assign_colors(session, static_cast<std::uint8_t>(color_));
}

void NewGameSetup::next_density(Session& session) {
  density_ = static_cast<std::int16_t>(density_ + 1);
  if (density_ == 5) {
    density_ = 0;
  }
  generate(session);
}

void NewGameSetup::next_race_count(Session& session) {
  race_count_ = static_cast<std::int16_t>(race_count_ + 1);
  if (race_count_ == 8) {
    race_count_ = 3;
  }
  cosmos_new_galaxy(session, race_count_, static_cast<std::uint8_t>(species_));
}

void NewGameSetup::next_atmosphere() {
  atmosphere_ = atmosphere_ == Atmosphere::kHostile
                    ? Atmosphere::kPeaceful
                    : static_cast<Atmosphere>(static_cast<std::uint8_t>(atmosphere_) + 1);
}

void NewGameSetup::pick_species(Session& session, std::uint8_t species) {
  if (species_ != species) {
    species_ = species;
    cosmos_new_galaxy(session, race_count_, static_cast<std::uint8_t>(species_));
  }
}

void NewGameSetup::pick_color(Session& session, std::uint8_t color) {
  color_ = color;
  cosmos_assign_colors(session, color);
}

void NewGameSetup::begin(Session& session) {
  cosmos_generate_planets(session);
  cosmos_compute_hops(session.game->cosmos);
  session.game->cosmos.atmosphere = atmosphere_;
}

}  // namespace oa::game
