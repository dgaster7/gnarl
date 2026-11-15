# GNARL architecture

## Targets

| Target | Kind | Contents |
|---|---|---|
| `gnarl_dsp` | static library, C++20, **no JUCE** | parameter registry, all DSP (oscillators, Shaper, filters, distortion, FX modules), modulation engine, voice + synth engine, FX engine, patch structure, factory wavetable generator, JSON patch I/O |
| `gnarl_app` | CMake INTERFACE source set (JUCE) | `AudioProcessor` base shared by both plugins, editor + all UI components, preset manager/browser, Mutate, updater. JUCE modules cannot be linked into a static library *and* a plugin without duplicate symbols, so this layer is compiled into each JUCE target. |
| `GNARL` | `juce_add_plugin` instrument | VST3, AU, Standalone |
| `GNARL_FX` | `juce_add_plugin` effect | VST3, AU (sidechain input, MIDI input) |
| `gnarl_tests` | Catch2 executable (links `gnarl_dsp` only) | unit tests, Check 1 parameter audit, preset gates, CPU benchmark |
| `gnarl_app_tests` | JUCE console app | UI binding audit (Check 3), state/preset round trips through the real processor, fixture loading, updater tests |
| `gnarl_render` | CLI | headless offline render of a patch to WAV / analysis JSON |
| `gnarl_host` | JUCE console app | headless VST3/AU host: loads the *built* plugin binary, runs the host-level effect test (Check 2) and drives the updater end to end |
| `gnarl_sign` | CLI (Monocypher) | Ed25519 keygen / sign / verify for release manifests |

## Threads

- **Audio thread**: `SynthEngine::process` / `FxEngine::process`. No allocation, locks, file/network I/O or logging. Denormals flushed (FTZ/DAZ via `ScopedNoDenormals` equivalent in `gnarl_dsp`).
- **Message thread**: UI, preset load/save, patch-structure edits.
- **Background thread**: updater network + file work, wavetable import, preset scans.

Host parameters live in JUCE `AudioProcessorValueTreeState` atomics. Each block the processor copies every raw parameter value into the engine's flat `float` array (lock-free atomic loads, index = registry index).

Non-parameter patch data (mod-route sources/destinations/curves, LFO and envelope drawings, step-sequencer steps, draw-distortion curves, FX slot order, wavetable selections, macro names) is a `PatchStructure`. Edits build a new immutable snapshot on the message thread and publish it through `SnapshotExchange` (atomic pointer + generation acknowledgement; retired snapshots are freed on the message thread only after the audio thread has acknowledged a newer generation). The audio thread never frees memory.

## Signal flow (synth voice)

```
OSC A ┐  (each: engine → Shaper → band-limit → unison/Multi → pan)
OSC B ├─ chain A→B→C→A (Add/Ring/Sync/PM/FM/AM per link)
OSC C ┘        │ per-osc balance F1↔F2, direct-FX send
NOISE ─────────┤
               ▼
   [voice oversampling ×1/2/4/8: polyphase IIR half-band]
   Filter 1 (drive pre/post) ─┐  Serial: F1 → saturation slot → F2
   Filter 2 (drive pre/post) ─┘  Parallel: F1 + F2 (both fed by both buses)
                                 Split: bus1→F1, bus2→F2
               ▼ downsample
          amp envelope × velocity → voice pan
               ▼
 sum voices + direct-FX send → mono post drive (optional) → FX chain (16 slots) → final clipper
 SUB (per voice, clean, mono) ─── default "bypass filters+FX": added after FX chain, before clipper
```

### Oscillators: one band-limited cycle path

Virtual analog, Wavetable and Grain/Formant engines all reduce to a single-cycle table (2048 samples):

