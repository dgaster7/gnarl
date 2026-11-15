#include "Engine.h"
#include "params/Registry.h"
#include <cstring>

namespace gnarl {
class PatchStructure {};
class EngineImpl {
public:
    bool fx = false;
    std::vector<float> params;
};
Engine::Engine(bool fxPlugin) : d(std::make_unique<EngineImpl>()) {
    d->fx = fxPlugin;
    auto& R = Registry::get();
    d->params.resize((size_t) R.size());
    for (int i = 0; i < R.size(); ++i) d->params[(size_t) i] = R[i].def;
}
Engine::~Engine() = default;
void Engine::prepare(double, int) {}
void Engine::reset() {}
void Engine::setParam(int i, float v) { d->params[(size_t) i] = v; }
float Engine::getParam(int i) const { return d->params[(size_t) i]; }
void Engine::setStructure(std::shared_ptr<const PatchStructure>) {}
void Engine::process(const float* inL, const float* inR, float* outL, float* outR, int n, const MidiEvent*, int, const HostInfo&, const float*, const float*) {
    if (d->fx && inL && inR) { std::memmove(outL, inL, sizeof(float) * (size_t) n); std::memmove(outR, inR, sizeof(float) * (size_t) n); }
    else { std::memset(outL, 0, sizeof(float) * (size_t) n); std::memset(outR, 0, sizeof(float) * (size_t) n); }
}
int Engine::latencySamples() const { return 0; }
double Engine::tailSeconds() const { return 0; }
bool Engine::isFx() const { return d->fx; }
} // namespace gnarl
