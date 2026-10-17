#include "game/negotiation.h"

#include <format>
#include <optional>
#include <string_view>

#include "core/error.h"
#include "game/raceneg.h"

namespace oa::game {

namespace {

// A reader of `fgets(buf, 200, f)` lines in DOS text mode: CRLF becomes the
// line end `\n`, which stays in the line; ^Z ends the file.
class FgetsReader {
 public:
  explicit FgetsReader(std::span<const std::uint8_t> data) : data_(data) {}

  std::optional<std::string> line() {
    if (pos_ >= data_.size() || data_[pos_] == 0x1a) {
      return std::nullopt;
    }
    std::string out;
    while (pos_ < data_.size() && out.size() < kBuffer - 1) {
      const char c = static_cast<char>(data_[pos_]);
      if (c == 0x1a) {
        break;
      }
      ++pos_;
      if (c == '\r' && pos_ < data_.size() && data_[pos_] == '\n') {
        continue;
      }
      out += c;
      if (c == '\n') {
        break;
      }
    }
    return out;
  }

  // `read_content_line` (0x2f578): the next line not starting with a space,
  // a tab, a line end, `/` or `#`; the original asserts at the end of the
  // file.
  std::string content_line() {
    while (std::optional<std::string> l = line()) {
      const char c = l->empty() ? '\n' : l->front();
      if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '/' && c != '#') {
        return *l;
      }
    }
    throw DataError("dip file: unexpected end of the file");
  }

 private:
  static constexpr std::size_t kBuffer = 200;
  std::span<const std::uint8_t> data_;
  std::size_t pos_ = 0;
};

// Reads `label` / `sentence` pairs into `out` until a label starting with
// `end` (the first label is in `label`).
template <std::size_t N>
void read_sentences(FgetsReader& in, std::string& label, std::string_view end, std::array<std::string, N>& out) {
  std::size_t n = 0;
  while (!label.starts_with(end)) {
    if (n >= N) {
      throw DataError(std::format("dip file: more than {} sentences before {}", N, end));
    }
    std::string sentence = in.content_line();
    if (sentence.size() > DipSentences::kMaxLength) {
      sentence.resize(DipSentences::kMaxLength);
    }
    out[n++] = std::move(sentence);
    label = in.content_line();
  }
}

}  // namespace

DipSentences parse_dip_sentences(std::span<const std::uint8_t> data) {
  FgetsReader in(data);
  std::string label;
  while (!label.starts_with("ACTIONS")) {
    label = in.content_line();
  }
  DipSentences out;
  label = in.content_line();
  read_sentences(in, label, "RESPONSES", out.actions);
  label = in.content_line();
  read_sentences(in, label, "END DIPLO", out.responses);
  return out;
}

Negotiation::Negotiation(Session& session, std::span<const std::string> species_names)
    : session_(session), species_names_(species_names) {}

std::int16_t Negotiation::third_race() const {
  if (static_cast<std::size_t>(selected_) >= third_parties_.size()) {
    return -1;
  }
  return third_parties_[static_cast<std::size_t>(selected_)];
}

const std::string& Negotiation::species_name(std::size_t race) const {
  static const std::string kUnknown;
  const Cosmos& cosmos = session_.game->cosmos;
  if (race >= cosmos.races.size()) {
    return kUnknown;
  }
  const std::size_t species = cosmos.races[race].species;
  return species < species_names_.size() ? species_names_[species] : kUnknown;
}

void Negotiation::list_third_parties() {
  const Cosmos& cosmos = session_.game->cosmos;
  const Race& player = cosmos.races[session_.player_race];
  const Race& target = cosmos.races[target_];
  third_parties_.clear();
  selected_ = 0;
  for (std::int16_t r = 0; r < cosmos.race_count; ++r) {
    const auto u = static_cast<std::size_t>(r);
    if (u != session_.player_race && u != target_ && cosmos.races[u].extinct != -1 && player.relation[u] != kNotMet &&
        target.relation[u] != kNotMet) {
      third_parties_.push_back(static_cast<RaceIndex>(r));
    }
  }
}

void Negotiation::list_actions() {
  actions_ = race_list_actions(session_, session_.game->cosmos.races[session_.player_race], target_, third_race());
}

void Negotiation::show_actions(bool keep_scroll) {
  options_.clear();
  for (const std::uint8_t code : actions_) {
    if (!used_.at(code)) {
      options_.push_back(Option{code, format_sentence(code, true, true)});
    }
  }
  keep_scroll_ = keep_scroll;
}

