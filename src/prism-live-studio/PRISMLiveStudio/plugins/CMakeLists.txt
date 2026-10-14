# HopCast: no accounts and Custom RTMP only, so the platform sign-in plugins
# (Facebook, YouTube, Twitch, NAVER TV, BAND, CHZZK, SOOP, Shopping Live, NCB2B)
# are not built. They were loaded at every start without ever being used.
if(NOT HOPCAST_NO_ACCOUNT)
  add_subdirectory(login)
endif()
add_subdirectory(source)
add_subdirectory(frontend-plugins)

