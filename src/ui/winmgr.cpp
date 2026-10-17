#include "ui/winmgr.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstring>
#include <format>
#include <fstream>

#include "assets/gif.h"
#include "assets/static_strings.h"
#include "assets/tokens.h"
#include "core/c_format.h"
#include "core/error.h"
#include "core/log.h"
#include "game/session.h"
#include "game/tables.h"
#include "ui/button_group.h"
#include "ui/edit_field.h"
#include "ui/help_wnd.h"
#include "ui/text_box.h"
#include "ui/toggle_button.h"

namespace oa {

namespace {

// BIOS scan codes of the hotkeys (`WinMgr_handle_hotkey` 0x50174).
constexpr std::uint16_t kScanEsc = 0x01;
constexpr std::uint16_t kScanS = 0x1f;
constexpr std::uint16_t kScanG = 0x22;
constexpr std::uint16_t kScanL = 0x26;
constexpr std::uint16_t kScanX = 0x2d;
constexpr std::uint16_t kScanN = 0x31;
constexpr std::uint16_t kScanM = 0x32;

// The screens of the hotkeys and the screen-specific rules.
constexpr int kScreenOpening = 0;
constexpr int kScreenStarMap = 1;
constexpr int kScreenSave = 2;
constexpr int kScreenLoad = 4;
constexpr int kScreenShipDesign = 8;
constexpr int kScreenPlanet = 17;

// The themes the music picker chooses from (tunes 0..4).
constexpr int kThemes = 5;
// The resumed theme's fade-in ends at this volume.
constexpr int kResumeVolume = 100;

// The window classes with a part in the saves' window state (`windows.txt`
// TYPE) and their sizes.
constexpr int kTypeStarMap = 2;
constexpr int kTypeButtonGroup = 3;
constexpr int kTypeStarSystem = 4;
constexpr int kTypePlanet = 7;
constexpr int kTypeResearch = 9;
constexpr int kTypeToggle = 25;
constexpr std::size_t kStarMapStateSize = 0xbe;
constexpr std::size_t kToggleStateSize = 4;
constexpr std::size_t kStarSystemStateSize = 0x1a08;
constexpr std::size_t kPlanetStateSize = 1;
constexpr std::size_t kCameraStateSize = 0x98;

// The star system window's state (`BatModeWnd_serialize` 0x19e2c): the
// 100 per-star "camera set" flags at +0x12c0 (cleared by Begin New Game).
constexpr std::size_t kStarSystemFlagsOffset = 0x12c0;
constexpr std::size_t kStarSystemFlagsSize = 100 * 4;
// A camera's orientation basis (`Camera` +0x74), reset by Begin New Game in
// the research window's camera.
constexpr std::size_t kCameraOrientationOffset = 0x74;

// Appends the little-endian bytes of `v`.
void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  for (int shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<std::uint8_t>(v >> shift));
  }
}
void put_f32(std::vector<std::uint8_t>& out, float v) { put_u32(out, std::bit_cast<std::uint32_t>(v)); }

// The bytes of a camera (`Camera_ctor` 0x1b4f0: identity matrices, the rest
// zero), with `fov`, `focal`, `near`, `far`, `position` and `scale` (all
// zero for a camera only constructed).
std::vector<std::uint8_t> camera_bytes(float fov, float focal, float near_clip, float far_clip,
                                       std::array<float, 3> position, float scale) {
  constexpr std::array<float, 9> kIdentity3 = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  constexpr std::array<float, 12> kIdentity34 = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
  std::vector<std::uint8_t> out;
  for (const float v : kIdentity3) {
    put_f32(out, v);
  }
  for (const float v : {fov, focal, near_clip, far_clip}) {
    put_f32(out, v);
  }
  for (const float v : position) {
    put_f32(out, v);
  }
  for (const float v : kIdentity34) {
    put_f32(out, v);
  }
  put_f32(out, scale);
  for (const float v : kIdentity3) {
    put_f32(out, v);
  }
  return out;
}

// The state of the windows that leave it to the window manager, as built:
// the star system view's (`BatModeWnd` ctor and init: 100 object slots with
// identity rotations, a constructed camera, the view basis, the screen
// centre (229, 232) and the five toggles -1, 0, -1, -1, -1), the planet
// window's display flags (5) and the research camera (`ResWnd_init`
// 0x4250c: fov 30, focal 320 / tan 15 degrees, near 1, far 100000, at
// (0, 0, -150), the view not built yet).
std::vector<std::uint8_t> default_state(int type) {
  std::vector<std::uint8_t> out;
  switch (type) {
    case kTypeStarSystem: {
      out.assign(0x4b0, 0);
      for (int slot = 0; slot < 100; ++slot) {
        for (int k = 0; k < 9; ++k) {
          put_f32(out, k % 4 == 0 ? 1.0F : 0.0F);
        }
      }
      out.resize(0x190c, 0);
      const std::vector<std::uint8_t> camera = camera_bytes(0, 0, 0, 0, {0, 0, 0}, 0);
      out.insert(out.end(), camera.begin(), camera.end());
      for (const float v :
           {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F}) {
        put_f32(out, v);
      }
      put_u32(out, 229U | (232U << 16));
      put_u32(out, 0);
      put_f32(out, 0);
      for (const std::uint32_t v : {0xffffffffU, 0U, 0xffffffffU, 0xffffffffU, 0xffffffffU}) {
        put_u32(out, v);
      }
      break;
    }
    case kTypePlanet:
      out.push_back(5);
      break;
    case kTypeResearch: {
      const double focal = 320.0 / std::tan(30.0 * static_cast<double>(0.017453292F) * 0.5);
      out = camera_bytes(30, static_cast<float>(focal), 1, 100000, {0, 0, -150}, 1);
      break;
    }
    default:
      break;
  }
  return out;
}

