#include "Displays.h"
#include "osc/Wavetable.h"

namespace gnarl::ui {

static float dv(UiContext& ctx, const std::string& id) {
    int i = Registry::get().indexOf(id);
    return i < 0 ? 0.f : ctx.proc.getEngine().displayValues()[i].load(std::memory_order_relaxed);
}

// ---------------- WaveDisplay ----------------
WaveDisplay::WaveDisplay(UiContext& c, int o) : ctx(c), osc(o) { startTimerHz(20); setInterceptsMouseClicks(false, false); }

void WaveDisplay::timerCallback() {
    const std::string p = std::string("osc") + char('a' + osc) + "_";
    const int engine = (int) std::lround(dv(ctx, p + "engine"));
    CycleInputs in;
    shown.assign(256, 0.f);
    if (engine == OE_VA || engine == OE_Wavetable || engine == OE_GrainFormant) {
        in.source = engine == OE_VA ? 0 : 1;
        in.vaShape = dv(ctx, p + "va_shape"); in.vaPw = dv(ctx, p + "va_pw"); in.syncRatio = engine == OE_VA ? std::max(1.f, dv(ctx, p + "va_sync")) : 1.f;
        const auto& st = ctx.proc.structure();
        in.table = st.userTable[(size_t) osc] ? st.userTable[(size_t) osc].get() : factoryTables()[(size_t) std::clamp((int) std::lround(dv(ctx, p + "wt_table")), 0, numFactoryTables() - 1)].get();
        in.framePos = engine == OE_GrainFormant ? dv(ctx, p + "gf_index") : dv(ctx, p + "wt_pos");
        in.warpMode = engine == OE_Wavetable ? (int) std::lround(dv(ctx, p + "wt_warp_mode")) : 0;
        in.warp = engine == OE_Wavetable ? dv(ctx, p + "wt_warp") : 0;
        const char* sn[19] = {"dc", "sym", "warp", "twist", "drive", "window", "offset", "wave", "bits", "shrink", "selfsync", "multipoint", "mp_width", "lpf", "hpf", "fold", "spectrum", "rnd_all", "rnd_odd"};
        for (int i = 0; i < 19; ++i) in.sh[(size_t) i] = dv(ctx, p + "sh_" + sn[i]);
        in.maxHarmonic = 256;
        if (!in.sameAs(last) || engine != lastEngine) { builder.build(in, table.data()); last = in; }
        for (int i = 0; i < 256; ++i) shown[(size_t) i] = readCycle(table.data(), (float) i / 256.f);
    } else if (engine == OE_HyperSaw) {
        for (int i = 0; i < 256; ++i) { float t = (float) i / 256.f; shown[(size_t) i] = 1.f - 2.f * std::fmod(t * std::max(1.f, dv(ctx, p + "hs_sync")), 1.f); }
    } else if (engine == OE_FM) {
        // one period of the operator stack at its current levels (envelopes at sustain)
        for (int i = 0; i < 256; ++i) {
            float t = (float) i / 256.f;
            float o4 = std::sin(kTwoPi * t * dv(ctx, p + "fm_op4_ratio")) * dv(ctx, p + "fm_op4_level") * 1.2f;
            float o3 = std::sin(kTwoPi * (t * dv(ctx, p + "fm_op3_ratio") + o4)) * dv(ctx, p + "fm_op3_level") * 1.2f;
            float o2 = std::sin(kTwoPi * (t * dv(ctx, p + "fm_op2_ratio") + o3)) * dv(ctx, p + "fm_op2_level") * 1.2f;
            shown[(size_t) i] = std::sin(kTwoPi * (t * dv(ctx, p + "fm_op1_ratio") + o2)) * dv(ctx, p + "fm_op1_level");
        }
    } else {
        const int n = (int) std::lround(dv(ctx, p + "ha_partials"));
        const float tilt = dv(ctx, p + "ha_tilt");
        for (int i = 0; i < 256; ++i) {
            float t = (float) i / 256.f, s = 0;
            for (int k = 1; k <= std::min(n, 48); ++k) s += std::pow((float) k, -1.f + tilt) * std::sin(kTwoPi * t * k);
            shown[(size_t) i] = s * 0.55f;
        }
    }
    lastEngine = engine;
    repaint();
}

void WaveDisplay::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(2);
    g.setColour(colours::panel2); g.fillRoundedRectangle(r, 4.f);
    g.setColour(colours::edge); g.drawHorizontalLine((int) r.getCentreY(), r.getX(), r.getRight());
    if (shown.empty()) return;
    float pk = 0.01f; for (float v : shown) pk = std::max(pk, std::fabs(v));
    juce::Path p;
    for (size_t i = 0; i < shown.size(); ++i) {
        float x = r.getX() + r.getWidth() * (float) i / (float) (shown.size() - 1);
        float y = r.getCentreY() - shown[i] / pk * r.getHeight() * 0.42f;
        if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
    }
    g.setColour(colours::accent); g.strokePath(p, juce::PathStrokeType(1.6f));
}

