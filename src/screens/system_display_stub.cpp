#include "screens/system_display_stub.h"

#include "game/battle_control.h"
#include "game/race.h"
#include "game/session.h"

namespace oa {

bool SystemDisplayStub::handle_event(int type, int a, int b) {
  if (type != kEventActivate) {
    return GWindow::handle_event(type, a, b);
  }
  shown_ = true;
  active_ = true;
  game::Session* session = wm_.services().game.core();
  if (session != nullptr && session->game->cosmos.selected_star >= 0) {
    game::Cosmos& cosmos = session->game->cosmos;
    game::Star& star = cosmos.stars.at(static_cast<std::size_t>(cosmos.selected_star));
    const game::Race& player = cosmos.races[session->player_race];
    const std::uint8_t present = star.owner_mask | star.ships_mask;
    if ((present & session->player_mask) != 0 && (present & game::race_at_war_mask(*session, player)) != 0) {
      game::BattleControl control;
      control.open(*session, star);
      if (control.start_auto(*session)) {
        control.finish_all(*session);
      }
      control.close(*session);
    }
  }
  return false;
}

void add_system_display_stub(WindowFactory& factory) {
  factory.add(19, [](WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes) -> std::unique_ptr<GWindow> {
    return std::make_unique<SystemDisplayStub>(wm, record, shapes);
  });
}

}  // namespace oa
