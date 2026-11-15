# GNARL parameters

Generated from `src/dsp/params/Registry.cpp` by `gnarl_paramdoc` — do not edit by hand.

Total: 865 parameters (synth: 854, FX: 528).

Skew is JUCE `NormalisableRange` skew (normalised = proportion^skew). Smoothing is a one-pole time constant applied to the base value; modulation is interpolated per 16-sample sub-block. Roles: **Mix** 0 = dry exactly / 1 = wet only; **Amount/Drive/Send/Depth** at zero = no effect; **Level** 0 = silence; **Neutral** at its neutral value = unchanged.


## Global

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `master_level` | Master Level | 0 … 1 | 0.7 | gain (dB shown) | 1 | 20 ms | Level | both | Output gain; 0 is true silence. |
| `voices` | Voices | 1 … 8 | 1 |  | 1 | — | Integer | synth | 1 = monophonic; up to 8-voice polyphony. |
| `legato` | Legato | 0 … 1 | 1 |  | 1 | — | Toggle | synth | Mono only: overlapping notes do not retrigger envelopes. |
| `glide_time` | Glide | 0 … 2000 | 0 | ms | 0.301 | 20 ms | Amount | synth | Portamento time; 0 = no glide. |
| `glide_curve` | Glide Curve | -1 … 1 | 0 |  | 1 | 20 ms | Continuous | synth | Shape of the glide trajectory (-1 fast start, +1 slow start). |
| `glide_legato` | Glide Legato Only | 0 … 1 | 1 |  | 1 | — | Toggle | synth | Glide only on overlapping notes. |
| `bend_range` | Bend Range | 0 … 24 | 2 | st | 1 | 20 ms | Continuous | synth | Pitch-bend range in semitones. |
| `oversampling` | Oversampling | 1x / 2x / 4x / 8x | 4x |  | 1 | — | Choice | both | Oversampling for voice filters/drive and FX nonlinear stages. |
| `render_os` | Offline Oversampling | Same / 2x Higher / 4x Higher | 2x Higher |  | 1 | — | Choice | both | Extra oversampling during offline bounce. |
| `post_drive_type` | Post Drive Type | Soft Clip / Hard Clip / Tube / Valve / Diode / Wavefolder / Sine Fold / Rectify / Fuzz / Bit Reduce / Downsample / Draw | Soft Clip |  | 1 | — | Choice | synth | Type of the optional mono drive after the voice mix. |
| `post_drive` | Post Drive | 0 … 1 | 0 | % | 1 | 20 ms | Drive | synth | Mono drive after the voice mix; 0 = bypassed. |

## Filters

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `filter_routing` | Filter Routing | Serial / Parallel / Split | Serial |  | 1 | — | Choice | both | Serial (F1 > sat > F2), Parallel, or Split (bus 1 > F1, bus 2 > F2). |
| `sat_type` | Saturation Type | Soft Clip / Hard Clip / Tube / Valve / Diode / Wavefolder / Sine Fold / Rectify / Fuzz / Bit Reduce / Downsample / Draw | Soft Clip |  | 1 | — | Choice | both | Saturation between filters (serial only). |
| `sat_drive` | Saturation Drive | 0 … 1 | 0 | % | 1 | 20 ms | Drive | both | Drive of the between-filter saturation; 0 = bypassed. |
| `sat_mix` | Saturation Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet of the saturation slot. |
| `filter_link` | Link Filters | 0 … 1 | 0 |  | 1 | — | Toggle | both | Filter 2 follows Filter 1 (type, cutoff, reso, sep) with offsets. |
| `link_cutoff` | Link Cutoff Offset | -48 … 48 | 0 | st | 1 | 20 ms | Neutral | both | Filter 2 cutoff offset when linked. |
| `link_reso` | Link Reso Offset | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Filter 2 resonance offset when linked. |
| `link_pan` | Link Pan Offset | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Filter 2 pan offset when linked (Filter 1 pans the opposite way). |

## Osc A

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `osca_engine` | Osc A Engine | Analog / HyperSaw / Wavetable / FM / Grain/Formant / Harmonic | Analog |  | 1 | — | Choice | synth | Oscillator engine. |
| `osca_octave` | Osc A Octave | -4 … 4 | 0 |  | 1 | — | Integer | synth | Octave transpose ±4. |
| `osca_semi` | Osc A Semi | -12 … 12 | 0 |  | 1 | — | Integer | synth | Semitone transpose. |
| `osca_fine` | Osc A Fine | -100 … 100 | 0 | ct | 1 | 20 ms | Neutral | synth | Fine tune in cents. |
| `osca_phase` | Osc A Phase | 0 … 360 | 0 | deg | 1 | — | Continuous | synth | Start phase (key-sync mode). |
| `osca_phase_mode` | Osc A Phase Mode | Key Sync / Free Run / Random | Key Sync |  | 1 | — | Choice | synth | Phase reset on note-on, free-running, or random start. |
| `osca_invert` | Osc A Invert | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Invert output polarity. |
| `osca_fixed` | Osc A Fixed Freq | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Ignore the keyboard and play the Fixed Freq. |
| `osca_fixed_hz` | Osc A Fixed Freq Hz | 1 … 20000 | 110 | Hz | 0.182 | 20 ms | Continuous | synth | Frequency used when Fixed Freq is on. |
| `osca_level` | Osc A Level | 0 … 1 | 0.8 | gain (dB shown) | 1 | 20 ms | Level | synth | Oscillator level; 0 = silent. |
| `osca_pan` | Osc A Pan | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | synth | Stereo position. |
| `osca_balance` | Osc A Filter Balance | -1 … 1 | -1 |  | 1 | 20 ms | Continuous | synth | -1 = all to Filter 1, +1 = all to Filter 2. |
| `osca_fx_send` | Osc A Direct FX Send | 0 … 1 | 0 | % | 1 | 20 ms | Send | synth | Extra signal sent straight to the FX chain, bypassing filters. |
| `osca_unison` | Osc A Unison | 1 … 9 | 1 |  | 1 | — | Integer | synth | Unison voices. |
| `osca_detune` | Osc A Unison Detune | 0 … 100 | 20 | ct | 1 | 20 ms | Amount | synth | Unison spread in cents. |
| `osca_spread` | Osc A Unison Stereo | 0 … 1 | 0.5 | % | 1 | 20 ms | Amount | synth | Unison stereo spread. |
| `osca_blend` | Osc A Unison Blend | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | synth | Level of the outer unison voices against the centre voice. |
| `osca_multi` | Osc A Multi Mode | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Eight detuned copies, four to each filter bus. |
| `osca_va_shape` | Osc A Shape | 0 … 3 | 2 |  | 1 | 20 ms | Continuous | synth | Morph sine > triangle > saw > pulse. |
| `osca_va_pw` | Osc A Pulse Width | 0.01 … 0.99 | 0.5 | % | 1 | 20 ms | Continuous | synth | Pulse width (shape at pulse). |
| `osca_va_sync` | Osc A Sync | 1 … 16 | 1 | ratio | 1 | 20 ms | Amount | synth | Hard sync ratio; 1 = no sync. |
| `osca_hs_density` | Osc A HyperSaw Density | 0 … 1 | 0.75 | % | 1 | 20 ms | Continuous | synth | Number/level of saws (1 to 9). |
| `osca_hs_spread` | Osc A HyperSaw Spread | 0 … 1 | 0.3 | % | 1 | 20 ms | Amount | synth | Detune between the saws. |
| `osca_hs_sync` | Osc A HyperSaw Sync | 1 … 16 | 1 | ratio | 1 | 20 ms | Amount | synth | Hard sync ratio of all saws; 1 = none. |
| `osca_wt_table` | Osc A Table | 0 … 63 | 0 |  | 1 | — | Integer | synth | Factory wavetable index (user tables override via the browser). |
| `osca_wt_pos` | Osc A WT Position | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | synth | Frame position, smoothly interpolated. |
| `osca_wt_warp_mode` | Osc A Warp Mode | None / Bend + / Bend - / Sync / PWM Squeeze / Mirror / Phase Dist / Fold / Quantize | None |  | 1 | — | Choice | synth | Phase/shape warp applied to the frame. |
| `osca_wt_warp` | Osc A WT Warp | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Warp amount; 0 = none. |
| `osca_wt_pwm_detune` | Osc A PWM Auto Detune | 0 … 1 | 0 |  | 1 | — | Toggle | synth | PWM Squeeze keeps perceived pitch by detuning the fundamental. |
| `osca_fm_alg` | Osc A FM Algorithm | 0 … 7 | 0 |  | 1 | — | Integer | synth | Operator routing algorithm (8). |
| `osca_fm_op1_ratio` | Osc A Op1 Ratio | 0.125 … 32 | 1 | ratio | 0.245 | 20 ms | Continuous | synth | Frequency ratio to the note. |
| `osca_fm_op1_fixed` | Osc A Op1 Fixed | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Operator ignores the keyboard. |
| `osca_fm_op1_freq` | Osc A Op1 Fixed Hz | 1 … 10000 | 440 | Hz | 0.222 | 20 ms | Continuous | synth | Operator frequency when Fixed. |
| `osca_fm_op1_level` | Osc A Op1 Level | 0 … 1 | 1 | gain (dB shown) | 1 | 20 ms | Level | synth | Carrier level. |
| `osca_fm_op1_fb` | Osc A Op1 Feedback | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Operator self-feedback. |
| `osca_fm_op1_wave` | Osc A Op1 Wave | Sine / Half Sine / Abs Sine / Quarter Sine / Triangle / Soft Square / Soft Saw | Sine |  | 1 | — | Choice | synth | Operator waveform. |
| `osca_fm_op1_a` | Osc A Op1 Attack | 0 … 10 | 0.001 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope attack. |
| `osca_fm_op1_d` | Osc A Op1 Decay | 0 … 10 | 0.5 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope decay. |
| `osca_fm_op1_s` | Osc A Op1 Sustain | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | synth | Operator envelope sustain. |
| `osca_fm_op1_r` | Osc A Op1 Release | 0 … 10 | 0.2 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope release. |
| `osca_fm_op2_ratio` | Osc A Op2 Ratio | 0.125 … 32 | 2 | ratio | 0.245 | 20 ms | Continuous | synth | Frequency ratio to the note. |
| `osca_fm_op2_fixed` | Osc A Op2 Fixed | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Operator ignores the keyboard. |
| `osca_fm_op2_freq` | Osc A Op2 Fixed Hz | 1 … 10000 | 440 | Hz | 0.222 | 20 ms | Continuous | synth | Operator frequency when Fixed. |
| `osca_fm_op2_level` | Osc A Op2 Level | 0 … 1 | 0 | gain (dB shown) | 1 | 20 ms | Amount | synth | Modulator index. |
| `osca_fm_op2_fb` | Osc A Op2 Feedback | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Operator self-feedback. |
| `osca_fm_op2_wave` | Osc A Op2 Wave | Sine / Half Sine / Abs Sine / Quarter Sine / Triangle / Soft Square / Soft Saw | Sine |  | 1 | — | Choice | synth | Operator waveform. |
| `osca_fm_op2_a` | Osc A Op2 Attack | 0 … 10 | 0.001 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope attack. |
| `osca_fm_op2_d` | Osc A Op2 Decay | 0 … 10 | 0.5 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope decay. |
| `osca_fm_op2_s` | Osc A Op2 Sustain | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | synth | Operator envelope sustain. |
| `osca_fm_op2_r` | Osc A Op2 Release | 0 … 10 | 0.2 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope release. |
| `osca_fm_op3_ratio` | Osc A Op3 Ratio | 0.125 … 32 | 2 | ratio | 0.245 | 20 ms | Continuous | synth | Frequency ratio to the note. |
| `osca_fm_op3_fixed` | Osc A Op3 Fixed | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Operator ignores the keyboard. |
| `osca_fm_op3_freq` | Osc A Op3 Fixed Hz | 1 … 10000 | 440 | Hz | 0.222 | 20 ms | Continuous | synth | Operator frequency when Fixed. |
| `osca_fm_op3_level` | Osc A Op3 Level | 0 … 1 | 0 | gain (dB shown) | 1 | 20 ms | Amount | synth | Modulator index. |
| `osca_fm_op3_fb` | Osc A Op3 Feedback | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Operator self-feedback. |
| `osca_fm_op3_wave` | Osc A Op3 Wave | Sine / Half Sine / Abs Sine / Quarter Sine / Triangle / Soft Square / Soft Saw | Sine |  | 1 | — | Choice | synth | Operator waveform. |
| `osca_fm_op3_a` | Osc A Op3 Attack | 0 … 10 | 0.001 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope attack. |
| `osca_fm_op3_d` | Osc A Op3 Decay | 0 … 10 | 0.5 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope decay. |
| `osca_fm_op3_s` | Osc A Op3 Sustain | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | synth | Operator envelope sustain. |
| `osca_fm_op3_r` | Osc A Op3 Release | 0 … 10 | 0.2 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope release. |
| `osca_fm_op4_ratio` | Osc A Op4 Ratio | 0.125 … 32 | 2 | ratio | 0.245 | 20 ms | Continuous | synth | Frequency ratio to the note. |
| `osca_fm_op4_fixed` | Osc A Op4 Fixed | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Operator ignores the keyboard. |
| `osca_fm_op4_freq` | Osc A Op4 Fixed Hz | 1 … 10000 | 440 | Hz | 0.222 | 20 ms | Continuous | synth | Operator frequency when Fixed. |
| `osca_fm_op4_level` | Osc A Op4 Level | 0 … 1 | 0 | gain (dB shown) | 1 | 20 ms | Amount | synth | Modulator index. |
| `osca_fm_op4_fb` | Osc A Op4 Feedback | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Operator self-feedback. |
| `osca_fm_op4_wave` | Osc A Op4 Wave | Sine / Half Sine / Abs Sine / Quarter Sine / Triangle / Soft Square / Soft Saw | Sine |  | 1 | — | Choice | synth | Operator waveform. |
| `osca_fm_op4_a` | Osc A Op4 Attack | 0 … 10 | 0.001 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope attack. |
| `osca_fm_op4_d` | Osc A Op4 Decay | 0 … 10 | 0.5 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope decay. |
| `osca_fm_op4_s` | Osc A Op4 Sustain | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | synth | Operator envelope sustain. |
| `osca_fm_op4_r` | Osc A Op4 Release | 0 … 10 | 0.2 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope release. |
| `osca_gf_index` | Osc A GF Index | 0 … 1 | 0.3 | % | 1 | 20 ms | Continuous | synth | Frame of the grain table. |
| `osca_gf_shift` | Osc A Formant Shift | -64 … 64 | 0 | st | 1 | 20 ms | Continuous | synth | Formant shift in semitones. |
| `osca_gf_spread` | Osc A Formant Spread | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Second grain stream spread (sub/sync-like). |
| `osca_gf_detune` | Osc A Local Detune | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Two detuned streams an octave apart. |
| `osca_gf_interp` | Osc A GF Interpolation | 0 … 1 | 1 |  | 1 | — | Toggle | synth | Interpolate between grain frames. |
| `osca_gf_complex` | Osc A GF Complex | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Simple: the fundamental stays audible; Complex: pure grains. |
| `osca_gf_mode` | Osc A GF Mode | Grain / Formant | Formant |  | 1 | — | Choice | synth | Formant mode holds formants at fixed pitch across the keyboard. |
| `osca_ha_partials` | Osc A Partials | 1 … 128 | 64 |  | 1 | — | Integer | synth | Number of partials. |
| `osca_ha_tilt` | Osc A Tilt | -1 … 1 | 0 |  | 1 | 20 ms | Continuous | synth | Spectral tilt (0 = saw-like 1/n). |
| `osca_ha_oddeven` | Osc A Odd/Even | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | synth | -1 odd only, +1 even (and fundamental) only. |
| `osca_ha_comb` | Osc A Comb | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Depth of spectral comb notches. |
| `osca_ha_comb_period` | Osc A Comb Period | 2 … 16 | 4 |  | 1 | — | Integer | synth | Partials between comb notches. |
| `osca_ha_stretch` | Osc A Stretch | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | synth | Pull partials away from harmonic positions. |
| `osca_ha_formant_hz` | Osc A Formant Freq | 100 … 8000 | 800 | Hz | 0.319 | 20 ms | Continuous | synth | Centre of the spectral formant. |
| `osca_ha_formant_gain` | Osc A Formant Amount | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Strength of the spectral formant. |
| `osca_ha_formant_width` | Osc A Formant Width | 0.05 … 2 | 0.5 | oct | 1 | 20 ms | Continuous | synth | Width of the formant in octaves. |