// ---------------- FilterDisplay ----------------
FilterDisplay::FilterDisplay(UiContext& c) : ctx(c) { startTimerHz(30); }
void FilterDisplay::timerCallback() { repaint(); }

FilterSettings FilterDisplay::settingsFor(int k) const {
    const bool linked = dv(ctx, "filter_link") > 0.5f;
    const std::string f = (k == 1 && linked) ? "f1_" : (k == 0 ? "f1_" : "f2_");
    FilterSettings s;
    s.type = (int) std::lround(dv(ctx, f + "type"));
    s.cutoff = dv(ctx, f + "cutoff"); s.reso = dv(ctx, f + "reso");
    if (k == 1 && linked) { s.cutoff *= std::exp2(dv(ctx, "link_cutoff") / 12.f); s.reso = juce::jlimit(0.f, 1.f, s.reso + dv(ctx, "link_reso")); }
    s.sep = dv(ctx, f + "sep"); s.morph = dv(ctx, f + "morph"); s.gender = dv(ctx, f + "gender"); s.shoulder = dv(ctx, f + "shoulder"); s.shelf = dv(ctx, f + "shelf");
    s.poles = (int) std::lround(dv(ctx, f + "poles")); s.stages = (int) std::lround(dv(ctx, f + "stages")); s.opposing = dv(ctx, f + "opposing") > 0.5f; s.boost = dv(ctx, f + "boost") > 0.5f;
    return s;
}
float FilterDisplay::xForHz(float hz) const { return getWidth() * std::log(hz / 20.f) / std::log(1000.f); }
float FilterDisplay::hzForX(float x) const { return 20.f * std::pow(1000.f, juce::jlimit(0.f, 1.f, x / (float) getWidth())); }
float FilterDisplay::yForDb(float db) const { return juce::jmap(juce::jlimit(-36.f, 30.f, db), 30.f, -36.f, 4.f, (float) getHeight() - 4.f); }

std::vector<FilterDisplay::Node> FilterDisplay::nodes() const {
    std::vector<Node> v;
    const float rate = 48000.f * 4;
    for (int k = 0; k < 2; ++k) {
        auto s = settingsFor(k);
        if (s.type == FT_Off) continue;
        if (k == 1 && dv(ctx, "filter_link") > 0.5f) continue; // linked: Filter 2 follows the Filter 1 nodes
        float db = gainToDb(filterMagnitude(s, s.cutoff, rate));
        v.push_back({k, 0, {xForHz(s.cutoff), yForDb(db)}});
        if (s.type == FT_TriPeak) {
            const float sp = std::exp2(0.15f + s.sep * 1.35f);
            v.push_back({k, 1, {xForHz(s.cutoff / sp), yForDb(gainToDb(filterMagnitude(s, s.cutoff / sp, rate)))}});
            v.push_back({k, 2, {xForHz(s.cutoff * sp), yForDb(gainToDb(filterMagnitude(s, s.cutoff * sp, rate)))}});
        }
    }
    return v;
}

void FilterDisplay::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    g.setColour(colours::panel2); g.fillRoundedRectangle(r, 5.f);
    g.setColour(colours::edge);
    for (float hz : {50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f}) g.drawVerticalLine((int) xForHz(hz), 2.f, r.getBottom() - 2);
    g.drawHorizontalLine((int) yForDb(0), 0, r.getRight());
    const float rate = 48000.f * 4;
    const juce::Colour cols[2] = {colours::accent, colours::accent2};
    for (int k = 1; k >= 0; --k) {
        auto s = settingsFor(k);
        if (s.type == FT_Off) continue;
        juce::Path p;
        for (int x = 0; x < getWidth(); x += 2) {
            float y = yForDb(gainToDb(filterMagnitude(s, hzForX((float) x), rate)));
            if (x == 0) p.startNewSubPath((float) x, y); else p.lineTo((float) x, y);
        }
        g.setColour(cols[k].withAlpha(0.9f)); g.strokePath(p, juce::PathStrokeType(1.8f));
        g.setColour(cols[k].withAlpha(0.8f)); g.setFont(juce::FontOptions(11.f));
        g.drawText(juce::String("F") + juce::String(k + 1) + " " + juce::String::fromUTF8(choices::filterTypes()[(size_t) s.type].c_str()), 8, 4 + k * 14, 160, 14, juce::Justification::left);
    }
    auto ns = nodes();
    for (size_t i = 0; i < ns.size(); ++i) {
        const auto& n = ns[i];
        g.setColour(cols[n.filter]);
        float rad = (int) i == hoverNode || (int) i == dragNode ? 7.f : 5.f;
        if (n.kind == 0) g.fillEllipse(n.pos.x - rad, n.pos.y - rad, rad * 2, rad * 2);
        else g.drawEllipse(n.pos.x - rad, n.pos.y - rad, rad * 2, rad * 2, 2.f);
    }
}

