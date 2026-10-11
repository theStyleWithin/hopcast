# HopCast secure store (encrypted secrets). GPL-2.0-or-later.
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)

add_library(hopcast-securestore STATIC secure_store.cpp key_provider.cpp secure_store.hpp)
add_library(hopcast::securestore ALIAS hopcast-securestore)
target_include_directories(hopcast-securestore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(hopcast-securestore PRIVATE OpenSSL::Crypto)
set_target_properties(hopcast-securestore PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 17 FOLDER hopcast)

# Decided 2026-10-11: HopCast does NOT use KWallet / GNOME Keyring by default. The
# encryption key is kept in an owner-only key file. The keyring code path stays
# available behind this option but is off, so no keyring service is ever contacted.
option(HOPCAST_USE_KEYRING "Keep the secure-store key in the desktop keyring (KWallet, GNOME Keyring) via libsecret" OFF)
if(HOPCAST_USE_KEYRING)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
  target_compile_definitions(hopcast-securestore PRIVATE HOPCAST_HAVE_LIBSECRET)
  target_link_libraries(hopcast-securestore PRIVATE PkgConfig::LIBSECRET)
  message(STATUS "HopCast: secure store key in the desktop keyring (libsecret ${LIBSECRET_VERSION})")
else()
  message(STATUS "HopCast: secure store key in an owner-only key file (no keyring)")
endif()
