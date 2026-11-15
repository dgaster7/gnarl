# GNARL manual QA checklist (about 25 minutes, Ableton Live or Bitwig)

Tick each box. If anything fails, note the step number and what you heard or saw.

## Setup (2 min)

1. [ ] Rescan plug-ins. Create a MIDI track with **GNARL** and an audio track with **GNARL FX**. Put a bass loop, or any audio, on the audio track.
2. [ ] Open GNARL. The top bar shows "Init". Hold C2: you hear a filtered saw. Release: it stops cleanly, with no click.

## Dry/wet and bypass first (8 min)

Use GNARL FX on the audio track for steps 3–9. Loop the audio throughout.

3. [ ] FX tab: right-click slot 1 > **Distortion A**. Turn **Drive** to about 60%. Set **Distortion A Mix** to **0%**: the sound is exactly the dry loop. Toggle **Distortion A On** off and on: no change and no click while Mix is 0%.
4. [ ] Set Mix to **100%**: only the distorted sound. Sweep Mix 0 → 100% slowly: a smooth blend with no hollow, phasey "comb" sound in the middle.
5. [ ] Repeat steps 3–4 with **Multiband Dist** (raise Mid Drive), **Spectral Shaper** (Gate Threshold −40) and **Resonance Tamer** (Depth 100%). These three add latency; the DAW compensates. At 50% Mix there is still no comb filtering.
6. [ ] Set **Distortion A Drive** to 0 with Mix 100%: the sound is identical to the dry loop.
7. [ ] **Width** module: Width at 1.00 changes nothing. Turn **Mono Bass** on at 150 Hz and put a correlation meter or a mono button on the master: the lows stay solid in mono.
8. [ ] Remove every module (right-click > Remove from chain), FILTER types both **Off**: the plugin is completely transparent (null test: invert the phase of a duplicate dry track; you hear silence).
9. [ ] DAW bypass button on GNARL FX on and off while playing: no clicks.

## Synth controls (7 min)

10. [ ] GNARL, OSC tab, Osc A: turn **Level** to 0. The oscillator is silent. Turn on **Sub** (Chain/Sub/Noise tab, Sub Level up): you hear only the clean sub.
11. [ ] Engine menu: try each engine (Analog, HyperSaw, Wavetable, FM, Grain/Formant, Harmonic). The waveform display and the sound change; the controls under the oscillator change with the engine.
12. [ ] Shaper: sweep **SelfSync**, **Multipoint** and **Spectrum** while holding a note. The sound changes smoothly; no crackles, even when fast.
13. [ ] FILTER: choose **Tri-Peak**. Drag the centre dot left/right (cutoff) and up/down (resonance), then drag a hollow shoulder dot (Separation). The curve and the sound follow.
14. [ ] Set Filter 1 **Env Amount** to −50% and then +50% with Env 2 Decay short: you hear downward and then upward sweeps.

## Modulation (5 min)

15. [ ] MOD > LFOs: drag the **LFO 1** tab onto Filter 1 **Cutoff**. A coloured ring appears and the sound wobbles. Drag the ring to change the depth. Double-click the ring: the wobble stops (depth 0).
16. [ ] Right-click Cutoff: the menu lists "LFO 1". Choose Bypass: the wobble stops; choose it again: it returns. Choose Remove: the ring disappears.
17. [ ] Drag **Macro 1** onto Osc A **Fold** (Shaper). Turn Macro 1: Fold follows. Double-click the Macro 1 name and rename it "Rot".
18. [ ] Drag **Mod Wheel** onto Filter 1 **Resonance** and move the wheel on your controller: resonance follows.
19. [ ] MATRIX tab: the two routes appear with source, destination and depth. Change a curve to "±1 oct" on a route to Osc A Fine: the pitch moves by an octave.

## Presets, undo, automation, state (3 min)

20. [ ] PRESETS: search "Neuro", load a few with the arrow keys, then press **<** and **>** in the top bar. Each loads without clicks or stuck notes.
21. [ ] Change three knobs, then press **Undo** three times: back to the preset. Press **Redo** once.
22. [ ] Save as "QA test" in the User bank. Load Init, then reload "QA test": identical.
23. [ ] Automate Filter 1 Cutoff in the DAW, then play: the knob moves with the automation, and the value shown in the knob tooltip matches the DAW's automation lane.
24. [ ] Save the DAW project, close it, reopen it: GNARL and GNARL FX sound exactly as before, including modulation routes and FX order.
25. [ ] Resize the window with the size menu (75%, 150%, 200%): everything scales and nothing overlaps.

## Updates (optional, 1 min)

26. [ ] SETTINGS: click **Check now**. It reports "Up to date" or offers an update. With Wi-Fi off it says "Offline" and nothing else happens.