// The click sound of the exit corners (`soundfx.txt` 0, `button`).
constexpr int kExitSound = 0;

// `ascend.cfg`: the SFX and music mute flags (i32, 0 / -1) and the rotation
// speed (float).
constexpr std::string_view kConfigFile = "ascend.cfg";
constexpr std::size_t kConfigSize = 12;

// The number of `gwshare.txt` names (`GWindow_load_shared_shapes`).
constexpr int kSharedShapes = 45;

// The static strings of the dialogs.
constexpr int kOkString = 0x1e;  // "OK"

// The message box (`WinMgr_message_box` 0x50adc): the box and its button.
constexpr Pane kMessageBoxPane{50, 50, 300, 300};
constexpr Pane kMessageButtonPane{260, 260, 290, 290};
// The input box (`WinMgr_input_box` 0x50be4): its x range, and the frame of
// `help.shp` drawn under it.
constexpr int kInputBoxX0 = 0xf2;
constexpr int kInputBoxX1 = 0x18d;
constexpr int kInputBoxFrame = 1;
constexpr int kHelpShapes = 37;
// `gwshare.txt`: the race slots' small ships and `planitem.shp`.
constexpr int kShipShapes = 14;
constexpr int kPlanItemShapes = 30;

std::string upper(std::string_view s) {
  std::string out(s);
  for (char& c : out) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  return out;
}

// Copies the pixels of `pane` of `screen` (`save_screen_under` 0x4f618).
Image8 save_under(const Image8& screen, const Pane& pane) {
  Image8 out(pane.x1 - pane.x0 + 1, pane.y1 - pane.y0 + 1);
  for (int y = pane.y0; y <= pane.y1; ++y) {
    for (int x = pane.x0; x <= pane.x1; ++x) {
      if (screen.contains(x, y)) {
        out.set(x - pane.x0, y - pane.y0, screen.at(x, y));
      }
    }
  }
  return out;
}

// `restore_screen_under` (0x4f6c0).
void restore_under(Image8& screen, const Pane& pane, const Image8& saved) {
  for (int y = pane.y0; y <= pane.y1; ++y) {
    for (int x = pane.x0; x <= pane.x1; ++x) {
      if (screen.contains(x, y)) {
        screen.set(x, y, saved.at(x - pane.x0, y - pane.y0));
      }
    }
  }
}

}  // namespace

WinMgr::WinMgr(UiServices services, const WindowFactory& factory)
    : services_(std::move(services)), shapes_(services_.files) {
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
    race_map_[i] = static_cast<std::uint8_t>(i);
  }
  focus_map_[GWindow::kBackColor] = GWindow::kHotBackColor;
  {
    const std::vector<std::uint8_t> data = services_.files.load("gwshare.txt");
    TokenReader r(data, "gwshare.txt");
    for (int i = 0; i < kSharedShapes; ++i) {
      shared_shape_files_.push_back(r.word());
    }
  }
  for (const auto& [screen, file] : txt.screen_shapes) {
    if (screen < 0 || screen >= kScreens) {
      throw DataError(std::format("windows.txt: screen {} out of range", screen));
    }
    screens_[static_cast<std::size_t>(screen)].shape_file = file;
  }
  load_windows(txt, factory);
  install_screen_handlers();
  load_config();
  services_.game.attach_windows(this);
}

WinMgr::~WinMgr() { services_.game.attach_windows(nullptr); }

std::size_t WinMgr::state_size(const GWindow& w) {
  switch (w.type()) {
    case kTypeStarMap:
      return kStarMapStateSize;
    case kTypeButtonGroup:
      if (const auto* group = dynamic_cast<const ButtonGroup*>(&w)) {
        return group->controls().size() * ButtonGroup::kControlRecordSize;
      }
      return 0;
    case kTypeStarSystem:
      return kStarSystemStateSize;
    case kTypePlanet:
      return kPlanetStateSize;
    case kTypeResearch:
      return kCameraStateSize;
    case kTypeToggle:
      return kToggleStateSize;
    default:
      return 0;
  }
}

