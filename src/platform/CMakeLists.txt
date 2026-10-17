oa_add_module(oa_platform
  SOURCES
    # The SDL sound output pulling from the mixer.
    audio_device.cpp
    # BIOS keyboard words (copied from SilentHeritage).
    bios_keys.cpp
    clock.cpp
    display.cpp
    input.cpp
    screenshot.cpp
  DEPS oa_core oa_assets oa_audio)
target_link_libraries(oa_platform PRIVATE SDL3::SDL3-static)