void FilterDisplay::setParam(const std::string& id, float value) {
    if (auto* p = ctx.proc.state().getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(value));
}

void FilterDisplay::mouseMove(const juce::MouseEvent& e) {
    int h = -1; auto ns = nodes();
    for (size_t i = 0; i < ns.size(); ++i) if (ns[i].pos.getDistanceFrom(e.position) < 10) h = (int) i;
    if (h != hoverNode) { hoverNode = h; repaint(); }
}

void FilterDisplay::mouseDown(const juce::MouseEvent& e) {
    dragNodes = nodes();
    dragNode = -1;
    for (size_t i = 0; i < dragNodes.size(); ++i) if (dragNodes[i].pos.getDistanceFrom(e.position) < 10) dragNode = (int) i;
    if (dragNode < 0) return;
    const auto& n = dragNodes[(size_t) dragNode];
    const std::string f = n.filter == 0 ? "f1_" : "f2_";
    ctx.proc.undo().beginNewTransaction("Filter node");
    for (auto id : {std::string("cutoff"), std::string("reso"), std::string("sep"), std::string("shoulder")}) if (auto* p = ctx.proc.state().getParameter(f + id)) p->beginChangeGesture();
    dragStart = e.position;
    if (n.kind == 0) { startA = ctx.proc.state().getRawParameterValue(f + "cutoff")->load(); startB = ctx.proc.state().getRawParameterValue(f + "reso")->load(); }
    else { startA = ctx.proc.state().getRawParameterValue(f + "sep")->load(); startB = ctx.proc.state().getRawParameterValue(f + "shoulder")->load(); }
}

void FilterDisplay::mouseDrag(const juce::MouseEvent& e) {
    if (dragNode < 0) return;
    const auto& n = dragNodes[(size_t) dragNode];
    const std::string f = n.filter == 0 ? "f1_" : "f2_";
    const float dx = e.position.x - dragStart.x, dy = e.position.y - dragStart.y;
    const float fine = e.mods.isShiftDown() ? 0.25f : 1.f;
    if (n.kind == 0) {
        setParam(f + "cutoff", juce::jlimit(20.f, 20000.f, startA * std::pow(1000.f, dx * fine / (float) getWidth())));
        setParam(f + "reso", juce::jlimit(0.f, 1.f, startB - dy * fine / (float) getHeight()));
    } else {
        const float dir = n.kind == 1 ? -1.f : 1.f; // dragging a shoulder outwards widens the spacing
        setParam(f + "sep", juce::jlimit(0.f, 1.f, startA + dir * dx * fine * 2.f / (float) getWidth()));
        setParam(f + "shoulder", juce::jlimit(0.f, 1.f, startB - dy * fine / (float) getHeight()));
    }
}

void FilterDisplay::mouseUp(const juce::MouseEvent&) {
    if (dragNode >= 0) {
        const std::string f = dragNodes[(size_t) dragNode].filter == 0 ? "f1_" : "f2_";
        for (auto id : {std::string("cutoff"), std::string("reso"), std::string("sep"), std::string("shoulder")}) if (auto* p = ctx.proc.state().getParameter(f + id)) p->endChangeGesture();
    }
    dragNode = -1;
}

