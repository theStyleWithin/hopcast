# HopCast (Linux): use the system OpenSSL 3 (Apache-2.0) instead of the bundled
# OpenSSL 1.1 binaries, exposing the same target names as the Windows/macOS builds.
find_package(OpenSSL 3 REQUIRED)
add_library(common-libs::third-party::libcrypto INTERFACE IMPORTED GLOBAL)
set_property(TARGET common-libs::third-party::libcrypto PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::Crypto)
add_library(common-libs::third-party::libssl INTERFACE IMPORTED GLOBAL)
set_property(TARGET common-libs::third-party::libssl PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::SSL)
add_library(common-libs::third-party::openssl INTERFACE IMPORTED GLOBAL)
set_property(TARGET common-libs::third-party::openssl PROPERTY
    INTERFACE_LINK_LIBRARIES common-libs::third-party::libcrypto common-libs::third-party::libssl)
