# HopCast: compatibility shims injected into the engine's CMake project via
# CMAKE_PROJECT_INCLUDE, so the pinned NAVER engine sources stay unmodified.

# NAVER's fork removed upstream OBS's legacy_check() helper, but the pinned
# obs-websocket submodule still calls it at the top of its CMakeLists.txt.
# Upstream it only switches to the pre-OBS-30 build path; on the modern path
# it is a no-op, so an empty macro is equivalent.
if(NOT COMMAND legacy_check)
  macro(legacy_check)
  endmacro()
endif()
