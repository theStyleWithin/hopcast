if(OS_WINDOWS)
add_subdirectory(liblog)
elseif(OS_MACOS)
add_subdirectory(liblog)
elseif(OS_LINUX) # HopCast: same cross-platform Qt sources
add_subdirectory(liblog)
endif()