## Osc A Shaper

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `osca_sh_dc` | Osc A DC | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | DC adjust: shifts the cycle up/down before Shaper stages (re-band-limited; output DC-blocked). |
| `osca_sh_sym` | Osc A Symmetry | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Stretch one half-cycle against the other. |
| `osca_sh_warp` | Osc A Warp | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Wrap the wave past a threshold. |
| `osca_sh_twist` | Osc A Twist | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Wrap and invert past a threshold. |
| `osca_sh_drive` | Osc A Drive | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Boost and clip. |
| `osca_sh_window` | Osc A Window | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Triangular window over the cycle. |
| `osca_sh_offset` | Osc A Offset | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Rotate the cycle (0..180 degrees phase offset). |
| `osca_sh_wave` | Osc A Wave | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Morph toward a sine. |
| `osca_sh_bits` | Osc A Bits | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Bit reduction from 32 bits down to 1 bit. |
| `osca_sh_shrink` | Osc A Shrink | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Compress the cycle and zero-fill. |
| `osca_sh_selfsync` | Osc A SelfSync | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Stretch and repeat (sync-like). |
| `osca_sh_multipoint` | Osc A Multipoint | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Zero the wave at three points. |
| `osca_sh_mp_width` | Osc A Multipoint Width | 0 … 1 | 0.1 | % | 1 | 10 ms | Continuous | synth | Width of the multipoint zero regions. |
| `osca_sh_lpf` | Osc A LPF | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Low-pass the cycle spectrum. |
| `osca_sh_hpf` | Osc A HPF | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | High-pass the cycle spectrum. |
| `osca_sh_fold` | Osc A Fold | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Wavefold the cycle. |
| `osca_sh_spectrum` | Osc A Spectrum | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Harmonic levels drift in alternating directions. |
| `osca_sh_rnd_all` | Osc A Random All | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Randomise all harmonic levels. |
| `osca_sh_rnd_odd` | Osc A Random Odd | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Randomise odd harmonic levels. |

## Osc B

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `oscb_engine` | Osc B Engine | Analog / HyperSaw / Wavetable / FM / Grain/Formant / Harmonic | Analog |  | 1 | — | Choice | synth | Oscillator engine. |
| `oscb_octave` | Osc B Octave | -4 … 4 | 0 |  | 1 | — | Integer | synth | Octave transpose ±4. |
| `oscb_semi` | Osc B Semi | -12 … 12 | 0 |  | 1 | — | Integer | synth | Semitone transpose. |
| `oscb_fine` | Osc B Fine | -100 … 100 | 0 | ct | 1 | 20 ms | Neutral | synth | Fine tune in cents. |
| `oscb_phase` | Osc B Phase | 0 … 360 | 0 | deg | 1 | — | Continuous | synth | Start phase (key-sync mode). |
| `oscb_phase_mode` | Osc B Phase Mode | Key Sync / Free Run / Random | Key Sync |  | 1 | — | Choice | synth | Phase reset on note-on, free-running, or random start. |
| `oscb_invert` | Osc B Invert | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Invert output polarity. |
| `oscb_fixed` | Osc B Fixed Freq | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Ignore the keyboard and play the Fixed Freq. |
| `oscb_fixed_hz` | Osc B Fixed Freq Hz | 1 … 20000 | 110 | Hz | 0.182 | 20 ms | Continuous | synth | Frequency used when Fixed Freq is on. |
| `oscb_level` | Osc B Level | 0 … 1 | 0 | gain (dB shown) | 1 | 20 ms | Level | synth | Oscillator level; 0 = silent. |
| `oscb_pan` | Osc B Pan | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | synth | Stereo position. |
| `oscb_balance` | Osc B Filter Balance | -1 … 1 | -1 |  | 1 | 20 ms | Continuous | synth | -1 = all to Filter 1, +1 = all to Filter 2. |
| `oscb_fx_send` | Osc B Direct FX Send | 0 … 1 | 0 | % | 1 | 20 ms | Send | synth | Extra signal sent straight to the FX chain, bypassing filters. |
| `oscb_unison` | Osc B Unison | 1 … 9 | 1 |  | 1 | — | Integer | synth | Unison voices. |
| `oscb_detune` | Osc B Unison Detune | 0 … 100 | 20 | ct | 1 | 20 ms | Amount | synth | Unison spread in cents. |
| `oscb_spread` | Osc B Unison Stereo | 0 … 1 | 0.5 | % | 1 | 20 ms | Amount | synth | Unison stereo spread. |
| `oscb_blend` | Osc B Unison Blend | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | synth | Level of the outer unison voices against the centre voice. |
| `oscb_multi` | Osc B Multi Mode | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Eight detuned copies, four to each filter bus. |
| `oscb_va_shape` | Osc B Shape | 0 … 3 | 2 |  | 1 | 20 ms | Continuous | synth | Morph sine > triangle > saw > pulse. |
| `oscb_va_pw` | Osc B Pulse Width | 0.01 … 0.99 | 0.5 | % | 1 | 20 ms | Continuous | synth | Pulse width (shape at pulse). |
| `oscb_va_sync` | Osc B Sync | 1 … 16 | 1 | ratio | 1 | 20 ms | Amount | synth | Hard sync ratio; 1 = no sync. |
| `oscb_hs_density` | Osc B HyperSaw Density | 0 … 1 | 0.75 | % | 1 | 20 ms | Continuous | synth | Number/level of saws (1 to 9). |
| `oscb_hs_spread` | Osc B HyperSaw Spread | 0 … 1 | 0.3 | % | 1 | 20 ms | Amount | synth | Detune between the saws. |
| `oscb_hs_sync` | Osc B HyperSaw Sync | 1 … 16 | 1 | ratio | 1 | 20 ms | Amount | synth | Hard sync ratio of all saws; 1 = none. |
| `oscb_wt_table` | Osc B Table | 0 … 63 | 0 |  | 1 | — | Integer | synth | Factory wavetable index (user tables override via the browser). |
| `oscb_wt_pos` | Osc B WT Position | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | synth | Frame position, smoothly interpolated. |
| `oscb_wt_warp_mode` | Osc B Warp Mode | None / Bend + / Bend - / Sync / PWM Squeeze / Mirror / Phase Dist / Fold / Quantize | None |  | 1 | — | Choice | synth | Phase/shape warp applied to the frame. |
| `oscb_wt_warp` | Osc B WT Warp | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Warp amount; 0 = none. |
| `oscb_wt_pwm_detune` | Osc B PWM Auto Detune | 0 … 1 | 0 |  | 1 | — | Toggle | synth | PWM Squeeze keeps perceived pitch by detuning the fundamental. |
| `oscb_fm_alg` | Osc B FM Algorithm | 0 … 7 | 0 |  | 1 | — | Integer | synth | Operator routing algorithm (8). |
| `oscb_fm_op1_ratio` | Osc B Op1 Ratio | 0.125 … 32 | 1 | ratio | 0.245 | 20 ms | Continuous | synth | Frequency ratio to the note. |
| `oscb_fm_op1_fixed` | Osc B Op1 Fixed | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Operator ignores the keyboard. |
| `oscb_fm_op1_freq` | Osc B Op1 Fixed Hz | 1 … 10000 | 440 | Hz | 0.222 | 20 ms | Continuous | synth | Operator frequency when Fixed. |
| `oscb_fm_op1_level` | Osc B Op1 Level | 0 … 1 | 1 | gain (dB shown) | 1 | 20 ms | Level | synth | Carrier level. |
| `oscb_fm_op1_fb` | Osc B Op1 Feedback | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Operator self-feedback. |
| `oscb_fm_op1_wave` | Osc B Op1 Wave | Sine / Half Sine / Abs Sine / Quarter Sine / Triangle / Soft Square / Soft Saw | Sine |  | 1 | — | Choice | synth | Operator waveform. |
| `oscb_fm_op1_a` | Osc B Op1 Attack | 0 … 10 | 0.001 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope attack. |
| `oscb_fm_op1_d` | Osc B Op1 Decay | 0 … 10 | 0.5 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope decay. |
| `oscb_fm_op1_s` | Osc B Op1 Sustain | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | synth | Operator envelope sustain. |
| `oscb_fm_op1_r` | Osc B Op1 Release | 0 … 10 | 0.2 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope release. |
| `oscb_fm_op2_ratio` | Osc B Op2 Ratio | 0.125 … 32 | 2 | ratio | 0.245 | 20 ms | Continuous | synth | Frequency ratio to the note. |
| `oscb_fm_op2_fixed` | Osc B Op2 Fixed | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Operator ignores the keyboard. |
| `oscb_fm_op2_freq` | Osc B Op2 Fixed Hz | 1 … 10000 | 440 | Hz | 0.222 | 20 ms | Continuous | synth | Operator frequency when Fixed. |
| `oscb_fm_op2_level` | Osc B Op2 Level | 0 … 1 | 0 | gain (dB shown) | 1 | 20 ms | Amount | synth | Modulator index. |
| `oscb_fm_op2_fb` | Osc B Op2 Feedback | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Operator self-feedback. |
| `oscb_fm_op2_wave` | Osc B Op2 Wave | Sine / Half Sine / Abs Sine / Quarter Sine / Triangle / Soft Square / Soft Saw | Sine |  | 1 | — | Choice | synth | Operator waveform. |
| `oscb_fm_op2_a` | Osc B Op2 Attack | 0 … 10 | 0.001 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope attack. |
| `oscb_fm_op2_d` | Osc B Op2 Decay | 0 … 10 | 0.5 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope decay. |
| `oscb_fm_op2_s` | Osc B Op2 Sustain | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | synth | Operator envelope sustain. |
| `oscb_fm_op2_r` | Osc B Op2 Release | 0 … 10 | 0.2 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope release. |
| `oscb_fm_op3_ratio` | Osc B Op3 Ratio | 0.125 … 32 | 2 | ratio | 0.245 | 20 ms | Continuous | synth | Frequency ratio to the note. |
| `oscb_fm_op3_fixed` | Osc B Op3 Fixed | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Operator ignores the keyboard. |
| `oscb_fm_op3_freq` | Osc B Op3 Fixed Hz | 1 … 10000 | 440 | Hz | 0.222 | 20 ms | Continuous | synth | Operator frequency when Fixed. |
| `oscb_fm_op3_level` | Osc B Op3 Level | 0 … 1 | 0 | gain (dB shown) | 1 | 20 ms | Amount | synth | Modulator index. |
| `oscb_fm_op3_fb` | Osc B Op3 Feedback | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Operator self-feedback. |
| `oscb_fm_op3_wave` | Osc B Op3 Wave | Sine / Half Sine / Abs Sine / Quarter Sine / Triangle / Soft Square / Soft Saw | Sine |  | 1 | — | Choice | synth | Operator waveform. |
| `oscb_fm_op3_a` | Osc B Op3 Attack | 0 … 10 | 0.001 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope attack. |
| `oscb_fm_op3_d` | Osc B Op3 Decay | 0 … 10 | 0.5 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope decay. |
| `oscb_fm_op3_s` | Osc B Op3 Sustain | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | synth | Operator envelope sustain. |
| `oscb_fm_op3_r` | Osc B Op3 Release | 0 … 10 | 0.2 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope release. |
| `oscb_fm_op4_ratio` | Osc B Op4 Ratio | 0.125 … 32 | 2 | ratio | 0.245 | 20 ms | Continuous | synth | Frequency ratio to the note. |
| `oscb_fm_op4_fixed` | Osc B Op4 Fixed | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Operator ignores the keyboard. |
| `oscb_fm_op4_freq` | Osc B Op4 Fixed Hz | 1 … 10000 | 440 | Hz | 0.222 | 20 ms | Continuous | synth | Operator frequency when Fixed. |
| `oscb_fm_op4_level` | Osc B Op4 Level | 0 … 1 | 0 | gain (dB shown) | 1 | 20 ms | Amount | synth | Modulator index. |
| `oscb_fm_op4_fb` | Osc B Op4 Feedback | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Operator self-feedback. |
| `oscb_fm_op4_wave` | Osc B Op4 Wave | Sine / Half Sine / Abs Sine / Quarter Sine / Triangle / Soft Square / Soft Saw | Sine |  | 1 | — | Choice | synth | Operator waveform. |
| `oscb_fm_op4_a` | Osc B Op4 Attack | 0 … 10 | 0.001 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope attack. |
| `oscb_fm_op4_d` | Osc B Op4 Decay | 0 … 10 | 0.5 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope decay. |
| `oscb_fm_op4_s` | Osc B Op4 Sustain | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | synth | Operator envelope sustain. |
| `oscb_fm_op4_r` | Osc B Op4 Release | 0 … 10 | 0.2 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope release. |
| `oscb_gf_index` | Osc B GF Index | 0 … 1 | 0.3 | % | 1 | 20 ms | Continuous | synth | Frame of the grain table. |
| `oscb_gf_shift` | Osc B Formant Shift | -64 … 64 | 0 | st | 1 | 20 ms | Continuous | synth | Formant shift in semitones. |
| `oscb_gf_spread` | Osc B Formant Spread | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Second grain stream spread (sub/sync-like). |
| `oscb_gf_detune` | Osc B Local Detune | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Two detuned streams an octave apart. |
| `oscb_gf_interp` | Osc B GF Interpolation | 0 … 1 | 1 |  | 1 | — | Toggle | synth | Interpolate between grain frames. |
| `oscb_gf_complex` | Osc B GF Complex | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Simple: the fundamental stays audible; Complex: pure grains. |
| `oscb_gf_mode` | Osc B GF Mode | Grain / Formant | Formant |  | 1 | — | Choice | synth | Formant mode holds formants at fixed pitch across the keyboard. |
| `oscb_ha_partials` | Osc B Partials | 1 … 128 | 64 |  | 1 | — | Integer | synth | Number of partials. |
| `oscb_ha_tilt` | Osc B Tilt | -1 … 1 | 0 |  | 1 | 20 ms | Continuous | synth | Spectral tilt (0 = saw-like 1/n). |
| `oscb_ha_oddeven` | Osc B Odd/Even | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | synth | -1 odd only, +1 even (and fundamental) only. |
| `oscb_ha_comb` | Osc B Comb | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Depth of spectral comb notches. |
| `oscb_ha_comb_period` | Osc B Comb Period | 2 … 16 | 4 |  | 1 | — | Integer | synth | Partials between comb notches. |
| `oscb_ha_stretch` | Osc B Stretch | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | synth | Pull partials away from harmonic positions. |
| `oscb_ha_formant_hz` | Osc B Formant Freq | 100 … 8000 | 800 | Hz | 0.319 | 20 ms | Continuous | synth | Centre of the spectral formant. |
| `oscb_ha_formant_gain` | Osc B Formant Amount | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Strength of the spectral formant. |
| `oscb_ha_formant_width` | Osc B Formant Width | 0.05 … 2 | 0.5 | oct | 1 | 20 ms | Continuous | synth | Width of the formant in octaves. |

