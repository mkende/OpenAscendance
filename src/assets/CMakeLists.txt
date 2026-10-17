oa_add_module(oa_assets
  SOURCES
    # The COB archives and loose files (cfile.cpp).
    cfile.cpp
    flc.cpp
    flic_script.cpp
    fnt.cpp
    gif.cpp
    pal.cpp
    pcm.cpp
    shp.cpp
    sound_tables.cpp
    tokens.cpp
    windows_txt.cpp
  DEPS oa_core)
