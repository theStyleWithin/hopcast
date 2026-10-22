# Packaging HopCast (hand-off)

The author does **not** publish HopCast to the AUR, Flathub or any other package source, and won't
submit, maintain or support those packages. Anyone who wants HopCast there is welcome to package it:
everything needed comes with each release, ready to unpack and examine first.

Whoever submits a package is its maintainer. Packages made by others are community packages and are
not endorsed or supported by the author.

## What each release contains

| File | For |
|---|---|
| `HopCast-release-<ver>-x86_64.AppImage` | the normal build (what the packages below repackage) |
| `HopCast-debug-<ver>-x86_64.AppImage` | a build that writes a crash log; not for packaging |
| `hopcast-source-<ver>.tar.gz` | the complete corresponding source (GPL-2.0), with build scripts in `hopcast/scripts/` |
| `hopcast-bin-aur-<ver>.tar.gz` | `PKGBUILD` and `.SRCINFO` for an AUR package named `hopcast-bin` |
| `hopcast-flatpak-<ver>.tar.gz` | a Flatpak manifest, AppStream metadata and notes (an untested template) |
| `SHA256SUMS` | checksums of every file above |

## F-Droid (Android, later)

HopCast for Android is still only a design idea. Placeholder files for an F-Droid entry live in
`hopcast/packaging/fdroid/` (a build recipe template and store descriptions), with a README that
says how to fill them in. The same policy applies: the author won't submit or maintain it.

## Before you publish

1. Verify the files against `SHA256SUMS` and read the source tarball. Nothing is hidden.
2. Build and test the package yourself. The AUR files are a template; the Flatpak manifest has not been run.
3. Flathub usually wants a build from source and screenshots. The manifest here repackages the AppImage because the source build needs a custom OBS engine; the source tarball and `hopcast/scripts/build-engine.sh` / `build-app.sh` show how to build it.
4. The license is GPL-2.0-or-later. Keep the notices, and keep the source available.
5. HopCast carries a golden rule: no quick links, recommendations or invitations toward any platform or community (see the README). Packages are free to be made, but the project won't accept changes that add them.