## Osc B Shaper

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `oscb_sh_dc` | Osc B DC | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | DC adjust: shifts the cycle up/down before Shaper stages (re-band-limited; output DC-blocked). |
| `oscb_sh_sym` | Osc B Symmetry | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Stretch one half-cycle against the other. |
| `oscb_sh_warp` | Osc B Warp | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Wrap the wave past a threshold. |
| `oscb_sh_twist` | Osc B Twist | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Wrap and invert past a threshold. |
| `oscb_sh_drive` | Osc B Drive | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Boost and clip. |
| `oscb_sh_window` | Osc B Window | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Triangular window over the cycle. |
| `oscb_sh_offset` | Osc B Offset | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Rotate the cycle (0..180 degrees phase offset). |
| `oscb_sh_wave` | Osc B Wave | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Morph toward a sine. |
| `oscb_sh_bits` | Osc B Bits | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Bit reduction from 32 bits down to 1 bit. |
| `oscb_sh_shrink` | Osc B Shrink | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Compress the cycle and zero-fill. |
| `oscb_sh_selfsync` | Osc B SelfSync | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Stretch and repeat (sync-like). |
| `oscb_sh_multipoint` | Osc B Multipoint | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Zero the wave at three points. |
| `oscb_sh_mp_width` | Osc B Multipoint Width | 0 … 1 | 0.1 | % | 1 | 10 ms | Continuous | synth | Width of the multipoint zero regions. |
| `oscb_sh_lpf` | Osc B LPF | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Low-pass the cycle spectrum. |
| `oscb_sh_hpf` | Osc B HPF | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | High-pass the cycle spectrum. |
| `oscb_sh_fold` | Osc B Fold | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Wavefold the cycle. |
| `oscb_sh_spectrum` | Osc B Spectrum | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Harmonic levels drift in alternating directions. |
| `oscb_sh_rnd_all` | Osc B Random All | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Randomise all harmonic levels. |
| `oscb_sh_rnd_odd` | Osc B Random Odd | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Randomise odd harmonic levels. |

## Osc C

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `oscc_engine` | Osc C Engine | Analog / HyperSaw / Wavetable / FM / Grain/Formant / Harmonic | Analog |  | 1 | — | Choice | synth | Oscillator engine. |
| `oscc_octave` | Osc C Octave | -4 … 4 | 0 |  | 1 | — | Integer | synth | Octave transpose ±4. |
| `oscc_semi` | Osc C Semi | -12 … 12 | 0 |  | 1 | — | Integer | synth | Semitone transpose. |
| `oscc_fine` | Osc C Fine | -100 … 100 | 0 | ct | 1 | 20 ms | Neutral | synth | Fine tune in cents. |
| `oscc_phase` | Osc C Phase | 0 … 360 | 0 | deg | 1 | — | Continuous | synth | Start phase (key-sync mode). |
| `oscc_phase_mode` | Osc C Phase Mode | Key Sync / Free Run / Random | Key Sync |  | 1 | — | Choice | synth | Phase reset on note-on, free-running, or random start. |
| `oscc_invert` | Osc C Invert | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Invert output polarity. |
| `oscc_fixed` | Osc C Fixed Freq | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Ignore the keyboard and play the Fixed Freq. |
| `oscc_fixed_hz` | Osc C Fixed Freq Hz | 1 … 20000 | 110 | Hz | 0.182 | 20 ms | Continuous | synth | Frequency used when Fixed Freq is on. |
| `oscc_level` | Osc C Level | 0 … 1 | 0 | gain (dB shown) | 1 | 20 ms | Level | synth | Oscillator level; 0 = silent. |
| `oscc_pan` | Osc C Pan | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | synth | Stereo position. |
| `oscc_balance` | Osc C Filter Balance | -1 … 1 | -1 |  | 1 | 20 ms | Continuous | synth | -1 = all to Filter 1, +1 = all to Filter 2. |
| `oscc_fx_send` | Osc C Direct FX Send | 0 … 1 | 0 | % | 1 | 20 ms | Send | synth | Extra signal sent straight to the FX chain, bypassing filters. |
| `oscc_unison` | Osc C Unison | 1 … 9 | 1 |  | 1 | — | Integer | synth | Unison voices. |
| `oscc_detune` | Osc C Unison Detune | 0 … 100 | 20 | ct | 1 | 20 ms | Amount | synth | Unison spread in cents. |
| `oscc_spread` | Osc C Unison Stereo | 0 … 1 | 0.5 | % | 1 | 20 ms | Amount | synth | Unison stereo spread. |
| `oscc_blend` | Osc C Unison Blend | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | synth | Level of the outer unison voices against the centre voice. |
| `oscc_multi` | Osc C Multi Mode | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Eight detuned copies, four to each filter bus. |
| `oscc_va_shape` | Osc C Shape | 0 … 3 | 2 |  | 1 | 20 ms | Continuous | synth | Morph sine > triangle > saw > pulse. |
| `oscc_va_pw` | Osc C Pulse Width | 0.01 … 0.99 | 0.5 | % | 1 | 20 ms | Continuous | synth | Pulse width (shape at pulse). |
| `oscc_va_sync` | Osc C Sync | 1 … 16 | 1 | ratio | 1 | 20 ms | Amount | synth | Hard sync ratio; 1 = no sync. |
| `oscc_hs_density` | Osc C HyperSaw Density | 0 … 1 | 0.75 | % | 1 | 20 ms | Continuous | synth | Number/level of saws (1 to 9). |
| `oscc_hs_spread` | Osc C HyperSaw Spread | 0 … 1 | 0.3 | % | 1 | 20 ms | Amount | synth | Detune between the saws. |
| `oscc_hs_sync` | Osc C HyperSaw Sync | 1 … 16 | 1 | ratio | 1 | 20 ms | Amount | synth | Hard sync ratio of all saws; 1 = none. |
| `oscc_wt_table` | Osc C Table | 0 … 63 | 0 |  | 1 | — | Integer | synth | Factory wavetable index (user tables override via the browser). |
| `oscc_wt_pos` | Osc C WT Position | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | synth | Frame position, smoothly interpolated. |
| `oscc_wt_warp_mode` | Osc C Warp Mode | None / Bend + / Bend - / Sync / PWM Squeeze / Mirror / Phase Dist / Fold / Quantize | None |  | 1 | — | Choice | synth | Phase/shape warp applied to the frame. |
| `oscc_wt_warp` | Osc C WT Warp | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Warp amount; 0 = none. |
| `oscc_wt_pwm_detune` | Osc C PWM Auto Detune | 0 … 1 | 0 |  | 1 | — | Toggle | synth | PWM Squeeze keeps perceived pitch by detuning the fundamental. |
| `oscc_fm_alg` | Osc C FM Algorithm | 0 … 7 | 0 |  | 1 | — | Integer | synth | Operator routing algorithm (8). |
| `oscc_fm_op1_ratio` | Osc C Op1 Ratio | 0.125 … 32 | 1 | ratio | 0.245 | 20 ms | Continuous | synth | Frequency ratio to the note. |
| `oscc_fm_op1_fixed` | Osc C Op1 Fixed | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Operator ignores the keyboard. |
| `oscc_fm_op1_freq` | Osc C Op1 Fixed Hz | 1 … 10000 | 440 | Hz | 0.222 | 20 ms | Continuous | synth | Operator frequency when Fixed. |
| `oscc_fm_op1_level` | Osc C Op1 Level | 0 … 1 | 1 | gain (dB shown) | 1 | 20 ms | Level | synth | Carrier level. |
| `oscc_fm_op1_fb` | Osc C Op1 Feedback | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Operator self-feedback. |
| `oscc_fm_op1_wave` | Osc C Op1 Wave | Sine / Half Sine / Abs Sine / Quarter Sine / Triangle / Soft Square / Soft Saw | Sine |  | 1 | — | Choice | synth | Operator waveform. |
| `oscc_fm_op1_a` | Osc C Op1 Attack | 0 … 10 | 0.001 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope attack. |
| `oscc_fm_op1_d` | Osc C Op1 Decay | 0 … 10 | 0.5 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope decay. |
| `oscc_fm_op1_s` | Osc C Op1 Sustain | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | synth | Operator envelope sustain. |
| `oscc_fm_op1_r` | Osc C Op1 Release | 0 … 10 | 0.2 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope release. |
| `oscc_fm_op2_ratio` | Osc C Op2 Ratio | 0.125 … 32 | 2 | ratio | 0.245 | 20 ms | Continuous | synth | Frequency ratio to the note. |
| `oscc_fm_op2_fixed` | Osc C Op2 Fixed | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Operator ignores the keyboard. |
| `oscc_fm_op2_freq` | Osc C Op2 Fixed Hz | 1 … 10000 | 440 | Hz | 0.222 | 20 ms | Continuous | synth | Operator frequency when Fixed. |
| `oscc_fm_op2_level` | Osc C Op2 Level | 0 … 1 | 0 | gain (dB shown) | 1 | 20 ms | Amount | synth | Modulator index. |
| `oscc_fm_op2_fb` | Osc C Op2 Feedback | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Operator self-feedback. |
| `oscc_fm_op2_wave` | Osc C Op2 Wave | Sine / Half Sine / Abs Sine / Quarter Sine / Triangle / Soft Square / Soft Saw | Sine |  | 1 | — | Choice | synth | Operator waveform. |
| `oscc_fm_op2_a` | Osc C Op2 Attack | 0 … 10 | 0.001 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope attack. |
| `oscc_fm_op2_d` | Osc C Op2 Decay | 0 … 10 | 0.5 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope decay. |
| `oscc_fm_op2_s` | Osc C Op2 Sustain | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | synth | Operator envelope sustain. |
| `oscc_fm_op2_r` | Osc C Op2 Release | 0 … 10 | 0.2 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope release. |
| `oscc_fm_op3_ratio` | Osc C Op3 Ratio | 0.125 … 32 | 2 | ratio | 0.245 | 20 ms | Continuous | synth | Frequency ratio to the note. |
| `oscc_fm_op3_fixed` | Osc C Op3 Fixed | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Operator ignores the keyboard. |
| `oscc_fm_op3_freq` | Osc C Op3 Fixed Hz | 1 … 10000 | 440 | Hz | 0.222 | 20 ms | Continuous | synth | Operator frequency when Fixed. |
| `oscc_fm_op3_level` | Osc C Op3 Level | 0 … 1 | 0 | gain (dB shown) | 1 | 20 ms | Amount | synth | Modulator index. |
| `oscc_fm_op3_fb` | Osc C Op3 Feedback | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Operator self-feedback. |
| `oscc_fm_op3_wave` | Osc C Op3 Wave | Sine / Half Sine / Abs Sine / Quarter Sine / Triangle / Soft Square / Soft Saw | Sine |  | 1 | — | Choice | synth | Operator waveform. |
| `oscc_fm_op3_a` | Osc C Op3 Attack | 0 … 10 | 0.001 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope attack. |
| `oscc_fm_op3_d` | Osc C Op3 Decay | 0 … 10 | 0.5 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope decay. |
| `oscc_fm_op3_s` | Osc C Op3 Sustain | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | synth | Operator envelope sustain. |
| `oscc_fm_op3_r` | Osc C Op3 Release | 0 … 10 | 0.2 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope release. |
| `oscc_fm_op4_ratio` | Osc C Op4 Ratio | 0.125 … 32 | 2 | ratio | 0.245 | 20 ms | Continuous | synth | Frequency ratio to the note. |
| `oscc_fm_op4_fixed` | Osc C Op4 Fixed | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Operator ignores the keyboard. |
| `oscc_fm_op4_freq` | Osc C Op4 Fixed Hz | 1 … 10000 | 440 | Hz | 0.222 | 20 ms | Continuous | synth | Operator frequency when Fixed. |
| `oscc_fm_op4_level` | Osc C Op4 Level | 0 … 1 | 0 | gain (dB shown) | 1 | 20 ms | Amount | synth | Modulator index. |
| `oscc_fm_op4_fb` | Osc C Op4 Feedback | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Operator self-feedback. |
| `oscc_fm_op4_wave` | Osc C Op4 Wave | Sine / Half Sine / Abs Sine / Quarter Sine / Triangle / Soft Square / Soft Saw | Sine |  | 1 | — | Choice | synth | Operator waveform. |
| `oscc_fm_op4_a` | Osc C Op4 Attack | 0 … 10 | 0.001 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope attack. |
| `oscc_fm_op4_d` | Osc C Op4 Decay | 0 … 10 | 0.5 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope decay. |
| `oscc_fm_op4_s` | Osc C Op4 Sustain | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | synth | Operator envelope sustain. |
| `oscc_fm_op4_r` | Osc C Op4 Release | 0 … 10 | 0.2 | s | 0.231 | 20 ms | Continuous | synth | Operator envelope release. |
| `oscc_gf_index` | Osc C GF Index | 0 … 1 | 0.3 | % | 1 | 20 ms | Continuous | synth | Frame of the grain table. |
| `oscc_gf_shift` | Osc C Formant Shift | -64 … 64 | 0 | st | 1 | 20 ms | Continuous | synth | Formant shift in semitones. |
| `oscc_gf_spread` | Osc C Formant Spread | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Second grain stream spread (sub/sync-like). |
| `oscc_gf_detune` | Osc C Local Detune | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Two detuned streams an octave apart. |
| `oscc_gf_interp` | Osc C GF Interpolation | 0 … 1 | 1 |  | 1 | — | Toggle | synth | Interpolate between grain frames. |
| `oscc_gf_complex` | Osc C GF Complex | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Simple: the fundamental stays audible; Complex: pure grains. |
| `oscc_gf_mode` | Osc C GF Mode | Grain / Formant | Formant |  | 1 | — | Choice | synth | Formant mode holds formants at fixed pitch across the keyboard. |
| `oscc_ha_partials` | Osc C Partials | 1 … 128 | 64 |  | 1 | — | Integer | synth | Number of partials. |
| `oscc_ha_tilt` | Osc C Tilt | -1 … 1 | 0 |  | 1 | 20 ms | Continuous | synth | Spectral tilt (0 = saw-like 1/n). |
| `oscc_ha_oddeven` | Osc C Odd/Even | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | synth | -1 odd only, +1 even (and fundamental) only. |
| `oscc_ha_comb` | Osc C Comb | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Depth of spectral comb notches. |
| `oscc_ha_comb_period` | Osc C Comb Period | 2 … 16 | 4 |  | 1 | — | Integer | synth | Partials between comb notches. |
| `oscc_ha_stretch` | Osc C Stretch | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | synth | Pull partials away from harmonic positions. |
| `oscc_ha_formant_hz` | Osc C Formant Freq | 100 … 8000 | 800 | Hz | 0.319 | 20 ms | Continuous | synth | Centre of the spectral formant. |
| `oscc_ha_formant_gain` | Osc C Formant Amount | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Strength of the spectral formant. |
| `oscc_ha_formant_width` | Osc C Formant Width | 0.05 … 2 | 0.5 | oct | 1 | 20 ms | Continuous | synth | Width of the formant in octaves. |

