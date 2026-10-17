# The headless game core: tables, state, saves and galaxy generation (no SDL).
oa_add_module(oa_game
  SOURCES
    float_math.cpp
    galaxy.cpp
    new_game.cpp
    planet.cpp
    race.cpp
    save.cpp
    tables.cpp
  DEPS oa_core oa_assets)
