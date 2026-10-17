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
  DEPS oa_core oa_assets oa_gfx oa_audio oa_platform oa_ui oa_screens)
target_link_libraries(oa_app PRIVATE SDL3::SDL3-static)

if(ANDROID)
  # The install setup's folder picker and the Storage Access Framework
  # folders (app/android_documents.h).
  target_sources(oa_app PRIVATE android_documents.cpp)
  # SDL's Java activity loads the program as libmain.so and calls SDL_main.
  add_library(${OA_EXECUTABLE} SHARED main.cpp)
  set_target_properties(${OA_EXECUTABLE} PROPERTIES OUTPUT_NAME main)
else()
  add_executable(${OA_EXECUTABLE} main.cpp)
endif()
if(WIN32)
  # A windowed program (no console window; app/main.cpp attaches to the
  # console it is started from), with its icon, version and manifest.
  set(OA_WINDOWS_ICON ${PROJECT_SOURCE_DIR}/packaging/icon/icon.ico CACHE FILEPATH
    "The executable's icon (.ico); none when empty")
  set(OA_WINDOWS_ICON_RESOURCE "")
  if(OA_WINDOWS_ICON)
    set(OA_WINDOWS_ICON_RESOURCE "1 ICON \"${OA_WINDOWS_ICON}\"")
  endif()
  set(OA_WINDOWS_MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/windows.manifest)
  configure_file(windows.rc.in ${CMAKE_CURRENT_BINARY_DIR}/windows.rc @ONLY)
  target_sources(${OA_EXECUTABLE} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/windows.rc)
  set_target_properties(${OA_EXECUTABLE} PROPERTIES WIN32_EXECUTABLE TRUE)
  set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/windows.rc APPEND PROPERTY OBJECT_DEPENDS
    ${OA_WINDOWS_MANIFEST} ${OA_WINDOWS_ICON})
endif()
target_link_libraries(${OA_EXECUTABLE} PRIVATE oa_app SDL3::SDL3-static)
