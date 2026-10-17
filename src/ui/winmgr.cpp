#include "ui/winmgr.h"

#include <algorithm>
#include <cctype>
#include <format>

#include "assets/gif.h"
#include "core/error.h"
#include "core/log.h"
#include "ui/flicwnd.h"

namespace oa {

namespace {

// BIOS scan codes of the hotkeys (`WinMgr_handle_hotkey` 0x50174).
constexpr std::uint16_t kScanEsc = 0x01;
constexpr std::uint16_t kScanX = 0x2d;
constexpr std::uint16_t kScanN = 0x31;
constexpr std::uint16_t kScanM = 0x32;

// The themes the music picker chooses from (tunes 0..4).
constexpr int kThemes = 5;
// The resumed theme's fade-in ends at this volume.
constexpr int kResumeVolume = 100;

std::string upper(std::string_view s) {
  std::string out(s);
  for (char& c : out) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  return out;
}

}  // namespace

WinMgr::WinMgr(UiServices services) : services_(std::move(services)), shapes_(services_.files) {
  const WindowsTxt txt = parse_windows_txt(services_.files.load("windows.txt"));
  std::vector<HazeTable>& haze = services_.gsystem.haze_tables();
  haze.clear();
  for (const std::string& file : txt.haze_tables) {
    haze.push_back(parse_haze_table(services_.files.load(file), file));
  }
  // The fonts of windows.txt get background 0.
  small_font_ = std::make_unique<GameFont>(parse_font(services_.files.load(txt.small_font), txt.small_font), 0);
  large_font_ = std::make_unique<GameFont>(parse_font(services_.files.load(txt.large_font), txt.large_font), 0);
  for (std::size_t i = 0; i < focus_map_.size(); ++i) {
    focus_map_[i] = static_cast<std::uint8_t>(i);
  }
  focus_map_[GWindow::kBackColor] = GWindow::kHotBackColor;
  for (const auto& [screen, file] : txt.screen_shapes) {
    if (screen < 0 || screen >= kScreens) {
      throw DataError(std::format("windows.txt: screen {} out of range", screen));
    }
    screens_[static_cast<std::size_t>(screen)].shape_file = file;
  }
  load_windows(txt);
}

WinMgr::~WinMgr() = default;

void WinMgr::load_windows(const WindowsTxt& txt) {
  for (const WindowRecord& record : txt.windows) {
    const ShapeTable* table = nullptr;
    if (!record.screens.empty()) {
      const int first = record.screens.front();
      if (first < 0 || first >= kScreens) {
        throw DataError(std::format("windows.txt: {} on screen {}", record.name, first));
      }
      const std::string& file = screens_[static_cast<std::size_t>(first)].shape_file;
      if (!file.empty()) {
        table = &shapes_.get(file);
      }
    }
    // The other classes come with their screens (milestones 2 on): until
    // then their records are plain windows.
    std::unique_ptr<GWindow> w =
        record.type == 1 ? std::make_unique<FlicWnd>(*this, record) : std::make_unique<GWindow>(*this, record, table);
    const int id = static_cast<int>(windows_.size()) + 1;
    w->set_id(id);
    if (upper(record.parent) != "NONE") {
      GWindow* parent = find_window(record.parent);
      if (parent == nullptr) {
        throw DataError(std::format("windows.txt: {} has no parent {}", record.name, record.parent));
      }
      w->set_parent(parent->id());
      parent->add_child(w.get());
    }
    for (const int s : record.screens) {
      std::vector<int>& list = screens_[static_cast<std::size_t>(s)].windows;
      if (list.size() == kScreenWindows) {
        throw DataError(std::format("windows.txt: more than {} windows on screen {}", kScreenWindows, s));
      }
      list.push_back(id);
    }
    windows_.push_back(std::move(w));
  }
}

GWindow* WinMgr::window(int id) const {
  if (id < 1 || static_cast<std::size_t>(id) > windows_.size()) {
    return nullptr;
  }
  return windows_[static_cast<std::size_t>(id - 1)].get();
}

GWindow* WinMgr::find_window(std::string_view name) const {
  const std::string wanted = upper(name.substr(0, 10));
  for (const auto& w : windows_) {
    if (upper(std::string_view(w->name()).substr(0, 10)) == wanted) {
      return w.get();
    }
  }
  return nullptr;
}

void WinMgr::release_hot(int id) {
  if (hot_ == id) {
    hot_ = -1;
  }
}

void WinMgr::set_screen(int screen, bool push) {
  if (screen == current_) {
    return;
  }
  if (screen < 0 || screen >= kScreens) {
    throw std::out_of_range(std::format("screen {}", screen));
  }
  if (hot_ != -1) {
    send_event(hot_, kEventMouseLeave, 0, 0);
  }
  broadcast_event(kEventDeactivate, 0, 0);
  if (push && current_ >= 0) {
    history_.push_back(current_);
    if (history_.size() > kHistory) {
      history_.pop_front();
    }
  }
  current_ = screen;
  if (screen == 0 && first_opening_) {
    // resume.gam is loaded with the game state (milestone 8).
    first_opening_ = false;
  }
  vfx_pane_wipe(services_.gsystem.screen(), GSystem::kScreenPane, 0);
  enter_screen(screen);
  broadcast_event(kEventActivate, 0, 0);
  hot_ = -1;
  update_hover(last_input_);
}

void WinMgr::enter_screen(int screen) {
  if (screen == 0) {
    // show_opening_gif 0x1cc94: the menu picture under game.pal.
    const Image8 gif = decode_gif(services_.files.load("DATA\\0OPENING.GIF"), "DATA\\0OPENING.GIF");
    Image8& out = services_.gsystem.screen();
    for (int y = 0; y < std::min(gif.height(), out.height()); ++y) {
      for (int x = 0; x < std::min(gif.width(), out.width()); ++x) {
        out.set(x, y, gif.at(x, y));
      }
    }
  }
  // The `.tmp` backgrounds of the other screens (screen_draw_background
  // 0x55730) come with those screens.
}

void WinMgr::go_back() {
  if (history_.empty()) {
    return;
  }
  const int screen = history_.back();
  history_.pop_back();
  set_screen(screen, false);
}

void WinMgr::post_message(int id, int p1, int p2) {
  if (messages_.size() < kMaxMessages) {
    messages_.push_back({id, p1, p2});
  }
}

bool WinMgr::send_event(int id, int type, int a, int b) {
  GWindow* w = window(id);
  return w != nullptr && w->handle_event(type, a, b);
}

bool WinMgr::broadcast_event(int type, int a, int b) {
  if (current_ < 0) {
    return false;
  }
  const std::vector<int> ids = screens_[static_cast<std::size_t>(current_)].windows;
  for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
    GWindow* w = window(*it);
    if (!w->shown() && type != kEventActivate && type != kEventShow) {
      continue;
    }
    if (w->parent() != 0 && type >= 3) {
      continue;
    }
    if (w->handle_event(type, a, b)) {
      return true;
    }
  }
  return false;
}

