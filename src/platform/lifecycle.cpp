#include "platform/lifecycle.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>

#include <format>
#include <stdexcept>
#include <utility>

namespace oa {

namespace {

using Handler = std::function<void(LifecycleEvent)>;

bool SDLCALL on_event(void* userdata, SDL_Event* event) {
  // Android's Java thread also pushes events (window, touch, text): only
  // the main thread's reach the handler.
  if (const std::optional<LifecycleEvent> e = lifecycle_event(event->type); e && SDL_IsMainThread()) {
    (*static_cast<Handler*>(userdata))(*e);
  }
  return true;
}

}  // namespace

std::string_view lifecycle_event_name(LifecycleEvent event) {
  switch (event) {
    case LifecycleEvent::kWillEnterBackground:
      return "will-enter-background";
    case LifecycleEvent::kDidEnterBackground:
      return "did-enter-background";
    case LifecycleEvent::kWillEnterForeground:
      return "will-enter-foreground";
    case LifecycleEvent::kDidEnterForeground:
      return "did-enter-foreground";
    case LifecycleEvent::kLowMemory:
      return "low-memory";
    case LifecycleEvent::kTerminating:
      return "terminating";
  }
  return "?";
}

std::optional<LifecycleEvent> lifecycle_event(std::uint32_t type) {
  switch (type) {
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
      return LifecycleEvent::kWillEnterBackground;
    case SDL_EVENT_DID_ENTER_BACKGROUND:
      return LifecycleEvent::kDidEnterBackground;
    case SDL_EVENT_WILL_ENTER_FOREGROUND:
      return LifecycleEvent::kWillEnterForeground;
    case SDL_EVENT_DID_ENTER_FOREGROUND:
      return LifecycleEvent::kDidEnterForeground;
    case SDL_EVENT_LOW_MEMORY:
      return LifecycleEvent::kLowMemory;
    case SDL_EVENT_TERMINATING:
      return LifecycleEvent::kTerminating;
    default:
      return std::nullopt;
  }
}

LifecycleWatch::LifecycleWatch(std::function<void(LifecycleEvent)> handler) : handler_(std::move(handler)) {
  if (!SDL_AddEventWatch(on_event, &handler_)) {
    throw std::runtime_error(std::format("SDL_AddEventWatch: {}", SDL_GetError()));
  }
}

LifecycleWatch::~LifecycleWatch() { SDL_RemoveEventWatch(on_event, &handler_); }

void LifecycleWatch::simulate_background() {
  for (const SDL_EventType type : {SDL_EVENT_WILL_ENTER_BACKGROUND, SDL_EVENT_DID_ENTER_BACKGROUND,
                                   SDL_EVENT_WILL_ENTER_FOREGROUND, SDL_EVENT_DID_ENTER_FOREGROUND}) {
    SDL_Event e{};
    e.type = type;
    SDL_PushEvent(&e);
  }
}

}  // namespace oa
