#pragma once
// Non-parameter patch data. Immutable once published to the audio thread.
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace gnarl {

struct WavetableData;

// ---- modulation sources ----
enum ModSource : int {
    MS_None = -1,
    MS_Env1 = 0, MS_Env2, MS_Env3, MS_Env4,
    MS_Lfo1, MS_Lfo2, MS_Lfo3, MS_Lfo4, MS_Lfo5, MS_Lfo6,
    MS_Seq1, MS_Seq2,
    MS_Rnd1, MS_Rnd2, MS_Rnd3, MS_Rnd4,
    MS_Macro1, MS_Macro2, MS_Macro3, MS_Macro4, MS_Macro5, MS_Macro6, MS_Macro7, MS_Macro8,
    MS_OscA, MS_OscB, MS_OscC,
    MS_Velocity, MS_ReleaseVel, MS_KeyTrack, MS_VoiceCount, MS_ModWheel, MS_Aftertouch, MS_PitchBend,
    MS_MpePressure, MS_MpeSlide, MS_Follower, MS_Transient,
    MS_Fn1, MS_Fn2, MS_Fn3, MS_Fn4,
    MS_Count
};
const char* modSourceId(int s);    // stable string id, e.g. "lfo1"
const char* modSourceName(int s);  // display name
int modSourceFromId(const std::string& id);
bool modSourceIsBipolar(int s);    // natural polarity
bool modSourceIsPerVoice(int s);

enum RouteCurve : uint8_t {
    RC_Linear = 0, RC_Slow, RC_Fast, RC_LinearRev, RC_SlowRev, RC_FastRev,
    RC_Semi1, RC_Tone1, RC_Oct1, RC_Oct4, RC_Count
};
const char* routeCurveName(int c);

struct ModRoute {
    int source = MS_None;
    int dest = -1;       // registry index
    int via = MS_None;   // optional depth scaler source
    uint8_t curve = RC_Linear;
    bool bypass = false;
    bool bipolar = true; // false: source mapped to 0..1 before depth
    bool active() const { return source != MS_None && dest >= 0 && !bypass; }
};
constexpr int kNumRoutes = 64;

struct ShapePoint { float x = 0, y = 0, curve = 0; }; // curve: -1..1 bend of the segment ending at this point

struct LfoShape {
    std::vector<ShapePoint> a, b; // y in -1..1, x in 0..1 ascending, first x = 0, last x = 1
    int grid = 8;
};

struct StepSeqData {
    std::array<float, 32> value{};  // -1..1
    std::array<uint8_t, 32> slide{};
    std::array<float, 32> curve{};  // -1..1 shape of the slide
};

struct EnvDrawing {
    std::vector<ShapePoint> pts; // x 0..1 (fraction of length), y 0..1
    int sustain = -1;            // index of the sustain point, -1 = none
};

struct DrawCurve { std::vector<ShapePoint> pts; float smooth = 0.5f; }; // x,y in -1..1

struct FunctionDef { int a = MS_None, b = MS_None; };

constexpr int kNumFxSlots = 16;
constexpr int kSplitMiniModules = 5; // filter, phaser, dist, comb, crush

class PatchStructure {
public:
    PatchStructure();
    std::array<ModRoute, kNumRoutes> routes;
    std::array<LfoShape, 6> lfo;
    std::array<StepSeqData, 2> seq;
    std::array<EnvDrawing, 4> env;
    std::array<DrawCurve, 2> draw; // curve 1, curve 2 (morph target)
    std::array<FunctionDef, 4> fn;
    std::array<int, kNumFxSlots> fxSlots; // module index or -1
    std::array<std::array<int, kSplitMiniModules>, 3> splitOrder;
    std::array<std::string, 8> macroNames;
    std::array<std::shared_ptr<const WavetableData>, 3> userTable; // null = factory table by param
    std::array<std::string, 3> userTableName;

    // Lookup tables rendered from the drawings (built by finalize(), read by the audio thread).
    static constexpr int kTableSize = 1024;
    std::array<std::array<float, kTableSize + 1>, 6> lfoTableA{}, lfoTableB{};
    std::array<std::array<float, kTableSize + 1>, 4> envTable{};
    std::array<std::array<float, kTableSize + 1>, 2> drawTable{};
    std::array<int, 4> envSustainSample{}; // index into envTable, -1 none
    void finalize();

    int slotOf(int module) const;
    void placeModule(int module); // put into the first empty slot if absent
};

float evalShape(const std::vector<ShapePoint>& pts, float x);

} // namespace gnarl
