# The program's parts, a library so that the tests can use them.
oa_add_module(oa_app
  SOURCES
    command_line.cpp
    # main 0x1c424 and the SDL host.
    corn.cpp
    # The remembered source of the game files and the start-up choice.
    install_source.cpp
    # The install setup's dialogs.
    install_setup.cpp
  DEPS oa_core oa_assets oa_gfx oa_audio oa_platform oa_ui)
target_link_libraries(oa_app PRIVATE SDL3::SDL3-static)

add_executable(${OA_EXECUTABLE} main.cpp)
target_link_libraries(${OA_EXECUTABLE} PRIVATE oa_app SDL3::SDL3-static)