void Negotiation::open(RaceIndex target, const DipSentences& alien, const DipSentences& player) {
  target_ = target;
  alien_ = &alien;
  player_ = &player;
  list_third_parties();
  used_.fill(false);
  replying_ = false;
  const Race& race = session_.game->cosmos.races.at(target_);
  if (race.pending_action == kDipNone) {
    list_actions();
    show_actions(false);
    alien_text_ = format_sentence(kNoResponse, false, false);
    return;
  }
  replying_ = true;
  options_.clear();
  for (const std::uint8_t code : race_player_reply_options(session_, race, race.pending_action)) {
    options_.push_back(Option{code, format_sentence(code, true, false)});
  }
  keep_scroll_ = false;
  alien_text_ = format_sentence(race.pending_action, false, true);
}

bool Negotiation::pick_third_party(int index) {
  if (index < 0 || static_cast<std::size_t>(index) >= third_parties_.size() || index == selected_) {
    return false;
  }
  selected_ = index;
  if (!replying_) {
    list_actions();
    show_actions(true);
  }
  return true;
}

void Negotiation::star_selected() {
  if (!replying_) {
    show_actions(true);
  }
}

Negotiation::Outcome Negotiation::choose(std::uint8_t code) {
  Cosmos& cosmos = session_.game->cosmos;
  Race& player = cosmos.races[session_.player_race];
  Race& target = cosmos.races[target_];
  if (replying_) {
    // The window proposes after any answer (`NegWnd_handle_event` 0x1c01).
    replying_ = false;
    if (code == 1) {
      // Accepting DiscoverAskHelp (never proposed by the AI of this build):
      // the alien's star becomes the selected one, nothing is applied.
      if (const std::optional<std::int32_t> star =
              record_at(session_, target.pending_action_arg, kStarsOffset, kStarSize, kMaxStars)) {
        cosmos.selected_star = static_cast<StarIndex>(*star);
      }
      return Outcome::kGoToStar;
    }
    race_apply_diplomatic_action(session_, target, session_.player_race, target.pending_action, code,
                                 static_cast<std::int32_t>(target.pending_action_arg));
    target.pending_action = kDipNone;
    list_actions();
    show_actions(false);
    alien_text_ = format_sentence(kDipNone, false, true);
    return Outcome::kApplied;
  }
  if (code == kDipNone) {
    return Outcome::kGoodbye;
  }
  used_.at(code) = true;
  std::int32_t arg = 0;
  switch (code) {
    case kDipOfferPeaceToOtherRace:
    case kDipDeclareWarOnOtherRace:
    case kDipBreakAllianceWithOtherRace:
    case kDipProposeAllianceToOtherRace:
      arg = third_race();
      break;
    case kDipHelpControlSystem:
      arg = cosmos.selected_star;
      break;
    default:
      break;
  }
  const std::uint8_t response = race_respond_to_diplomatic_action(session_, target, session_.player_race, code, arg);
  alien_text_ = format_sentence(response, false, false);
  race_apply_diplomatic_action(session_, player, target_, code, response, arg);
  list_actions();
  show_actions(false);
  return Outcome::kApplied;
}

std::string Negotiation::format_sentence(std::uint8_t code, bool player, bool action) const {
  const DipSentences& d = player ? *player_ : *alien_;
  const std::string& text = action ? d.actions.at(code) : d.responses.at(code);
  const Cosmos& cosmos = session_.game->cosmos;
  const Race& target = cosmos.races[target_];
  const std::size_t speaker = player ? session_.player_race : target_;
  const std::size_t listener = player ? target_ : session_.player_race;
  // A star's name: the empty string for none (the original reads whatever
  // its null or stale pointer gives).
  const auto star_name = [&](std::optional<std::int32_t> star) -> std::string {
    if (!star || *star < 0 || *star >= cosmos.star_count) {
      return {};
    }
    return std::string(name_view(cosmos.stars[static_cast<std::size_t>(*star)].name));
  };
  std::string out;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '%' || i + 1 >= text.size()) {
      out += text[i];
      continue;
    }
    switch (text[i + 1]) {
      case 'm':
        out += species_name(speaker);
        break;
      case 'y':
        out += species_name(listener);
        break;
      case 's':
        out += player ? star_name(cosmos.selected_star)
                      : star_name(record_at(session_, target.pending_action_arg, kStarsOffset, kStarSize, kMaxStars));
        break;
      case 't': {
        const std::int32_t race =
            player && action ? third_race() : static_cast<std::int32_t>(target.pending_action_arg);
        if (race >= 0) {
          out += species_name(static_cast<std::size_t>(race));
        }
        break;
      }
      default:
        // Not a code: the `%` is copied and the next character follows.
        out += '%';
        continue;
    }
    ++i;
  }
  return out;
}

}  // namespace oa::game
