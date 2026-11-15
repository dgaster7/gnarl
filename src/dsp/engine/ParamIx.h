#pragma once
// Registry index lookup tables used by the DSP (resolved once at engine construction).
#include "params/Registry.h"
#include <array>
#include <string>

namespace gnarl {

inline int ix(const std::string& id) {
    int i = Registry::get().indexOf(id);
    if (i < 0) { std::fprintf(stderr, "GNARL: unknown parameter %s\n", id.c_str()); std::abort(); }
    return i;
}

struct OscIx {
    int engine, octave, semi, fine, phase, phaseMode, invert, fixed, fixedHz, level, pan, balance, fxSend, unison, detune, spread, blend, multi;
    int vaShape, vaPw, vaSync, hsDensity, hsSpread, hsSync, wtTable, wtPos, wtWarpMode, wtWarp, wtPwmDetune, fmAlg;
    struct Op { int ratio, fixed, freq, level, fb, wave, a, d, s, r; };
    std::array<Op, 4> op;
    int gfIndex, gfShift, gfSpread, gfDetune, gfInterp, gfComplex, gfMode;
    int haPartials, haTilt, haOddEven, haComb, haCombPeriod, haStretch, haFormantHz, haFormantGain, haFormantWidth;
    std::array<int, 19> sh;
    explicit OscIx(int k) {
        std::string o = std::string("osc") + char('a' + k) + "_";
        engine = ix(o + "engine"); octave = ix(o + "octave"); semi = ix(o + "semi"); fine = ix(o + "fine"); phase = ix(o + "phase");
        phaseMode = ix(o + "phase_mode"); invert = ix(o + "invert"); fixed = ix(o + "fixed"); fixedHz = ix(o + "fixed_hz");
        level = ix(o + "level"); pan = ix(o + "pan"); balance = ix(o + "balance"); fxSend = ix(o + "fx_send"); unison = ix(o + "unison");
        detune = ix(o + "detune"); spread = ix(o + "spread"); blend = ix(o + "blend"); multi = ix(o + "multi");
        vaShape = ix(o + "va_shape"); vaPw = ix(o + "va_pw"); vaSync = ix(o + "va_sync");
        hsDensity = ix(o + "hs_density"); hsSpread = ix(o + "hs_spread"); hsSync = ix(o + "hs_sync");
        wtTable = ix(o + "wt_table"); wtPos = ix(o + "wt_pos"); wtWarpMode = ix(o + "wt_warp_mode"); wtWarp = ix(o + "wt_warp"); wtPwmDetune = ix(o + "wt_pwm_detune");
        fmAlg = ix(o + "fm_alg");
        for (int i = 0; i < 4; ++i) {
            std::string p = o + "fm_op" + std::to_string(i + 1) + "_";
            op[(size_t) i] = {ix(p + "ratio"), ix(p + "fixed"), ix(p + "freq"), ix(p + "level"), ix(p + "fb"), ix(p + "wave"), ix(p + "a"), ix(p + "d"), ix(p + "s"), ix(p + "r")};
        }
        gfIndex = ix(o + "gf_index"); gfShift = ix(o + "gf_shift"); gfSpread = ix(o + "gf_spread"); gfDetune = ix(o + "gf_detune");
        gfInterp = ix(o + "gf_interp"); gfComplex = ix(o + "gf_complex"); gfMode = ix(o + "gf_mode");
        haPartials = ix(o + "ha_partials"); haTilt = ix(o + "ha_tilt"); haOddEven = ix(o + "ha_oddeven"); haComb = ix(o + "ha_comb");
        haCombPeriod = ix(o + "ha_comb_period"); haStretch = ix(o + "ha_stretch"); haFormantHz = ix(o + "ha_formant_hz");
        haFormantGain = ix(o + "ha_formant_gain"); haFormantWidth = ix(o + "ha_formant_width");
        const char* sn[19] = {"dc", "sym", "warp", "twist", "drive", "window", "offset", "wave", "bits", "shrink", "selfsync", "multipoint", "mp_width", "lpf", "hpf", "fold", "spectrum", "rnd_all", "rnd_odd"};
        for (int i = 0; i < 19; ++i) sh[(size_t) i] = ix(o + "sh_" + sn[i]);
    }
};

struct FilterIx {
    int type, cutoff, reso, sep, morph, gender, shoulder, shelf, poles, stages, opposing, keytrack, drive, driveType, drivePos, env, envSrc, boost, limiter, pan, level;
    explicit FilterIx(int k) {
        std::string f = "f" + std::to_string(k + 1) + "_";
        type = ix(f + "type"); cutoff = ix(f + "cutoff"); reso = ix(f + "reso"); sep = ix(f + "sep"); morph = ix(f + "morph"); gender = ix(f + "gender");
        shoulder = ix(f + "shoulder"); shelf = ix(f + "shelf"); poles = ix(f + "poles"); stages = ix(f + "stages"); opposing = ix(f + "opposing");
        keytrack = ix(f + "keytrack"); drive = ix(f + "drive"); driveType = ix(f + "drive_type"); drivePos = ix(f + "drive_pos"); env = ix(f + "env");
        envSrc = ix(f + "env_src"); boost = ix(f + "boost"); limiter = ix(f + "limiter"); pan = ix(f + "pan"); level = ix(f + "level");
    }
};

struct EnvIx {
    int a, d, s, r, ac, dc, rc, beats, ab, db, rb, mode, len, lenb;
    explicit EnvIx(int k) {
        std::string e = "env" + std::to_string(k + 1) + "_";
        a = ix(e + "a"); d = ix(e + "d"); s = ix(e + "s"); r = ix(e + "r"); ac = ix(e + "ac"); dc = ix(e + "dc"); rc = ix(e + "rc");
        beats = ix(e + "beats"); ab = ix(e + "a_b"); db = ix(e + "d_b"); rb = ix(e + "r_b"); mode = ix(e + "mode"); len = ix(e + "len"); lenb = ix(e + "len_b");
    }
};

struct LfoIx {
    int rate, sync, div, phase, offset, delay, fade, smooth, mode, morph, morphTime;
    explicit LfoIx(int k) {
        std::string l = "lfo" + std::to_string(k + 1) + "_";
        rate = ix(l + "rate"); sync = ix(l + "sync"); div = ix(l + "div"); phase = ix(l + "phase"); offset = ix(l + "offset");
        delay = ix(l + "delay"); fade = ix(l + "fade"); smooth = ix(l + "smooth"); mode = ix(l + "mode"); morph = ix(l + "morph"); morphTime = ix(l + "morph_time");
    }
};

struct GlobalIx {
    int master, voices, legato, glideTime, glideCurve, glideLegato, bendRange, oversampling, renderOs, postDriveType, postDrive;
    int routing, satType, satDrive, satMix, link, linkCutoff, linkReso, linkPan;
    int subWave, subOctave, subLevel, subDirect, noiseType, noiseLevel, noiseTrack, noiseBalance;
    std::array<int, 3> linkMode, linkAmount;
    std::array<int, 2> seqDiv, seqSteps, seqSlide, seqMode;
    std::array<int, 4> rndType, rndRate, rndSync, rndDiv, rndBipolar, fnOp, fnLag;
    std::array<int, 8> macro;
    std::array<int, 64> routeDepth;
    int efAttack, efRelease, efGain, trSens, trDecay, efSidechain, envTrigger, inGain;
    int clipOn, clipMode, clipCeiling, clipDrive;
    GlobalIx() {
        master = ix("master_level"); voices = ix("voices"); legato = ix("legato"); glideTime = ix("glide_time"); glideCurve = ix("glide_curve");
        glideLegato = ix("glide_legato"); bendRange = ix("bend_range"); oversampling = ix("oversampling"); renderOs = ix("render_os");
        postDriveType = ix("post_drive_type"); postDrive = ix("post_drive");
        routing = ix("filter_routing"); satType = ix("sat_type"); satDrive = ix("sat_drive"); satMix = ix("sat_mix"); link = ix("filter_link");
        linkCutoff = ix("link_cutoff"); linkReso = ix("link_reso"); linkPan = ix("link_pan");
        subWave = ix("sub_wave"); subOctave = ix("sub_octave"); subLevel = ix("sub_level"); subDirect = ix("sub_direct");
        noiseType = ix("noise_type"); noiseLevel = ix("noise_level"); noiseTrack = ix("noise_track"); noiseBalance = ix("noise_balance");
        const char* ln[3] = {"ab", "bc", "ca"};
        for (int i = 0; i < 3; ++i) { linkMode[(size_t) i] = ix(std::string("link_") + ln[i] + "_mode"); linkAmount[(size_t) i] = ix(std::string("link_") + ln[i] + "_amount"); }
        for (int i = 0; i < 2; ++i) { std::string s = "seq" + std::to_string(i + 1) + "_"; seqDiv[(size_t) i] = ix(s + "div"); seqSteps[(size_t) i] = ix(s + "steps"); seqSlide[(size_t) i] = ix(s + "slide"); seqMode[(size_t) i] = ix(s + "mode"); }
        for (int i = 0; i < 4; ++i) {
            std::string s = "rnd" + std::to_string(i + 1) + "_";
            rndType[(size_t) i] = ix(s + "type"); rndRate[(size_t) i] = ix(s + "rate"); rndSync[(size_t) i] = ix(s + "sync"); rndDiv[(size_t) i] = ix(s + "div"); rndBipolar[(size_t) i] = ix(s + "bipolar");
            std::string f = "fn" + std::to_string(i + 1) + "_";
            fnOp[(size_t) i] = ix(f + "op"); fnLag[(size_t) i] = ix(f + "lag");
        }
        for (int i = 0; i < 8; ++i) macro[(size_t) i] = ix("macro" + std::to_string(i + 1));
        for (int i = 0; i < 64; ++i) routeDepth[(size_t) i] = ix("mod" + std::to_string(i + 1) + "_depth");
        efAttack = ix("ef_attack"); efRelease = ix("ef_release"); efGain = ix("ef_gain"); trSens = ix("tr_sens"); trDecay = ix("tr_decay");
        efSidechain = ix("ef_sidechain"); envTrigger = ix("fxp_env_trigger"); inGain = ix("fxp_in_gain");
        clipOn = ix("fx_clip_on"); clipMode = ix("fx_clip_mode"); clipCeiling = ix("fx_clip_ceiling"); clipDrive = ix("fx_clip_drive");
    }
};

} // namespace gnarl
