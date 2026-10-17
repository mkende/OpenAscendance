#include "assets/flic_script.h"

#include <format>

#include "assets/tokens.h"
#include "core/watcom_qsort.h"

namespace oa {

FlicScript parse_flic_script(std::span<const std::uint8_t> data) {
  FlicScript s;
  TokenReader r(data, "SUBTITLE.TXT");
  const auto add = [&](std::uint16_t frame, std::uint16_t type, std::uint32_t a1, std::uint32_t a2) {
    if (s.events.size() == FlicScript::kMaxEvents) {
      r.fail(std::format("more than {} events", FlicScript::kMaxEvents));
    }
    s.events.push_back({frame, type, a1, a2});
  };
  for (;;) {
    const int type = r.keyword_integer();
    switch (type) {
      case -1: {
        watcom_qsort(std::span(s.events),
                     [](const FlicEvent& a, const FlicEvent& b) { return int{a.frame} - int{b.frame}; });
        return s;
      }
      case 0:
        s.title_x0 = r.keyword_integer();
        s.title_y0 = r.keyword_integer();
        s.title_x1 = r.keyword_integer();
        s.title_y1 = r.keyword_integer();
        break;
      case 1:
        s.font_file = r.keyword_word();
        s.palette_file = r.keyword_word();
        s.palette_start = r.keyword_integer();
        s.palette_count = r.keyword_integer();
        s.back_color = r.keyword_integer();
        break;
      case 2: {
        const std::string text = replace_with_spaces(r.keyword_word(), '_');
        const int start = r.keyword_integer();
        const int end = r.keyword_integer();
        if (start - FlicScript::kSubtitleLead <= 0) {
          r.fail(std::format("subtitle '{}' starts at frame {}", text, start));
        }
        const auto index = static_cast<std::uint32_t>(s.subtitles.size());
        s.subtitles.push_back(text);
        add(static_cast<std::uint16_t>(start - FlicScript::kSubtitleLead), FlicEvent::kShowSubtitle, index, 0);
        add(static_cast<std::uint16_t>(end), FlicEvent::kHideSubtitle, 0, 0);
        break;
      }
      case 3: {
        const int frame = r.keyword_integer();
        const int rate = r.keyword_integer();
        add(static_cast<std::uint16_t>(frame), FlicEvent::kFrameRate, static_cast<std::uint32_t>(rate), 0);
        break;
      }
      case 11: {
        const int channel = r.keyword_integer();
        const int volume = r.keyword_integer();
        const int frame = r.keyword_integer();
        add(static_cast<std::uint16_t>(frame), FlicEvent::kMusicVolume, static_cast<std::uint32_t>(channel),
            static_cast<std::uint32_t>(volume));
        break;
      }
      case 16: {
        const int tune = r.keyword_integer();
        const int command = r.keyword_integer();
        const int frame = r.keyword_integer();
        add(static_cast<std::uint16_t>(frame), command == 1 ? FlicEvent::kPlayTune : FlicEvent::kStopTune,
            static_cast<std::uint32_t>(tune), 0);
        break;
      }
      default:
        r.fail(std::format("unknown record type {}", type));
    }
  }
}

}  // namespace oa