## Osc C Shaper

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `oscc_sh_dc` | Osc C DC | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | DC adjust: shifts the cycle up/down before Shaper stages (re-band-limited; output DC-blocked). |
| `oscc_sh_sym` | Osc C Symmetry | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Stretch one half-cycle against the other. |
| `oscc_sh_warp` | Osc C Warp | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Wrap the wave past a threshold. |
| `oscc_sh_twist` | Osc C Twist | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Wrap and invert past a threshold. |
| `oscc_sh_drive` | Osc C Drive | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Boost and clip. |
| `oscc_sh_window` | Osc C Window | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Triangular window over the cycle. |
| `oscc_sh_offset` | Osc C Offset | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Rotate the cycle (0..180 degrees phase offset). |
| `oscc_sh_wave` | Osc C Wave | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Morph toward a sine. |
| `oscc_sh_bits` | Osc C Bits | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Bit reduction from 32 bits down to 1 bit. |
| `oscc_sh_shrink` | Osc C Shrink | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Compress the cycle and zero-fill. |
| `oscc_sh_selfsync` | Osc C SelfSync | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Stretch and repeat (sync-like). |
| `oscc_sh_multipoint` | Osc C Multipoint | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Zero the wave at three points. |
| `oscc_sh_mp_width` | Osc C Multipoint Width | 0 … 1 | 0.1 | % | 1 | 10 ms | Continuous | synth | Width of the multipoint zero regions. |
| `oscc_sh_lpf` | Osc C LPF | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Low-pass the cycle spectrum. |
| `oscc_sh_hpf` | Osc C HPF | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | High-pass the cycle spectrum. |
| `oscc_sh_fold` | Osc C Fold | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Wavefold the cycle. |
| `oscc_sh_spectrum` | Osc C Spectrum | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Harmonic levels drift in alternating directions. |
| `oscc_sh_rnd_all` | Osc C Random All | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Randomise all harmonic levels. |
| `oscc_sh_rnd_odd` | Osc C Random Odd | 0 … 1 | 0 | % | 1 | 10 ms | Amount | synth | Randomise odd harmonic levels. |

## Osc Chain

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `link_ab_mode` | Link A>B Mode | Add / Ring / Sync / PM / FM / AM | FM |  | 1 | — | Choice | synth | How the source oscillator modulates the next one. |
| `link_ab_amount` | Link A>B Amount | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Amount of the link; 0 = none. |
| `link_bc_mode` | Link B>C Mode | Add / Ring / Sync / PM / FM / AM | FM |  | 1 | — | Choice | synth | How the source oscillator modulates the next one. |
| `link_bc_amount` | Link B>C Amount | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Amount of the link; 0 = none. |
| `link_ca_mode` | Link C>A Mode | Add / Ring / Sync / PM / FM / AM | FM |  | 1 | — | Choice | synth | How the source oscillator modulates the next one. |
| `link_ca_amount` | Link C>A Amount | 0 … 1 | 0 | % | 1 | 20 ms | Amount | synth | Amount of the link; 0 = none. |

## Sub

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `sub_wave` | Sub Wave | Sine / Triangle / Soft Square | Sine |  | 1 | — | Choice | synth | Sub waveform. |
| `sub_octave` | Sub Octave | -1 / -2 | -1 |  | 1 | — | Choice | synth | Sub octave below the note. |
| `sub_level` | Sub Level | 0 … 1 | 0 | gain (dB shown) | 1 | 20 ms | Level | synth | Sub level; 0 = silent. |
| `sub_direct` | Sub Bypass Filters+FX | 0 … 1 | 1 |  | 1 | — | Toggle | synth | On: sub skips filters and FX (clean, mono). |

## Noise

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `noise_type` | Noise Type | White / Pink / Crackle / Metallic / Digital | White |  | 1 | — | Choice | synth | Noise colour/texture. |
| `noise_level` | Noise Level | 0 … 1 | 0 | gain (dB shown) | 1 | 20 ms | Level | synth | Noise level; 0 = silent. |
| `noise_track` | Noise Pitch Track | 0 … 1 | 0 |  | 1 | — | Toggle | synth | Textured noise follows the played pitch. |
| `noise_balance` | Noise Filter Balance | -1 … 1 | -1 |  | 1 | 20 ms | Continuous | synth | -1 = Filter 1, +1 = Filter 2. |

## Filter 1

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `f1_type` | F1 Type | see choices | LP 24 |  | 1 | — | Choice | both | Filter model. |
| `f1_cutoff` | F1 Cutoff | 20 … 20000 | 6000 | Hz | 0.23 | 20 ms | Continuous | both | Cutoff / centre frequency. |
| `f1_reso` | F1 Resonance | 0 … 1 | 0.1 | % | 1 | 20 ms | Continuous | both | Resonance / emphasis. |
| `f1_sep` | F1 Separation | 0 … 1 | 0.3 | % | 1 | 20 ms | Continuous | both | Spacing of the resonant peaks (multi-peak types). |
| `f1_morph` | F1 Morph / Vowel | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Morph LP>BP>HP>Notch; vowel A-E-I-O-U for Formant. |
| `f1_gender` | F1 Gender | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Formant gender shift. |
| `f1_shoulder` | F1 Shoulder Level | 0 … 1 | 0.6 | % | 1 | 20 ms | Continuous | both | Tri-Peak shoulder peak level. |
| `f1_shelf` | F1 Shelf Boost | 0 … 1 | 0.5 | % | 1 | 20 ms | Continuous | both | Tri-Peak raised shelf below the peaks. |
| `f1_poles` | F1 Ladder Poles | 1 … 4 | 4 |  | 1 | — | Integer | both | Ladder slope 6-24 dB/oct. |
| `f1_stages` | F1 Phaser Stages | 4 … 12 | 8 |  | 1 | — | Integer | both | Phaser-filter all-pass stages. |
| `f1_opposing` | F1 Opposing | 0 … 1 | 0 |  | 1 | — | Toggle | both | Dual-Reso peaks move in opposite directions with cutoff. |
| `f1_keytrack` | F1 Key Track | 0 … 1 | 0 | % | 1 | 20 ms | Amount | both | Cutoff follows the played note (100% = 1 oct/oct). |
| `f1_drive` | F1 Drive | 0 … 1 | 0 | % | 1 | 20 ms | Drive | both | Filter drive; 0 = none. |
| `f1_drive_type` | F1 Drive Type | Soft Clip / Hard Clip / Tube / Valve / Diode / Wavefolder / Sine Fold / Rectify / Fuzz / Bit Reduce / Downsample / Draw | Tube |  | 1 | — | Choice | both | Filter drive curve. |
| `f1_drive_pos` | F1 Drive Position | Pre / Post | Pre |  | 1 | — | Choice | both | Drive before or after the filter. |
| `f1_env` | F1 Env Amount | -1 … 1 | 0 |  | 1 | 20 ms | Depth | both | Bipolar envelope amount on cutoff (±8 octaves). |
| `f1_env_src` | F1 Env Source | Env 1 / Env 2 / Env 3 / Env 4 | Env 2 |  | 1 | — | Choice | both | Which envelope the Env Amount uses. |
| `f1_boost` | F1 Reso Boost | 0 … 1 | 0 |  | 1 | — | Toggle | both | Extends the resonance range. |
| `f1_limiter` | F1 Reso Limiter | Off / Slow / Fast | Off |  | 1 | — | Choice | both | Tames runaway resonance by level. |
| `f1_pan` | F1 Pan | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Filter output pan. |
| `f1_level` | F1 Level | 0 … 1 | 1 | gain (dB shown) | 1 | 20 ms | Level | both | Filter output level; 0 = silent. |

## Filter 2

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `f2_type` | F2 Type | see choices | Off |  | 1 | — | Choice | both | Filter model. |
| `f2_cutoff` | F2 Cutoff | 20 … 20000 | 1000 | Hz | 0.23 | 20 ms | Continuous | both | Cutoff / centre frequency. |
| `f2_reso` | F2 Resonance | 0 … 1 | 0.1 | % | 1 | 20 ms | Continuous | both | Resonance / emphasis. |
| `f2_sep` | F2 Separation | 0 … 1 | 0.3 | % | 1 | 20 ms | Continuous | both | Spacing of the resonant peaks (multi-peak types). |
| `f2_morph` | F2 Morph / Vowel | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Morph LP>BP>HP>Notch; vowel A-E-I-O-U for Formant. |
| `f2_gender` | F2 Gender | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Formant gender shift. |
| `f2_shoulder` | F2 Shoulder Level | 0 … 1 | 0.6 | % | 1 | 20 ms | Continuous | both | Tri-Peak shoulder peak level. |
| `f2_shelf` | F2 Shelf Boost | 0 … 1 | 0.5 | % | 1 | 20 ms | Continuous | both | Tri-Peak raised shelf below the peaks. |
| `f2_poles` | F2 Ladder Poles | 1 … 4 | 4 |  | 1 | — | Integer | both | Ladder slope 6-24 dB/oct. |
| `f2_stages` | F2 Phaser Stages | 4 … 12 | 8 |  | 1 | — | Integer | both | Phaser-filter all-pass stages. |
| `f2_opposing` | F2 Opposing | 0 … 1 | 0 |  | 1 | — | Toggle | both | Dual-Reso peaks move in opposite directions with cutoff. |
| `f2_keytrack` | F2 Key Track | 0 … 1 | 0 | % | 1 | 20 ms | Amount | both | Cutoff follows the played note (100% = 1 oct/oct). |
| `f2_drive` | F2 Drive | 0 … 1 | 0 | % | 1 | 20 ms | Drive | both | Filter drive; 0 = none. |
| `f2_drive_type` | F2 Drive Type | Soft Clip / Hard Clip / Tube / Valve / Diode / Wavefolder / Sine Fold / Rectify / Fuzz / Bit Reduce / Downsample / Draw | Tube |  | 1 | — | Choice | both | Filter drive curve. |
| `f2_drive_pos` | F2 Drive Position | Pre / Post | Pre |  | 1 | — | Choice | both | Drive before or after the filter. |
| `f2_env` | F2 Env Amount | -1 … 1 | 0 |  | 1 | 20 ms | Depth | both | Bipolar envelope amount on cutoff (±8 octaves). |
| `f2_env_src` | F2 Env Source | Env 1 / Env 2 / Env 3 / Env 4 | Env 3 |  | 1 | — | Choice | both | Which envelope the Env Amount uses. |
| `f2_boost` | F2 Reso Boost | 0 … 1 | 0 |  | 1 | — | Toggle | both | Extends the resonance range. |
| `f2_limiter` | F2 Reso Limiter | Off / Slow / Fast | Off |  | 1 | — | Choice | both | Tames runaway resonance by level. |
| `f2_pan` | F2 Pan | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Filter output pan. |
| `f2_level` | F2 Level | 0 … 1 | 1 | gain (dB shown) | 1 | 20 ms | Level | both | Filter output level; 0 = silent. |

## Envelope 1

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `env1_a` | Env 1 Attack | 0 … 20 | 0.002 | s | 0.188 | 20 ms | Continuous | both | Attack time (seconds mode). |
| `env1_d` | Env 1 Decay | 0 … 20 | 0.3 | s | 0.188 | 20 ms | Continuous | both | Decay time (seconds mode). |
| `env1_s` | Env 1 Sustain | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | both | Sustain level. |
| `env1_r` | Env 1 Release | 0 … 20 | 0.08 | s | 0.188 | 20 ms | Continuous | both | Release time (seconds mode). |
| `env1_ac` | Env 1 Attack Curve | -1 … 1 | 0 |  | 1 | 20 ms | Continuous | both | Attack curvature. |
| `env1_dc` | Env 1 Decay Curve | -1 … 1 | -0.5 |  | 1 | 20 ms | Continuous | both | Decay curvature. |
| `env1_rc` | Env 1 Release Curve | -1 … 1 | -0.5 |  | 1 | 20 ms | Continuous | both | Release curvature. |
| `env1_beats` | Env 1 Tempo Sync | 0 … 1 | 0 |  | 1 | — | Toggle | both | Stage times in beats instead of seconds. |
| `env1_a_b` | Env 1 Attack (beats) | 0 … 16 | 0 | beats | 0.25 | 20 ms | Continuous | both | Attack in beats (tempo mode). |
| `env1_d_b` | Env 1 Decay (beats) | 0 … 16 | 0.5 | beats | 0.25 | 20 ms | Continuous | both | Decay in beats (tempo mode). |
| `env1_r_b` | Env 1 Release (beats) | 0 … 16 | 0.25 | beats | 0.25 | 20 ms | Continuous | both | Release in beats (tempo mode). |
| `env1_mode` | Env 1 Mode | ADSR / Drawn | ADSR |  | 1 | — | Choice | both | ADSR or drawn multi-segment shape. |
| `env1_len` | Env 1 Drawn Length | 0.01 … 20 | 1 | s | 0.231 | 20 ms | Continuous | both | Length of the drawn envelope (seconds). |
| `env1_len_b` | Env 1 Drawn Length (beats) | 0.0625 … 32 | 2 | beats | 0.247 | 20 ms | Continuous | both | Length of the drawn envelope (tempo mode). |

## Envelope 2

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `env2_a` | Env 2 Attack | 0 … 20 | 0.001 | s | 0.188 | 20 ms | Continuous | both | Attack time (seconds mode). |
| `env2_d` | Env 2 Decay | 0 … 20 | 0.4 | s | 0.188 | 20 ms | Continuous | both | Decay time (seconds mode). |
| `env2_s` | Env 2 Sustain | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Sustain level. |
| `env2_r` | Env 2 Release | 0 … 20 | 0.3 | s | 0.188 | 20 ms | Continuous | both | Release time (seconds mode). |
| `env2_ac` | Env 2 Attack Curve | -1 … 1 | 0 |  | 1 | 20 ms | Continuous | both | Attack curvature. |
| `env2_dc` | Env 2 Decay Curve | -1 … 1 | -0.5 |  | 1 | 20 ms | Continuous | both | Decay curvature. |
| `env2_rc` | Env 2 Release Curve | -1 … 1 | -0.5 |  | 1 | 20 ms | Continuous | both | Release curvature. |
| `env2_beats` | Env 2 Tempo Sync | 0 … 1 | 0 |  | 1 | — | Toggle | both | Stage times in beats instead of seconds. |
| `env2_a_b` | Env 2 Attack (beats) | 0 … 16 | 0 | beats | 0.25 | 20 ms | Continuous | both | Attack in beats (tempo mode). |
| `env2_d_b` | Env 2 Decay (beats) | 0 … 16 | 0.5 | beats | 0.25 | 20 ms | Continuous | both | Decay in beats (tempo mode). |
| `env2_r_b` | Env 2 Release (beats) | 0 … 16 | 0.25 | beats | 0.25 | 20 ms | Continuous | both | Release in beats (tempo mode). |
| `env2_mode` | Env 2 Mode | ADSR / Drawn | ADSR |  | 1 | — | Choice | both | ADSR or drawn multi-segment shape. |
| `env2_len` | Env 2 Drawn Length | 0.01 … 20 | 1 | s | 0.231 | 20 ms | Continuous | both | Length of the drawn envelope (seconds). |
| `env2_len_b` | Env 2 Drawn Length (beats) | 0.0625 … 32 | 2 | beats | 0.247 | 20 ms | Continuous | both | Length of the drawn envelope (tempo mode). |

