// The application lifecycle events the system sends the program (a remake
// addition, adapted from SilentHeritage's): going to the background and
// back, low memory, termination. On Android the program may be killed
// without further notice once in the background, and SDL blocks the main
// thread there right after the events, so they are handled as they are
// sent, on the main thread, from inside the event poll that receives them.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace oa {

enum class LifecycleEvent : std::uint8_t {
  kWillEnterBackground,  // SDL_EVENT_WILL_ENTER_BACKGROUND
  kDidEnterBackground,   // SDL_EVENT_DID_ENTER_BACKGROUND
  kWillEnterForeground,  // SDL_EVENT_WILL_ENTER_FOREGROUND
  kDidEnterForeground,   // SDL_EVENT_DID_ENTER_FOREGROUND
  kLowMemory,            // SDL_EVENT_LOW_MEMORY
  kTerminating,          // SDL_EVENT_TERMINATING
};

// Returns the event's name, for the log.
std::string_view lifecycle_event_name(LifecycleEvent event);

// Returns the lifecycle event of the SDL event type `type`, nothing for
// the others.
std::optional<LifecycleEvent> lifecycle_event(std::uint32_t type);

// Calls a handler with the lifecycle events while it exists (an SDL event
// watch; only the events sent on the main thread). Throws
// `std::runtime_error` if SDL fails.
class LifecycleWatch {
 public:
  explicit LifecycleWatch(std::function<void(LifecycleEvent)> handler);
  ~LifecycleWatch();
  LifecycleWatch(const LifecycleWatch&) = delete;
  LifecycleWatch& operator=(const LifecycleWatch&) = delete;

  // Sends the events of the program going to the background and back, as
  // the system does (a scripted run's `kBackground` step).
  static void simulate_background();

 private:
  std::function<void(LifecycleEvent)> handler_;
};

}  // namespace oa
