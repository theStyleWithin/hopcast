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

# NAVER's libobs/CMakeLists.txt globs pls/*.m into libobs on every platform.
# Those Objective-C files are wrapped in #ifdef __APPLE__ (empty elsewhere),
# but GCC on Linux can't compile Objective-C at all. After the whole project
# is configured, mark them header-only so they're excluded from the build.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  function(hopcast_exclude_objc_sources)
    file(GLOB _hopcast_objc "${CMAKE_SOURCE_DIR}/libobs/pls/*.m" "${CMAKE_SOURCE_DIR}/libobs/pls/*.mm")
    if(_hopcast_objc)
      set_source_files_properties(${_hopcast_objc} DIRECTORY "${CMAKE_SOURCE_DIR}/libobs"
                                  PROPERTIES HEADER_FILE_ONLY TRUE)
      message(STATUS "HopCast: excluded Objective-C sources on Linux: ${_hopcast_objc}")
    endif()
  endfunction()
  cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL hopcast_exclude_objc_sources)
endif()
