oa_add_module(oa_platform
  SOURCES
    # The SDL sound output pulling from the mixer.
    audio_device.cpp
    # BIOS keyboard words (copied from SilentHeritage).
    bios_keys.cpp
    clock.cpp
    display.cpp
    input.cpp
    # The system's lifecycle events (the background, the end).
    lifecycle.cpp
    screenshot.cpp
    # The touch screen as the mouse (a remake addition).
    touch.cpp
  DEPS oa_core oa_assets oa_audio)
target_link_libraries(oa_platform PRIVATE SDL3::SDL3-static)
if(ANDROID)
  # The display cutout, from the game's activity.
  target_sources(oa_platform PRIVATE android_cutout.cpp)
endif()
