# HopCast: Windows adjustments for PRISM's CMake tree. Included from src/CMakeLists.txt on Windows.
# The Windows build is a portable program (no installer, no registry); see PACKAGING.md / README.

include("${CMAKE_CURRENT_LIST_DIR}/hopcast-options.cmake")

# HopCast builds the engine first (hopcast-windows workflow) and installs it, like on Linux, instead of
# letting PRISM's own scripts (obs-configure.cmd / obs-build.cmd) configure and build it from the app.
option(HOPCAST_PREBUILT_ENGINE "The engine is already built and installed; skip PRISM's engine build scripts" ON)
