# HopCast: Linux-wide adjustments for PRISM's CMake tree.
# Included from src/CMakeLists.txt when building on Linux.

# Several PRISM targets glob *.mm / *.m on every platform (MSVC silently ignores
# them on Windows). GCC on Linux cannot compile Objective-C, so once the whole
# tree is configured, mark every Objective-C source of every target header-only.
function(hopcast_collect_targets dir out_var)
  get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
  get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
  foreach(_sub IN LISTS _subdirs)
    hopcast_collect_targets("${_sub}" _sub_targets)
    list(APPEND _targets ${_sub_targets})
  endforeach()
  set(${out_var} ${_targets} PARENT_SCOPE)
endfunction()

function(hopcast_exclude_objc_everywhere)
  hopcast_collect_targets("${CMAKE_SOURCE_DIR}" _all)
  set(_count 0)
  foreach(_t IN LISTS _all)
    get_target_property(_type ${_t} TYPE)
    if(_type STREQUAL "INTERFACE_LIBRARY" OR _type STREQUAL "UTILITY")
      continue()
    endif()
    get_target_property(_srcs ${_t} SOURCES)
    get_target_property(_srcdir ${_t} SOURCE_DIR)
    if(NOT _srcs)
      continue()
    endif()
    foreach(_s IN LISTS _srcs)
      if(_s MATCHES "\\.(mm|m)$")
        # Relative source paths are relative to the target's own source dir.
        if(NOT IS_ABSOLUTE "${_s}")
          set(_s "${_srcdir}/${_s}")
        endif()
        set_source_files_properties("${_s}" TARGET_DIRECTORY ${_t} PROPERTIES HEADER_FILE_ONLY TRUE)
        math(EXPR _count "${_count} + 1")
      endif()
    endforeach()
  endforeach()
  message(STATUS "HopCast: excluded ${_count} Objective-C source entries from Linux targets")
endfunction()

cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL hopcast_exclude_objc_everywhere)

# MSVC calling-convention keywords used in shared PRISM headers (clang on macOS
# accepts them; GCC doesn't). x86-64 Linux has a single calling convention.
add_compile_definitions(__cdecl= __stdcall=)

# libobs headers use SIMDe with native aliases on non-MSVC compilers; if GCC's
# <immintrin.h> is included *after* them (e.g. via Qt's qsimd.h), the alias macros
# break GCC's intrinsics headers at -O2. Including <immintrin.h> first avoids it.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
  string(APPEND CMAKE_CXX_FLAGS " -include immintrin.h")
endif()

# Compile C++ as ISO C++17 (not gnu++17): in GNU mode GCC predefines the macro
# `linux` as 1, which breaks e.g. `bool linux` in obs-ui/update/models/whatsnew.hpp.
# Upstream OBS builds with extensions off as well.
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Install layout (HopCast) -------------------------------------------------
# The app is installed into the same prefix as the engine, which is the layout
# the AppImage uses:  bin/  lib/ (libobs, PRISM libs)  lib/obs-plugins/
#                     lib/hopcast/plugins/ (PRISM plugins)  share/obs/  share/hopcast/
# NAVER's copy_* helpers only know Windows/macOS staging, so on Linux every
# built binary is installed generically once the tree is configured.
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib;$ORIGIN/../..")
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)

function(hopcast_install_everything)
  hopcast_collect_targets("${CMAKE_SOURCE_DIR}" _all)
  foreach(_t IN LISTS _all)
    get_target_property(_imported ${_t} IMPORTED)
    if(_imported)
      continue()
    endif()
    get_target_property(_type ${_t} TYPE)
    if(_type STREQUAL "EXECUTABLE")
      install(TARGETS ${_t} RUNTIME DESTINATION bin)
    elseif(_type STREQUAL "SHARED_LIBRARY")
      install(TARGETS ${_t} LIBRARY DESTINATION lib)
    elseif(_type STREQUAL "MODULE_LIBRARY")
      install(TARGETS ${_t} LIBRARY DESTINATION lib/hopcast/plugins)
    endif()
  endforeach()
endfunction()

cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL hopcast_install_everything)

# ---- No accounts (HopCast) ----------------------------------------------------
# PRISM gates the studio behind a NAVER/PRISM account login (Facebook, Google,
# Twitch, NAVER, Apple, LINE or e-mail). HopCast doesn't use NAVER's account
# service, so the app opens straight into the studio and the account UI
# (login window, Logout menu item, Settings > Account) is removed.
option(HOPCAST_NO_ACCOUNT "Start without a PRISM account and hide account UI" ON)
if(HOPCAST_NO_ACCOUNT)
  add_compile_definitions(HOPCAST_NO_ACCOUNT)
endif()
