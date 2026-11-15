# Releasing GNARL

A release is one command, run from a clean `main`:

```bash
scripts/release.sh 1.2.3 "What changed, in plain words"
```

What it does:

1. Writes `VERSION`, builds `gnarl_app_tests` locally and saves this version's saved-state fixtures into `tests/fixtures/1.2.3/` (the compatibility contract: every later release must load them).
2. Commits, creates the annotated tag `v1.2.3` (the message becomes the release notes) and pushes.
3. The **Release** workflow (`.github/workflows/release.yml`, macOS runner, `permissions: contents: write`) builds universal binaries, then runs every gate:
   - Check 1 (parameter audit), the preset gates, unit and contract tests, aliasing, CPU, and Check 3 (UI binding) via `ctest`.
   - Check 2 via `scripts/validate.sh`: pluginval at strictness 10 on all four binaries, `auval` on both AUs, and the host-level effect test through the built VST3s.

   It then packages and ad-hoc signs (`scripts/package.sh`), writes `manifest.json`, signs it with the Ed25519 key from the `GNARL_SIGNING_KEY` secret and publishes the GitHub Release with `GNARL-macOS.zip`, `GNARL-content.zip`, `manifest.json`, `manifest.json.sig` and `reports.zip`.
4. Waits for the workflow and prints the release's assets.

Options:

- `scripts/release.sh 1.2.3 "notes" --content` also bumps `CONTENT_VERSION` (new presets/wavetables shipped with a plugin release).
- `scripts/release.sh --content-only "notes"` bumps only `CONTENT_VERSION` and tags `v<VERSION>-c<N>`. The plugin binaries are unchanged; plugins in the field install the new presets and wavetables without a restart.

Factory presets are regenerated with `build/tools/gnarl_presetgen content/presets build/preset_report.md` (all must pass the gates before committing).

## Signing key

- The private key is `~/.gnarl/signing-key` (owner-only) and the GitHub secret `GNARL_SIGNING_KEY`. Back up `~/.gnarl/signing-key` somewhere safe and offline. Without it you cannot publish updates that existing installs will accept.
- The public key is compiled into `src/app/Updater.h` (`kUpdatePublicKey`).
- If the key is ever lost: generate a new pair with `gnarl_sign keygen`, put the new public key in `Updater.h`, set the secret again and release. Existing installs must then be updated once with `install.sh`, because they only trust the old key.

## Compatibility rules (enforced by tests)

- Never change the plugin codes (`Gnrl`, `Gsyn`, `Gfxp`) or bundle IDs.
- Never rename or reuse a parameter ID. Add new parameters at the end of the registry, with defaults that leave old patches sounding the same.
- Bump `kPatchSchemaVersion` and add a migration in `migratePatchJson` whenever the preset JSON layout changes.
