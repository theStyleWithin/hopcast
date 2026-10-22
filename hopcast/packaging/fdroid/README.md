# F-Droid hand-off (for HopCast for Android)

**Status: placeholder.** The Android app is still only a design idea, so there is nothing to
package yet. These files are a ready-to-fill starting point so that when an Android release exists,
whoever wants it on F-Droid has everything in one place.

The author does **not** publish to F-Droid and won't submit, maintain or support the F-Droid
entry. Whoever submits it is its maintainer. See `PACKAGING.md` at the top of the source for the
full hand-off policy (it is the same as for the AUR and Flatpak).

## What is here

| File | For |
|---|---|
| `io.github.theStyleWithin.hopcast.android.yml.in` | the F-Droid build recipe for the `fdroiddata` repository (fill in the `@...@` marks) |
| `metadata/en-US/short_description.txt` | one-line description |
| `metadata/en-US/full_description.txt` | long description |
| `metadata/en-US/changelogs/` | one file per version code, named `<versionCode>.txt` |

## How to use it, once an Android release exists

1. Replace each `@...@` mark in the `.yml.in` file (version name, version code, commit, folder of the Gradle project) and save it as `metadata/io.github.theStyleWithin.hopcast.android.yml` in your `fdroiddata` fork.
2. Build it from source the way F-Droid does (`fdroid build -v -l <app id>`) and test the result on a real phone. Nothing here has been run.
3. Check the app's own `AndroidManifest.xml` and dependencies against F-Droid's inclusion rules: free licenses only, no trackers, no non-free libraries.
4. Open a merge request to `fdroiddata` and answer the reviewers yourself.

## Things the maintainer should know

- HopCast has no account, no telemetry and sends nothing to the author, so no `Anti-Features` should be needed. Add one if a dependency requires it.
- The app captures the screen (Android's MediaProjection) and needs the permissions to do so. These are used for streaming and recording on the phone only.
- The license is GPL-2.0-or-later. Keep the notices and keep the source available.
- Golden rule: no quick links, recommendations or invitations toward any platform or community, and no viewer counts or follower numbers. The project won't accept changes that add them, including in a package's description.