## Envelope 3

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `env3_a` | Env 3 Attack | 0 … 20 | 0.001 | s | 0.188 | 20 ms | Continuous | both | Attack time (seconds mode). |
| `env3_d` | Env 3 Decay | 0 … 20 | 0.4 | s | 0.188 | 20 ms | Continuous | both | Decay time (seconds mode). |
| `env3_s` | Env 3 Sustain | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Sustain level. |
| `env3_r` | Env 3 Release | 0 … 20 | 0.3 | s | 0.188 | 20 ms | Continuous | both | Release time (seconds mode). |
| `env3_ac` | Env 3 Attack Curve | -1 … 1 | 0 |  | 1 | 20 ms | Continuous | both | Attack curvature. |
| `env3_dc` | Env 3 Decay Curve | -1 … 1 | -0.5 |  | 1 | 20 ms | Continuous | both | Decay curvature. |
| `env3_rc` | Env 3 Release Curve | -1 … 1 | -0.5 |  | 1 | 20 ms | Continuous | both | Release curvature. |
| `env3_beats` | Env 3 Tempo Sync | 0 … 1 | 0 |  | 1 | — | Toggle | both | Stage times in beats instead of seconds. |
| `env3_a_b` | Env 3 Attack (beats) | 0 … 16 | 0 | beats | 0.25 | 20 ms | Continuous | both | Attack in beats (tempo mode). |
| `env3_d_b` | Env 3 Decay (beats) | 0 … 16 | 0.5 | beats | 0.25 | 20 ms | Continuous | both | Decay in beats (tempo mode). |
| `env3_r_b` | Env 3 Release (beats) | 0 … 16 | 0.25 | beats | 0.25 | 20 ms | Continuous | both | Release in beats (tempo mode). |
| `env3_mode` | Env 3 Mode | ADSR / Drawn | ADSR |  | 1 | — | Choice | both | ADSR or drawn multi-segment shape. |
| `env3_len` | Env 3 Drawn Length | 0.01 … 20 | 1 | s | 0.231 | 20 ms | Continuous | both | Length of the drawn envelope (seconds). |
| `env3_len_b` | Env 3 Drawn Length (beats) | 0.0625 … 32 | 2 | beats | 0.247 | 20 ms | Continuous | both | Length of the drawn envelope (tempo mode). |

## Envelope 4

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `env4_a` | Env 4 Attack | 0 … 20 | 0.001 | s | 0.188 | 20 ms | Continuous | both | Attack time (seconds mode). |
| `env4_d` | Env 4 Decay | 0 … 20 | 0.4 | s | 0.188 | 20 ms | Continuous | both | Decay time (seconds mode). |
| `env4_s` | Env 4 Sustain | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Sustain level. |
| `env4_r` | Env 4 Release | 0 … 20 | 0.3 | s | 0.188 | 20 ms | Continuous | both | Release time (seconds mode). |
| `env4_ac` | Env 4 Attack Curve | -1 … 1 | 0 |  | 1 | 20 ms | Continuous | both | Attack curvature. |
| `env4_dc` | Env 4 Decay Curve | -1 … 1 | -0.5 |  | 1 | 20 ms | Continuous | both | Decay curvature. |
| `env4_rc` | Env 4 Release Curve | -1 … 1 | -0.5 |  | 1 | 20 ms | Continuous | both | Release curvature. |
| `env4_beats` | Env 4 Tempo Sync | 0 … 1 | 0 |  | 1 | — | Toggle | both | Stage times in beats instead of seconds. |
| `env4_a_b` | Env 4 Attack (beats) | 0 … 16 | 0 | beats | 0.25 | 20 ms | Continuous | both | Attack in beats (tempo mode). |
| `env4_d_b` | Env 4 Decay (beats) | 0 … 16 | 0.5 | beats | 0.25 | 20 ms | Continuous | both | Decay in beats (tempo mode). |
| `env4_r_b` | Env 4 Release (beats) | 0 … 16 | 0.25 | beats | 0.25 | 20 ms | Continuous | both | Release in beats (tempo mode). |
| `env4_mode` | Env 4 Mode | ADSR / Drawn | ADSR |  | 1 | — | Choice | both | ADSR or drawn multi-segment shape. |
| `env4_len` | Env 4 Drawn Length | 0.01 … 20 | 1 | s | 0.231 | 20 ms | Continuous | both | Length of the drawn envelope (seconds). |
| `env4_len_b` | Env 4 Drawn Length (beats) | 0.0625 … 32 | 2 | beats | 0.247 | 20 ms | Continuous | both | Length of the drawn envelope (tempo mode). |

## LFO 1

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `lfo1_rate` | LFO 1 Rate | 0.01 … 2000 | 2 | Hz | 0.111 | 20 ms | Continuous | both | Free rate (up to audio range). |
| `lfo1_sync` | LFO 1 Tempo Sync | 0 … 1 | 0 |  | 1 | — | Toggle | both | Rate from tempo division. |
| `lfo1_div` | LFO 1 Division | see choices | 1/4 |  | 1 | — | Choice | both | Tempo division incl. dotted/triplet. |
| `lfo1_phase` | LFO 1 Phase | 0 … 360 | 0 | deg | 1 | 20 ms | Continuous | both | Start phase. |
| `lfo1_offset` | LFO 1 Offset | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Output offset. |
| `lfo1_delay` | LFO 1 Delay | 0 … 10 | 0 | s | 0.301 | 20 ms | Continuous | both | Delay before the LFO starts. |
| `lfo1_fade` | LFO 1 Fade In | 0 … 10 | 0 | s | 0.301 | 20 ms | Continuous | both | Fade-in time after the delay. |
| `lfo1_smooth` | LFO 1 Smooth | 0 … 1 | 0 | % | 1 | 20 ms | Amount | both | Smooths the shape (slew). |
| `lfo1_mode` | LFO 1 Trigger Mode | Retrigger / Free / One Shot | Free |  | 1 | — | Choice | both | Retrigger on note, free-run, or one-shot. |
| `lfo1_morph` | LFO 1 Shape Morph | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Morph shape A > shape B. |
| `lfo1_morph_time` | LFO 1 Morph Time | 0 … 30 | 0 | s | 0.256 | 20 ms | Amount | both | Auto-morph from Morph to B over this time after note-on; 0 = manual. |

## LFO 2

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `lfo2_rate` | LFO 2 Rate | 0.01 … 2000 | 2 | Hz | 0.111 | 20 ms | Continuous | both | Free rate (up to audio range). |
| `lfo2_sync` | LFO 2 Tempo Sync | 0 … 1 | 0 |  | 1 | — | Toggle | both | Rate from tempo division. |
| `lfo2_div` | LFO 2 Division | see choices | 1/4 |  | 1 | — | Choice | both | Tempo division incl. dotted/triplet. |
| `lfo2_phase` | LFO 2 Phase | 0 … 360 | 0 | deg | 1 | 20 ms | Continuous | both | Start phase. |
| `lfo2_offset` | LFO 2 Offset | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Output offset. |
| `lfo2_delay` | LFO 2 Delay | 0 … 10 | 0 | s | 0.301 | 20 ms | Continuous | both | Delay before the LFO starts. |
| `lfo2_fade` | LFO 2 Fade In | 0 … 10 | 0 | s | 0.301 | 20 ms | Continuous | both | Fade-in time after the delay. |
| `lfo2_smooth` | LFO 2 Smooth | 0 … 1 | 0 | % | 1 | 20 ms | Amount | both | Smooths the shape (slew). |
| `lfo2_mode` | LFO 2 Trigger Mode | Retrigger / Free / One Shot | Free |  | 1 | — | Choice | both | Retrigger on note, free-run, or one-shot. |
| `lfo2_morph` | LFO 2 Shape Morph | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Morph shape A > shape B. |
| `lfo2_morph_time` | LFO 2 Morph Time | 0 … 30 | 0 | s | 0.256 | 20 ms | Amount | both | Auto-morph from Morph to B over this time after note-on; 0 = manual. |

## LFO 3

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `lfo3_rate` | LFO 3 Rate | 0.01 … 2000 | 2 | Hz | 0.111 | 20 ms | Continuous | both | Free rate (up to audio range). |
| `lfo3_sync` | LFO 3 Tempo Sync | 0 … 1 | 0 |  | 1 | — | Toggle | both | Rate from tempo division. |
| `lfo3_div` | LFO 3 Division | see choices | 1/4 |  | 1 | — | Choice | both | Tempo division incl. dotted/triplet. |
| `lfo3_phase` | LFO 3 Phase | 0 … 360 | 0 | deg | 1 | 20 ms | Continuous | both | Start phase. |
| `lfo3_offset` | LFO 3 Offset | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Output offset. |
| `lfo3_delay` | LFO 3 Delay | 0 … 10 | 0 | s | 0.301 | 20 ms | Continuous | both | Delay before the LFO starts. |
| `lfo3_fade` | LFO 3 Fade In | 0 … 10 | 0 | s | 0.301 | 20 ms | Continuous | both | Fade-in time after the delay. |
| `lfo3_smooth` | LFO 3 Smooth | 0 … 1 | 0 | % | 1 | 20 ms | Amount | both | Smooths the shape (slew). |
| `lfo3_mode` | LFO 3 Trigger Mode | Retrigger / Free / One Shot | Free |  | 1 | — | Choice | both | Retrigger on note, free-run, or one-shot. |
| `lfo3_morph` | LFO 3 Shape Morph | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Morph shape A > shape B. |
| `lfo3_morph_time` | LFO 3 Morph Time | 0 … 30 | 0 | s | 0.256 | 20 ms | Amount | both | Auto-morph from Morph to B over this time after note-on; 0 = manual. |

## LFO 4

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `lfo4_rate` | LFO 4 Rate | 0.01 … 2000 | 2 | Hz | 0.111 | 20 ms | Continuous | both | Free rate (up to audio range). |
| `lfo4_sync` | LFO 4 Tempo Sync | 0 … 1 | 0 |  | 1 | — | Toggle | both | Rate from tempo division. |
| `lfo4_div` | LFO 4 Division | see choices | 1/4 |  | 1 | — | Choice | both | Tempo division incl. dotted/triplet. |
| `lfo4_phase` | LFO 4 Phase | 0 … 360 | 0 | deg | 1 | 20 ms | Continuous | both | Start phase. |
| `lfo4_offset` | LFO 4 Offset | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Output offset. |
| `lfo4_delay` | LFO 4 Delay | 0 … 10 | 0 | s | 0.301 | 20 ms | Continuous | both | Delay before the LFO starts. |
| `lfo4_fade` | LFO 4 Fade In | 0 … 10 | 0 | s | 0.301 | 20 ms | Continuous | both | Fade-in time after the delay. |
| `lfo4_smooth` | LFO 4 Smooth | 0 … 1 | 0 | % | 1 | 20 ms | Amount | both | Smooths the shape (slew). |
| `lfo4_mode` | LFO 4 Trigger Mode | Retrigger / Free / One Shot | Free |  | 1 | — | Choice | both | Retrigger on note, free-run, or one-shot. |
| `lfo4_morph` | LFO 4 Shape Morph | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Morph shape A > shape B. |
| `lfo4_morph_time` | LFO 4 Morph Time | 0 … 30 | 0 | s | 0.256 | 20 ms | Amount | both | Auto-morph from Morph to B over this time after note-on; 0 = manual. |

## LFO 5

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `lfo5_rate` | LFO 5 Rate | 0.01 … 2000 | 2 | Hz | 0.111 | 20 ms | Continuous | both | Free rate (up to audio range). |
| `lfo5_sync` | LFO 5 Tempo Sync | 0 … 1 | 0 |  | 1 | — | Toggle | both | Rate from tempo division. |
| `lfo5_div` | LFO 5 Division | see choices | 1/4 |  | 1 | — | Choice | both | Tempo division incl. dotted/triplet. |
| `lfo5_phase` | LFO 5 Phase | 0 … 360 | 0 | deg | 1 | 20 ms | Continuous | both | Start phase. |
| `lfo5_offset` | LFO 5 Offset | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Output offset. |
| `lfo5_delay` | LFO 5 Delay | 0 … 10 | 0 | s | 0.301 | 20 ms | Continuous | both | Delay before the LFO starts. |
| `lfo5_fade` | LFO 5 Fade In | 0 … 10 | 0 | s | 0.301 | 20 ms | Continuous | both | Fade-in time after the delay. |
| `lfo5_smooth` | LFO 5 Smooth | 0 … 1 | 0 | % | 1 | 20 ms | Amount | both | Smooths the shape (slew). |
| `lfo5_mode` | LFO 5 Trigger Mode | Retrigger / Free / One Shot | Retrigger |  | 1 | — | Choice | both | Retrigger on note, free-run, or one-shot. |
| `lfo5_morph` | LFO 5 Shape Morph | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Morph shape A > shape B. |
| `lfo5_morph_time` | LFO 5 Morph Time | 0 … 30 | 0 | s | 0.256 | 20 ms | Amount | both | Auto-morph from Morph to B over this time after note-on; 0 = manual. |

## LFO 6

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `lfo6_rate` | LFO 6 Rate | 0.01 … 2000 | 2 | Hz | 0.111 | 20 ms | Continuous | both | Free rate (up to audio range). |
| `lfo6_sync` | LFO 6 Tempo Sync | 0 … 1 | 0 |  | 1 | — | Toggle | both | Rate from tempo division. |
| `lfo6_div` | LFO 6 Division | see choices | 1/4 |  | 1 | — | Choice | both | Tempo division incl. dotted/triplet. |
| `lfo6_phase` | LFO 6 Phase | 0 … 360 | 0 | deg | 1 | 20 ms | Continuous | both | Start phase. |
| `lfo6_offset` | LFO 6 Offset | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Output offset. |
| `lfo6_delay` | LFO 6 Delay | 0 … 10 | 0 | s | 0.301 | 20 ms | Continuous | both | Delay before the LFO starts. |
| `lfo6_fade` | LFO 6 Fade In | 0 … 10 | 0 | s | 0.301 | 20 ms | Continuous | both | Fade-in time after the delay. |
| `lfo6_smooth` | LFO 6 Smooth | 0 … 1 | 0 | % | 1 | 20 ms | Amount | both | Smooths the shape (slew). |
| `lfo6_mode` | LFO 6 Trigger Mode | Retrigger / Free / One Shot | Retrigger |  | 1 | — | Choice | both | Retrigger on note, free-run, or one-shot. |
| `lfo6_morph` | LFO 6 Shape Morph | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Morph shape A > shape B. |
| `lfo6_morph_time` | LFO 6 Morph Time | 0 … 30 | 0 | s | 0.256 | 20 ms | Amount | both | Auto-morph from Morph to B over this time after note-on; 0 = manual. |

