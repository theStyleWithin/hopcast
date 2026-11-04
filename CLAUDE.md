# Working on HopCast (notes for AI-assisted sessions)

The owner's standing rules for this project. Follow them every session.

## Releases

- **Every release has every platform.** When a version is released, the Linux build, the Windows build and (once it exists) the Android build all get that same version, built from the same commit, and all are attached to the same GitHub release. Versions are bumped together and updated together; a platform is never left on an older version, even if only one platform changed.
- **Platform priority: Linux first, Android second, Windows last.** This is the order for building, fixing and releasing. When time or effort is short, Linux comes before Android, and Android before Windows. So a release goes Linux, then Android, then Windows.
- Linux: the release workflow (`hopcast-app.yml`) with `package=true`, `release_tag=vX.Y.Z`, `prerelease=false`.
- Windows: `hopcast-windows.yml` with the same `release_tag`, then `hopcast-release-notes.yml` so the release text carries the Windows and Android notices.
- Android: `hopcast-android.yml` with the same `release_tag` attaches `HopCast-android-<ver>.apk`, `hopcast-fdroid-<ver>.tar.gz` (the F-Droid hand-off package) and `SHA256SUMS-android`. The Android project is in `android/`. It builds on every change to it and runs on pull requests.
- Never publish a release without asking the owner first. One yes covers all platforms of that version.
- Do not publish to the AUR, Flathub or F-Droid. Those are hand-offs only (`PACKAGING.md`).

## Platforms behave the same

- Keep the Linux, Windows and Android builds running as identically as possible. When a feature or fix lands on one platform, the others are owed it; where a platform can't do something (for example virtual camera or capture cards on Windows), say so plainly in the release text.
- **One deliberate exception: the live-preview security check is Android-only.** A phone puts your whole life on screen and it is much harder to control what gets recorded, so HopCast for Android builds its own barriers: the app block list, the Game source (only one app is ever shown), black boxes, and the live-preview security check (tag app icons on your home screen before going live; HopCast keeps a black box over them). Desktop does not need these.
- The owner tests only Linux and Android, for errors and viruses. Windows is checked by automated builds only, and its release notice says so.
- Everyone should find their version on the one release page, with the same version number in every file name.

## Branches and pull requests

- Pushing branches and merging pull requests into `main` does not need asking; tell the owner the result afterwards.
- One pull request per change. Don't open extra pull requests for branches already covered by another one.
- `dev` has unrelated history from `main`. Do not push to it.

## Golden rule

No nudges: no quick links, recommendations or invitations toward any platform or community, and no viewer counts, follower numbers, rankings or metrics. This applies to every Gamehopper app and to package descriptions.

## Android test device

The owner tests the Android app on a RedMagic 8S Pro (model NX729J, Android 15 on RedMagic OS 10, Snapdragon 8 Gen 2, 16 GB RAM plus 10 GB virtual, 512 GB storage, 6.8 inch AMOLED at 2480x1116 and 120 Hz, no camera cutout because the front camera is under the display, built-in cooling fan). Design and tune for that phone first: hardware video encoding (H.264 and HEVC), a long gaming session on a sustained load, and a tall 20:9 screen. Keep it working on older and slower phones too, but this is the one that gets tested.

## Android as a second test point for streaming problems

Streaming to Rumble and Odysee is still refused, and a multi-destination stream once dropped the owner's Wi-Fi (see `KNOWN-ISSUES.md`). The Android app is a second place to try the same destinations: same servers and keys from a different device, network (mobile data as well as Wi-Fi) and program. Keep its log of what each server answered in the same plain form as the desktop's "What HopCast saw", so results from Linux, Windows and Android can be compared side by side. Compare first; don't patch on a guess. Each platform has its own way of accepting streams, so meet it on its terms.
