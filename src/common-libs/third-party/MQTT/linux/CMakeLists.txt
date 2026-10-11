# HopCast (Linux): PRISM's "MQTT" dependency is Eclipse Mosquitto; use the system
# libmosquitto / libmosquittopp (EPL-2.0/EDL-1.0), same target names as Windows/macOS.
find_package(PkgConfig REQUIRED)
pkg_check_modules(HOPCAST_MOSQUITTO REQUIRED IMPORTED_TARGET libmosquitto)
pkg_check_modules(HOPCAST_MOSQUITTOPP REQUIRED IMPORTED_TARGET libmosquittopp)
add_library(common-libs::third-party::mosquitto INTERFACE IMPORTED GLOBAL)
set_property(TARGET common-libs::third-party::mosquitto PROPERTY INTERFACE_LINK_LIBRARIES PkgConfig::HOPCAST_MOSQUITTO)
add_library(common-libs::third-party::mosquittopp INTERFACE IMPORTED GLOBAL)
set_property(TARGET common-libs::third-party::mosquittopp PROPERTY INTERFACE_LINK_LIBRARIES PkgConfig::HOPCAST_MOSQUITTOPP)
add_library(common-libs::third-party::mqtt INTERFACE IMPORTED GLOBAL)
set_property(TARGET common-libs::third-party::mqtt PROPERTY
    INTERFACE_LINK_LIBRARIES common-libs::third-party::mosquitto common-libs::third-party::mosquittopp)