// ---------------- ShapeEditor ----------------
ShapeEditor::ShapeEditor(UiContext& c, Getter g, Setter s, float a, float b, float lo, float hi, int gr) : ctx(c), get(std::move(g)), set(std::move(s)), x0(a), x1(b), y0(lo), y1(hi), grid(gr) {
    setTooltip("Click to add a point, drag to move, double-click to delete, alt-drag a segment to bend it. Shift disables grid snap.");
}
juce::Point<float> ShapeEditor::toScreen(const ShapePoint& p) const {
    auto r = getLocalBounds().toFloat().reduced(6);
    return {r.getX() + (p.x - x0) / (x1 - x0) * r.getWidth(), r.getBottom() - (p.y - y0) / (y1 - y0) * r.getHeight()};
}
ShapePoint ShapeEditor::fromScreen(juce::Point<float> s, bool snap) const {
    auto r = getLocalBounds().toFloat().reduced(6);
    ShapePoint p;
    float fx = juce::jlimit(0.f, 1.f, (s.x - r.getX()) / r.getWidth()), fy = juce::jlimit(0.f, 1.f, (r.getBottom() - s.y) / r.getHeight());
    if (snap && grid > 0) { fx = std::round(fx * (float) grid) / (float) grid; fy = std::round(fy * (float) grid) / (float) grid; }
    p.x = x0 + fx * (x1 - x0); p.y = y0 + fy * (y1 - y0);
    return p;
}
void ShapeEditor::paint(juce::Graphics& g) {
    if (drag < 0) pts = get(ctx.proc.structure());
    auto r = getLocalBounds().toFloat();
    g.setColour(colours::panel2); g.fillRoundedRectangle(r, 4.f);
    auto in = r.reduced(6);
    g.setColour(colours::edge.withAlpha(0.6f));
    for (int i = 1; i < grid; ++i) {
        g.drawVerticalLine((int) (in.getX() + in.getWidth() * (float) i / (float) grid), in.getY(), in.getBottom());
        g.drawHorizontalLine((int) (in.getY() + in.getHeight() * (float) i / (float) grid), in.getX(), in.getRight());
    }
    if (pts.size() < 2) return;
    juce::Path p;
    const int N = 200;
    for (int i = 0; i <= N; ++i) {
        float x = x0 + (x1 - x0) * (float) i / (float) N;
        auto s = toScreen({x, evalShape(pts, x), 0});
        if (i == 0) p.startNewSubPath(s); else p.lineTo(s);
    }
    g.setColour(colours::accent); g.strokePath(p, juce::PathStrokeType(1.8f));
    for (auto& q : pts) { auto s = toScreen(q); g.setColour(colours::text); g.fillEllipse(s.x - 3.5f, s.y - 3.5f, 7.f, 7.f); }
    if (playhead) { float ph = playhead(); if (ph >= 0) { g.setColour(colours::accent2); g.drawVerticalLine((int) (in.getX() + ph * in.getWidth()), in.getY(), in.getBottom()); } }
    if (caption.isNotEmpty()) { g.setColour(colours::dim); g.setFont(juce::FontOptions(10.f)); g.drawText(caption, in.reduced(2), juce::Justification::topLeft); }
}
void ShapeEditor::mouseDown(const juce::MouseEvent& e) {
    pts = get(ctx.proc.structure());
    drag = -1; curveDrag = false; downPos = e.position;
    for (size_t i = 0; i < pts.size(); ++i) if (toScreen(pts[i]).getDistanceFrom(e.position) < 8) drag = (int) i;
    if (drag < 0 && e.mods.isAltDown()) {
        auto q = fromScreen(e.position, false);
        for (size_t i = 1; i < pts.size(); ++i) if (q.x >= pts[i - 1].x && q.x <= pts[i].x) { drag = (int) i; curveDrag = true; curveStart = pts[i].curve; break; }
        return;
    }
    if (drag < 0) {
        auto q = fromScreen(e.position, !e.mods.isShiftDown());
        auto it = std::upper_bound(pts.begin(), pts.end(), q.x, [](float x, const ShapePoint& p) { return x < p.x; });
        drag = (int) (it - pts.begin());
        pts.insert(it, q);
        commit(false);
    }
}
void ShapeEditor::mouseDrag(const juce::MouseEvent& e) {
    if (drag < 0 || drag >= (int) pts.size()) return;
    if (curveDrag) { pts[(size_t) drag].curve = juce::jlimit(-1.f, 1.f, curveStart + (float) (downPos.y - e.position.y) / 80.f); commit(false); return; }
    auto q = fromScreen(e.position, !e.mods.isShiftDown());
    auto& p = pts[(size_t) drag];
    const bool endpoint = drag == 0 || drag == (int) pts.size() - 1;
    if (!endpoint) {
        float lo = pts[(size_t) drag - 1].x, hi = pts[(size_t) drag + 1].x;
        p.x = juce::jlimit(lo, hi, q.x);
    }
    p.y = q.y;
    commit(false);
}
void ShapeEditor::mouseUp(const juce::MouseEvent&) { if (drag >= 0) commit(true); drag = -1; }
void ShapeEditor::mouseDoubleClick(const juce::MouseEvent& e) {
    pts = get(ctx.proc.structure());
    for (size_t i = 1; i + 1 < pts.size(); ++i) if (toScreen(pts[i]).getDistanceFrom(e.position) < 8) { pts.erase(pts.begin() + (long) i); commit(true); return; }
}
void ShapeEditor::commit(bool undoable) {
    auto copy = pts;
    ctx.proc.editStructure("Edit shape", [this, copy](PatchStructure& s) { set(s, copy); }, undoable);
    repaint();
}

