# GNARL and GNARL FX

**GNARL** is a bass-design synthesizer, and **GNARL FX** runs its filters, effects and modulation over any audio. They're built for squelchy, vocal, tearing, liquid, alien basses: glitch hop, psychedelic bass, neuro, halftime and drum & bass.

macOS 11 or newer, Apple Silicon and Intel. VST3 and Audio Unit.

## Install (one line)

Open **Terminal** (press Cmd+Space, type `Terminal`, press Return), paste this and press Return:

```bash
curl -fsSL https://raw.githubusercontent.com/dgaster7/gnarl/main/install.sh | bash
```

It downloads the latest release and checks every file against the release's signed manifest (SHA-256). It then installs both plugins in both formats into your own plug-in folders (no admin password needed) and adds the factory presets and wavetables. Finally it tests both Audio Units with Apple's `auval`. You can run it again at any time; it simply reinstalls the latest version.

Installed files:

- `~/Library/Audio/Plug-Ins/VST3/GNARL.vst3` and `GNARL FX.vst3`
- `~/Library/Audio/Plug-Ins/Components/GNARL.component` and `GNARL FX.component`
- Presets and wavetables: `~/Library/Application Support/GNARL/`

Your own presets are saved in `~/Library/Application Support/GNARL/User/`, and updates never touch them.

## Make your DAW see GNARL

- **Ableton Live:** Settings > Plug-Ins > turn on *Use VST3 Plug-In User Folder* and *Use Audio Units*, then click **Rescan**. GNARL is under Plug-Ins > GNARL Audio.
- **Bitwig Studio:** Settings > Locations > Plug-ins: make sure *~/Library/Audio/Plug-Ins/VST3* is ticked, then click **Rescan**. Search for "GNARL" in the browser.
- **Logic Pro:** quit Logic and open it again. It scans new Audio Units on launch. GNARL is under *AU Instruments > GNARL Audio*; GNARL FX is under *Audio FX* and *MIDI-controlled Effects*. If it is missing, open *Logic Pro > Settings > Plug-in Manager* and click *Reset & Rescan Selection*.

## Updates

GNARL checks for updates once a day in the background, and never while you're playing. When a new version exists, an **Update** badge appears in the top bar. Choose what happens in the **SETTINGS** tab:

- **Updates off**: GNARL never checks.
- **Notify me** (default): the badge appears; click **Install update** when you like.
- **Install automatically**: new presets and wavetables appear straight away; new plugin versions install on their own.

New presets and wavetables show up immediately. A new plugin version is installed to disk and takes effect **the next time you start your DAW**. A running session is never replaced underneath you. Every download is verified (Ed25519 signature on the manifest, SHA-256 on each file); an update that fails verification is refused and nothing changes.

**Roll back:** SETTINGS > **Roll back**. GNARL keeps the previous version and swaps it back in (restart your DAW). Clicking again rolls forward.

## Uninstall

Paste into Terminal:

```bash
rm -rf ~/Library/Audio/Plug-Ins/VST3/GNARL*.vst3 ~/Library/Audio/Plug-Ins/Components/GNARL*.component
```

To also delete presets, wavetables and settings (including your own presets!):

```bash
rm -rf "$HOME/Library/Application Support/GNARL"
```

## A quick tour

- **Top bar:** preset name (click to browse), previous/next, A/B compare, undo/redo, **Mutate** (random variations within safe ranges), master level, meter, size (75-200%).
- **OSC:** three oscillators, each with six engines (Analog, HyperSaw, Wavetable, FM, Grain/Formant, Harmonic), unison, Multi mode and the Shaper. The **Chain** tab links A > B > C > A (Add, Ring, Sync, PM, FM, AM). Sub and noise are there too.
- **FILTER:** two filters (Serial, Parallel or Split) with saturation between them, including Tri-Peak, Dual-Reso, Separation LP, Formant, Comb, Ladder and Phaser. Drag the dots on the curve.
- **MOD:** envelopes (drawable), LFOs (drawable, tempo-synced), step sequencers, random sources, functions, follower.
- **Modulating anything:** drag a coloured source tab (an LFO, an envelope, a macro, Velocity, Mod Wheel...) onto any knob. Drag the coloured ring on the knob to set the depth, alt-click the ring for bipolar, double-click the ring to zero it, right-click the knob to list, bypass or remove its modulations. The **MATRIX** tab shows all 64 routes.
- **FX:** 16 slots; drag to reorder, click a dot to switch a module on or off, right-click a slot to add a module. The final clipper is always last.
- **Macros:** the eight knobs at the bottom; double-click a name to rename it.
- Every knob: double-click to type a value, shift-drag for fine control, alt-click to reset, hover for a description with units.

## About the factory presets

All 200 factory presets (160 GNARL, 40 GNARL FX) were **designed by measurement, not by ear**. Levels are calibrated from rendered loudness and peaks, and each preset passes automated checks: it makes sound, its tail is not too long, it does not clip, it has no DC offset, its low end is mono, every macro and the mod wheel does something, and it is not a near-duplicate of another preset. See `docs/PRESET_REPORT.md`. Audition them and keep what you like.

## For developers

Build: see `CLAUDE.md`. Releasing: `docs/RELEASING.md`. Manual QA checklist: `docs/MANUAL_QA.md`. Architecture: `docs/ARCHITECTURE.md`. Every parameter: `docs/PARAMETERS.md`.

GNARL is built with the JUCE framework, used under JUCE's free **Starter** licence (allowed while annual revenue or funding is under $20,000; above that an Indie or Pro licence is needed). See `docs/FINAL_REPORT.md`.
