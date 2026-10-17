// The game-event box (`eventwnd.cpp`, class `EventWnd`, `windows.txt` TYPE
// 23 `EVENTWIN`, ctor 0x25ec0, vtable 0x85af0): a help window showing the
// `help.txt` topic `event%02d` of one queued game event, its `%s` fields and
// pictures filled from the event's objects (`EventWnd_set_event` 0x25f94).
// The window manager runs it modally on the star map
// (`WinMgr::show_game_events`); its buttons' MSG values are the modal
// results the window manager acts on.
#pragma once

#include <optional>

#include "game/events.h"
#include "ui/help_wnd.h"

namespace oa {

class EventWnd final : public HelpWnd {
 public:
  // The topics' help file.
  static constexpr const char* kHelpFile = "help.txt";

  EventWnd(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes);

  // `EventWnd_set_event` (0x25f94): opens the topic `event%02d` of the
  // event's code for the current screen and fills its first text (the
  // topic's text as the format of the event's names and numbers), its
  // first picture and, for some events, its second one. The game core must
  // hold the event's objects (the original asserts the ship's location
  // kinds). Throws `DataError` when the topic is missing.
  void set_event(const game::GameEvent& event);
  // The event shown last (`+0xdaf`), nothing before the first.
  const std::optional<game::GameEvent>& event() const { return event_; }

 private:
  std::optional<game::GameEvent> event_;
};

}  // namespace oa
