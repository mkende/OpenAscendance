# Unit tests. Tests needing the original game files read the install from the
# OA_DATA environment variable and are skipped when it is unset
# (game_data.h).
add_executable(oa_tests
  # Watcom rand and qsort.
  core_test.cpp
  # The COB archives and the asset decoders.
  assets_test.cpp
  # The text tables.
  tables_test.cpp
  # Blitters, fonts and palette fades.
  gfx_test.cpp
  # The mixer and the sound system.
  audio_test.cpp
  # Input polling.
  platform_test.cpp
  # The intro player and the window manager.
  ui_test.cpp
  # The toolkit's window classes.
  widgets_test.cpp
  # The help window and help.txt.
  help_test.cpp
  # The load/save and tutorial screens.
  load_test.cpp
  # The new-game and species history screens.
  newgame_test.cpp
  # The command line and the install choice.
  app_test.cpp
)
target_link_libraries(oa_tests PRIVATE oa_core oa_assets oa_gfx oa_audio oa_platform oa_ui oa_screens oa_app
  Catch2::Catch2WithMain SDL3::Headers)
include(Catch)
catch_discover_tests(oa_tests)

# The headless game core (src/game): its own executable, without SDL.
# OA_ORACLE names `work/oracle` (the original's reference saves; skipped
# when unset).
add_executable(oa_game_tests
  # Tables, saves and galaxy generation.
  game_test.cpp
)
target_link_libraries(oa_game_tests PRIVATE oa_game Catch2::Catch2WithMain)
catch_discover_tests(oa_game_tests)