std::vector<std::uint8_t> WinMgr::initial_window_state() const {
  std::vector<std::uint8_t> state;
  for (const auto& w : windows_) {
    const std::size_t size = w ? state_size(*w) : 0;
    if (size == 0) {
      continue;
    }
    std::vector<std::uint8_t> part(size, 0);
    if (!w->save_state(part)) {
      part = default_state(w->type());
      part.resize(size, 0);
    }
    state.insert(state.end(), part.begin(), part.end());
  }
  return state;
}

void WinMgr::store_window_state() {
  game::Session* core = services_.game.core();
  if (core == nullptr) {
    return;
  }
  std::vector<std::uint8_t> initial = initial_window_state();
  if (core->window_state.size() != initial.size()) {
    core->window_state = std::move(initial);
    return;
  }
  std::size_t at = 0;
  for (const auto& w : windows_) {
    const std::size_t size = w ? state_size(*w) : 0;
    if (size != 0) {
      w->save_state(std::span(core->window_state).subspan(at, size));
      at += size;
    }
  }
}

void WinMgr::restore_window_state() {
  setup_race_shapes();
  broadcast_event(kEventRefresh, 0, 0);
  game::Session* core = services_.game.core();
  if (core == nullptr) {
    return;
  }
  std::size_t total = 0;
  for (const auto& w : windows_) {
    total += w ? state_size(*w) : 0;
  }
  if (core->window_state.size() != total) {
    log_warning("window state of {} bytes, the windows have {}", core->window_state.size(), total);
    return;
  }
  std::size_t at = 0;
  for (const auto& w : windows_) {
    const std::size_t size = w ? state_size(*w) : 0;
    if (size != 0) {
      w->load_state(std::span<const std::uint8_t>(core->window_state).subspan(at, size));
      at += size;
    }
  }
}

void WinMgr::new_game_windows() {
  game::Session* core = services_.game.core();
  if (core != nullptr) {
    store_window_state();
  }
  std::size_t at = 0;
  for (const auto& w : windows_) {
    if (!w) {
      continue;
    }
    w->new_game();
    const std::size_t size = state_size(*w);
    if (size == 0) {
      continue;
    }
    std::vector<std::uint8_t> scratch(size);
    if (core != nullptr && !w->save_state(scratch)) {
      // The parts kept here: the star system's flags, the research camera's
      // orientation.
      const std::span<std::uint8_t> part = std::span(core->window_state).subspan(at, size);
      if (w->type() == kTypeStarSystem) {
        std::fill_n(part.begin() + kStarSystemFlagsOffset, kStarSystemFlagsSize, 0);
      } else if (w->type() == kTypeResearch) {
        const std::vector<std::uint8_t> identity = camera_bytes(0, 0, 0, 0, {0, 0, 0}, 0);
        std::copy(identity.begin() + kCameraOrientationOffset, identity.end(), part.begin() + kCameraOrientationOffset);
      }
    }
    at += size;
  }
  setup_race_shapes();
}

void WinMgr::setup_race_shapes() {
  const GameSession& game = services_.game;
  for (int race = 0; race < game.race_count() && race < 7; ++race) {
    const int species = game.race_species(race);
    const auto slot = static_cast<std::size_t>(race);
    shared_shape_files_.at(slot) = std::format("DATA\\SMRACE{:02d}.SHP", species);
    shared_shape_files_.at(slot + 7) = std::format("DATA\\LGRACE{:02d}.SHP", species);
    shared_shape_files_.at(slot + 14) = std::format("DATA\\SMSHIP{:02d}.SHP", species);
    shared_shape_files_.at(slot + 21) = std::format("DATA\\DKSHIP{:02d}.SHP", species);
  }
}

void WinMgr::reset_history_to_opening() {
  history_[0] = kScreenOpening;
  history_count_ = 1;
  history_top_ = 1;
}

void WinMgr::load_windows(const WindowsTxt& txt, const WindowFactory& factory) {
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
    GWindow* parent = nullptr;
    if (upper(record.parent) != "NONE") {
      parent = find_window(record.parent);
      if (parent == nullptr) {
        throw DataError(std::format("windows.txt: {} has no parent {}", record.name, record.parent));
      }
    }
    std::unique_ptr<GWindow> w = factory.create(*this, record, table);
    GWindow* raw = w.get();
    const int id = register_window(std::move(w));
    if (parent != nullptr) {
      raw->set_parent(parent->id());
      parent->add_child(raw);
    }
    for (const int s : record.screens) {
      if (s < 0 || s >= kScreens || !add_to_screen(id, s)) {
        throw DataError(std::format("windows.txt: {} cannot go on screen {}", record.name, s));
      }
    }
  }
}

