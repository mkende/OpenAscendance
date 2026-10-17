oa_add_module(oa_ui
  SOURCES
    # The ButtonGroup, ToggleButton, ListBox, DigitCounter, EditField and
    # TextBox classes (gwindow.cpp).
    button_group.cpp
    digit_counter.cpp
    edit_field.cpp
    flic.cpp
    # The FLIC window (flicwin.cpp).
    flicwnd.cpp
    # The game session over the game core (saves, new game, the day).
    game_core_session.cpp
    gwindow.cpp
    # The help window (helpwin.cpp).
    help_wnd.cpp
    list_box.cpp
    shape_cache.cpp
    # TYPE 31 / 32 windows and the placeholders of the screens to come.
    shape_button.cpp
    text_box.cpp
    toggle_button.cpp
    window_factory.cpp
    winmgr.cpp
  DEPS oa_core oa_assets oa_gfx oa_audio oa_platform oa_game)
