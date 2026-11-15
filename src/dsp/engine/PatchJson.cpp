#include "PatchJson.h"
#include "params/Registry.h"
#include <algorithm>

namespace gnarl {

static Json pointsToJson(const std::vector<ShapePoint>& pts) {
    Json a = Json::array();
    for (auto& p : pts) { Json q = Json::array(); q.push(p.x); q.push(p.y); q.push(p.curve); a.push(q); }
    return a;
}
static std::vector<ShapePoint> pointsFromJson(const Json& a, float ylo, float yhi, float xlo = 0, float xhi = 1) {
    std::vector<ShapePoint> v;
    for (auto& q : a.elements()) {
        ShapePoint p;
        p.x = std::clamp((float) q.at(0).num(), xlo, xhi);
        p.y = std::clamp((float) q.at(1).num(), ylo, yhi);
        p.curve = std::clamp((float) q.at(2).num(), -1.f, 1.f);
        v.push_back(p);
    }
    std::stable_sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.x < b.x; });
    return v;
}

Json patchToJson(const Patch& p, bool fxPlugin) {
    auto& R = Registry::get();
    Json j = Json::object();
    j["schemaVersion"] = kPatchSchemaVersion;
    j["plugin"] = fxPlugin ? "GNARL FX" : "GNARL";
    j["version"] = GNARL_VERSION_STRING;
    j["name"] = p.name; j["category"] = p.category; j["author"] = p.author;
    Json tags = Json::array(); for (auto& t : p.tags) tags.push(t); j["tags"] = tags;
    Json params = Json::object();
    for (int i : R.forPlugin(fxPlugin ? kFx : kSynth)) params[R[i].id] = (double) p.values[(size_t) i];
    j["params"] = params;
    const auto& s = *p.structure;
    Json st = Json::object();
    Json routes = Json::array();
    for (int k = 0; k < kNumRoutes; ++k) {
        auto& r = s.routes[(size_t) k];
        if (r.source == MS_None && r.dest < 0) continue;
        Json o = Json::object();
        o["slot"] = k; o["source"] = modSourceId(r.source); o["dest"] = r.dest >= 0 ? R[r.dest].id : std::string();
        o["via"] = modSourceId(r.via); o["curve"] = (int) r.curve; o["bypass"] = r.bypass; o["bipolar"] = r.bipolar;
        routes.push(o);
    }
    st["routes"] = routes;
    Json lfos = Json::array();
    for (auto& l : s.lfo) { Json o = Json::object(); o["a"] = pointsToJson(l.a); o["b"] = pointsToJson(l.b); o["grid"] = l.grid; lfos.push(o); }
    st["lfo"] = lfos;
    Json seqs = Json::array();
    for (auto& q : s.seq) {
        Json o = Json::object(); Json v = Json::array(), sl = Json::array(), cu = Json::array();
        for (int i = 0; i < 32; ++i) { v.push(q.value[(size_t) i]); sl.push((int) q.slide[(size_t) i]); cu.push(q.curve[(size_t) i]); }
        o["value"] = v; o["slide"] = sl; o["curve"] = cu; seqs.push(o);
    }
    st["seq"] = seqs;
    Json envs = Json::array();
    for (auto& e : s.env) { Json o = Json::object(); o["points"] = pointsToJson(e.pts); o["sustain"] = e.sustain; envs.push(o); }
    st["env"] = envs;
    Json draws = Json::array();
    for (auto& d : s.draw) { Json o = Json::object(); o["points"] = pointsToJson(d.pts); o["smooth"] = d.smooth; draws.push(o); }
    st["draw"] = draws;
    Json fns = Json::array();
    for (auto& f : s.fn) { Json o = Json::object(); o["a"] = modSourceId(f.a); o["b"] = modSourceId(f.b); fns.push(o); }
    st["functions"] = fns;
    Json slots = Json::array();
    for (int m : s.fxSlots) slots.push(m >= 0 ? Json(fxModuleIds()[(size_t) m]) : Json());
    st["fxSlots"] = slots;
    Json split = Json::array();
    for (auto& o : s.splitOrder) { Json a = Json::array(); for (int m : o) a.push(m); split.push(a); }
    st["splitOrder"] = split;
    Json macros = Json::array(); for (auto& m : s.macroNames) macros.push(m); st["macroNames"] = macros;
    Json tables = Json::array(); for (auto& t : s.userTableName) tables.push(t); st["userTables"] = tables;
    j["structure"] = st;
    return j;
}

Json migratePatchJson(Json j) {
    // schemaVersion 1 is the first public format. Future: if (v < 2) { ...transform...; v = 2; }
    if (!j.has("schemaVersion")) j["schemaVersion"] = 1;
    return j;
}

