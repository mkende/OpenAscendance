oa_add_module(oa_ui
  SOURCES
    flic.cpp
    # The FLIC window (flicwin.cpp).
    flicwnd.cpp
    gwindow.cpp
    shape_cache.cpp
    winmgr.cpp
  DEPS oa_core oa_assets oa_gfx oa_audio oa_platform)
