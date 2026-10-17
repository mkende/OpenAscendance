// The negotiation screen's dialogue (`game/negotiation.h`) on the shipped
// day-820 game: the `.dip` sentences, their placeholders, the third
// parties, answering an alien's proposal and the choices' outcomes (the
// screens' pixels and saves against the original are `[diplomacy]`).

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <string>
#include <vector>

#include "game/address.h"
#include "game/negotiation.h"
#include "game/raceneg.h"
#include "game/save.h"
#include "game/session.h"
#include "game_data.h"
#include "game_oracle.h"

namespace oa::game {
namespace {

// Stand-ins for the species names (static strings 101..121).
std::vector<std::string> species_names() {
  std::vector<std::string> names;
  for (int s = 0; s < 21; ++s) {
    names.push_back(std::format("S{}", s));
  }
  return names;
}

// The day-820 game (Chamachies, species 19, at peace with the Capelons,
// species 13, slot 1) and the dialogue of the Capelons and of the player.
struct Dialogue {
  explicit Dialogue(const GameTables& tables, const GameFiles& files)
      : session(tables),
        alien(parse_dip_sentences(files.load("RACE13.DIP"))),
        player(parse_dip_sentences(files.load("PLAYER.DIP"))),
        negotiation(session, names) {
    load_game(session, parse_save(files.load("01.SAV")));
    session.set_player_race(0);
  }
  Race& race(int r) { return session.game->cosmos.races.at(static_cast<std::size_t>(r)); }
  std::vector<int> codes() const {
    std::vector<int> out;
    for (const Negotiation::Option& o : negotiation.options()) {
      out.push_back(o.code);
    }
    return out;
  }

