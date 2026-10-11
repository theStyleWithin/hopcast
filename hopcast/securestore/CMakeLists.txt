# HopCast secure store (encrypted secrets). GPL-2.0-or-later.
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)

add_library(hopcast-securestore STATIC secure_store.cpp key_provider.cpp secure_store.hpp)
add_library(hopcast::securestore ALIAS hopcast-securestore)
target_include_directories(hopcast-securestore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(hopcast-securestore PRIVATE OpenSSL::Crypto)
set_target_properties(hopcast-securestore PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 17 FOLDER hopcast)

if(LIBSECRET_FOUND)
  target_compile_definitions(hopcast-securestore PRIVATE HOPCAST_HAVE_LIBSECRET)
  target_link_libraries(hopcast-securestore PRIVATE PkgConfig::LIBSECRET)
  message(STATUS "HopCast: secure store uses the desktop keyring (libsecret ${LIBSECRET_VERSION})")
else()
  message(WARNING "HopCast: libsecret-1 not found - stream keys will be encrypted with an owner-only key file instead of the desktop keyring")
endif()