// ---------------- StepEditor ----------------
StepEditor::StepEditor(UiContext& c, int s) : ctx(c), seq(s) { setTooltip("Drag to draw step values; right-click a step to toggle slide."); }
void StepEditor::paint(juce::Graphics& g) {
    if (!editing) data = ctx.proc.structure().seq[(size_t) seq];
    auto r = getLocalBounds().toFloat();
    g.setColour(colours::panel2); g.fillRoundedRectangle(r, 4.f);
    const int steps = (int) std::lround(dv(ctx, "seq" + std::to_string(seq + 1) + "_steps"));
    const float w = r.getWidth() / 32.f;
    const float cy = r.getCentreY();
    for (int i = 0; i < 32; ++i) {
        float v = data.value[(size_t) i];
        auto bar = juce::Rectangle<float>(r.getX() + i * w + 1, std::min(cy, cy - v * r.getHeight() * 0.45f), w - 2, std::fabs(v) * r.getHeight() * 0.45f + 1);
        g.setColour((i < steps ? colours::forSource(MS_Seq1) : colours::dim).withAlpha(i < steps ? 0.9f : 0.3f));
        g.fillRect(bar);
        if (data.slide[(size_t) i]) { g.setColour(colours::accent2); g.fillRect(r.getX() + i * w + 1, r.getBottom() - 4, w - 2, 3.f); }
    }
    g.setColour(colours::edge); g.drawHorizontalLine((int) cy, r.getX(), r.getRight());
}
void StepEditor::setAt(juce::Point<float> p) {
    auto r = getLocalBounds().toFloat();
    int i = juce::jlimit(0, 31, (int) ((p.x - r.getX()) / (r.getWidth() / 32.f)));
    data.value[(size_t) i] = juce::jlimit(-1.f, 1.f, (r.getCentreY() - p.y) / (r.getHeight() * 0.45f));
}
void StepEditor::mouseDown(const juce::MouseEvent& e) {
    data = ctx.proc.structure().seq[(size_t) seq];
    if (e.mods.isPopupMenu()) {
        int i = juce::jlimit(0, 31, (int) (e.position.x / ((float) getWidth() / 32.f)));
        const int s = seq;
        ctx.proc.editStructure("Toggle slide", [s, i](PatchStructure& st) { st.seq[(size_t) s].slide[(size_t) i] ^= 1; });
        repaint(); return;
    }
    editing = true; setAt(e.position);
    auto d = data; const int s = seq;
    ctx.proc.editStructure("Edit steps", [s, d](PatchStructure& st) { st.seq[(size_t) s] = d; }, false);
    repaint();
}
void StepEditor::mouseDrag(const juce::MouseEvent& e) {
    if (!editing) return;
    setAt(e.position);
    auto d = data; const int s = seq;
    ctx.proc.editStructure("Edit steps", [s, d](PatchStructure& st) { st.seq[(size_t) s] = d; }, false);
    repaint();
}
void StepEditor::mouseUp(const juce::MouseEvent&) {
    if (!editing) return;
    editing = false;
    auto d = data; const int s = seq;
    ctx.proc.editStructure("Edit steps", [s, d](PatchStructure& st) { st.seq[(size_t) s] = d; }, true);
}

// ---------------- Meter ----------------
void Meter::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    g.setColour(colours::panel2); g.fillRoundedRectangle(r, 3.f);
    float vals[2] = {ctx.proc.getEngine().peakL(), ctx.proc.getEngine().peakR()};
    for (int c = 0; c < 2; ++c) {
        auto lane = r.reduced(2).withWidth(r.getWidth() / 2 - 3).translated(c * (r.getWidth() / 2 - 1), 0);
        float db = gainToDb(vals[c]);
        float h = juce::jmap(juce::jlimit(-60.f, 0.f, db), -60.f, 0.f, 0.f, lane.getHeight());
        g.setColour(db > -1 ? colours::warn : colours::accent);
        g.fillRect(lane.withTop(lane.getBottom() - h));
    }
}

} // namespace gnarl::ui
