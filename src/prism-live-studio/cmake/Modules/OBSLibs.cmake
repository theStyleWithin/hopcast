if(OS_WINDOWS)
    add_library(OBS::libobs SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::libobs PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/libobs;$ENV{OBS_BUILD_DIR}/config")
    set_target_properties(
        OBS::libobs PROPERTIES
        IMPORTED_IMPLIB_DEBUG "$ENV{OBS_BUILD_DIR}/libobs/Debug/obs.lib"
        IMPORTED_IMPLIB_RELEASE "$ENV{OBS_BUILD_DIR}/libobs/Release/obs.lib"
        IMPORTED_IMPLIB_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/libobs/RelWithDebInfo/obs.lib"
        IMPORTED_IMPLIB_MINSIZEREL "$ENV{OBS_BUILD_DIR}/libobs/RelWithDebInfo/obs.lib"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/libobs/Debug/obs.dll"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/libobs/Release/obs.dll"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/libobs/RelWithDebInfo/obs.dll"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/libobs/RelWithDebInfo/obs.dll")

    add_library(OBS::frontend-api SHARED IMPORTED GLOBAL)
	set_target_properties(
		OBS::frontend-api PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/UI/obs-frontend-api")
    set_target_properties(
        OBS::frontend-api PROPERTIES
        IMPORTED_IMPLIB_DEBUG "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Debug/obs-frontend-api.lib"
        IMPORTED_IMPLIB_RELEASE "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Release/obs-frontend-api.lib"
        IMPORTED_IMPLIB_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/RelWithDebInfo/obs-frontend-api.lib"
        IMPORTED_IMPLIB_MINSIZEREL "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/RelWithDebInfo/obs-frontend-api.lib"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Debug/obs-frontend-api.dll"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Release/obs-frontend-api.dll"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/RelWithDebInfo/obs-frontend-api.dll"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/RelWithDebInfo/obs-frontend-api.dll")

	add_library(OBS::w32-pthreads SHARED IMPORTED GLOBAL)
	set_target_properties(
		OBS::w32-pthreads PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/deps/w32-pthreads")
    set_target_properties(
        OBS::w32-pthreads PROPERTIES
        IMPORTED_IMPLIB_DEBUG "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/Debug/w32-pthreads.lib"
        IMPORTED_IMPLIB_RELEASE "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/Release/w32-pthreads.lib"
        IMPORTED_IMPLIB_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/RelWithDebInfo/w32-pthreads.lib"
        IMPORTED_IMPLIB_MINSIZEREL "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/RelWithDebInfo/w32-pthreads.lib"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/Debug/w32-pthreads.dll"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/Release/w32-pthreads.dll"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/RelWithDebInfo/w32-pthreads.dll"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/RelWithDebInfo/w32-pthreads.dll")

    add_library(OBS::blake2 STATIC IMPORTED GLOBAL)
    set_target_properties(
        OBS::blake2 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/deps/blake2/src")
    set_target_properties(
        OBS::blake2 PROPERTIES
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/deps/blake2/blake2.dir/Debug/blake2.lib"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/deps/blake2/blake2.dir/Release/blake2.lib"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/blake2/blake2.dir/RelWithDebInfo/blake2.lib"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/deps/blake2/blake2.dir/RelWithDebInfo/blake2.lib")

    add_library(obs-browser-panels INTERFACE IMPORTED GLOBAL)
    add_library(OBS::browser-panels ALIAS obs-browser-panels)
    target_include_directories(
        obs-browser-panels INTERFACE "$ENV{OBS_SRC_DIR}/plugins/obs-browser/panel")

    add_library(OBS::scripting SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::scripting PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/shared/obs-scripting")
    set_target_properties(
        OBS::scripting PROPERTIES
        IMPORTED_IMPLIB_DEBUG "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/Debug/obs-scripting.lib"
        IMPORTED_IMPLIB_RELEASE "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/Release/obs-scripting.lib"
        IMPORTED_IMPLIB_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/RelWithDebInfo/obs-scripting.lib"
        IMPORTED_IMPLIB_MINSIZEREL "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/RelWithDebInfo/obs-scripting.lib"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/Debug/obs-scripting.dll"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/Release/obs-scripting.dll"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/RelWithDebInfo/obs-scripting.dll"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/RelWithDebInfo/obs-scripting.dll")

    add_library(OBS::caption STATIC IMPORTED GLOBAL)
    set_target_properties(
        OBS::caption PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/deps/libcaption")
    set_target_properties(
        OBS::caption PROPERTIES
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/deps/libcaption/Debug/caption.lib"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/deps/libcaption/Release/caption.lib"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/libcaption/RelWithDebInfo/caption.lib"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/deps/libcaption/RelWithDebInfo/caption.lib")

    add_library(OBS::bpm STATIC IMPORTED GLOBAL)
    set_target_properties(
        OBS::bpm PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/shared/bpm")
    set_target_properties(
        OBS::bpm PROPERTIES
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/shared/bpm/bpm.dir/Debug/bpm.lib"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/shared/bpm/bpm.dir/Release/bpm.lib"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/shared/bpm/bpm.dir/RelWithDebInfo/bpm.lib"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/shared/bpm/bpm.dir/RelWithDebInfo/bpm.lib")

    add_library(OBS::aja-support STATIC IMPORTED GLOBAL)
    set_target_properties(
        OBS::aja-support PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/plugins/aja")
    set_target_properties(
        OBS::aja-support PROPERTIES
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/plugins/aja/aja-support.dir/Debug/aja-support.lib"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/plugins/aja/aja-support.dir/Release/aja-support.lib"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/plugins/aja/aja-support.dir/RelWithDebInfo/aja-support.lib"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/plugins/aja/aja-support.dir/RelWithDebInfo/aja-support.lib")
elseif(OS_MACOS)
    add_library(OBS::libobs SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::libobs PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/libobs;$ENV{OBS_BUILD_DIR}/config")
    set_target_properties(
        OBS::libobs PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/UI/Debug/OBS.app/Contents/Frameworks/libobs.framework/Versions/A/libobs"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/UI/Release/OBS.app/Contents/Frameworks/libobs.framework/Versions/A/libobs"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/UI/RelWithDebInfo/OBS.app/Contents/Frameworks/libobs.framework/Versions/A/libobs"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/UI/RelWithDebInfo/OBS.app/Contents/Frameworks/libobs.framework/Versions/A/libobs"
        IMPORTED_SONAME "@rpath/llibobs.framework/Versions/A/libobs"
    )
    
    add_library(OBS::frontend-api SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::frontend-api PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/UI/obs-frontend-api"
    )
    set_target_properties(
        OBS::frontend-api PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Debug/obs-frontend-api.dylib"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Release/obs-frontend-api.dylib"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/RelWithDebInfo/obs-frontend-api.dylib"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/RelWithDebInfo/obs-frontend-api.dylib"
        IMPORTED_SONAME "@rpath/obs-frontend-api.dylib"
    )

    add_library(OBS::blake2 SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::blake2 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/deps/blake2/src"
    )
    set_target_properties(
        OBS::blake2 PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/build/blake2.build/Debug/libblake2.a"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/build/blake2.build/Release/libblake2.a"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/build/blake2.build/RelWithDebInfo/libblake2.a"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/build/blake2.build/RelWithDebInfo/libblake2.a"
    )

    add_library(obs-browser-panels INTERFACE IMPORTED GLOBAL)
    add_library(OBS::browser-panels ALIAS obs-browser-panels)
    target_include_directories(
        obs-browser-panels INTERFACE "$ENV{OBS_SRC_DIR}/plugins/obs-browser/panel")

    add_library(OBS::scripting SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::scripting PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/shared/obs-scripting")
    set_target_properties(
        OBS::scripting PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/Debug/obs-scripting.dylib"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/Release/obs-scripting.dylib"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/RelWithDebInfo/obs-scripting.dylib"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/RelWithDebInfo/obs-scripting.dylib"
        IMPORTED_SONAME "@rpath/obs-scripting.dylib"
    )

    add_library(OBS::caption SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::caption PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/deps/libcaption"
    )
    set_target_properties(
        OBS::caption PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/deps/libcaption/Debug/libcaption.a"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/deps/libcaption/Release/libcaption.a"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/libcaption/RelWithDebInfo/libcaption.a"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/deps/libcaption/RelWithDebInfo/libcaption.a"
    )

    add_library(OBS::bpm STATIC IMPORTED GLOBAL)
    set_target_properties(
        OBS::bpm PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/shared/bpm")
    set_target_properties(
        OBS::bpm PROPERTIES
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/build/bpm.build/Debug/libbpm.a"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/build/bpm.build/Release/libbpm.a"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/build/bpm.build/RelWithDebInfo/libbpm.a"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/build/bpm.build/RelWithDebInfo/libbpm.a")

    add_library(OBS::aja-support STATIC IMPORTED GLOBAL)
    set_target_properties(
        OBS::aja-support PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/plugins/aja")
    set_target_properties(
        OBS::aja-support PROPERTIES
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/build/aja-support.build/Debug/libaja-support.a"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/build/aja-support.build/Release/libaja-support.a"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/build/aja-support.build/RelWithDebInfo/libaja-support.a"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/build/aja-support.build/RelWithDebInfo/libaja-support.a")
elseif(OS_LINUX)
    # HopCast: Linux. libobs comes from the engine built by hopcast/scripts/build-engine.sh
    # (OBS_BUILD_DIR). The small helper libraries that NAVER's Windows/macOS builds take
    # from the engine's own UI build are compiled here straight from the engine sources,
    # so the engine can stay a UI-less build.
    file(GLOB _hopcast_libobs "$ENV{OBS_BUILD_DIR}/libobs/libobs.so.[0-9]*")
    list(SORT _hopcast_libobs)
    list(GET _hopcast_libobs 0 _hopcast_libobs)
    if(NOT EXISTS "${_hopcast_libobs}")
        message(FATAL_ERROR "HopCast: libobs not found in $ENV{OBS_BUILD_DIR}/libobs - run hopcast/scripts/build-engine.sh first")
    endif()
    message(STATUS "HopCast: using engine libobs ${_hopcast_libobs}")

    add_library(OBS::libobs SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::libobs PROPERTIES
        IMPORTED_LOCATION "${_hopcast_libobs}"
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/libobs;$ENV{OBS_BUILD_DIR}/config")
    find_package(Threads REQUIRED)
    set_property(TARGET OBS::libobs APPEND PROPERTY INTERFACE_LINK_LIBRARIES Threads::Threads)

    # libcaption (static, from engine source)
    add_library(hopcast-caption STATIC
        "$ENV{OBS_SRC_DIR}/deps/libcaption/src/caption.c"
        "$ENV{OBS_SRC_DIR}/deps/libcaption/src/cea708.c"
        "$ENV{OBS_SRC_DIR}/deps/libcaption/src/eia608.c"
        "$ENV{OBS_SRC_DIR}/deps/libcaption/src/eia608_charmap.c"
        "$ENV{OBS_SRC_DIR}/deps/libcaption/src/eia608_from_utf8.c"
        "$ENV{OBS_SRC_DIR}/deps/libcaption/src/mpeg.c"
        "$ENV{OBS_SRC_DIR}/deps/libcaption/src/scc.c"
        "$ENV{OBS_SRC_DIR}/deps/libcaption/src/srt.c"
        "$ENV{OBS_SRC_DIR}/deps/libcaption/src/utf8.c"
        "$ENV{OBS_SRC_DIR}/deps/libcaption/src/xds.c")
    target_include_directories(hopcast-caption
        PRIVATE "$ENV{OBS_SRC_DIR}/deps/libcaption/caption"
        PUBLIC "$ENV{OBS_SRC_DIR}/deps/libcaption")
    target_compile_definitions(hopcast-caption PRIVATE __STDC_CONSTANT_MACROS)
    set_target_properties(hopcast-caption PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER hopcast/engine)
    add_library(OBS::caption ALIAS hopcast-caption)

    # bpm (static, from engine source)
    add_library(hopcast-bpm STATIC "$ENV{OBS_SRC_DIR}/shared/bpm/bpm.c")
    target_include_directories(hopcast-bpm PUBLIC "$ENV{OBS_SRC_DIR}/shared/bpm")
    target_link_libraries(hopcast-bpm PUBLIC OBS::libobs OBS::caption)
    set_target_properties(hopcast-bpm PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER hopcast/engine)
    add_library(OBS::bpm ALIAS hopcast-bpm)

    # blake2 (static, from engine source)
    add_library(hopcast-blake2 STATIC "$ENV{OBS_SRC_DIR}/deps/blake2/src/blake2b-ref.c")
    target_include_directories(hopcast-blake2 PUBLIC "$ENV{OBS_SRC_DIR}/deps/blake2/src")
    set_target_properties(hopcast-blake2 PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER hopcast/engine)
    add_library(OBS::blake2 ALIAS hopcast-blake2)
    add_library(OBS::blake2_static ALIAS hopcast-blake2)

    # obs-frontend-api (shared, from engine source incl. NAVER's pls-frontend-api)
    add_library(obs-frontend-api SHARED
        "$ENV{OBS_SRC_DIR}/UI/obs-frontend-api/obs-frontend-api.cpp"
        "$ENV{OBS_SRC_DIR}/UI/obs-frontend-api/pls-frontend-api.cpp")
    target_compile_definitions(obs-frontend-api PRIVATE OBS_FRONTEND_API_EXPORTS)
    target_include_directories(obs-frontend-api PUBLIC "$ENV{OBS_SRC_DIR}/UI/obs-frontend-api")
    target_link_libraries(obs-frontend-api PRIVATE OBS::libobs)
    set_target_properties(obs-frontend-api PROPERTIES FOLDER hopcast/engine)
    add_library(OBS::frontend-api ALIAS obs-frontend-api)

    # Browser panel interface (headers only; CEF is loaded at runtime by obs-browser)
    add_library(obs-browser-panels INTERFACE IMPORTED GLOBAL)
    add_library(OBS::browser-panels ALIAS obs-browser-panels)
    target_include_directories(
        obs-browser-panels INTERFACE "$ENV{OBS_SRC_DIR}/plugins/obs-browser/panel")

    # Not yet available on Linux: OBS::scripting (frontend-tools is skipped),
    # OBS::aja-support (AJA is off in the engine). OBS::w32-pthreads is Windows-only.
endif()
