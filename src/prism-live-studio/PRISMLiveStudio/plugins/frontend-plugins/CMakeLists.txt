# HopCast: on Linux these are added back as they are ported
# (frontend-tools needs the engine's scripting library; DeckLink/AJA are off in the engine).
if(OS_LINUX)
  return()
endif()

add_subdirectory(decklink-output-ui)
add_subdirectory(frontend-tools)
add_subdirectory(decklink-captions)
add_subdirectory(aja-output-ui)