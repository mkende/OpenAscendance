# Releasing

How the packages of Open Ascendance are built, signed and published, the
one-time setup it relies on, and the steps of each release (adapted from
SilentHeritage's).

The workflows are in `reimpl/.github/` while the remake lives in the
reverse-engineering repository: GitHub only runs the workflows of a
repository's root `.github/workflows`, so they are inactive until the remake
is split into its own repository, where they work unchanged (their paths
are relative to the remake's root).

## How it works

- `ci.yml`, on the pushes to `main` and the pull requests: the tests (Linux
  debug build with the sanitizers, and macOS) and every package built as a
  check (`build.yml`). It has no secret.
- `release.yml`, on a pushed tag `vMAJOR.MINOR.PATCH[-suffix]`:
  1. checks the tag;
  2. builds every package, unsigned (`build.yml`, no secret);
  3. signs the Android package in a job of the `release` environment, the
     only one with the release key, which runs no build
     (`android/sign_apk.sh`);
  4. creates a **draft** release: the packages, `SHA256SUMS`, GitHub's build
     provenance attestations of every file, the notes from the version's
     section of `CHANGELOG.md` (empty without one: write them in the draft)
     and the Android version code.
  Nothing is public until the draft is published by hand.

The packages:

- **Android 7.0+** (arm64-v8a, x86_64): `OpenAscendance-<version>-android.apk`,
  by `android/build_apk.sh --unsigned`, then `android/sign_apk.sh`.
- **Linux** x86_64 and aarch64 (glibc 2.35+):
  `OpenAscendance-<version>-linux-<arch>.tar.gz` and `.AppImage`, by
  `packaging/build_desktop.sh linux` in Ubuntu 22.04.
- **Windows** x64: `OpenAscendance-<version>-windows-x64.zip`, by
  `packaging/build_desktop.sh windows` (mingw-w64 cross build).
- **macOS 13.3+** (Apple Silicon, Intel): `OpenAscendance-<version>-macos.dmg`,
  by `packaging/build_desktop.sh macos` on macOS 15.

Versions: the version name is the tag without its `v`; the Android version
code is `release.yml`'s run number, so that it grows with every release,
pre-releases included. Keep the workflow's file name: a renamed workflow
counts from 1 again. Re-running a failed release keeps its run number.

## One-time setup

### 1. The Android release key

Every update of an Android app must be signed with the same key as the
installed one: losing the key means no more updates. Make it on your own
computer:

    keytool -genkeypair -keystore openascendance-release.p12 -storetype PKCS12 \
      -alias openascendance -keyalg RSA -keysize 4096 -validity 18250 \
      -dname "CN=<your name>"

with a long random password (e.g. `openssl rand -base64 24`). Note its
SHA-256 fingerprint (`keytool -list -v -keystore openascendance-release.p12`).
Keep two offline copies of the keystore and the password in your password
manager; never commit it (`.gitignore` excludes `*.p12`, `*.keystore`,
`*.jks`).

### 2. The `release` environment

Settings → Environments → New environment, named `release`:

- Deployment branches and tags: a **Tag** rule, pattern `v*`.
- Environment secrets: `ANDROID_KEYSTORE_BASE64` (`base64 -w0
  openascendance-release.p12`), `ANDROID_KEY_ALIAS` (`openascendance`),
  `ANDROID_KEYSTORE_PASSWORD`, and `ANDROID_KEY_PASSWORD` only for a key
  whose password differs.

Do not add the key as a repository secret: those reach every workflow.

### 3. The repository's settings

As for SilentHeritage: a tag ruleset restricting the creation, update and
deletion of `v*` tags to the administrators; Actions limited to GitHub's
own (the workflows use only `actions/*`, pinned to a commit), approval
required for external contributors' workflows, read-only workflow
permissions; Dependabot (`.github/dependabot.yml`) and secret scanning on.
The Linux aarch64 packages build on `ubuntu-24.04-arm` runners, free for
public repositories.

### 4. Android developer verification

Register the package `io.github.mkende.openascendance` with the release key's
SHA-256 fingerprint in the Play Console (Android developer verification),
needed for packages installed outside Google Play on certified devices.

## A release

1. Optionally, a `CHANGELOG.md` section for the version, e.g.
   `## [1.0.0] - 2027-01-02`: the release's notes.
2. Commit and push to `main`; wait for `ci` to pass.
3. Tag and push the tag:

       git tag -a v1.0.0 -m "Open Ascendance 1.0.0"
       git push origin v1.0.0

4. Follow the `release` run in the Actions tab.
5. On the Releases page, check the draft: the files (1 APK, 2 tarballs, 2
   AppImages, 1 zip, 1 disk image, `SHA256SUMS`), the notes, the
   pre-release mark for a suffixed version
   (`apksigner verify --print-certs <apk>` shows the release key).
6. Publish.

A failed run: fix the cause, then either re-run the failed jobs (same
version code), or delete the tag and the draft and tag again (new version
code).