## Step Seq 1

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `seq1_div` | Seq 1 Rate | see choices | 1/16 |  | 1 | — | Choice | both | Step length. |
| `seq1_steps` | Seq 1 Steps | 1 … 32 | 16 |  | 1 | — | Integer | both | Sequence length. |
| `seq1_slide` | Seq 1 Slide Time | 0 … 1 | 0.5 | % | 1 | 20 ms | Continuous | both | Glide time for steps with slide on (fraction of a step). |
| `seq1_mode` | Seq 1 Mode | Retrigger / Free | Free |  | 1 | — | Choice | both | Restart on note or follow the transport. |

## Step Seq 2

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `seq2_div` | Seq 2 Rate | see choices | 1/16 |  | 1 | — | Choice | both | Step length. |
| `seq2_steps` | Seq 2 Steps | 1 … 32 | 16 |  | 1 | — | Integer | both | Sequence length. |
| `seq2_slide` | Seq 2 Slide Time | 0 … 1 | 0.5 | % | 1 | 20 ms | Continuous | both | Glide time for steps with slide on (fraction of a step). |
| `seq2_mode` | Seq 2 Mode | Retrigger / Free | Free |  | 1 | — | Choice | both | Restart on note or follow the transport. |

## Random 1

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `rnd1_type` | Random 1 Type | Sample & Hold / Smooth / Chaos / Per Note | Sample & Hold |  | 1 | — | Choice | both | Random generator type. |
| `rnd1_rate` | Random 1 Rate | 0.01 … 100 | 4 | Hz | 0.215 | 20 ms | Continuous | both | Rate. |
| `rnd1_sync` | Random 1 Tempo Sync | 0 … 1 | 0 |  | 1 | — | Toggle | both | Tempo-synced rate. |
| `rnd1_div` | Random 1 Division | see choices | 1/8 |  | 1 | — | Choice | both | Tempo division. |
| `rnd1_bipolar` | Random 1 Bipolar | 0 … 1 | 1 |  | 1 | — | Toggle | both | Bipolar (-1..1) or unipolar (0..1). |

## Random 2

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `rnd2_type` | Random 2 Type | Sample & Hold / Smooth / Chaos / Per Note | Sample & Hold |  | 1 | — | Choice | both | Random generator type. |
| `rnd2_rate` | Random 2 Rate | 0.01 … 100 | 4 | Hz | 0.215 | 20 ms | Continuous | both | Rate. |
| `rnd2_sync` | Random 2 Tempo Sync | 0 … 1 | 0 |  | 1 | — | Toggle | both | Tempo-synced rate. |
| `rnd2_div` | Random 2 Division | see choices | 1/8 |  | 1 | — | Choice | both | Tempo division. |
| `rnd2_bipolar` | Random 2 Bipolar | 0 … 1 | 1 |  | 1 | — | Toggle | both | Bipolar (-1..1) or unipolar (0..1). |

## Random 3

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `rnd3_type` | Random 3 Type | Sample & Hold / Smooth / Chaos / Per Note | Sample & Hold |  | 1 | — | Choice | both | Random generator type. |
| `rnd3_rate` | Random 3 Rate | 0.01 … 100 | 4 | Hz | 0.215 | 20 ms | Continuous | both | Rate. |
| `rnd3_sync` | Random 3 Tempo Sync | 0 … 1 | 0 |  | 1 | — | Toggle | both | Tempo-synced rate. |
| `rnd3_div` | Random 3 Division | see choices | 1/8 |  | 1 | — | Choice | both | Tempo division. |
| `rnd3_bipolar` | Random 3 Bipolar | 0 … 1 | 1 |  | 1 | — | Toggle | both | Bipolar (-1..1) or unipolar (0..1). |

## Random 4

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `rnd4_type` | Random 4 Type | Sample & Hold / Smooth / Chaos / Per Note | Sample & Hold |  | 1 | — | Choice | both | Random generator type. |
| `rnd4_rate` | Random 4 Rate | 0.01 … 100 | 4 | Hz | 0.215 | 20 ms | Continuous | both | Rate. |
| `rnd4_sync` | Random 4 Tempo Sync | 0 … 1 | 0 |  | 1 | — | Toggle | both | Tempo-synced rate. |
| `rnd4_div` | Random 4 Division | see choices | 1/8 |  | 1 | — | Choice | both | Tempo division. |
| `rnd4_bipolar` | Random 4 Bipolar | 0 … 1 | 1 |  | 1 | — | Toggle | both | Bipolar (-1..1) or unipolar (0..1). |

## Macros

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `macro1` | Macro 1 | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Macro knob (assign by dragging onto any control). |
| `macro2` | Macro 2 | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Macro knob (assign by dragging onto any control). |
| `macro3` | Macro 3 | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Macro knob (assign by dragging onto any control). |
| `macro4` | Macro 4 | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Macro knob (assign by dragging onto any control). |
| `macro5` | Macro 5 | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Macro knob (assign by dragging onto any control). |
| `macro6` | Macro 6 | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Macro knob (assign by dragging onto any control). |
| `macro7` | Macro 7 | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Macro knob (assign by dragging onto any control). |
| `macro8` | Macro 8 | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Macro knob (assign by dragging onto any control). |

## Functions

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fn1_op` | Function 1 Op | Add / Multiply / Min / Max / Lag | Add |  | 1 | — | Choice | both | Combines inputs A and B into a new source. |
| `fn1_lag` | Function 1 Lag | 0 … 2000 | 0 | ms | 0.231 | 20 ms | Amount | both | Lag (slew) applied to the function output. |
| `fn2_op` | Function 2 Op | Add / Multiply / Min / Max / Lag | Add |  | 1 | — | Choice | both | Combines inputs A and B into a new source. |
| `fn2_lag` | Function 2 Lag | 0 … 2000 | 0 | ms | 0.231 | 20 ms | Amount | both | Lag (slew) applied to the function output. |
| `fn3_op` | Function 3 Op | Add / Multiply / Min / Max / Lag | Add |  | 1 | — | Choice | both | Combines inputs A and B into a new source. |
| `fn3_lag` | Function 3 Lag | 0 … 2000 | 0 | ms | 0.231 | 20 ms | Amount | both | Lag (slew) applied to the function output. |
| `fn4_op` | Function 4 Op | Add / Multiply / Min / Max / Lag | Add |  | 1 | — | Choice | both | Combines inputs A and B into a new source. |
| `fn4_lag` | Function 4 Lag | 0 … 2000 | 0 | ms | 0.231 | 20 ms | Amount | both | Lag (slew) applied to the function output. |

## Follower

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `ef_attack` | Follower Attack | 0.1 … 500 | 5 | ms | 0.215 | 20 ms | Continuous | both | Envelope follower attack. |
| `ef_release` | Follower Release | 1 … 2000 | 80 | ms | 0.231 | 20 ms | Continuous | both | Envelope follower release. |
| `ef_gain` | Follower Gain | -24 … 24 | 0 | dB | 1 | 20 ms | Continuous | both | Follower input gain. |
| `tr_sens` | Transient Sensitivity | 0 … 1 | 0.5 | % | 1 | 20 ms | Continuous | both | Transient detector sensitivity. |
| `tr_decay` | Transient Decay | 1 … 1000 | 60 | ms | 0.245 | 20 ms | Continuous | both | Transient source decay. |
| `ef_sidechain` | Follower Sidechain | 0 … 1 | 0 |  | 1 | — | Toggle | fx | Follower/transient listen to the sidechain input. |
| `fxp_env_trigger` | Envelope Trigger | MIDI / Transient / Both | Both |  | 1 | — | Choice | fx | What retriggers the envelopes in GNARL FX. |
| `fxp_in_gain` | Input Gain | -24 … 24 | 0 | dB | 1 | 20 ms | Neutral | fx | Input gain (0 dB = unchanged). |

## Mod Routes

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `mod1_depth` | Route 1 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 1 (source/destination set in the matrix). |
| `mod2_depth` | Route 2 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 2 (source/destination set in the matrix). |
| `mod3_depth` | Route 3 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 3 (source/destination set in the matrix). |
| `mod4_depth` | Route 4 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 4 (source/destination set in the matrix). |
| `mod5_depth` | Route 5 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 5 (source/destination set in the matrix). |
| `mod6_depth` | Route 6 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 6 (source/destination set in the matrix). |
| `mod7_depth` | Route 7 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 7 (source/destination set in the matrix). |
| `mod8_depth` | Route 8 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 8 (source/destination set in the matrix). |
| `mod9_depth` | Route 9 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 9 (source/destination set in the matrix). |
| `mod10_depth` | Route 10 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 10 (source/destination set in the matrix). |
| `mod11_depth` | Route 11 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 11 (source/destination set in the matrix). |
| `mod12_depth` | Route 12 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 12 (source/destination set in the matrix). |
| `mod13_depth` | Route 13 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 13 (source/destination set in the matrix). |
| `mod14_depth` | Route 14 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 14 (source/destination set in the matrix). |
| `mod15_depth` | Route 15 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 15 (source/destination set in the matrix). |
| `mod16_depth` | Route 16 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 16 (source/destination set in the matrix). |
| `mod17_depth` | Route 17 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 17 (source/destination set in the matrix). |
| `mod18_depth` | Route 18 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 18 (source/destination set in the matrix). |
| `mod19_depth` | Route 19 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 19 (source/destination set in the matrix). |
| `mod20_depth` | Route 20 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 20 (source/destination set in the matrix). |
| `mod21_depth` | Route 21 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 21 (source/destination set in the matrix). |
| `mod22_depth` | Route 22 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 22 (source/destination set in the matrix). |
| `mod23_depth` | Route 23 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 23 (source/destination set in the matrix). |
| `mod24_depth` | Route 24 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 24 (source/destination set in the matrix). |
| `mod25_depth` | Route 25 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 25 (source/destination set in the matrix). |
| `mod26_depth` | Route 26 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 26 (source/destination set in the matrix). |
| `mod27_depth` | Route 27 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 27 (source/destination set in the matrix). |
| `mod28_depth` | Route 28 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 28 (source/destination set in the matrix). |
| `mod29_depth` | Route 29 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 29 (source/destination set in the matrix). |
| `mod30_depth` | Route 30 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 30 (source/destination set in the matrix). |
| `mod31_depth` | Route 31 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 31 (source/destination set in the matrix). |
| `mod32_depth` | Route 32 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 32 (source/destination set in the matrix). |
| `mod33_depth` | Route 33 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 33 (source/destination set in the matrix). |
| `mod34_depth` | Route 34 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 34 (source/destination set in the matrix). |
| `mod35_depth` | Route 35 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 35 (source/destination set in the matrix). |
| `mod36_depth` | Route 36 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 36 (source/destination set in the matrix). |
| `mod37_depth` | Route 37 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 37 (source/destination set in the matrix). |
| `mod38_depth` | Route 38 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 38 (source/destination set in the matrix). |
| `mod39_depth` | Route 39 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 39 (source/destination set in the matrix). |
| `mod40_depth` | Route 40 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 40 (source/destination set in the matrix). |
| `mod41_depth` | Route 41 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 41 (source/destination set in the matrix). |
| `mod42_depth` | Route 42 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 42 (source/destination set in the matrix). |
| `mod43_depth` | Route 43 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 43 (source/destination set in the matrix). |
| `mod44_depth` | Route 44 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 44 (source/destination set in the matrix). |
| `mod45_depth` | Route 45 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 45 (source/destination set in the matrix). |
| `mod46_depth` | Route 46 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 46 (source/destination set in the matrix). |
| `mod47_depth` | Route 47 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 47 (source/destination set in the matrix). |
| `mod48_depth` | Route 48 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 48 (source/destination set in the matrix). |
| `mod49_depth` | Route 49 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 49 (source/destination set in the matrix). |
| `mod50_depth` | Route 50 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 50 (source/destination set in the matrix). |
| `mod51_depth` | Route 51 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 51 (source/destination set in the matrix). |
| `mod52_depth` | Route 52 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 52 (source/destination set in the matrix). |
| `mod53_depth` | Route 53 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 53 (source/destination set in the matrix). |
| `mod54_depth` | Route 54 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 54 (source/destination set in the matrix). |
| `mod55_depth` | Route 55 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 55 (source/destination set in the matrix). |
| `mod56_depth` | Route 56 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 56 (source/destination set in the matrix). |
| `mod57_depth` | Route 57 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 57 (source/destination set in the matrix). |
| `mod58_depth` | Route 58 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 58 (source/destination set in the matrix). |
| `mod59_depth` | Route 59 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 59 (source/destination set in the matrix). |
| `mod60_depth` | Route 60 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 60 (source/destination set in the matrix). |
| `mod61_depth` | Route 61 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 61 (source/destination set in the matrix). |
| `mod62_depth` | Route 62 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 62 (source/destination set in the matrix). |
| `mod63_depth` | Route 63 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 63 (source/destination set in the matrix). |
| `mod64_depth` | Route 64 Depth | -1 … 1 | 0 |  | 1 | 10 ms | Depth | both | Depth of modulation route 64 (source/destination set in the matrix). |

## FX Distortion A

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_dist1_on` | Distortion A On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_dist1_mix` | Distortion A Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_dist1_type` | Dist A Type | Soft Clip / Hard Clip / Tube / Valve / Diode / Wavefolder / Sine Fold / Rectify / Fuzz / Bit Reduce / Downsample / Draw | Soft Clip |  | 1 | — | Choice | both | Distortion curve. |
| `fx_dist1_drive` | Dist A Drive | 0 … 1 | 0.3 | % | 1 | 20 ms | Drive | both | Drive; 0 = none. |
| `fx_dist1_bias` | Dist A Bias | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Asymmetry bias into the shaper (DC-blocked). |
| `fx_dist1_tone` | Dist A Tone | 200 … 20000 | 20000 | Hz | 0.354 | 20 ms | Neutral | both | Post low-pass; max = open. |
| `fx_dist1_out` | Dist A Output | -24 … 12 | 0 | dB | 1 | 20 ms | Neutral | both | Output gain of the wet signal. |
| `fx_dist1_morph` | Dist A Draw Morph | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Draw type: morph curve 1 > curve 2. |

