# GNARL: final report

Built from scratch in one run on 15 November 2026. Every number below was measured in this session; nothing here is an estimate unless it says so.

## Download and install

- Download page: **https://github.com/dgaster7/gnarl/releases/latest**
- Install or update on any Mac (paste into Terminal):

```bash
curl -fsSL https://raw.githubusercontent.com/dgaster7/gnarl/main/install.sh | bash
```

## What is installed on this Mac right now

| Item | Version | Location |
|---|---|---|
| GNARL (VST3, universal arm64 + x86_64) | 1.0.2 | `~/Library/Audio/Plug-Ins/VST3/GNARL.vst3` |
| GNARL FX (VST3) | 1.0.2 | `~/Library/Audio/Plug-Ins/VST3/GNARL FX.vst3` |
| GNARL (AU) | 1.0.2 | `~/Library/Audio/Plug-Ins/Components/GNARL.component` |
| GNARL FX (AU) | 1.0.2 | `~/Library/Audio/Plug-Ins/Components/GNARL FX.component` |
| Presets and wavetables | content version 2 (205 synth/FX presets + 64 wavetables) | `~/Library/Application Support/GNARL/Content` |
| Rollback copy | 1.0.1 | `~/Library/Application Support/GNARL/Backup/1.0.1` |

Releases published: [v1.0.0](https://github.com/dgaster7/gnarl/releases/tag/v1.0.0), [v1.0.1](https://github.com/dgaster7/gnarl/releases/tag/v1.0.1), [v1.0.2](https://github.com/dgaster7/gnarl/releases/tag/v1.0.2), and the content-only update [v1.0.2-c2](https://github.com/dgaster7/gnarl/releases/tag/v1.0.2-c2). Each one passed every check in GitHub Actions before publishing.

Verified from the installed copies just now:
- pluginval at strictness 10 passes on all four binaries.
- auval passes for both AUs.
- The host-level effect test passes 854/854 (GNARL) and 528/528 (GNARL FX).
- All four bundles are universal (arm64 + x86_64), ad-hoc signed and verified, with no quarantine flag.

The User preset folder (`~/Library/Application Support/GNARL/User`) is untouched by installs and updates.

## Test results

| Check | Result |
|---|---|
| **Check 1: parameter audit** (`build/parameter_audit.md`) | **865 / 865 parameters pass.** Each parameter gets six tests: effect at min/default/max/mid; zero = stage removed (−120 dBFS, bit-exact in practice); full-wet with the dry path silenced; a 50 ms automation sweep with no discontinuity; preset JSON round trip; and as a modulation destination, both full-depth effect and depth-0/bypass equal to no route. |
| Check 1: range test | Random values on all parameters, 64 random routes, all 24 modules in the chain, at 44.1, 48 and 96 kHz with random block sizes from 1 to 2048 samples. No NaN or infinity, output bounded at 0 dBFS, no denormal slowdown. |
| **Check 2: pluginval** strictness 10 | **PASS on all four binaries** (GNARL and GNARL FX, VST3 and AU), in CI with the editor tests and locally from the installed copies. |
| Check 2: auval | **PASS** for `aumu Gsyn Gnrl` and `aumf Gfxp Gnrl`, from the build and from the installed copies after every update and rollback. |
| Check 2: host-level effect test (`gnarl_host`) | **GNARL 854 / 854, GNARL FX 528 / 528.** Every parameter is set through the host API of the built (and installed) VST3 and changes the audio. |
| **Check 3: UI binding audit** | **PASS.** Every registered parameter is reachable from exactly one control, in both plugins, with no orphan controls. UI value, host value and DSP value agree after host automation, preset load, undo, and resizing to 75% and 150%. |
| Contract tests | Mono below X is really mono (side more than 20 dB down below X, stereo kept above). Dry/wet of latency modules is bit-exact at 50%. Neutral width is bit-identical. |
| Aliasing (`docs/ALIASING.md`) | Non-harmonic energy at C7: about −88 to −97 dB for every table, HyperSaw, Harmonic, FM and Grain engine and the Shaper, analog and HyperSaw hard sync included (computed inside the band-limited cycle). Oscillator-chain Sync link (BLEP): −64 dB. |
| Filters (`docs/filters/`) | Measured impulse-response plots for all 19 models, plus a Tri-Peak Separation sweep, Tri-Peak shoulders, and Dual-Reso together vs opposing. The display curve matches the measurement within 0.12 dB. |
| **Preset gates** (`docs/PRESET_REPORT.md`) | **205 / 205 pass**: 160 synth + 40 FX factory presets plus the 5-preset content update. |
| CPU (`build/cpu_benchmark.md`) | Per preset, one held note, 48 kHz, 256-sample blocks, 4× oversampling, one core of this machine (Apple M2 Max): **median 4.4%, max 6.5%, none over 25%.** An M1 is roughly 20–25% slower per core, so expect a median of about 5.5% there (estimate, not measured). |
| Saved-state fixtures | `tests/fixtures/1.0.0`, `1.0.1` and `1.0.2` load in CI. A 1.0.0 project state recalls with 0 mismatches (854 + 528 parameters) in 1.0.1 and 1.0.2 through the installed VST3s. |
| Updater, end to end | A plugin instance's own updater, running inside the installed plugin and loaded by a headless host, updated 1.0.0 → 1.0.1. The table below shows the full sequence. |

### Update proof (this Mac, installed copies)

| Step | Installed afterwards | Backup | 1.0.0 saved state recall |
|---|---|---|---|
| 1.0.0 plugin installs update | 1.0.1 | 1.0.0 | 0 mismatches |
| Roll back | 1.0.0 | 1.0.1 | 0 mismatches |
| Roll forward | 1.0.1 | 1.0.0 | 0 mismatches |
| 1.0.1 plugin installs v1.0.2 | 1.0.2 | 1.0.1 | 0 mismatches |
| Content-only update (v1.0.2-c2) | 1.0.2 (plugin untouched) | 1.0.1 | presets 1 → 2; 5 new presets present; a User preset stayed byte-identical (SHA-256) |

After every step: code signatures valid, no quarantine flag, auval passes for both AUs.

## Bugs the checks found and that were fixed during the run

- The full audit initially failed 163 parameters. The causes:
  - Global LFOs, random sources and sequencers read their own modulated parameters a block too early, so modulation of them was lost.
  - FM operator levels stepped every 16 samples (zipper noise).
  - Resonator coefficients stepped.
  - Several "removed stage" semantics were wrong.

  All fixed; 865 / 865 now pass.
- Every distortion stage now removes DC from its output. Even symmetric curves create DC from asymmetric waveforms, which the preset gates caught on 109 presets.
- "Mono below X Hz" was not truly mono near X. It now uses an 8th-order side high-pass placed above X.
- Step sequencers froze when the DAW transport was stopped. The host-level test caught this; they now run on their own clock.
- pluginval found a crash when a DAW restores state from a background thread while the window is open. Fixed with thread-safe patch swapping.
- The updater could reinstall over its own backup when two GNARL instances were open. It now compares with the version installed on disk (fixed in 1.0.2).
- High notes on wavetables whose energy sits only in upper harmonics went silent. Fixed with quarter-octave band-limit steps in 1.0.0.
- Two parameters shared the display name "Osc X Warp". Renamed to "WT Warp" in 1.0.2, and a test now enforces unique names.

## Cut, changed or done differently (and why)

- **"Shared code in one static library."** The DSP, modulation, parameter registry and preset format are one JUCE-free static library (`gnarl_dsp`). The JUCE-dependent UI, preset browser and updater are compiled into each plugin from one shared source set, because JUCE modules cannot be linked as a static library into two plugins without duplicate symbols.
- **FX slots.** There are 24 module instances, two of them Distortion, and 16 slots that each hold one module. The same module cannot appear twice except the two Distortions. The 3-Band Split holds five fixed mini-modules per band (filter, phaser, distortion, comb, crush) whose order you set. It is not a free nested chain.
- **Draw distortion.** There is one pair of drawn curves per patch, shared by every stage set to "Draw". Only the two FX Distortion modules have a Draw Morph knob; other Draw stages use curve 1.
- **Oscillators as modulation sources** update every 16 samples, about 3 kHz at 48 kHz. Audio-rate interaction between oscillators goes through the oscillator chain (Ring, Sync, PM, FM, AM), which is per-sample.
- **Modulation** is evaluated every 16 samples with linear interpolation, as the brief allows. Global LFOs see their own modulated parameters one 16-sample block late.
- **Grain/Formant "Simple" mode** keeps the fundamental audible by adding a sine at the note pitch under the grains. "Complex" plays the grains alone with shorter windows.
- **PWM Squeeze "auto detune"** lowers the oscillator pitch by the squeeze factor so the squeezed segment keeps its pitch, which sounds like classic PWM.
- **Post drive** is a mid-only (mono) drive after the voice mix; the stereo side passes untouched.
- **GNARL FX** has no output DC blocker, so neutral settings stay bit-transparent; its nonlinear modules block their own DC. The synth output has a 5 Hz DC blocker.
- **The automation click test** fails a sweep only when it shows a discontinuity: an HF burst plus a transient second-difference spike. A swept resonance legitimately produces a smooth chirp, which is not a click. The method is written into the audit report.
- **Processor state round trip** is exact through the preset JSON. Through JUCE's host parameters it is identical within one or two float ulps (≤ 2 × 10⁻⁶ normalised), because JUCE stores denormalised values.
- **Presets were gated all together** (the gates run in parallel), not in literal batches of 20.
- **pluginval's editor tests** open plugin windows on screen. On this Mac they now run with `PLUGINVAL_SKIP_GUI=1`; the full GUI tests run in GitHub Actions on every release.
- **No Apple Developer ID.** Everything is ad-hoc signed, as you asked. Gatekeeper never sees the plugins because `install.sh` and the updater clear the quarantine flag.

## Known limitations

- The presets were **designed by measurement, not by ear.** Levels, tails, low-end mono, macro and mod-wheel response and distinctness are verified; whether they sound good is for you to judge.
- Hard sync between two oscillators through the **chain Sync link** still has some aliasing at high pitches (−58 to −64 dB). Analog and HyperSaw sync are exact.
- FM at very high notes with high modulation index aliases more than the table engines (−66 dB at C5 with modulators at 0.5).
- The VST3 exposes JUCE's 2,048 hidden MIDI-CC parameters, which is how VST3 hosts deliver mod wheel and pitch bend. A few hosts list them in automation menus.
- A new plugin version installs to disk and takes effect the next time the DAW starts, by design.
- `install.sh` checks SHA-256 hashes against the release manifest. The plugin's built-in updater additionally verifies the Ed25519 signature. The shell installer does not, because macOS has no built-in Ed25519 tool.

## The 20 presets to audition first

These have the most extreme measurements: movement over time, macro range and drive.

1. Molten Vortex (Neuro-Reese)
2. Glass Mouth (Neuro-Reese)
3. Ghost Tide (Talking-Formant)
4. Black Reese (Neuro-Reese)
5. Lunar Grinder (Neuro-Reese)
6. Iron Engine (Neuro-Reese)
7. Elastic Hive (DnB)
8. Acid Gear (Talking-Formant)
9. Cryo Throat (Neuro-Reese)
10. Elastic Chirp (Neuro-Reese)
11. Hydra Wire (Talking-Formant)
12. Vapor Engine (Talking-Formant)
13. Venom Gear (DnB)
14. Tectonic Wraith (Glitch-Rhythmic)
15. Iron Spine (Talking-Formant)
16. Hollow Mutter (Talking-Formant)
17. Bent Throat (DnB)
18. Black Gremlin (Talking-Formant)
19. Wet Gear (Talking-Formant)
20. Hydra Vortex (Glitch-Rhythmic)

## Signing key

- The private key is in **`~/.gnarl/signing-key`** (owner-only permissions), and the GitHub secret is `GNARL_SIGNING_KEY`. The public key is compiled into the plugin (`src/app/Updater.h`).
- **Back up `~/.gnarl/signing-key` now** to a password manager or an offline drive. Without it you cannot publish updates that installed copies will accept. If it is lost, see `docs/RELEASING.md`; every install would need to be refreshed once with `install.sh`.

## JUCE licence

JUCE 9.0.5 is dual-licensed: AGPLv3, or the JUCE 9 EULA. Under the EULA the **Starter licence is free while annual revenue or funding is up to $20,000**, so it applies to a one-person project with no revenue yet. The paid Indie licence ($40/month or $800 perpetual) covers up to $300,000, and Pro has no limit. You must move to Indie before GNARL earns more than $20,000 a year. Because this repository is public, the AGPLv3 route is also open to you, but it would oblige you to keep GNARL's source published under the AGPL. This is not legal advice; JUCE's licensing FAQ is the authority.

## Where things are

| What | Where |
|---|---|
| Progress log | `PROGRESS.md` |
| Build notes | `CLAUDE.md` |
| Architecture | `docs/ARCHITECTURE.md` |
| Every parameter | `docs/PARAMETERS.md` |
| Releasing | `docs/RELEASING.md` |
| Your 25-minute checklist (dry/wet and bypass first) | `docs/MANUAL_QA.md` |
| Plain-language guide | `README.md` |
