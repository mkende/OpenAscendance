oa_add_module(oa_screens
  SOURCES
    # The load/save and tutorial lists (loadwin.cpp).
    load_wnd.cpp
    # The new-game set-up (status.cpp).
    new_game_wnd.cpp
    screens.cpp
    # The species history at the start of a game.
    start_game_wnd.cpp
  DEPS oa_core oa_assets oa_gfx oa_audio oa_ui)