void WinMgr::install_screen_handlers() {
  // main 0x1c424: screen 0 shows 0opening.gif, the others their .tmp
  // background.
  set_enter_handler(kScreenOpening, [this](int /*screen*/) {
    const Image8 gif = decode_gif(services_.files.load("DATA\\0OPENING.GIF"), "DATA\\0OPENING.GIF");
    Image8& out = screen();
    for (int y = 0; y < std::min(gif.height(), out.height()); ++y) {
      for (int x = 0; x < std::min(gif.width(), out.width()); ++x) {
        out.set(x, y, gif.at(x, y));
      }
    }
  });
  for (const int s : {1, 2, 3, 4, 7, 8, 9, 10, 12, 13, 14, 15, 16, 18, 20}) {
    set_enter_handler(s, [this](int screen) { draw_background(screen); });
  }
  // starmap_screen_leave 0x21b88: NEXTTURNCONT off (not redrawn).
  set_leave_handler(kScreenStarMap, [this](int /*screen*/) {
    if (auto* auto_turn = dynamic_cast<ToggleButton*>(find_window("NEXTTURNCONT"))) {
      auto_turn->set_state(false);
    }
  });
}

void WinMgr::draw_background(int screen) {
  std::string_view file;
  switch (screen) {
    case 1:
      file = "DATA\\1starmap.tmp";
      break;
    case 2:
    case 4:
    case 10:
    case 14:
    case 15:
    case 18:
      file = "DATA\\10status.tmp";
      break;
    case 3:
      file = "DATA\\3cfgnew.tmp";
      break;
    case 7:
      file = "DATA\\7racesel.tmp";
      break;
    case 8:
      file = "DATA\\8shipdes.tmp";
      break;
    case 9:
      file = "DATA\\9ENDGAME.tmp";
      break;
    case 12:
      file = "DATA\\12start.tmp";
      break;
    case 13:
      file = "DATA\\13negot.tmp";
      break;
    case 16:
      file = "DATA\\16battle.tmp";
      break;
    case 20:
      file = "DATA\\20res.tmp";
      break;
    default:
      return;
  }
  vfx_shape_draw(this->screen(), GSystem::kScreenPane, shapes_.get(file).at(0), 0, 0);
}