bool patchFromJson(const Json& in, Patch& out, std::string* error) {
    if (!in.isObject()) { if (error) *error = "not an object"; return false; }
    Json j = migratePatchJson(in);
    if (j["schemaVersion"].num() > kPatchSchemaVersion) { if (error) *error = "preset was saved by a newer GNARL"; }
    auto& R = Registry::get();
    out = Patch();
    out.name = j["name"].str(); out.category = j["category"].str(); out.author = j["author"].str();
    for (auto& t : j["tags"].elements()) out.tags.push_back(t.str());
    for (auto& [k, v] : j["params"].items()) {
        int i = R.indexOf(k);
        if (i < 0 || !v.isNumber()) continue;
        out.values[(size_t) i] = std::clamp((float) v.num(), R[i].min, R[i].max);
    }
    auto& s = *out.structure;
    const Json& st = j["structure"];
    if (st.isObject()) {
        if (st.has("routes")) {
            for (auto& r : s.routes) r = ModRoute();
            for (auto& o : st["routes"].elements()) {
                int k = (int) o["slot"].num(-1);
                if (k < 0 || k >= kNumRoutes) continue;
                auto& r = s.routes[(size_t) k];
                r.source = modSourceFromId(o["source"].str());
                r.dest = R.indexOf(o["dest"].str());
                if (r.dest >= 0 && !R[r.dest].modulatable) r.dest = -1;
                r.via = modSourceFromId(o["via"].str());
                r.curve = (uint8_t) std::clamp((int) o["curve"].num(), 0, RC_Count - 1);
                r.bypass = o["bypass"].boolean();
                r.bipolar = o["bipolar"].boolean(true);
            }
        }
        for (size_t k = 0; k < 6 && k < st["lfo"].size(); ++k) {
            auto& o = st["lfo"].at(k);
            auto a = pointsFromJson(o["a"], -1, 1), b = pointsFromJson(o["b"], -1, 1);
            if (a.size() >= 2) s.lfo[k].a = a;
            if (b.size() >= 2) s.lfo[k].b = b;
            s.lfo[k].grid = std::clamp((int) o["grid"].num(8), 1, 64);
        }
        for (size_t k = 0; k < 2 && k < st["seq"].size(); ++k) {
            auto& o = st["seq"].at(k);
            for (size_t i = 0; i < 32; ++i) {
                s.seq[k].value[i] = std::clamp((float) o["value"].at(i).num(), -1.f, 1.f);
                s.seq[k].slide[i] = (uint8_t) (o["slide"].at(i).num() != 0);
                s.seq[k].curve[i] = std::clamp((float) o["curve"].at(i).num(), -1.f, 1.f);
            }
        }
        for (size_t k = 0; k < 4 && k < st["env"].size(); ++k) {
            auto pts = pointsFromJson(st["env"].at(k)["points"], 0, 1);
            if (pts.size() >= 2) s.env[k].pts = pts;
            s.env[k].sustain = (int) st["env"].at(k)["sustain"].num(-1);
        }
        for (size_t k = 0; k < 2 && k < st["draw"].size(); ++k) {
            auto pts = pointsFromJson(st["draw"].at(k)["points"], -1, 1, -1, 1);
            if (pts.size() >= 2) s.draw[k].pts = pts;
            s.draw[k].smooth = std::clamp((float) st["draw"].at(k)["smooth"].num(0.5), 0.f, 1.f);
        }
        for (size_t k = 0; k < 4 && k < st["functions"].size(); ++k) {
            s.fn[k].a = modSourceFromId(st["functions"].at(k)["a"].str());
            s.fn[k].b = modSourceFromId(st["functions"].at(k)["b"].str());
        }
        if (st.has("fxSlots")) {
            s.fxSlots.fill(-1);
            auto& ids = fxModuleIds();
            for (size_t k = 0; k < (size_t) kNumFxSlots && k < st["fxSlots"].size(); ++k) {
                auto it = std::find(ids.begin(), ids.end(), st["fxSlots"].at(k).str());
                int m = it == ids.end() ? -1 : (int) (it - ids.begin());
                if (m >= 0 && s.slotOf(m) < 0) s.fxSlots[k] = m;
            }
        }
        for (size_t b = 0; b < 3 && b < st["splitOrder"].size(); ++b) {
            std::array<int, kSplitMiniModules> o{}; bool used[kSplitMiniModules] = {}; bool ok = true;
            for (size_t i = 0; i < (size_t) kSplitMiniModules; ++i) {
                int m = (int) st["splitOrder"].at(b).at(i).num(-1);
                if (m < 0 || m >= kSplitMiniModules || used[m]) { ok = false; break; }
                used[m] = true; o[i] = m;
            }
            if (ok) s.splitOrder[b] = o;
        }
        for (size_t k = 0; k < 8 && k < st["macroNames"].size(); ++k) s.macroNames[k] = st["macroNames"].at(k).str(s.macroNames[k]);
        for (size_t k = 0; k < 3 && k < st["userTables"].size(); ++k) s.userTableName[k] = st["userTables"].at(k).str();
    }
    s.finalize();
    return true;
}

} // namespace gnarl