## FX Distortion B

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_dist2_on` | Distortion B On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_dist2_mix` | Distortion B Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_dist2_type` | Dist B Type | Soft Clip / Hard Clip / Tube / Valve / Diode / Wavefolder / Sine Fold / Rectify / Fuzz / Bit Reduce / Downsample / Draw | Soft Clip |  | 1 | — | Choice | both | Distortion curve. |
| `fx_dist2_drive` | Dist B Drive | 0 … 1 | 0.3 | % | 1 | 20 ms | Drive | both | Drive; 0 = none. |
| `fx_dist2_bias` | Dist B Bias | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Asymmetry bias into the shaper (DC-blocked). |
| `fx_dist2_tone` | Dist B Tone | 200 … 20000 | 20000 | Hz | 0.354 | 20 ms | Neutral | both | Post low-pass; max = open. |
| `fx_dist2_out` | Dist B Output | -24 … 12 | 0 | dB | 1 | 20 ms | Neutral | both | Output gain of the wet signal. |
| `fx_dist2_morph` | Dist B Draw Morph | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Draw type: morph curve 1 > curve 2. |

## FX Multiband Dist

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_mbd_on` | Multiband Dist On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_mbd_mix` | Multiband Dist Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_mbd_xlo` | MB Dist Low X-over | 40 … 2000 | 250 | Hz | 0.343 | 20 ms | Continuous | both | Low/mid crossover. |
| `fx_mbd_xhi` | MB Dist High X-over | 400 … 12000 | 2500 | Hz | 0.406 | 20 ms | Continuous | both | Mid/high crossover. |
| `fx_mbd_converge` | MB Dist Converge | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Narrow (+) or widen (-) the mid band around its centre. |
| `fx_mbd_b1_type` | MB Dist Low Type | Soft Clip / Hard Clip / Tube / Valve / Diode / Wavefolder / Sine Fold / Rectify / Fuzz / Bit Reduce / Downsample / Draw | Soft Clip |  | 1 | — | Choice | both | Band distortion type. |
| `fx_mbd_b1_drive` | MB Dist Low Drive | 0 … 1 | 0 | % | 1 | 20 ms | Drive | both | Band drive; 0 = clean. |
| `fx_mbd_b1_mix` | MB Dist Low Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Band dry/wet. |
| `fx_mbd_b1_dyn` | MB Dist Low Dynamics | -1 … 1 | 0 |  | 1 | 20 ms | Depth | both | -1 gate ... +1 compress. |
| `fx_mbd_b1_fb` | MB Dist Low Feedback | 0 … 0.95 | 0 | % | 1 | 20 ms | Amount | both | Band feedback. |
| `fx_mbd_b2_type` | MB Dist Mid Type | Soft Clip / Hard Clip / Tube / Valve / Diode / Wavefolder / Sine Fold / Rectify / Fuzz / Bit Reduce / Downsample / Draw | Soft Clip |  | 1 | — | Choice | both | Band distortion type. |
| `fx_mbd_b2_drive` | MB Dist Mid Drive | 0 … 1 | 0 | % | 1 | 20 ms | Drive | both | Band drive; 0 = clean. |
| `fx_mbd_b2_mix` | MB Dist Mid Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Band dry/wet. |
| `fx_mbd_b2_dyn` | MB Dist Mid Dynamics | -1 … 1 | 0 |  | 1 | 20 ms | Depth | both | -1 gate ... +1 compress. |
| `fx_mbd_b2_fb` | MB Dist Mid Feedback | 0 … 0.95 | 0 | % | 1 | 20 ms | Amount | both | Band feedback. |
| `fx_mbd_b3_type` | MB Dist High Type | Soft Clip / Hard Clip / Tube / Valve / Diode / Wavefolder / Sine Fold / Rectify / Fuzz / Bit Reduce / Downsample / Draw | Soft Clip |  | 1 | — | Choice | both | Band distortion type. |
| `fx_mbd_b3_drive` | MB Dist High Drive | 0 … 1 | 0 | % | 1 | 20 ms | Drive | both | Band drive; 0 = clean. |
| `fx_mbd_b3_mix` | MB Dist High Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Band dry/wet. |
| `fx_mbd_b3_dyn` | MB Dist High Dynamics | -1 … 1 | 0 |  | 1 | 20 ms | Depth | both | -1 gate ... +1 compress. |
| `fx_mbd_b3_fb` | MB Dist High Feedback | 0 … 0.95 | 0 | % | 1 | 20 ms | Amount | both | Band feedback. |

## FX Phaser

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_phaser_on` | Phaser On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_phaser_mix` | Phaser Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_phaser_rate` | Phaser Rate | 0.01 … 20 | 0.5 | Hz | 0.231 | 20 ms | Continuous | both | Sweep rate. |
| `fx_phaser_depth` | Phaser Depth | 0 … 1 | 0.5 | % | 1 | 20 ms | Continuous | both | Sweep depth (0 = static notches). |
| `fx_phaser_fb` | Phaser Feedback | -0.95 … 0.95 | 0.3 | % | 1 | 20 ms | Continuous | both | Feedback. |
| `fx_phaser_centre` | Phaser Centre | 50 … 10000 | 800 | Hz | 0.295 | 20 ms | Continuous | both | Centre frequency. |
| `fx_phaser_stages` | Phaser Stages | 2 … 12 | 6 |  | 1 | — | Integer | both | All-pass stages. |
| `fx_phaser_stereo` | Phaser Stereo | 0 … 180 | 90 | deg | 1 | 20 ms | Continuous | both | L/R LFO phase offset. |

## FX Flanger

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_flanger_on` | Flanger On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_flanger_mix` | Flanger Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_flanger_rate` | Flanger Rate | 0.01 … 20 | 0.3 | Hz | 0.231 | 20 ms | Continuous | both | Sweep rate. |
| `fx_flanger_depth` | Flanger Depth | 0 … 1 | 0.5 | % | 1 | 20 ms | Continuous | both | Sweep depth. |
| `fx_flanger_delay` | Flanger Delay | 0.1 … 20 | 2 | ms | 0.36 | 20 ms | Continuous | both | Base delay. |
| `fx_flanger_fb` | Flanger Feedback | -0.98 … 0.98 | 0.5 | % | 1 | 20 ms | Continuous | both | Feedback. |
| `fx_flanger_stereo` | Flanger Stereo | 0 … 180 | 90 | deg | 1 | 20 ms | Continuous | both | L/R phase offset. |

## FX Chorus

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_chorus_on` | Chorus On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_chorus_mix` | Chorus Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_chorus_rate` | Chorus Rate | 0.01 … 10 | 0.6 | Hz | 0.3 | 20 ms | Continuous | both | Rate. |
| `fx_chorus_depth` | Chorus Depth | 0 … 1 | 0.5 | % | 1 | 20 ms | Continuous | both | Depth. |
| `fx_chorus_delay` | Chorus Delay | 1 … 40 | 12 | ms | 0.548 | 20 ms | Continuous | both | Base delay. |
| `fx_chorus_width` | Chorus Width | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | both | Stereo width of the wet signal. |
| `fx_chorus_mono` | Chorus Mono Below | 20 … 500 | 150 | Hz | 0.531 | 20 ms | Continuous | both | Wet is mono below this frequency (dry low end untouched). |

## FX Comb

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_comb_on` | Comb On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_comb_mix` | Comb Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_comb_freq` | Comb Freq | 20 … 5000 | 200 | Hz | 0.241 | 20 ms | Continuous | both | Comb frequency. |
| `fx_comb_fb` | Comb Feedback | -0.99 … 0.99 | 0.7 | % | 1 | 20 ms | Continuous | both | Feedback (+/- polarity). |
| `fx_comb_damp` | Comb Damping | 0 … 1 | 0.2 | % | 1 | 20 ms | Amount | both | High damping in the loop. |
| `fx_comb_key` | Comb Key Track | 0 … 1 | 0 |  | 1 | — | Toggle | both | Frequency follows the last played note. |

## FX Vowel EQ

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_vowel_on` | Vowel EQ On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_vowel_mix` | Vowel EQ Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_vowel_morph` | Vowel Morph | 0 … 1 | 0 | % | 1 | 20 ms | Continuous | both | Vowel A>E>I>O>U (moves all 3 peaks). |
| `fx_vowel_shift` | Vowel Shift | -12 … 12 | 0 | st | 1 | 20 ms | Neutral | both | Shift the peaks. |
| `fx_vowel_q` | Vowel Q | 0.5 … 20 | 6 |  | 1 | 20 ms | Continuous | both | Peak sharpness. |
| `fx_vowel_gain` | Vowel Gain | 0 … 24 | 12 | dB | 1 | 20 ms | Amount | both | Peak boost; 0 dB = flat. |

## FX EQ

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_eq_on` | EQ On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_eq_mix` | EQ Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_eq_f1` | EQ Low Shelf Freq | 20 … 20000 | 120 | Hz | 0.23 | 20 ms | Continuous | both | Band frequency. |
| `fx_eq_g1` | EQ Low Shelf Gain | -24 … 24 | 0 | dB | 1 | 20 ms | Neutral | both | Band gain; 0 dB = flat. |
| `fx_eq_f2` | EQ Peak 1 Freq | 20 … 20000 | 600 | Hz | 0.23 | 20 ms | Continuous | both | Band frequency. |
| `fx_eq_g2` | EQ Peak 1 Gain | -24 … 24 | 0 | dB | 1 | 20 ms | Neutral | both | Band gain; 0 dB = flat. |
| `fx_eq_q2` | EQ Peak 1 Q | 0.1 … 18 | 1 |  | 1 | 20 ms | Continuous | both | Band Q. |
| `fx_eq_f3` | EQ Peak 2 Freq | 20 … 20000 | 3000 | Hz | 0.23 | 20 ms | Continuous | both | Band frequency. |
| `fx_eq_g3` | EQ Peak 2 Gain | -24 … 24 | 0 | dB | 1 | 20 ms | Neutral | both | Band gain; 0 dB = flat. |
| `fx_eq_q3` | EQ Peak 2 Q | 0.1 … 18 | 1 |  | 1 | 20 ms | Continuous | both | Band Q. |
| `fx_eq_f4` | EQ High Shelf Freq | 20 … 20000 | 8000 | Hz | 0.23 | 20 ms | Continuous | both | Band frequency. |
| `fx_eq_g4` | EQ High Shelf Gain | -24 … 24 | 0 | dB | 1 | 20 ms | Neutral | both | Band gain; 0 dB = flat. |

## FX Freq Shifter

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_fshift_on` | Freq Shifter On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_fshift_mix` | Freq Shifter Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_fshift_hz` | Shift | -2000 … 2000 | 0 | Hz | 1 | 20 ms | Continuous | both | Frequency shift in Hz. |
| `fx_fshift_fb` | Shift Feedback | 0 … 0.95 | 0 | % | 1 | 20 ms | Amount | both | Feedback. |
| `fx_fshift_stereo` | Shift Stereo | -1 … 1 | 0 |  | 1 | 20 ms | Neutral | both | Opposite shift on the right channel (fraction). |

## FX Ring Mod

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_ring_on` | Ring Mod On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_ring_mix` | Ring Mod Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_ring_freq` | Ring Freq | 1 … 5000 | 200 | Hz | 0.215 | 20 ms | Continuous | both | Modulator frequency. |
| `fx_ring_wave` | Ring Wave | Sine / Triangle / Square | Sine |  | 1 | — | Choice | both | Modulator waveform. |
| `fx_ring_key` | Ring Key Track | 0 … 1 | 0 |  | 1 | — | Toggle | both | Frequency follows the last note (freq becomes ratio×note). |

## FX Bitcrusher

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_crush_on` | Bitcrusher On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_crush_mix` | Bitcrusher Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_crush_bits` | Crush Bits | 1 … 24 | 8 | bits | 1 | 20 ms | Continuous | both | Bit depth. |
| `fx_crush_rate` | Crush Rate | 100 … 48000 | 12000 | Hz | 0.331 | 20 ms | Continuous | both | Sample-rate reduction target. |
| `fx_crush_dither` | Crush Dither | 0 … 1 | 0 |  | 1 | — | Toggle | both | TPDF dither before quantising. |

## FX Multiband Comp

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_mbcomp_on` | Multiband Comp On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_mbcomp_mix` | Multiband Comp Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_mbcomp_depth` | MB Comp Depth | 0 … 1 | 0.6 | % | 1 | 20 ms | Amount | both | Overall amount; 0 = unity. |
| `fx_mbcomp_time` | MB Comp Time | 0.1 … 10 | 1 | ratio | 1 | 20 ms | Continuous | both | Attack/release time scale. |
| `fx_mbcomp_up` | MB Comp Upward | 0 … 1 | 0.6 | % | 1 | 20 ms | Amount | both | Upward compression amount. |
| `fx_mbcomp_down` | MB Comp Downward | 0 … 1 | 0.8 | % | 1 | 20 ms | Amount | both | Downward compression amount. |
| `fx_mbcomp_xlo` | MB Comp Low X-over | 40 … 1000 | 120 | Hz | 0.32 | 20 ms | Continuous | both | Low/mid crossover. |
| `fx_mbcomp_xhi` | MB Comp High X-over | 1000 … 12000 | 2500 | Hz | 0.348 | 20 ms | Continuous | both | Mid/high crossover. |
| `fx_mbcomp_out` | MB Comp Output | -24 … 24 | 0 | dB | 1 | 20 ms | Neutral | both | Output gain. |

## FX Compressor

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_comp_on` | Compressor On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_comp_mix` | Compressor Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_comp_thresh` | Comp Threshold | -60 … 0 | -18 | dB | 1 | 20 ms | Continuous | both | Threshold. |
| `fx_comp_ratio` | Comp Ratio | 1 … 20 | 4 | ratio | 1 | 20 ms | Amount | both | Ratio; 1 = none. |
| `fx_comp_attack` | Comp Attack | 0.1 … 200 | 5 | ms | 0.231 | 20 ms | Continuous | both | Attack. |
| `fx_comp_release` | Comp Release | 5 … 2000 | 100 | ms | 0.228 | 20 ms | Continuous | both | Release. |
| `fx_comp_makeup` | Comp Makeup | -12 … 24 | 0 | dB | 1 | 20 ms | Neutral | both | Makeup gain. |
| `fx_comp_clip` | Comp Soft Clip | 0 … 1 | 0 |  | 1 | — | Toggle | both | Soft clip after makeup. |

## FX Delay

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_delay_on` | Delay On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_delay_mix` | Delay Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_delay_time` | Delay Time | 1 … 2000 | 250 | ms | 0.333 | 20 ms | Continuous | both | Delay time (free). |
| `fx_delay_sync` | Delay Sync | 0 … 1 | 0 |  | 1 | — | Toggle | both | Tempo-synced time. |
| `fx_delay_div` | Delay Division | see choices | 1/8 |  | 1 | — | Choice | both | Tempo division. |
| `fx_delay_fb` | Delay Feedback | 0 … 0.98 | 0.4 | % | 1 | 20 ms | Amount | both | Feedback. |
| `fx_delay_lp` | Delay High Cut | 200 … 20000 | 12000 | Hz | 0.42 | 20 ms | Continuous | both | Low-pass in the loop. |
| `fx_delay_hp` | Delay Low Cut | 20 … 2000 | 80 | Hz | 0.289 | 20 ms | Continuous | both | High-pass in the loop. |
| `fx_delay_pp` | Delay Ping-Pong | 0 … 1 | 0 |  | 1 | — | Toggle | both | Ping-pong between channels. |

## FX Reverb

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_reverb_on` | Reverb On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_reverb_mix` | Reverb Mix | 0 … 1 | 0.3 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_reverb_size` | Reverb Size | 0 … 1 | 0.5 | % | 1 | 20 ms | Continuous | both | Room size. |
| `fx_reverb_decay` | Reverb Decay | 0.1 … 20 | 2 | s | 0.295 | 20 ms | Continuous | both | Decay time (RT60). |
| `fx_reverb_pre` | Reverb Pre-delay | 0 … 250 | 10 | ms | 0.327 | 20 ms | Continuous | both | Pre-delay. |
| `fx_reverb_damp` | Reverb Damping | 0 … 1 | 0.4 | % | 1 | 20 ms | Amount | both | High damping. |
| `fx_reverb_duck` | Reverb Ducking | 0 … 1 | 0 | % | 1 | 20 ms | Amount | both | Duck the wet signal while input is loud. |
| `fx_reverb_width` | Reverb Width | 0 … 1 | 1 | % | 1 | 20 ms | Continuous | both | Wet stereo width. |

## FX Mod Delay

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_moddelay_on` | Mod Delay On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_moddelay_mix` | Mod Delay Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_moddelay_time` | Mod Delay Time | 0.1 … 50 | 5 | ms | 0.299 | 20 ms | Continuous | both | Delay time; click-free under fast modulation. |
| `fx_moddelay_rate` | Mod Delay LFO Rate | 0.01 … 2000 | 3 | Hz | 0.131 | 20 ms | Continuous | both | Internal modulation rate. |
| `fx_moddelay_depth` | Mod Delay LFO Depth | 0 … 1 | 0 | % | 1 | 20 ms | Amount | both | Internal modulation depth. |
| `fx_moddelay_fb` | Mod Delay Feedback | -0.98 … 0.98 | 0.6 | % | 1 | 20 ms | Continuous | both | Feedback. |
| `fx_moddelay_bp` | Mod Delay BP Freq | 50 … 12000 | 1500 | Hz | 0.329 | 20 ms | Continuous | both | Band-pass in the loop. |
| `fx_moddelay_q` | Mod Delay BP Q | 0.1 … 10 | 0.7 |  | 1 | 20 ms | Continuous | both | Band-pass Q. |

