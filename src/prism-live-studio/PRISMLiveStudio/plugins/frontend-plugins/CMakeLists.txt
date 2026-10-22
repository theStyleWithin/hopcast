# HopCast: on Linux and Windows these are left out (frontend-tools needs the engine's scripting
# library; DeckLink/AJA capture-card support is off in the engine).
if(OS_LINUX OR OS_WINDOWS)
  return()
endif()

add_subdirectory(decklink-output-ui)
add_subdirectory(frontend-tools)
add_subdirectory(decklink-captions)
add_subdirectory(aja-output-ui)