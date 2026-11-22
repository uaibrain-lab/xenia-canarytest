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
xe_target_defaults(xenia-launcher-xenia-backend)

if(XENIA_BUILD_TESTS)
  add_subdirectory(testing)
endif()
