# src/xenia/launcher/CMakeLists.txt - launcheruwp integration layer.
# Only built when XENIA_BUILD_LAUNCHER=ON (see src/xenia/CMakeLists.txt).

# Portable, header-only contracts (execution modes, portal upload rules,
# runtime backend interface). No dependency on the emulator core.
add_library(xenia-launcher-core INTERFACE)
target_include_directories(xenia-launcher-core INTERFACE
  ${PROJECT_SOURCE_DIR}/src
)

# Backend that drives the Xenia engine (emulated mode).
add_library(xenia-launcher-xenia-backend STATIC)
target_sources(xenia-launcher-xenia-backend PRIVATE
  xenia_backend.cc
  xenia_backend.h
)
target_link_libraries(xenia-launcher-xenia-backend PUBLIC
  xenia-launcher-core
  xenia-core
)
if(WIN32)
  target_sources(xenia-launcher-xenia-backend PRIVATE
    embedded_core_window.h embedded_xenia_runtime.h embedded_xenia_runtime.cc)
  target_link_libraries(xenia-launcher-xenia-backend PUBLIC
    xenia-ui xenia-apu-xaudio2 xenia-gpu-d3d12 xenia-hid-xinput xenia-ui-d3d12
    xenia-apu xenia-cpu xenia-gpu xenia-hid xenia-kernel xenia-patcher xenia-vfs
  )
  if(XE_TARGET_X86_64)
    target_link_libraries(xenia-launcher-xenia-backend PUBLIC xenia-cpu-backend-x64)
  elseif(XE_TARGET_AARCH64)
    target_link_libraries(xenia-launcher-xenia-backend PUBLIC xenia-cpu-backend-a64)
  endif()
endif()
xe_target_defaults(xenia-launcher-xenia-backend)

if(XENIA_BUILD_TESTS)
  add_subdirectory(testing)
endif()

# The real embedded engine, not a launcher executable. Keep the standalone
# app and static launcher library unchanged. Use the Store toolchain for
# the DLL that will be staged into Jadyx's existing package.
option(XENIA_BUILD_EMBEDDED_UWP "Build the embedded Xenia UWP DLL" OFF)
if(XENIA_BUILD_EMBEDDED_UWP)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore" OR NOT MSVC OR NOT XE_TARGET_X86_64)
    message(FATAL_ERROR "Embedded Xenia requires MSVC x64 WindowsStore")
  endif()
  add_library(uaibrain_xenia_embedded SHARED embedded_xenia_runtime.cc)
  target_link_libraries(uaibrain_xenia_embedded PRIVATE
    xenia-launcher-xenia-backend
    xenia-apu xenia-apu-nop xenia-base xenia-core xenia-cpu
    xenia-gpu xenia-gpu-null xenia-hid xenia-hid-nop
    xenia-kernel xenia-patcher xenia-ui xenia-vfs
    aes_128 capstone fmt dxbc imgui libavcodec libavutil
    mspack snappy xxhash
    WindowsApp.lib
  )
  target_link_options(uaibrain_xenia_embedded PRIVATE /APPCONTAINER)
  set_target_properties(uaibrain_xenia_embedded PROPERTIES
    OUTPUT_NAME uaibrain_xenia_embedded
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/embedded-xenia/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/embedded-xenia/$<CONFIG>"
  )
  xe_target_defaults(uaibrain_xenia_embedded)
endif()
