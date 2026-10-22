# Working on HopCast (notes for AI-assisted sessions)

The owner's standing rules for this project. Follow them every session.

## Releases

- **Every release has every platform.** When a version is released, the Linux build, the Windows build and (once it exists) the Android build all get that same version, built from the same commit, and all are attached to the same GitHub release. Versions are bumped together and updated together; a platform is never left on an older version, even if only one platform changed.
- **Platform priority: Linux first, Android second, Windows last.** This is the order for building, fixing and releasing. When time or effort is short, Linux comes before Android, and Android before Windows. Until Android exists, that means Linux, then Windows.
- Linux: the release workflow (`hopcast-app.yml`) with `package=true`, `release_tag=vX.Y.Z`, `prerelease=false`.
- Windows: `hopcast-windows.yml` with the same `release_tag`, then `hopcast-release-notes.yml` so the release text carries the Windows notice.
- Android: not built yet; when it exists it joins the same release and the same rule.
- Never publish a release without asking the owner first. One yes covers all platforms of that version.
- Do not publish to the AUR, Flathub or F-Droid. Those are hand-offs only (`PACKAGING.md`).

## Branches and pull requests

- Pushing branches and merging pull requests into `main` does not need asking; tell the owner the result afterwards.
- One pull request per change. Don't open extra pull requests for branches already covered by another one.
- `dev` has unrelated history from `main`. Do not push to it.

## Golden rule

No nudges: no quick links, recommendations or invitations toward any platform or community, and no viewer counts, follower numbers, rankings or metrics. This applies to every Gamehopper app and to package descriptions.

## Android test device

The owner tests the Android app on a RedMagic 8 Pro (Snapdragon 8 Gen 2, 12 to 16 GB RAM, 6.8 inch AMOLED at 2480x1116 and 120 Hz, no camera cutout because the front camera is under the display, built-in cooling fan, Android 13 at launch). Design and tune for that phone first: hardware video encoding (H.264 and HEVC), a long gaming session on a sustained load, and a tall 20:9 screen. Keep it working on older and slower phones too, but this is the one that gets tested.

## Android as a second test point for streaming problems

Streaming to Rumble and Odysee is still refused, and a multi-destination stream once dropped the owner's Wi-Fi (see `KNOWN-ISSUES.md`). The Android app is a second place to try the same destinations: same servers and keys from a different device, network (mobile data as well as Wi-Fi) and program. Keep its log of what each server answered in the same plain form as the desktop's "What HopCast saw", so results from Linux, Windows and Android can be compared side by side. Compare first; don't patch on a guess. Each platform has its own way of accepting streams, so meet it on its terms.
