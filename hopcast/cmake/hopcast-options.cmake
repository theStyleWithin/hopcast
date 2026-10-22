# HopCast: build options shared by every platform (included from prism-linux.cmake and prism-windows.cmake).

# ---- No accounts (HopCast) ----------------------------------------------------
# PRISM gates the studio behind a NAVER/PRISM account login (Facebook, Google,
# Twitch, NAVER, Apple, LINE or e-mail). HopCast doesn't use NAVER's account
# service, so the app opens straight into the studio and the account UI
# (login window, Logout menu item, Settings > Account) is removed.
option(HOPCAST_NO_ACCOUNT "Start without a PRISM account and hide account UI" ON)
if(HOPCAST_NO_ACCOUNT)
  add_compile_definitions(HOPCAST_NO_ACCOUNT)
endif()

# ---- No NAVER services (HopCast) -------------------------------------------------
# HopCast must never talk to NAVER's servers (we have no right to use them, and the
# public source ships without their URLs/keys anyway). With HOPCAST_NO_NAVER:
#  - libhttp-client refuses any request to a NAVER-operated host (hard block),
#  - the startup calls (app init, gpop, notices, updates, resource CDN, thumbnails,
#    PRISM token) are not made; bundled defaults are used instead,
#  - NAVER-only channels (NAVER TV, BAND, Shopping Live, CHZZK, NCB2B, Whale, NOW),
#    the welcome/banner window and PRISM Lens onboarding are removed,
#  - the app is called HopCast and shows no PRISM logo.
option(HOPCAST_NO_NAVER "Remove all use of NAVER services and PRISM branding" ON)
if(HOPCAST_NO_NAVER)
  add_compile_definitions(HOPCAST_NO_NAVER)
endif()
