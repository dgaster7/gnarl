#pragma once
// GNARL parameter registry: the single source of truth for every host parameter.
// IDs are APPEND-ONLY. Never rename, reorder-in-meaning, or reuse an ID.
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace gnarl {

enum class Unit : uint8_t { None, Hz, Semitones, Cents, Percent, Gain, Db, Ms, Seconds, Beats, Degrees, Ratio, Bits, Octaves };

// Role drives which Check-1 tests apply.
enum class Role : uint8_t {
    Continuous, // generic continuous control (effect + automation + round trip)
    Mix,        // 0 = dry exactly, 1 = wet only (zero + full-wet tests)
    Amount,     // 0 = no effect (zero test)
    Drive,      // 0 = no effect (zero test)
    Depth,      // 0 = no effect (zero test); may be bipolar (0 at centre)
    Send,       // 0 = no effect (zero test)
    Level,      // 0 = true silence
    Neutral,    // a value (default) that leaves signal unchanged (e.g. width 1, pan 0, gain 0 dB)
    Choice,
    Toggle,
    Integer
};

enum PluginMask : uint8_t { kSynth = 1, kFx = 2, kBoth = 3 };

// Where the parameter's DSP lives (determines which modulation sources may reach it per-voice).
enum class Scope : uint8_t { Voice, Global };

struct ContextValue { std::string id; float value; };

// A test-only modulation route used to make modulation-shape parameters (LFO, envelope, ...) audible.
struct ContextRoute { std::string source; std::string dest; float depth; };

struct ParamDef {
    std::string id;
    std::string name;
    std::string section;
    float min = 0, max = 1, def = 0;
    float skew = 1;      // JUCE NormalisableRange skew (norm = p^skew)
    float interval = 0;  // 0 = continuous, 1 = integer steps
    Unit unit = Unit::None;
    Role role = Role::Continuous;
    Scope scope = Scope::Voice;
    uint8_t plugins = kBoth;
    float smoothMs = 20;
    bool modulatable = true;
    std::vector<std::string> choices;
    std::string doc;                    // what it must audibly do
    std::vector<ContextValue> context;  // values that make the parameter active for testing
    std::vector<ContextRoute> contextRoutes;
    float neutral = 0;                  // for Neutral / bipolar Depth: the "no effect" value
    float zeroValue() const { return (role == Role::Depth || role == Role::Neutral) ? neutral : min; }

    bool isDiscrete() const { return interval > 0 || role == Role::Choice || role == Role::Toggle || role == Role::Integer; }

    float toNorm(float v) const {
        if (max <= min) return 0;
        float p = (v - min) / (max - min);
        p = p < 0 ? 0 : (p > 1 ? 1 : p);
        return skew == 1 ? p : std::pow(p, skew);
    }
    float fromNorm(float n) const {
        n = n < 0 ? 0 : (n > 1 ? 1 : n);
        float p = (skew == 1 || n <= 0) ? n : std::exp(std::log(n) / skew);
        float v = min + (max - min) * p;
        if (interval > 0) v = min + std::round((v - min) / interval) * interval;
        return v < min ? min : (v > max ? max : v);
    }
};

class Registry {
public:
    static const Registry& get();
    const std::vector<ParamDef>& all() const { return params; }
    int size() const { return (int) params.size(); }
    int indexOf(const std::string& id) const;           // -1 if unknown
    const ParamDef& operator[](int i) const { return params[(size_t) i]; }
    const ParamDef* find(const std::string& id) const { int i = indexOf(id); return i < 0 ? nullptr : &params[(size_t) i]; }
    std::vector<int> forPlugin(uint8_t mask) const;

private:
    Registry();
    std::vector<ParamDef> params;
    std::unordered_map<std::string, int> index;
};

// Tempo-sync divisions shared by LFOs, sequencers, random sources, delay.
struct SyncDivision { const char* name; double beats; };
const std::vector<SyncDivision>& syncDivisions();

// Static choice lists that DSP and UI share.
namespace choices {
const std::vector<std::string>& oscEngines();
const std::vector<std::string>& filterTypes();
const std::vector<std::string>& distTypes();
const std::vector<std::string>& warpModes();
const std::vector<std::string>& chainModes();
const std::vector<std::string>& fmWaves();
} // namespace choices

// Filter type indices (must match choices::filterTypes order; append only).
enum FilterType : int {
    FT_Off = 0, FT_LP12, FT_LP24, FT_BP12, FT_BP24, FT_HP12, FT_HP24, FT_Notch12, FT_Notch24,
    FT_Ladder, FT_SepLP24, FT_SepLP36, FT_TriPeak, FT_DualResoBP, FT_DualResoBR, FT_Morph,
    FT_CombPlus, FT_CombMinus, FT_Formant, FT_Phaser, FT_Count
};
enum DistType : int {
    DT_Soft = 0, DT_Hard, DT_Tube, DT_Valve, DT_Diode, DT_Fold, DT_SineFold, DT_Rectify, DT_Fuzz,
    DT_Bits, DT_Downsample, DT_Draw, DT_Count
};
enum OscEngine : int { OE_VA = 0, OE_HyperSaw, OE_Wavetable, OE_FM, OE_GrainFormant, OE_Harmonic, OE_Count };
enum WarpMode : int { WM_None = 0, WM_BendPlus, WM_BendMinus, WM_Sync, WM_PWM, WM_Mirror, WM_PhaseDist, WM_Fold, WM_Quantize, WM_Count };
enum ChainMode : int { CM_Add = 0, CM_Ring, CM_Sync, CM_PM, CM_FM, CM_AM, CM_Count };

// Ordered FX module identifiers (index = module id used by slot structure; append only).
const std::vector<std::string>& fxModuleIds();     // e.g. "dist1"
const std::vector<std::string>& fxModuleNames();   // e.g. "Distortion A"

} // namespace gnarl