1. Generate the raw cycle (VA shape morph, interpolated wavetable frames, or the grain source frame).
2. Apply the wavetable **warp** (phase-domain, on the cycle) and the **Shaper** stack (18 transforms) on the cycle.
3. **Re-band-limit**: real FFT of the cycle (Accelerate vDSP), zero every harmonic above `floor(0.45·fs / f0_max)`, inverse FFT.

Re-band-limiting is realtime-safe: every buffer is preallocated in `prepare`, FFT setups are created in `prepare`, and the rebuild only runs when its inputs changed (dirty check on the quantised inputs, at most once per 64-sample tick per oscillator per voice). Each rebuild crossfades from the previous table to the new one over the next 64 samples, so modulating any Shaper amount is click-free. Pitch changes only trigger a rebuild when the note crosses into a new half-octave band-limit band. Playback uses 4-point Hermite interpolation. Hard sync and chain-Sync phase resets apply a polyBLEP correction to the step at the fractional reset position.

HyperSaw and FM run directly (polyBLEP saws; through-zero phase-accumulator FM at 2× internal rate). Harmonic runs as an additive bank of up to 128 complex-rotator partials; partials above Nyquist are dropped.

## Modulation

- Processing in **sub-blocks of 16 samples**. At each sub-block boundary all sources are advanced, the matrix is evaluated, and every routed destination gets an end value; DSP linearly interpolates from the previous value across the sub-block (no zipper on cutoff, pitch or FM).
- Modulation is applied in the destination's normalised (0..1) domain: `norm = clamp(base + Σ depth·curve(src)·via)`. Fixed-pitch curves (±1 st, ±1 tone, ±1 oct, ±4 oct) add an exact semitone offset for Hz/semitone destinations.
- Per-voice sources (envelopes, LFO 5–6, velocity, key, per-note random, osc outputs) drive per-voice destinations. Global destinations (FX, global LFO rates, macros-as-targets) read per-voice sources from the most recently triggered voice.
- Route depth is a host parameter (so it can be automated and modulated); route source/destination/via/curve/bypass are patch structure.

## Parameters

`gnarl_dsp/params/Registry` is the single source of truth: ID, name, range, default, unit, skew, smoothing time, role (Mix / Amount / Drive / Depth / Send / Level / Neutral / Choice / Toggle / Other), and an *activation context* (other parameter values needed for it to be audible). From it we generate: the JUCE parameter layout, `docs/PARAMETERS.md`, the Check 1 test list, the mod-destination list and the UI binding audit. IDs are append-only and never reused (see CLAUDE.md).

## FX chain

25 module instances exist permanently (two Distortions). Sixteen slots each reference one module (or are empty); order is patch structure, drag-to-reorder in the UI. Every module has `on` + `mix`. Modules with nonlinearities oversample with linear-phase FIR (integer latency) and delay their dry path by the same amount; the engine reports `Σ latency of enabled modules in slots`. Disabled modules and empty slots are skipped entirely (bit-transparent). A fixed Clipper/Limiter is the final safety stage after the slots.

GNARL FX = the same `FxEngine` plus the filter pair (as an insert section), the full modulation system, envelope follower and transient sources from the input, MIDI for envelopes/key-track, sidechain input (used by the Vocoder and the follower source switch).

## Presets

JSON, `schemaVersion`, migration functions per version. Factory bank inside the plugin (BinaryData) and mirrored into `~/Library/Application Support/GNARL/Factory`, content updates into `~/Library/Application Support/GNARL/Content/<contentVersion>`, user bank in `.../User` (never touched by updates).

## Updater

GitHub Releases of the public repo, `manifest.json` signed with Ed25519 (public key compiled in, private key only in the GitHub secret `GNARL_SIGNING_KEY` and `~/.gnarl/signing-key`). Background thread only. Plugin update = download → SHA-256 verify → unzip to temp → move current bundles to `~/Library/Application Support/GNARL/Backup/<version>` → move new into `~/Library/Audio/Plug-Ins/...` → verify signature/quarantine → roll back on any failure. Takes effect on next host start.