void WinMgr::set_timer(int id, int msg, int p1, int p2, std::uint32_t period) {
  if (timers_.size() < kMaxTimers) {
    timers_.push_back({id, msg, p1, p2, period, services_.ticks() + period});
  }
}

void WinMgr::kill_timer(int id, int msg) {
  std::erase_if(timers_, [&](const Timer& t) { return t.window == id && (msg == -1 || t.msg == msg); });
}

void WinMgr::run_timers() {
  const std::vector<Timer> due = timers_;
  for (const Timer& t : due) {
    const std::uint32_t now = services_.ticks();
    if (static_cast<std::int32_t>(now - t.due) < 0) {
      continue;
    }
    // A timer killed by an earlier one's event does not fire.
    const auto it = std::ranges::find_if(
        timers_, [&](const Timer& x) { return x.window == t.window && x.msg == t.msg && x.due == t.due; });
    if (it == timers_.end()) {
      continue;
    }
    it->due = now + t.period;
    send_event(t.window, t.msg, t.p1, t.p2);
  }
}

void WinMgr::start_pending_fade() {
  fade_pending_ = true;
  fade_step_ = 0;
}

void WinMgr::update_hover(const InputState& input) {
  const int x = input.x;
  const int y = input.y;
  if (hot_ != -1) {
    GWindow* hot = window(hot_);
    if (hot->contains(x, y)) {
      // Inside the hot window: track its child under the pointer.
      for (GWindow* child : hot->children()) {
        if (child->shown() && child->contains(x, y)) {
          send_event(hot_, kEventMouseMove, x, y);
          send_event(hot_, kEventMouseLeave, 0, 0);
          send_event(child->id(), kEventMouseEnter, 0, 0);
          hot_ = child->id();
          return;
        }
      }
      return;
    }
    send_event(hot_, kEventMouseMove, x, y);
    send_event(hot_, kEventMouseLeave, 0, 0);
    hot_ = -1;
  }
  if (current_ < 0) {
    return;
  }
  const std::vector<int>& ids = screens_[static_cast<std::size_t>(current_)].windows;
  for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
    GWindow* w = window(*it);
    if (w->contains(x, y) && w->shown()) {
      hot_ = w->id();
      send_event(hot_, kEventMouseEnter, 0, 0);
      return;
    }
  }
}

