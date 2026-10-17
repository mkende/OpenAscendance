// The window manager (`winmgr.cpp`, global `WinMgr` 0xfa344): the windows of
// `windows.txt`, the screens listing them, event delivery, the system message
// queue, window timers, the screen history and the per-pass input handling
// and refresh of the main loop (`docs/07-ui-framework.md`, "Window manager").
//
// Milestone 1 subset: screen changes, back, quit, hover and clicks, the Esc
// and Alt+X hotkeys, window timers, the music picker and the pending palette
// fade. Help mode, exit corners, modal loops, notifications, game events and
// event recording come later.
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "assets/cfile.h"
#include "assets/windows_txt.h"
#include "audio/sound_system.h"
#include "gfx/font.h"
#include "gfx/gsystem.h"
#include "platform/input.h"
#include "ui/gwindow.h"
#include "ui/shape_cache.h"

namespace oa {

// System messages (`WinMgr_dispatch_messages` 0x51290).
enum SystemMessage : int {
  kMessageNone = 0,
  kMessageChangeScreen = 1,  // p1 screen, p2 != 0 pushes the current one
  kMessageQuit = 2,
  kMessageBack = 3,
};

// What the window manager and the windows use of the rest of the program.
struct UiServices {
  const GameFiles& files;
  GSystem& gsystem;
  SoundSystem& sound;
  // Microseconds of the 100 Hz timer's clock (the FLIC timer's base).
  std::function<std::uint64_t()> now_us;
  // The 100 Hz tick counter (`0x12202c`).
  std::function<std::uint32_t()> ticks;
  // Waits about a millisecond (the main loop's 3-tick wait).
  std::function<void()> idle;
  // Shows or hides the software mouse pointer (`MOUSE_show` / `MOUSE_hide`).
  std::function<void(bool)> show_pointer;
};

class WinMgr {
 public:
  static constexpr int kScreens = 32;
  static constexpr int kScreenWindows = 32;
  static constexpr int kHistory = 16;
  static constexpr int kMaxMessages = 32;
  static constexpr int kMaxTimers = 16;
  // The main loop's minimum pass length, in ticks.
  static constexpr std::uint32_t kPassTicks = 3;

  // `WinMgr_init` (0x529cc) and `WinMgr_load_windows` (0x52a80): reads
  // `windows.txt`, the haze tables and fonts it names, and creates the
  // windows. Throws `DataError` when a file is missing or malformed.
  explicit WinMgr(UiServices services);
  ~WinMgr();
  WinMgr(const WinMgr&) = delete;
  WinMgr& operator=(const WinMgr&) = delete;

  UiServices& services() { return services_; }
  ShapeCache& shapes() { return shapes_; }
  GameFont& small_font() { return *small_font_; }
  GameFont& large_font() { return *large_font_; }
  Image8& screen() { return services_.gsystem.screen(); }
  // The WMGR_MAPFOCUS remap (`+0x3012`): identity with 0xf2 -> 0x96.
  const HazeTable& focus_map() const { return focus_map_; }

  // Whether the game runs (`0x865f4`; message 2 and Alt+X clear it).
  bool running() const { return running_; }
  int current_screen() const { return current_; }
  // The hot (mouse-over) window id (`+0x3002`), -1 for none.
  int hot_window() const { return hot_; }
  // Returns window `id` (1-based), null when there is none.
  GWindow* window(int id) const;
  // `WinMgr_find_window` (0x521b8): the window named `name` (first 10
  // characters), null when there is none.
  GWindow* find_window(std::string_view name) const;

  // Clears the hot window when it is `id` (a deactivated window).
  void release_hot(int id);

  // `WinMgr_set_screen` (0x522ac): switches to `screen`, pushing the current
  // one on the history with `push`.
  void set_screen(int screen, bool push);
  // `WinMgr_go_back` (0x524bc): pops the history and switches to it.
  void go_back();
  // `WinMgr_post_message` (0x51f70): queues a system message (dropped when
  // the queue is full).
  void post_message(int id, int p1, int p2);
  // `WinMgr_send_event` (0x52140): `handle_event` of window `id`.
  bool send_event(int id, int type, int a, int b);
  // `WinMgr_broadcast_event` (0x51ff8): to the windows of the current screen
  // from the top down until one consumes it.
  bool broadcast_event(int type, int a, int b);
  // `WinMgr_set_timer` (0x51810): sends event `msg` (p1, p2) to window `id`
  // every `period` ticks (each pass for 0). `WinMgr_kill_timer` (0x518d0)
  // removes the window's timers of `msg` (-1: all).
  void set_timer(int id, int msg, int p1, int p2, std::uint32_t period);
  void kill_timer(int id, int msg);

  // Arms the pending palette fade (`0x122034`): `fade_to_palette_step` 0..64,
  // one per refresh.
  void start_pending_fade();
  void cancel_pending_fade() { fade_pending_ = false; }

  // `WinMgr_process_input` (0x4faac): one main-loop pass of input handling,
  // messages, the music picker and the wait until 3 ticks have passed since
  // the previous pass.
  void process_input(const InputState& input);
  // `WinMgr_refresh_screen` (0x5049c): advances the pending fade.
  void refresh_screen();

 private:
  struct Screen {
    std::vector<int> windows;  // z order, last on top
    std::string shape_file;    // TYPE 100 SHAPEFILE
  };
  struct Timer {
    int window;
    int msg;
    int p1;
    int p2;
    std::uint32_t period;
    std::uint32_t due;
  };
  struct Message {
    int id;
    int p1;
    int p2;
  };

  void load_windows(const WindowsTxt& txt);
  void enter_screen(int screen);
  void update_hover(const InputState& input);
  void run_timers();
  void handle_hotkey(const InputState& input);
  void dispatch_messages();
  void pick_music(const InputState& input);

  UiServices services_;
  ShapeCache shapes_;
  std::unique_ptr<GameFont> small_font_;
  std::unique_ptr<GameFont> large_font_;
  HazeTable focus_map_{};
  std::vector<std::unique_ptr<GWindow>> windows_;  // id - 1
  std::array<Screen, kScreens> screens_;
  int current_ = -1;
  std::deque<int> history_;
  std::vector<Message> messages_;
  std::vector<Timer> timers_;
  int hot_ = -1;
  bool running_ = true;
  bool fade_pending_ = false;
  int fade_step_ = 0;
  bool first_opening_ = true;  // `0x86a4c`: resume.gam not loaded yet
  // The music picker (`0x86a3c`, `0x86a40`).
  int previous_tune_ = -1;
  int volume_ramp_ = -1;
  std::uint32_t last_pass_tick_ = 0;  // `0x122008`
  InputState last_input_;
};

}  // namespace oa
