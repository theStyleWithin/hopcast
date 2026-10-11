if(OS_WINDOWS)
    add_subdirectory(windows)
elseif(OS_MACOS)
    add_subdirectory(mac)
elseif(OS_LINUX) # HopCast
    add_subdirectory(linux)
endif()