# Nightly GitHub Release body

The text shown on the [**nightly** release](https://github.com/SanGraphic/Apotris-Android/releases/tag/nightly) is **generated in CI**, not taken from this file.

- **Workflow:** `.github/workflows/nightly-android.yml` → job `publish-release` → step **Write nightly release body**
- **Direct download URLs** (after assets are uploaded):

  - `https://github.com/SanGraphic/Apotris-Android/releases/download/nightly/apotris-arm64-v8a.apk`
  - `https://github.com/SanGraphic/Apotris-Android/releases/download/nightly/apotris-universal.apk` (scheduled nightlies always attempt this; manual runs only if enabled, or missing if that job failed)

Edit the shell block in that step to change wording or badges.