void WinMgr::handle_hotkey(const InputState& input) {
  if (input.scan_code == kScanEsc) {
    go_back();
    return;
  }
  if (!input.alt) {
    return;
  }
  switch (input.scan_code) {
    case kScanX:
      running_ = false;
      break;
    case kScanN:
      services_.sound.set_sfx_muted(!services_.sound.sfx_muted());
      break;
    case kScanM:
      services_.sound.set_music_muted(!services_.sound.music_muted());
      break;
    default:
      // Alt+S/L (save/load screens), Alt+G (screenshot) and the developer
      // keys come with their screens.
      break;
  }
}

void WinMgr::dispatch_messages() {
  const std::vector<Message> queue = std::move(messages_);
  messages_.clear();
  for (const Message& m : queue) {
    switch (m.id) {
      case kMessageNone:
        break;
      case kMessageChangeScreen:
        set_screen(m.p1, m.p2 != 0);
        break;
      case kMessageQuit:
        running_ = false;
        break;
      case kMessageBack:
        go_back();
        break;
      default:
        log_debug("system message {} ({}, {}) not handled yet", m.id, m.p1, m.p2);
        break;
    }
  }
}

void WinMgr::pick_music(const InputState& input) {
  SoundSystem& sound = services_.sound;
  if (!sound.music_ready() || sound.music_muted()) {
    return;
  }
  if (!sound.tune_playing() && current_ != 6 && current_ != 7 && current_ != 13) {
    int tune = -1;
    bool resume = false;
    if (previous_tune_ != -1 && sound.saved_position(previous_tune_) != 0) {
      resume = true;
      tune = previous_tune_;
      volume_ramp_ = 0;
    }
    if (tune == -1) {
      tune = previous_tune_ == -1
                 ? 0
                 : static_cast<int>((services_.ticks() + static_cast<std::uint32_t>(input.x)) % kThemes);
      if (tune == previous_tune_) {
        tune = (previous_tune_ + 1) % kThemes;
      }
    }
    sound.play_tune(tune, resume);
    previous_tune_ = tune;
  }
  if (volume_ramp_ != -1) {
    sound.set_music_volume(volume_ramp_);
    if (++volume_ramp_ == kResumeVolume) {
      volume_ramp_ = -1;
    }
  }
}

void WinMgr::process_input(const InputState& input) {
  last_input_ = input;
  if (input.moved) {
    update_hover(input);
  }
  run_timers();
  if (input.key_event) {
    if (input.alt || input.scan_code == kScanEsc) {
      handle_hotkey(input);
    } else {
      broadcast_event(kEventKey, input.ascii, input.scan_code);
    }
  }
  if (input.left_changed && input.left_down) {
    broadcast_event(kEventLeftClick, input.x, input.y);
  }
  if (input.right_changed && input.right_down) {
    broadcast_event(kEventRightClick, input.x, input.y);
  }
  dispatch_messages();
  pick_music(input);
  SoundSystem& sound = services_.sound;
  do {
    if (sound.music_ready() && sound.tune_playing()) {
      if (sound.music_muted()) {
        sound.stop_tune();
      } else {
        sound.stream_music();
      }
    }
    if (services_.ticks() - last_pass_tick_ < kPassTicks) {
      services_.idle();
    }
  } while (services_.ticks() - last_pass_tick_ < kPassTicks);
  last_pass_tick_ = services_.ticks();
}

void WinMgr::refresh_screen() {
  if (fade_pending_) {
    services_.gsystem.fade_to_palette_step(fade_step_);
    if (++fade_step_ > 64) {
      fade_step_ = 0;
      fade_pending_ = false;
    }
  }
}

}  // namespace oa
