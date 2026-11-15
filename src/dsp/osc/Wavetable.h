#pragma once
#include <memory>
#include <string>
#include <vector>

namespace gnarl {

constexpr int kCycle = 2048;       // samples per frame
constexpr int kMaxFrames = 256;

struct WavetableData {
    std::string name;
    int frames = 1;
    std::vector<float> data;        // frames * kCycle
    const float* frame(int i) const { i = i < 0 ? 0 : (i >= frames ? frames - 1 : i); return data.data() + (size_t) i * kCycle; }
};

// 64 original factory tables, generated in code at first use (call from a non-audio thread first).
const std::vector<std::shared_ptr<const WavetableData>>& factoryTables();
int numFactoryTables();

// Serum-style import: mono/stereo PCM16/24/32f WAV, 2048-sample frames (uses the 'clm ' chunk if present).
// Returns null on failure (error set).
std::shared_ptr<WavetableData> loadWavetableWav(const std::vector<unsigned char>& bytes, const std::string& name, std::string* error);
std::vector<unsigned char> saveWavetableWav(const WavetableData& t);

} // namespace gnarl
