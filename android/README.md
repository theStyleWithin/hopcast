# HopCast for Android (early test build)

Streams or records your phone screen while you use other apps, with no account and no telemetry.
Android 10 or newer. GPL-2.0-or-later.

- Stream to any RTMP, RTMPS or SRT server you type in. Every session is also saved on the phone as an MP4 (`Movies/HopCast`).
- Starts when HopCast leaves the screen, so the screen shape (portrait or landscape) matches your game. HopCast's own screen is never shown on the stream.
- Microphone is off until you turn it on. Game sound is captured only from apps that allow it.
- A floating bubble (needs "show over other apps") holds quick buttons: hide screen (viewers see black), microphone, stop. It is marked secure so Android leaves it out of the picture.
- The stream key is stored encrypted with a key the phone's keystore holds.

Not built yet: the app block list (black screen whenever a listed app is shown), several channels at once, and following a game's rotation after the stream has started (the picture is fitted inside the stream's shape instead).

## Build

```
cd android
./gradlew assembleRelease
```

Needs JDK 17 or newer and the Android SDK. The version comes from `hopcast/VERSION`
(`versionName` = its first three numbers, `versionCode` = major*10000 + minor*100 + patch).

The only dependency is [RootEncoder](https://github.com/pedroSG94/RootEncoder) (Apache-2.0, from JitPack), which does the video encoding and the RTMP/SRT sending.

## Test signing key

`hopcast-test.keystore` is a **public test key** (its passwords are in `app/build.gradle.kts`). It lets test builds
install over each other and nothing more. Don't treat an APK signed with it as proof of who built it.
A real release key will replace it before any wider release. F-Droid signs its own builds.

## Known to check on a real phone

- The bubble should not appear in the stream or the recording (it uses a secure window flag).
- A game that rotates after the stream has started should be fitted inside the stream's shape.
- Long sessions: heat, battery and the size of the MP4.
