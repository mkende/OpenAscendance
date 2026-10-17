oa_add_module(oa_screens
  SOURCES
    # The 3D camera of the research tree (Camera, vector helpers).
    camera3d.cpp
    # The star map and the Species button (coswnd.cpp).
    cos_wnd.cpp
    # The load/save and tutorial lists (loadwin.cpp).
    load_wnd.cpp
    # The new-game set-up (status.cpp).
    new_game_wnd.cpp
    # The planet screen and its square dialog (planwin.cpp, psqwin.cpp).
    plan_wnd.cpp
    # The research screen (reswin.cpp).
    res_wnd.cpp
    screens.cpp
    # The species history at the start of a game.
    start_game_wnd.cpp
    # The planet status list (status.cpp).
    status_wnd.cpp
  DEPS oa_core oa_assets oa_gfx oa_audio oa_ui oa_game)