  Session session;
  std::vector<std::string> names = species_names();
  DipSentences alien;
  DipSentences player;
  Negotiation negotiation;
};

TEST_CASE("the .dip sentences keep their line end, in the files' order", "[negotiation][content]") {
  OA_REQUIRE_GAME_FILES(files);
  const DipSentences player = parse_dip_sentences(files.load("PLAYER.DIP"));
  CHECK(player.actions[kDipDeclareWar] == "We declare war on you.  Prepare to die.\n");
  CHECK(player.actions[kDipNone] == "Goodbye.  I trust we'll speak again.\n");
  CHECK(player.responses[0] == "We look forward to many interesting encounters.\n");
  const DipSentences capelons = parse_dip_sentences(files.load("RACE13.DIP"));
  CHECK(capelons.responses[Negotiation::kNoResponse] == "The %m acknowledge the %y.  Is there something you want?\n");
  for (int s = 0; s < 21; ++s) {
    const DipSentences d = parse_dip_sentences(files.load(std::format("RACE{:02}.DIP", s)));
    CHECK_FALSE(d.responses.back().empty());
  }
}

TEST_CASE("the placeholders name the speaker, the listener, the third party and the star", "[negotiation][content]") {
  OA_REQUIRE_TABLES(tables);
  OA_REQUIRE_GAME_FILES(files);
  Dialogue d(*tables, files);
  d.negotiation.open(1, d.alien, d.player);
  CHECK_FALSE(d.negotiation.replying());
  // The alien speaks: %m the Capelons (13), %y the Chamachies (19).
  CHECK(d.negotiation.alien_text() == "The S13 acknowledge the S19.  Is there something you want?\n");
  // The player speaks: the other way round; %s the selected star.
  CHECK(d.negotiation.format_sentence(kDipDiscover, true, true).starts_with("The S19 extend greetings to the S13."));
  const Cosmos& c = d.session.game->cosmos;
  const std::string star(name_view(c.stars.at(static_cast<std::size_t>(c.selected_star)).name));
  CHECK(d.negotiation.format_sentence(kDipHelpControlSystem, true, true) ==
        std::format("We need to rid the {} system of enemy ships.  Can you help?\n", star));
  // No third party: %t of a player action names nothing.
  CHECK(d.negotiation.format_sentence(kDipOfferPeaceToOtherRace, true, true) ==
        "War is ugly.  Please offer peace to the .\n");
  // The alien's %t is the race of its pending argument.
  d.race(1).pending_action_arg = 2;
  CHECK(d.negotiation.format_sentence(12, false, false).find("the S2 ") != std::string::npos);
}

TEST_CASE("the player's options at peace; Goodbye leaves; a choice is used once", "[negotiation][content]") {
  OA_REQUIRE_TABLES(tables);
  OA_REQUIRE_GAME_FILES(files);
  Dialogue d(*tables, files);
  d.negotiation.open(1, d.alien, d.player);
  CHECK(d.negotiation.third_parties().empty());
  CHECK(d.negotiation.third_race() == -1);
  CHECK(d.codes() == std::vector<int>{14, 2, 3, 4, 5, 6});
  CHECK(d.negotiation.choose(kDipDeclareWar) == Negotiation::Outcome::kApplied);
  CHECK(d.race(0).relation[1] == kWar);
  CHECK(d.race(1).relation[0] == kWar);
  // The alien's answer to a declaration of war (response 3).
  CHECK(d.negotiation.alien_text() == d.negotiation.format_sentence(3, false, false));
  CHECK(d.negotiation.alien_text().starts_with("You shall soon see why the histories of past wars involving the S13"));
  CHECK(d.codes() == std::vector<int>{14, 13});
  CHECK(d.negotiation.choose(kDipNone) == Negotiation::Outcome::kGoodbye);
  // Opened again, the used options show again.
  d.negotiation.open(1, d.alien, d.player);
  CHECK(d.codes() == std::vector<int>{14, 13});
}

TEST_CASE("answering a proposal applies the alien's action and clears it", "[negotiation][content]") {
  OA_REQUIRE_TABLES(tables);
  OA_REQUIRE_GAME_FILES(files);
  Dialogue d(*tables, files);
  d.race(1).pending_action = kDipProposeAlliance;
  d.negotiation.open(1, d.alien, d.player);
  CHECK(d.negotiation.replying());
  CHECK(d.codes() == std::vector<int>{4, 5});
  CHECK(d.negotiation.options()[0].text == d.player.responses[4]);
  CHECK(d.negotiation.alien_text() == d.alien.actions[kDipProposeAlliance]);
  CHECK(d.negotiation.choose(4) == Negotiation::Outcome::kApplied);
  CHECK_FALSE(d.negotiation.replying());
  CHECK(d.race(1).pending_action == kDipNone);
  CHECK(d.race(0).relation[1] == kAlliance);
  CHECK(d.race(1).relation[0] == kAlliance);
  // Allied: the alien says its NoDipAction sentence; the actions follow.
  CHECK(d.negotiation.alien_text() == "Anything else on your mind, S19?  Be honest.\n");
  CHECK(d.codes() == std::vector<int>{14, 8, 4, 5, 6, 12});
}

TEST_CASE("accepting DiscoverAskHelp selects the alien's star and applies nothing", "[negotiation][content]") {
  OA_REQUIRE_TABLES(tables);
  OA_REQUIRE_GAME_FILES(files);
  Dialogue d(*tables, files);
  d.race(1).pending_action = kDipDiscoverAskHelp;
  d.race(1).pending_action_arg = star_address(d.session, 5);
  d.negotiation.open(1, d.alien, d.player);
  CHECK(d.codes() == std::vector<int>{1, 2});
  CHECK(d.negotiation.choose(1) == Negotiation::Outcome::kGoToStar);
  CHECK(d.session.game->cosmos.selected_star == 5);
  CHECK(d.race(1).pending_action == kDipDiscoverAskHelp);
}

TEST_CASE("the third parties: known to both, alive; picking one lists its actions", "[negotiation][content]") {
  OA_REQUIRE_TABLES(tables);
  OA_REQUIRE_GAME_FILES(files);
  Dialogue d(*tables, files);
  // Allied with the Capelons, who are at war with the Minions (2) and at
  // peace with the Arbryls (3), both met by the player.
  for (const auto& [a, b, r] : std::vector<std::tuple<int, int, std::uint8_t>>{
           {0, 1, kAlliance}, {1, 0, kAlliance}, {0, 2, kPeace}, {0, 3, kPeace}, {1, 2, kWar}, {2, 1, kWar}}) {
    d.race(a).relation.at(static_cast<std::size_t>(b)) = r;
  }
  d.negotiation.open(1, d.alien, d.player);
  CHECK(d.negotiation.third_parties() == std::vector<RaceIndex>{2, 3});
  CHECK(d.codes() == std::vector<int>{14, 8, 4, 5, 6, 12, 7});
  CHECK(d.negotiation.options().back().text == "War is ugly.  Please offer peace to the S2.\n");
  CHECK_FALSE(d.negotiation.pick_third_party(0));
  CHECK_FALSE(d.negotiation.pick_third_party(2));
  CHECK(d.negotiation.pick_third_party(1));
  CHECK(d.negotiation.keep_scroll());
  CHECK(d.codes() == std::vector<int>{14, 8, 4, 5, 6, 12, 9, 11});
  // An extinct race is no third party.
  d.race(3).extinct = -1;
  d.negotiation.open(1, d.alien, d.player);
  CHECK(d.negotiation.third_parties() == std::vector<RaceIndex>{2});
}

TEST_CASE("without a third party the actions read the byte before the relations", "[negotiation][content]") {
  OA_REQUIRE_TABLES(tables);
  OA_REQUIRE_GAME_FILES(files);
  Dialogue d(*tables, files);
  // `relation[-1]` is the high byte of `attitude[6]`: the player's nonzero
  // ("known"), the Capelons' 2 ("at war"): OfferPeaceToOtherRace shows.
  d.race(0).attitude[6] = 0x0100;
  d.race(1).attitude[6] = 0x0200;
  const std::vector<std::uint8_t> actions = race_list_actions(d.session, d.race(0), 1, -1);
  CHECK(actions == std::vector<std::uint8_t>{14, 2, 3, 4, 5, 6, 7});
}

}  // namespace
}  // namespace oa::game