void WinMgr::load_config() {
  std::optional<std::vector<std::uint8_t>> data;
  if (!services_.user_dir.empty()) {
    std::ifstream in(services_.user_dir / kConfigFile, std::ios::binary);
    if (in) {
      data.emplace(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
  }
  if (!data) {
    // The install's own, the original's current directory.
    data = services_.files.try_load(kConfigFile);
  }
  if (!data || data->size() < kConfigSize) {
    return;
  }
  const auto i32 = [&](std::size_t at) {
    std::uint32_t v = 0;
    for (std::size_t k = 0; k < 4; ++k) {
      v |= static_cast<std::uint32_t>((*data)[at + k]) << (8 * k);
    }
    return v;
  };
  services_.sound.set_sfx_muted(i32(0) != 0);
  services_.sound.set_music_muted(i32(4) != 0);
  rotation_speed_ = std::bit_cast<float>(i32(8));
}

void WinMgr::save_config() const {
  if (services_.user_dir.empty()) {
    return;
  }
  std::vector<std::uint8_t> data;
  const auto put = [&](std::uint32_t v) {
    for (int k = 0; k < 4; ++k) {
      data.push_back(static_cast<std::uint8_t>(v >> (8 * k)));
    }
  };
  put(services_.sound.sfx_muted() ? 0xffffffffU : 0U);
  put(services_.sound.music_muted() ? 0xffffffffU : 0U);
  put(std::bit_cast<std::uint32_t>(rotation_speed_));
  std::ofstream out(services_.user_dir / kConfigFile, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  if (!out) {
    log_warning("cannot write {}", (services_.user_dir / kConfigFile).string());
  }
}

void WinMgr::draw_colored(const Pane& pane, const Shape& shape, int x, int y, int color) {
  for (int k = 0; k < 4; ++k) {
    race_map_[static_cast<std::size_t>(0x13 - k)] = static_cast<std::uint8_t>(color * 4 + 0x13 - k);
  }
  vfx_shape_translate_draw(screen(), pane, shape, x, y, race_map_);
}

const std::string& WinMgr::static_string(int index) {
  if (static_strings_.empty()) {
    static_strings_ = parse_static_strings(services_.files.load("static.txt"));
  }
  return static_strings_.at(static_cast<std::size_t>(index));
}

const ShapeTable& WinMgr::shared_shapes(int index) {
  return shapes_.get(shared_shape_files_.at(static_cast<std::size_t>(index)));
}

WinMgr::Screen& WinMgr::screen_record(int screen) {
  if (screen < 0 || screen >= kScreens) {
    throw std::out_of_range(std::format("screen {}", screen));
  }
  return screens_[static_cast<std::size_t>(screen)];
}

std::vector<int> WinMgr::history() const {
  std::vector<int> out;
  for (int k = history_count_; k > 0; --k) {
    out.push_back(history_[static_cast<std::size_t>((history_top_ - k + kHistory) % kHistory)]);
  }
  return out;
}

GWindow* WinMgr::window(int id) const {
  if (id < 1 || static_cast<std::size_t>(id) > windows_.size()) {
    return nullptr;
  }
  return windows_[static_cast<std::size_t>(id - 1)].get();
}

GWindow* WinMgr::find_window(std::string_view name) const {
  const std::string wanted = upper(name.substr(0, std::min<std::size_t>(name.size(), 10)));
  for (const auto& w : windows_) {
    if (w && upper(std::string_view(w->name()).substr(0, std::min<std::size_t>(w->name().size(), 10))) == wanted) {
      return w.get();
    }
  }
  return nullptr;
}

int WinMgr::register_window(std::unique_ptr<GWindow> window) {
  windows_.push_back(std::move(window));
  const int id = static_cast<int>(windows_.size());
  windows_.back()->set_id(id);
  return id;
}

void WinMgr::release_window(int id) {
  if (id < 1 || static_cast<std::size_t>(id) > windows_.size()) {
    return;
  }
  for (Screen& s : screens_) {
    for (int& slot : s.slots) {
      if (slot == id) {
        slot = 0;
        --s.count;
      }
    }
  }
  release_hot(id);
  kill_timer(id, -1);
  cancel_notify(id, -1);
  windows_[static_cast<std::size_t>(id - 1)].reset();
}

bool WinMgr::add_to_screen(int id, int screen) {
  Screen& s = screen_record(screen);
  for (int& slot : s.slots) {
    if (slot == 0) {
      slot = id;
      ++s.count;
      return true;
    }
  }
  return false;
}

bool WinMgr::remove_from_screen(int id, int screen) {
  Screen& s = screen_record(screen);
  for (int& slot : s.slots) {
    if (slot == id) {
      slot = 0;
      --s.count;
      return true;
    }
  }
  return false;
}

void WinMgr::release_hot(int id) {
  if (hot_ == id) {
    hot_ = -1;
  }
}

void WinMgr::set_enter_handler(int screen, std::function<void(int)> handler) {
  screen_record(screen).enter = std::move(handler);
}

void WinMgr::set_leave_handler(int screen, std::function<void(int)> handler) {
  screen_record(screen).leave = std::move(handler);
}

void WinMgr::set_screen(int screen, bool push) {
  if (screen == current_) {
    return;
  }
  Screen& next = screen_record(screen);
  if (current_ >= 0 && screen_record(current_).leave) {
    screen_record(current_).leave(current_);
  }
  if (hot_ != -1) {
    send_event(hot_, kEventMouseLeave, 0, 0);
  }
  broadcast_event(kEventDeactivate, 0, 0);
  if (push && current_ >= 0) {
    history_[static_cast<std::size_t>(history_top_)] = current_;
    if (++history_top_ == kHistory) {
      history_top_ = 0;
    }
    history_count_ = std::min(history_count_ + 1, kHistory);
  }
  current_ = screen;
  services_.game.set_current_screen(screen);
  if (screen == kScreenOpening && first_opening_ && services_.game.resume_exists()) {
    services_.game.load_resume();
  }
  first_opening_ = false;
  vfx_pane_wipe(services_.gsystem.screen(), GSystem::kScreenPane, 0);
  if (next.enter) {
    next.enter(screen);
  }
  broadcast_event(kEventActivate, 0, 0);
  hot_ = -1;
  update_hover(last_input_);
}

void WinMgr::go_back() {
  if (history_count_ == 0) {
    return;
  }
  history_top_ = history_top_ == 0 ? kHistory - 1 : history_top_ - 1;
  const int screen = history_[static_cast<std::size_t>(history_top_)];
  --history_count_;
  set_screen(screen, false);
}

void WinMgr::post_message(int id, int p1, int p2, std::string text) {
  if (messages_.size() < kMaxMessages) {
    messages_.push_back({id, p1, p2, std::move(text)});
  }
}

bool WinMgr::send_event(int id, int type, int a, int b) {
  GWindow* w = window(id);
  return w != nullptr && w->handle_event(type, a, b);
}

bool WinMgr::broadcast_event(int type, int a, int b) { return broadcast(type, a, b, false); }

bool WinMgr::broadcast(int type, int a, int b, bool filter) {
  if (current_ < 0) {
    return false;
  }
  const std::array<int, kScreenWindows> slots = screen_record(current_).slots;
  for (int k = kScreenWindows - 1; k >= 0; --k) {
    const std::size_t slot = static_cast<std::size_t>(k);
    GWindow* w = window(slots[slot]);
    if (w == nullptr || (filter && !modal_flags_[slot])) {
      continue;
    }
    // Hidden windows only get 1 and 0xe; children get all but 1 and 2 (their
    // parent forwards those).
    const bool takes =
        !((!w->shown() && type != kEventActivate && type != kEventShow) || (w->parent() != 0 && type < kEventKey));
    if (filter) {
      modal_flags_[slot] = takes;
    }
    if (takes && w->handle_event(type, a, b)) {
      return true;
    }
  }
  return false;
}

void WinMgr::set_timer(int id, int msg, int p1, int p2, std::uint32_t period) {
  for (Timer& t : timers_) {
    if (t.window == 0) {
      t = Timer{id, msg, p1, p2, period, services_.ticks() + period};
      return;
    }
  }
  log_warning("window {}: no free timer", id);
}

void WinMgr::kill_timer(int id, int msg) {
  for (Timer& t : timers_) {
    if (t.window == id && (msg == -1 || t.msg == msg)) {
      t.window = 0;
    }
  }
}

int WinMgr::timer_period(int id, int msg) const {
  for (const Timer& t : timers_) {
    if (t.window == id && t.msg == msg) {
      return static_cast<int>(t.period);
    }
  }
  return -1;
}

void WinMgr::set_timer_period(int id, int msg, std::uint32_t period) {
  for (Timer& t : timers_) {
    if (t.window == id && t.msg == msg) {
      t.period = period;
    }
  }
}

void WinMgr::run_timers() {
  for (Timer& t : timers_) {
    if (t.window == 0 || t.due > services_.ticks()) {
      continue;
    }
    // The slot is read again after the event: a timer killed by its own
    // event keeps its due time unused.
    send_event(t.window, t.msg, t.p1, t.p2);
    t.due += t.period;
  }
}

void WinMgr::request_notify(int id, int mask, int msg, int p1, int p2) {
  for (Notify& n : notifies_) {
    if (n.window == 0) {
      n = Notify{id, mask, msg, p1, p2};
      return;
    }
  }
  log_warning("window {}: no free notification", id);
}

void WinMgr::cancel_notify(int id, int msg) {
  for (Notify& n : notifies_) {
    if (n.window == id && (msg == -1 || n.msg == msg)) {
      n.window = 0;
    }
  }
}

void WinMgr::run_notifies(const InputState& input) {
  const int buttons = (input.left_changed ? kNotifyLeft : 0) | (input.right_changed ? kNotifyRight : 0);
  const int move = input.moved ? kNotifyMove : 0;
  for (const Notify& n : notifies_) {
    if (n.window == 0) {
      continue;
    }
    const Notify copy = n;
    if ((copy.mask & buttons) != 0) {
      send_event(copy.window, copy.msg, copy.p1, copy.p2);
    }
    if ((copy.mask & move) != 0) {
      send_event(copy.window, copy.msg, input.x, input.y);
    }
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
    if (hot != nullptr && hot->contains(x, y)) {
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
  const std::array<int, kScreenWindows>& slots = screen_record(current_).slots;
  for (int k = kScreenWindows - 1; k >= 0; --k) {
    GWindow* w = window(slots[static_cast<std::size_t>(k)]);
    if (w != nullptr && w->contains(x, y) && w->shown()) {
      hot_ = w->id();
      send_event(hot_, kEventMouseEnter, 0, 0);
      return;
    }
  }
}

void WinMgr::set_pointer(int frame) {
  // WinMgr_set_pointer 0x55680: help mode keeps its pointer.
  if (!help_mode_) {
    services_.set_pointer_frame(frame);
  }
}

bool WinMgr::handle_hotkey(const InputState& input) {
  switch (input.scan_code) {
    case kScanEsc:
      if (modal_) {
        end_modal(0);
        return true;
      }
      if (current_ == kScreenPlanet) {
        return false;  // the planet window takes it
      }
      go_back();
      return true;
    case kScanS:
    case kScanL:
      if (!modal_ && current_ != kScreenShipDesign) {
        set_screen(input.scan_code == kScanS ? kScreenSave : kScreenLoad, true);
      }
      return true;
    case kScanG:
      save_screenshot();
      return true;
    case kScanX:
      if (!modal_) {
        running_ = false;
      }
      return true;
    case kScanN:
      services_.sound.set_sfx_muted(!services_.sound.sfx_muted());
      return true;
    case kScanM:
      services_.sound.set_music_muted(!services_.sound.music_muted());
      return true;
    default:
      // The developer keys (W, E, R, P, D, F) need `nougat.lf`.
      return false;
  }
}

void WinMgr::save_screenshot() {
  if (services_.user_dir.empty()) {
    return;
  }
  const std::filesystem::path path = services_.user_dir / std::format("scr{:03d}.gif", screenshot_count_);
  const std::vector<std::uint8_t> gif =
      encode_gif(services_.shown_frame ? services_.shown_frame() : screen(), services_.gsystem.dac());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(gif.data()), static_cast<std::streamsize>(gif.size()));
  if (!out) {
    log_warning("cannot write {}", path.string());
    return;
  }
  ++screenshot_count_;
  log_info("screenshot saved to {}", path.string());
}

void WinMgr::show_help(const Message& m) {
  auto* help = dynamic_cast<HelpWnd*>(find_window("HELPWINDOW"));
  if (help == nullptr) {
    throw DataError("windows.txt: no HELPWINDOW");
  }
  switch (m.p2) {
    case kHelpTopic:
      // Without a topic the window runs as it was prepared (the planet
      // screen's colony and invasion reports).
      if (!m.text.empty()) {
        help->open_topic("help.txt", m.text);
      }
      break;
    case kHelpNumbered:
      help->open_topic("help.txt", std::format("help{}", m.p1));
      break;
    case kHelpNoIntro:
      help->open_topic("help.txt", "NOINTRO");
      break;
    case kHelpPlanet: {
      // A planet item (the planet screen's build list): its name in the
      // text, its picture (a Ship: the player's small ship).
      const game::Session* core = services_.game.core();
      if (core == nullptr) {
        return;
      }
      help->open_topic("planhelp.txt", std::format("plitem{:02d}", m.p1));
      HelpTopic& topic = help->content();
      if (topic.element_count < 2 || topic.elements[0].kind != HelpElement::Kind::kText ||
          topic.elements[1].kind != HelpElement::Kind::kShape) {
        throw DataError(std::format("planhelp.txt: plitem{:02d} is not a text and a picture", m.p1));
      }
      const std::string& name = core->tables->planitems.items.at(static_cast<std::size_t>(m.p1)).name;
      topic.elements[0].text = c_format(topic.elements[0].text, name.c_str());
      if (m.p1 == game::kItemShip) {
        topic.elements[1].shape_index = kShipShapes + services_.game.player_race();
        topic.elements[1].frame = 0;
      } else {
        topic.elements[1].shape_index = kPlanItemShapes;
        topic.elements[1].frame = m.p1;
      }
      break;
    }
    default:
      // The special abilities and gizmo help come with their screens.
      log_debug("help kind {} not handled yet", m.p2);
      return;
  }
  if (!modal_) {
    do_modal(*help);
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
      case kMessageHelp:
        show_help(m);
        break;
      case kMessageEndModal:
        end_modal(m.p1);
        break;
      case kMessageNextTurn:
        next_turn();
        break;
      case kMessageAutoTurn:
        if (services_.game.next_contested_star() < 0) {
          auto_turn_ = !auto_turn_;
        } else {
          log_debug("contested star: the game event 19 comes with the game events");
        }
        break;
      default:
        // 4 (trace replay) comes with the tutorials.
        log_debug("system message {} ({}, {}) not handled yet", m.id, m.p1, m.p2);
        break;
    }
  }
}

void WinMgr::next_turn() {
  GameSession& game = services_.game;
  if (game.next_contested_star() >= 0) {
    // Game event 19 (a battle to fight or skip) comes with the game events.
    log_debug("contested star: the game event 19 comes with the game events");
    return;
  }
  game.end_day();
  setup_race_shapes();
  broadcast_event(kEventRefresh, 0, 0);
  refresh_screen();
}

void WinMgr::end_modal(int result) {
  modal_ = false;
  modal_result_ = result;
}

int WinMgr::do_modal(GWindow& dialog) {
  if (modal_) {
    throw std::logic_error("nested modal loop");
  }
  // The auto next turn stops (and stays off: only the developer's AI test
  // restores it).
  auto_turn_ = false;
  if (auto* next_turn = dynamic_cast<ToggleButton*>(find_window("NEXTTURNCONT"))) {
    next_turn->set_state(false);
    next_turn->draw(kDrawNormal);
  }
  if (hot_ != -1) {
    send_event(hot_, kEventMouseLeave, 0, 0);
  }
  hot_ = -1;
  modal_flags_.fill(true);
  broadcast(kEventHide, 0, 0, true);
  const Pane under = dialog.pane();
  std::optional<Image8> saved;
  if (save_under_) {
    saved = save_under(screen(), under);
  }
  add_to_screen(dialog.id(), current_);
  for (GWindow* child : dialog.children()) {
    add_to_screen(child->id(), current_);
  }
  dialog.handle_event(kEventModalOpen, 0, 0);
  dialog.handle_event(kEventActivate, 0, 0);

  // WinMgr_run_modal_loop 0x50a28.
  modal_ = true;
  modal_result_ = 0;
  while (running_ && modal_) {
    run_pass();
  }

  dialog.handle_event(kEventDeactivate, 0, 0);
  for (GWindow* child : dialog.children()) {
    remove_from_screen(child->id(), current_);
  }
  remove_from_screen(dialog.id(), current_);
  hot_ = -1;
  if (saved) {
    restore_under(screen(), under, *saved);
  }
  broadcast(kEventShow, 0, 0, true);
  save_under_ = true;
  return modal_result_;
}

void WinMgr::message_box(std::string text) {
  auto box = std::make_unique<TextBox>(*this, "MSGBOX", kMessageBoxPane);
  box->set_text(std::move(text));
  TextBox& box_ref = *box;
  const int box_id = register_window(std::move(box));
  auto button = std::make_unique<GWindow>(*this, static_string(kOkString), kMessageButtonPane);
  button->set_mouse_focus(true);
  button->set_message(kEventOk, 0, 0);
  button->set_parent(box_id);
  GWindow& button_ref = *button;
  const int button_id = register_window(std::move(button));
  box_ref.add_child(&button_ref);
  do_modal(box_ref);
  release_window(button_id);
  release_window(box_id);
}

int WinMgr::input_box(std::string_view title, std::string& buffer, int max_length) {
  const int font_height = large_font().font().height;
  const int h = font_height * 5 + 5;
  const int y0 = (kScreenHeight - h) / 2;
  auto box =
      std::make_unique<GWindow>(*this, std::string(title.substr(0, 0x13)), Pane{kInputBoxX0, y0, kInputBoxX1, y0 + h});
  box->set_text_pos(-1, 5);
  GWindow& box_ref = *box;
  const int box_id = register_window(std::move(box));

  const Pane& p = box_ref.pane();
  const std::string ok = static_string(kOkString);
  const int ok_width = large_font().string_width(ok);
  auto button =
      std::make_unique<GWindow>(*this, ok, Pane{p.x1 - ok_width - 10, p.y1 - font_height - 5, p.x1 - 3, p.y1 - 2});
  button->set_text_pos(-1, 2);
  button->set_mouse_focus(true);
  button->set_border(true);
  button->set_message(kEventOk, 0, 0);
  button->set_parent(box_id);
  GWindow& button_ref = *button;
  const int button_id = register_window(std::move(button));
  box_ref.add_child(&button_ref);

  const int edit_y0 = p.y0 + font_height + 8;
  auto edit = std::make_unique<EditField>(*this, Pane{p.x0 + 15, edit_y0, p.x1 - 15, edit_y0 + font_height + 8});
  edit->set_parent(box_id);
  edit->set_buffer(buffer, max_length);
  EditField& edit_ref = *edit;
  const int edit_id = register_window(std::move(edit));
  box_ref.add_child(&edit_ref);

  // The frame under the box, saved and restored here (not by do_modal).
  const Pane under{p.x0 - 8, p.y0 - 8, p.x1 + 8, p.y1 + 7};
  save_under_ = false;
  const Image8 saved = save_under(screen(), under);
  vfx_shape_draw(screen(), under, shared_shapes(kHelpShapes).at(kInputBoxFrame), 0, 0);
  const int result = do_modal(box_ref);
  restore_under(screen(), under, saved);
  buffer = edit_ref.text();
  release_window(button_id);
  release_window(edit_id);
  release_window(box_id);
  return result;
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
  // Shift held: help mode.
  if (input.shift && !help_mode_ && !modal_) {
    help_mode_ = true;
    services_.set_pointer_frame(kPointerHelp);
  } else if (help_mode_ && !input.shift) {
    help_mode_ = false;
    services_.set_pointer_frame(kPointerNormal);
    if (hot_ != -1) {
      send_event(hot_, kEventMouseLeave, 0, 0);
      send_event(hot_, kEventMouseEnter, 0, 0);
    }
  }
  // The exit corners: a corner pixel of any screen but the opening menu.
  int corner = -1;
  if (!help_mode_ && !modal_ && current_ != kScreenOpening) {
    const int right = kScreenWidth - 1;
    const int bottom = kScreenHeight - 1;
    if (input.y == 0) {
      corner = input.x == 0 ? 0 : input.x == right ? 1 : -1;
    } else if (input.y == bottom) {
      corner = input.x == 0 ? 2 : input.x == right ? 3 : -1;
    }
  }
  if (corner != exit_corner_) {
    exit_corner_ = corner;
    set_pointer(corner == -1 ? kPointerNormal : kPointerExitCorner + corner);
  }
  if (input.moved) {
    update_hover(input);
  }
  if (!modal_) {
    run_notifies(input);
    run_timers();
  }
  if (input.key_event) {
    const bool hotkey = (input.alt || input.scan_code == kScanEsc) && handle_hotkey(input);
    if (!hotkey) {
      broadcast_event(kEventKey, input.ascii, input.scan_code);
    }
  }
  if (input.left_changed && input.left_down) {
    if (exit_corner_ == -1) {
      // Alt + click is the developer's palette pick: nothing otherwise.
      if (!input.alt) {
        broadcast_event(help_mode_ && !modal_ ? kEventHelpClick : kEventLeftClick, input.x, input.y);
      }
    } else if (!modal_) {
      services_.sound.play_sfx(kExitSound);
      set_pointer(kPointerNormal);
      exit_corner_ = -1;
      go_back();
    } else {
      end_modal(-1);
    }
  }
  if (input.right_changed && input.right_down) {
    if (help_mode_) {
      help_mode_ = false;
      services_.set_pointer_frame(kPointerNormal);
    } else {
      broadcast_event(kEventRightClick, input.x, input.y);
    }
  }
  while (!messages_.empty()) {
    dispatch_messages();
  }
  // The game events of the star map (`WinMgr_show_game_events` 0x50f84)
  // come with the game events; the auto next turn waits for them.
  if (auto_turn_ && !services_.game.has_game_events() && !modal_) {
    post_message(kMessageNextTurn, 0, 0);
  }
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

void WinMgr::run_pass() {
  process_input(services_.poll_input());
  refresh_screen();
  services_.present();
}

void WindowFactory::add(int type, WindowCreator create) { creators_[type] = std::move(create); }

std::unique_ptr<GWindow> WindowFactory::create(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes) const {
  if (const auto it = creators_.find(record.type); it != creators_.end()) {
    return it->second(wm, record, shapes);
  }
  return std::make_unique<GWindow>(wm, record, shapes);
}

}  // namespace oa
