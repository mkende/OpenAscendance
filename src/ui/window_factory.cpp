// `WindowFactory::standard`: the toolkit's classes by `windows.txt` TYPE.

#include "ui/button_group.h"
#include "ui/digit_counter.h"
#include "ui/event_wnd.h"
#include "ui/flicwnd.h"
#include "ui/help_wnd.h"
#include "ui/list_box.h"
#include "ui/shape_button.h"
#include "ui/text_box.h"
#include "ui/toggle_button.h"
#include "ui/winmgr.h"

namespace oa {

namespace {

template <typename T>
WindowCreator creator() {
  return [](WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes) -> std::unique_ptr<GWindow> {
    return std::make_unique<T>(wm, record, shapes);
  };
}

}  // namespace

WindowFactory WindowFactory::standard() {
  WindowFactory f;
  f.add(1, [](WinMgr& wm, const WindowRecord& record, const ShapeTable* /*shapes*/) -> std::unique_ptr<GWindow> {
    return std::make_unique<FlicWnd>(wm, record);
  });
  f.add(3, creator<ButtonGroup>());
  f.add(10, creator<ListBox>());
  f.add(15, creator<DigitCounter>());
  f.add(20, creator<HelpWnd>());
  f.add(23, creator<EventWnd>());
  f.add(25, creator<ToggleButton>());
  f.add(26, creator<TextBox>());
  f.add(31, creator<RaceColorButton>());
  // The screens' classes (`screens/screens.h`: the star map, battle,
  // planet, ship design, research, load/save, intelligence, planet square,
  // status, fleet, new game, battle control, diplomacy, special abilities,
  // end of game, start of game, battle items) draw nothing
  // until they are added; 5, 6 and 29 (buttons with their own look) are
  // plain buttons until then.
  for (const int type : {2, 4, 7, 8, 9, 11, 12, 13, 14, 16, 17, 18, 19, 21, 22, 24, 30, 32}) {
    f.add(type, creator<PlaceholderWnd>());
  }
  return f;
}

}  // namespace oa