## FX Resonator

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_reso_on` | Resonator On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_reso_mix` | Resonator Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_reso_freq` | Resonator Freq | 20 … 2000 | 110 | Hz | 0.289 | 20 ms | Continuous | both | Root frequency. |
| `fx_reso_key` | Resonator Key Track | 0 … 1 | 0 |  | 1 | — | Toggle | both | Root follows the last note. |
| `fx_reso_fifth` | Resonator Fifth | 0 … 1 | 0 |  | 1 | — | Toggle | both | Adds a resonator a fifth above. |
| `fx_reso_decay` | Resonator Decay | 0.01 … 10 | 0.5 | s | 0.23 | 20 ms | Continuous | both | Decay time. |
| `fx_reso_bright` | Resonator Brightness | 0 … 1 | 0.5 | % | 1 | 20 ms | Continuous | both | Upper partial level. |
| `fx_reso_sc` | Resonator Sidechain Exciter | 0 … 1 | 0 |  | 1 | — | Toggle | fx | Excite the resonators from the sidechain input instead of the main input. |

## FX Disperser

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_disperse_on` | Disperser On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_disperse_mix` | Disperser Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_disperse_freq` | Disperser Freq | 50 … 10000 | 600 | Hz | 0.239 | 20 ms | Continuous | both | Centre of the phase smear. |
| `fx_disperse_amount` | Disperser Amount | 0 … 1 | 0.5 | % | 1 | 20 ms | Amount | both | Number of all-pass stages (0 = none). |
| `fx_disperse_pinch` | Disperser Pinch | 0.1 … 10 | 1 |  | 1 | 20 ms | Continuous | both | All-pass Q (narrow/wide smear). |
| `fx_disperse_spread` | Disperser Spread | 0 … 1 | 0 | % | 1 | 20 ms | Amount | both | Spread stage frequencies around the centre. |

## FX Spectral Shaper

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_spectral_on` | Spectral Shaper On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_spectral_mix` | Spectral Shaper Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_spectral_thresh` | Spectral Gate Threshold | -100 … 0 | -100 | dB | 1 | 20 ms | Amount | both | Per-bin gate threshold; -100 = open. |
| `fx_spectral_tilt` | Spectral Tilt | -6 … 6 | 0 | dB | 1 | 20 ms | Neutral | both | Spectral tilt in dB/octave around 1 kHz. |
| `fx_spectral_contrast` | Spectral Contrast | 0 … 1 | 0 | % | 1 | 20 ms | Amount | both | Exaggerate peaks against the spectral envelope. |
| `fx_spectral_smooth` | Spectral Smooth | 0 … 500 | 20 | ms | 0.301 | 20 ms | Continuous | both | Gate smoothing time. |

## FX Resonance Tamer

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_tamer_on` | Resonance Tamer On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_tamer_mix` | Resonance Tamer Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_tamer_depth` | Tamer Depth | 0 … 1 | 0.5 | % | 1 | 20 ms | Amount | both | Max suppression; 0 = none. |
| `fx_tamer_sens` | Tamer Sensitivity | 0 … 1 | 0.5 | % | 1 | 20 ms | Continuous | both | How peaky a band must be to be suppressed. |
| `fx_tamer_speed` | Tamer Speed | 1 … 500 | 30 | ms | 0.244 | 20 ms | Continuous | both | Response time. |
| `fx_tamer_lo` | Tamer Low Limit | 20 … 20000 | 200 | Hz | 0.23 | 20 ms | Continuous | both | Lowest band processed. |
| `fx_tamer_hi` | Tamer High Limit | 20 … 20000 | 16000 | Hz | 0.23 | 20 ms | Continuous | both | Highest band processed. |

## FX 3-Band Split

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_split_on` | 3-Band Split On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_split_mix` | 3-Band Split Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_split_xlo` | Split Low X-over | 40 … 1000 | 150 | Hz | 0.32 | 20 ms | Continuous | both | Low/mid crossover. |
| `fx_split_xhi` | Split High X-over | 800 … 12000 | 2500 | Hz | 0.368 | 20 ms | Continuous | both | Mid/high crossover. |
| `fx_split_b1_gain` | Split Low Level | 0 … 2 | 1 | gain (dB shown) | 1 | 20 ms | Level | both | Band level; 0 = silent. |
| `fx_split_b1_flt_on` | Split Low Filter On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Band filter. |
| `fx_split_b1_flt_type` | Split Low Filter Type | LP / BP / HP / Notch | BP |  | 1 | — | Choice | both | Band filter type. |
| `fx_split_b1_flt_cut` | Split Low Filter Cutoff | 20 … 20000 | 100 | Hz | 0.23 | 20 ms | Continuous | both | Band filter cutoff. |
| `fx_split_b1_flt_res` | Split Low Filter Reso | 0 … 1 | 0.3 | % | 1 | 20 ms | Continuous | both | Band filter resonance. |
| `fx_split_b1_ph_on` | Split Low Phaser On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Band phaser. |
| `fx_split_b1_ph_rate` | Split Low Phaser Rate | 0.01 … 20 | 0.5 | Hz | 0.231 | 20 ms | Continuous | both | Band phaser rate. |
| `fx_split_b1_ph_depth` | Split Low Phaser Depth | 0 … 1 | 0.6 | % | 1 | 20 ms | Continuous | both | Band phaser depth. |
| `fx_split_b1_ph_fb` | Split Low Phaser Feedback | -0.95 … 0.95 | 0.4 | % | 1 | 20 ms | Continuous | both | Band phaser feedback. |
| `fx_split_b1_dist_on` | Split Low Dist On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Band distortion. |
| `fx_split_b1_dist_type` | Split Low Dist Type | Soft Clip / Hard Clip / Tube / Valve / Diode / Wavefolder / Sine Fold / Rectify / Fuzz / Bit Reduce / Downsample / Draw | Soft Clip |  | 1 | — | Choice | both | Band distortion type. |
| `fx_split_b1_dist_drive` | Split Low Dist Drive | 0 … 1 | 0.5 | % | 1 | 20 ms | Drive | both | Band drive. |
| `fx_split_b1_comb_on` | Split Low Comb On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Band comb. |
| `fx_split_b1_comb_freq` | Split Low Comb Freq | 20 … 5000 | 300 | Hz | 0.241 | 20 ms | Continuous | both | Band comb frequency. |
| `fx_split_b1_comb_fb` | Split Low Comb Feedback | -0.99 … 0.99 | 0.6 | % | 1 | 20 ms | Continuous | both | Band comb feedback. |
| `fx_split_b1_crush_on` | Split Low Crush On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Band bitcrusher. |
| `fx_split_b1_crush_bits` | Split Low Crush Bits | 1 … 24 | 6 | bits | 1 | 20 ms | Continuous | both | Band bit depth. |
| `fx_split_b2_gain` | Split Mid Level | 0 … 2 | 1 | gain (dB shown) | 1 | 20 ms | Level | both | Band level; 0 = silent. |
| `fx_split_b2_flt_on` | Split Mid Filter On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Band filter. |
| `fx_split_b2_flt_type` | Split Mid Filter Type | LP / BP / HP / Notch | BP |  | 1 | — | Choice | both | Band filter type. |
| `fx_split_b2_flt_cut` | Split Mid Filter Cutoff | 20 … 20000 | 600 | Hz | 0.23 | 20 ms | Continuous | both | Band filter cutoff. |
| `fx_split_b2_flt_res` | Split Mid Filter Reso | 0 … 1 | 0.3 | % | 1 | 20 ms | Continuous | both | Band filter resonance. |
| `fx_split_b2_ph_on` | Split Mid Phaser On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Band phaser. |
| `fx_split_b2_ph_rate` | Split Mid Phaser Rate | 0.01 … 20 | 0.5 | Hz | 0.231 | 20 ms | Continuous | both | Band phaser rate. |
| `fx_split_b2_ph_depth` | Split Mid Phaser Depth | 0 … 1 | 0.6 | % | 1 | 20 ms | Continuous | both | Band phaser depth. |
| `fx_split_b2_ph_fb` | Split Mid Phaser Feedback | -0.95 … 0.95 | 0.4 | % | 1 | 20 ms | Continuous | both | Band phaser feedback. |
| `fx_split_b2_dist_on` | Split Mid Dist On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Band distortion. |
| `fx_split_b2_dist_type` | Split Mid Dist Type | Soft Clip / Hard Clip / Tube / Valve / Diode / Wavefolder / Sine Fold / Rectify / Fuzz / Bit Reduce / Downsample / Draw | Soft Clip |  | 1 | — | Choice | both | Band distortion type. |
| `fx_split_b2_dist_drive` | Split Mid Dist Drive | 0 … 1 | 0.5 | % | 1 | 20 ms | Drive | both | Band drive. |
| `fx_split_b2_comb_on` | Split Mid Comb On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Band comb. |
| `fx_split_b2_comb_freq` | Split Mid Comb Freq | 20 … 5000 | 300 | Hz | 0.241 | 20 ms | Continuous | both | Band comb frequency. |
| `fx_split_b2_comb_fb` | Split Mid Comb Feedback | -0.99 … 0.99 | 0.6 | % | 1 | 20 ms | Continuous | both | Band comb feedback. |
| `fx_split_b2_crush_on` | Split Mid Crush On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Band bitcrusher. |
| `fx_split_b2_crush_bits` | Split Mid Crush Bits | 1 … 24 | 6 | bits | 1 | 20 ms | Continuous | both | Band bit depth. |
| `fx_split_b3_gain` | Split High Level | 0 … 2 | 1 | gain (dB shown) | 1 | 20 ms | Level | both | Band level; 0 = silent. |
| `fx_split_b3_flt_on` | Split High Filter On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Band filter. |
| `fx_split_b3_flt_type` | Split High Filter Type | LP / BP / HP / Notch | BP |  | 1 | — | Choice | both | Band filter type. |
| `fx_split_b3_flt_cut` | Split High Filter Cutoff | 20 … 20000 | 3000 | Hz | 0.23 | 20 ms | Continuous | both | Band filter cutoff. |
| `fx_split_b3_flt_res` | Split High Filter Reso | 0 … 1 | 0.3 | % | 1 | 20 ms | Continuous | both | Band filter resonance. |
| `fx_split_b3_ph_on` | Split High Phaser On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Band phaser. |
| `fx_split_b3_ph_rate` | Split High Phaser Rate | 0.01 … 20 | 0.5 | Hz | 0.231 | 20 ms | Continuous | both | Band phaser rate. |
| `fx_split_b3_ph_depth` | Split High Phaser Depth | 0 … 1 | 0.6 | % | 1 | 20 ms | Continuous | both | Band phaser depth. |
| `fx_split_b3_ph_fb` | Split High Phaser Feedback | -0.95 … 0.95 | 0.4 | % | 1 | 20 ms | Continuous | both | Band phaser feedback. |
| `fx_split_b3_dist_on` | Split High Dist On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Band distortion. |
| `fx_split_b3_dist_type` | Split High Dist Type | Soft Clip / Hard Clip / Tube / Valve / Diode / Wavefolder / Sine Fold / Rectify / Fuzz / Bit Reduce / Downsample / Draw | Soft Clip |  | 1 | — | Choice | both | Band distortion type. |
| `fx_split_b3_dist_drive` | Split High Dist Drive | 0 … 1 | 0.5 | % | 1 | 20 ms | Drive | both | Band drive. |
| `fx_split_b3_comb_on` | Split High Comb On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Band comb. |
| `fx_split_b3_comb_freq` | Split High Comb Freq | 20 … 5000 | 300 | Hz | 0.241 | 20 ms | Continuous | both | Band comb frequency. |
| `fx_split_b3_comb_fb` | Split High Comb Feedback | -0.99 … 0.99 | 0.6 | % | 1 | 20 ms | Continuous | both | Band comb feedback. |
| `fx_split_b3_crush_on` | Split High Crush On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Band bitcrusher. |
| `fx_split_b3_crush_bits` | Split High Crush Bits | 1 … 24 | 6 | bits | 1 | 20 ms | Continuous | both | Band bit depth. |

## FX Vocoder

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_vocoder_on` | Vocoder On | 0 … 1 | 0 |  | 1 | — | Toggle | fx | Module on/off; off = bit-transparent. |
| `fx_vocoder_mix` | Vocoder Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | fx | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_vocoder_bands` | Vocoder Bands | 8 … 32 | 16 |  | 1 | — | Integer | fx | Number of bands. |
| `fx_vocoder_attack` | Vocoder Attack | 0.5 … 100 | 5 | ms | 0.295 | 20 ms | Continuous | fx | Band follower attack. |
| `fx_vocoder_release` | Vocoder Release | 5 … 1000 | 60 | ms | 0.268 | 20 ms | Continuous | fx | Band follower release. |
| `fx_vocoder_shift` | Vocoder Formant Shift | -12 … 12 | 0 | st | 1 | 20 ms | Neutral | fx | Shift carrier bands against modulator bands. |
| `fx_vocoder_hf` | Vocoder HF Pass | 0 … 1 | 0 | % | 1 | 20 ms | Amount | fx | Pass modulator highs (sibilance) through. |

## FX Width

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_width_on` | Width On | 0 … 1 | 0 |  | 1 | — | Toggle | both | Module on/off; off = bit-transparent. |
| `fx_width_mix` | Width Mix | 0 … 1 | 1 | % | 1 | 20 ms | Mix | both | Dry/wet: 0% = dry exactly, 100% = wet only. |
| `fx_width_amount` | Width | 0 … 2 | 1 |  | 1 | 20 ms | Neutral | both | Stereo width; 1 = unchanged, 0 = mono. |
| `fx_width_mono_on` | Mono Bass | 0 … 1 | 0 |  | 1 | — | Toggle | both | Make everything below Mono Freq mono. |
| `fx_width_mono_hz` | Mono Below | 20 … 1000 | 120 | Hz | 0.343 | 20 ms | Continuous | both | Mono crossover frequency. |

## FX Clipper

| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |
|---|---|---|---|---|---|---|---|---|---|
| `fx_clip_on` | Final Clipper On | 0 … 1 | 1 |  | 1 | — | Toggle | both | Final safety clipper/limiter. |
| `fx_clip_mode` | Final Stage Mode | Soft Clip / Hard Clip / Limiter | Soft Clip |  | 1 | — | Choice | both | Clipper type. |
| `fx_clip_ceiling` | Ceiling | -12 … 0 | -0.3 | dB | 1 | 20 ms | Continuous | both | Output ceiling. |
| `fx_clip_drive` | Clip Drive | 0 … 24 | 0 | dB | 1 | 20 ms | Neutral | both | Gain into the final stage. |
