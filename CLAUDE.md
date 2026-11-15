# GNARL — working notes for Claude Code

Read `PROGRESS.md` first; resume from the first unchecked item. Commit and push after every item.

## Identity (NEVER change)

| | Value |
|---|---|
| Manufacturer | GNARL Audio |
| Manufacturer code | `Gnrl` |
| Synth plugin code | `Gsyn` (AU type `aumu`) |
| FX plugin code | `Gfxp` (AU type `aufx`... `aumf` because it accepts MIDI) |
| Bundle IDs | `audio.gnarl.synth`, `audio.gnarl.fx` |
| GitHub | `dgaster7/gnarl` (public) |

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build build
```
Fast dev loop (native arch only): `cmake -S . -B build-dev -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo` then `cmake --build build-dev --target gnarl_tests`.

Plugins are copied by the build to `~/Library/Audio/Plug-Ins/{VST3,Components}` only when `-DGNARL_COPY_PLUGINS=ON`.

## Test

```bash
ctest --test-dir build --output-on-failure          # everything
./build/tests/gnarl_tests "[audit]"                 # Check 1 parameter audit -> build/parameter_audit.md
./build/tests/gnarl_tests "[presets]"               # preset gates -> build/preset_report.md
./build/tests/gnarl_app_tests                       # UI binding audit, state round trips, fixtures
PLUGINVAL_SKIP_GUI=1 scripts/validate.sh build/src/plugins Release   # locally: skip editor-window tests (they pop up on the desktop); CI runs them
```

## Realtime-safety rules (audio thread)

- No allocation (`new`, `std::vector` growth, `std::string`, `std::function` capture), no locks, no file/network, no logging, no `juce::String`.
- All buffers sized in `prepare()` for max block size and max oversampling.
- Cross-thread data: parameters via atomics; `PatchStructure` snapshots via `SnapshotExchange` (audio thread only loads/acks, never frees).
- Flush denormals (`gnarl::ScopedFlushDenormals`) at the top of every process call.
- Process in sub-blocks of 16 samples; modulated values interpolate linearly inside the sub-block.

## Parameter contract (section 10 of the brief — enforced by tests)

1. Minimum means off where off is meaningful. Mix 0% = dry exactly, 100% = wet only. Amount/drive/depth/send at 0 = zero effect. Bypassed module / empty slot = bit-transparent.
2. Dry and wet latency-aligned (modules with latency delay their dry path by the same integer amount).
3. Level at minimum = true silence (`0.0f` gain, not −120 dB).
4. A mod route at depth 0 or bypassed changes nothing (route skipped entirely).
5. All continuous parameters smoothed (smoothing time in the registry); no clicks under fast automation.
6. UI value = host value = DSP value, same units, after preset load, automation, undo, resize.
7. Width at neutral = unchanged; "mono below X Hz" really is mono below X.

## Compatibility contract

- Plugin codes and parameter IDs never change and are never reused. New parameters are **appended** to the registry with defaults that leave old patches sounding identical.
- Every release adds saved-state fixtures to `tests/fixtures/<version>/`; CI loads all of them.
- Preset JSON has `schemaVersion`; add a migration step for every bump.

## Releasing

`scripts/release.sh 1.2.3` (see docs/RELEASING.md). Private signing key: `~/.gnarl/signing-key` (also GitHub secret `GNARL_SIGNING_KEY`). Never commit it.
