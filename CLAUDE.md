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
