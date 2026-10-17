oa_add_module(oa_gfx
  SOURCES
    font.cpp
    gsystem.cpp
    vfx.cpp
    # The polygon rasterizers and the scaled shape draw.
    vfx_poly.cpp
  DEPS oa_core oa_assets)
