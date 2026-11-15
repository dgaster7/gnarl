#include "Pages.h"
#include "osc/Wavetable.h"

namespace gnarl::ui {

static std::vector<std::string> withPrefix(const std::string& prefix, const std::vector<std::string>& suffixes) {
    std::vector<std::string> v; for (auto& s : suffixes) v.push_back(prefix + s); return v;
}
static std::vector<std::string> allWithPrefix(const std::string& prefix) {
    std::vector<std::string> v;
    for (auto& p : Registry::get().all()) if (p.id.rfind(prefix, 0) == 0) v.push_back(p.id);
    return v;
}

// ---------------- Flow ----------------
int Flow::layout(int width) {
    const int colW = std::max(200, width / std::max(cols, 1));
    std::vector<int> colY((size_t) cols, 0);
    for (auto& it : items) {
        if (!it.c->isVisible()) continue;
        int span = std::min(it.span, cols);
        // find the column run with the lowest top
        int best = 0, bestY = INT_MAX;
        for (int c = 0; c + span <= cols; ++c) { int y = 0; for (int k = c; k < c + span; ++k) y = std::max(y, colY[(size_t) k]); if (y < bestY) { bestY = y; best = c; } }
        int w = colW * span;
        int h = it.height(w);
        it.c->setBounds(best * colW, bestY, w, h);
        for (int k = best; k < best + span; ++k) colY[(size_t) k] = bestY + h;
    }
    int total = 0; for (int y : colY) total = std::max(total, y);
    return total + 4;
}

// ================= OSC PAGE =================
struct OscPage::OscTab : public ScrollPage {
    int k; UiContext& ctx;
    Section *common, *va, *hs, *wt, *fm, *gf, *ha, *shaper;
    WaveDisplay wave;
    juce::TextButton loadTable{"Load .wav"}, factoryTable{"Factory"}, copyShaper{"Copy"}, pasteShaper{"Paste"};
    SourceHandle outHandle;
    juce::Label tableName;
    OscTab(UiContext& c, int osc) : ScrollPage(3), k(osc), ctx(c), wave(c, osc), outHandle(c, MS_OscA + osc, juce::String("Osc ") + char('A' + osc) + " out") {
        const std::string o = std::string("osc") + char('a' + osc) + "_";
        auto* top = make<juce::Component>();
        top->addAndMakeVisible(wave); top->addAndMakeVisible(outHandle);
        flow.add(top, [this, top](int w) { wave.setBounds(4, 4, w - 8, 92); outHandle.setBounds(w - 130, 100, 124, 22); return 128; });
        common = section(c, juce::String("Osc ") + char('A' + osc), withPrefix(o, {"engine", "octave", "semi", "fine", "level", "pan", "balance", "fx_send", "unison", "detune", "spread", "blend", "multi", "phase", "phase_mode", "invert", "fixed", "fixed_hz"}), 6, 2);
        va = section(c, "Analog", withPrefix(o, {"va_shape", "va_pw", "va_sync"}), 3);
        hs = section(c, "HyperSaw", withPrefix(o, {"hs_density", "hs_spread", "hs_sync"}), 3);
        wt = section(c, "Wavetable", withPrefix(o, {"wt_table", "wt_pos", "wt_warp_mode", "wt_warp", "wt_pwm_detune"}), 5, 2);
        wt->addHeaderComponent(&tableName); tableName.setSize(140, 16); tableName.setColour(juce::Label::textColourId, colours::dim);
        loadTable.setSize(80, 18); factoryTable.setSize(64, 18);
        wt->addHeaderComponent(&factoryTable); wt->addHeaderComponent(&loadTable);
        loadTable.onClick = [this] { chooseTable(); };
        factoryTable.onClick = [this] { ctx.proc.clearUserWavetable(k); };
        std::vector<std::string> fmIds = {o + "fm_alg"};
        for (int op = 1; op <= 4; ++op) for (auto s : {"ratio", "fixed", "freq", "level", "fb", "wave", "a", "d", "s", "r"}) fmIds.push_back(o + "fm_op" + std::to_string(op) + "_" + s);
        fm = section(c, "FM (4 operators: ratio, fixed, Hz, level, feedback, wave, A, D, S, R per row)", fmIds, 10, 3);
        gf = section(c, "Grain / Formant", withPrefix(o, {"gf_index", "gf_shift", "gf_spread", "gf_detune", "gf_interp", "gf_complex", "gf_mode"}), 7, 2);
        ha = section(c, "Harmonic", withPrefix(o, {"ha_partials", "ha_tilt", "ha_oddeven", "ha_comb", "ha_comb_period", "ha_stretch", "ha_formant_hz", "ha_formant_gain", "ha_formant_width"}), 9, 3);
        shaper = section(c, "Shaper (stacked, re-band-limited)", allWithPrefix(o + "sh_"), 10, 3);
        copyShaper.setSize(50, 18); pasteShaper.setSize(50, 18);
        shaper->addHeaderComponent(&copyShaper); shaper->addHeaderComponent(&pasteShaper);
        copyShaper.onClick = [this] {
            juce::String s; for (auto& id : allWithPrefix(std::string("osc") + char('a' + k) + "_sh_")) s << ctx.proc.state().getRawParameterValue(id)->load() << " ";
            juce::SystemClipboard::copyTextToClipboard("GNARL-SHAPER " + s);
        };
        pasteShaper.onClick = [this] {
            auto t = juce::SystemClipboard::getTextFromClipboard();
            if (!t.startsWith("GNARL-SHAPER ")) return;
            auto vals = juce::StringArray::fromTokens(t.fromFirstOccurrenceOf(" ", false, false), " ", "");
            auto ids = allWithPrefix(std::string("osc") + char('a' + k) + "_sh_");
            ctx.proc.undo().beginNewTransaction("Paste shaper");
            for (size_t i = 0; i < ids.size() && (int) i < vals.size(); ++i)
                if (auto* p = ctx.proc.state().getParameter(ids[i])) p->setValueNotifyingHost(p->convertTo0to1(vals[(int) i].getFloatValue()));
        };
    }
    void chooseTable() {
        auto fc = std::make_shared<juce::FileChooser>("Load a wavetable (.wav, 2048-sample frames)", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.wav");
        fc->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, fc](const juce::FileChooser& f) {
            auto file = f.getResult();
            if (!file.existsAsFile()) return;
            juce::String err;
            if (!ctx.proc.loadUserWavetable(k, file, &err)) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Wavetable", "Could not load " + file.getFileName() + ": " + err);
        });
    }
    // drag & drop .wav files onto the oscillator tab
    void refreshTableName() {
        const auto& st = ctx.proc.structure();
        tableName.setText(st.userTableName[(size_t) k].empty() ? juce::String("factory table") : juce::String::fromUTF8(st.userTableName[(size_t) k].c_str()), juce::dontSendNotification);
    }
};

OscPage::OscPage(UiContext& c) : ctx(c) {
    for (int k = 0; k < 3; ++k) { auto* t = new OscTab(ctx, k); oscTabs.push_back(t); tabs.addTab(juce::String("Osc ") + char('A' + k), colours::panel, t, true); }
    auto* chain = new ScrollPage(3);
    chain->section(ctx, "Oscillator chain A > B > C > A (mode and amount per link)", allWithPrefix("link_ab_"), 2);
    chain->section(ctx, "Link B > C", allWithPrefix("link_bc_"), 2);
    chain->section(ctx, "Link C > A", allWithPrefix("link_ca_"), 2);
    chain->section(ctx, "Sub", {"sub_wave", "sub_octave", "sub_level", "sub_direct"}, 4);
    chain->section(ctx, "Noise", {"noise_type", "noise_level", "noise_track", "noise_balance"}, 4);
    tabs.addTab("Chain / Sub / Noise", colours::panel, chain, true);
    addAndMakeVisible(tabs);
    for (int k = 0; k < 3; ++k) ctx.proc.state().addParameterListener(std::string("osc") + char('a' + k) + "_engine", this);
    updateVisibility();
}
OscPage::~OscPage() { for (int k = 0; k < 3; ++k) ctx.proc.state().removeParameterListener(std::string("osc") + char('a' + k) + "_engine", this); }
void OscPage::resized() { tabs.setBounds(getLocalBounds()); }
void OscPage::updateVisibility() {
    for (auto* t : oscTabs) {
        int e = (int) ctx.proc.state().getRawParameterValue(std::string("osc") + char('a' + t->k) + "_engine")->load();
        t->va->setVisible(e == OE_VA); t->hs->setVisible(e == OE_HyperSaw); t->wt->setVisible(e == OE_Wavetable || e == OE_GrainFormant);
        t->fm->setVisible(e == OE_FM); t->gf->setVisible(e == OE_GrainFormant); t->ha->setVisible(e == OE_Harmonic);
        t->shaper->setVisible(e == OE_VA || e == OE_Wavetable || e == OE_GrainFormant);
        t->refreshTableName();
        t->resized();
    }
}

// ================= FILTER PAGE =================
FilterPage::FilterPage(UiContext& ctx) : ScrollPage(2) {
    auto* disp = make<FilterDisplay>(ctx);
    flow.add(disp, [](int) { return 210; }, 2);
    section(ctx, "Filter 1", allWithPrefix("f1_"), 7);
    section(ctx, "Filter 2", allWithPrefix("f2_"), 7);
    section(ctx, "Routing, saturation between filters, link", {"filter_routing", "sat_type", "sat_drive", "sat_mix", "filter_link", "link_cutoff", "link_reso", "link_pan"}, 8, 2);
}

// ================= MOD PAGE =================
namespace {
struct EnvPanel : public juce::Component {
    Section sec; ShapeEditor shape; SourceHandle handle;
    EnvPanel(UiContext& ctx, int k)
        : sec(ctx, "Envelope " + juce::String(k + 1) + (k == 0 ? " (amp)" : ""), allWithPrefix("env" + std::to_string(k + 1) + "_"), 7),
          shape(ctx, [k](const PatchStructure& s) { return s.env[(size_t) k].pts; }, [k](PatchStructure& s, const std::vector<ShapePoint>& p) { s.env[(size_t) k].pts = p; }, 0, 1, 0, 1, 16),
          handle(ctx, MS_Env1 + k) {
        shape.caption = "Drawn shape (Mode = Drawn)";
        addAndMakeVisible(sec); addAndMakeVisible(shape); addAndMakeVisible(handle);
    }
    void resized() override { auto r = getLocalBounds(); auto right = r.removeFromRight(r.getWidth() / 3); handle.setBounds(right.removeFromTop(24).reduced(2)); shape.setBounds(right.reduced(2)); sec.setBounds(r); }
};
struct LfoPanel : public juce::Component {
    Section sec; ShapeEditor a, b; SourceHandle handle; juce::ComboBox grid;
    LfoPanel(UiContext& ctx, int k)
        : sec(ctx, "LFO " + juce::String(k + 1) + (k >= 4 ? " (per voice)" : " (global)"), allWithPrefix("lfo" + std::to_string(k + 1) + "_"), 6),
          a(ctx, [k](const PatchStructure& s) { return s.lfo[(size_t) k].a; }, [k](PatchStructure& s, const std::vector<ShapePoint>& p) { s.lfo[(size_t) k].a = p; }, 0, 1, -1, 1, 8),
          b(ctx, [k](const PatchStructure& s) { return s.lfo[(size_t) k].b; }, [k](PatchStructure& s, const std::vector<ShapePoint>& p) { s.lfo[(size_t) k].b = p; }, 0, 1, -1, 1, 8),
          handle(ctx, MS_Lfo1 + k) {
        a.caption = "Shape A"; b.caption = "Shape B (morph target)";
        for (int g : {1, 2, 3, 4, 6, 8, 12, 16, 32}) grid.addItem("Grid " + juce::String(g), g);
        grid.setSelectedId(8, juce::dontSendNotification);
        grid.onChange = [this] { a.setGrid(grid.getSelectedId()); b.setGrid(grid.getSelectedId()); };
        addAndMakeVisible(sec); addAndMakeVisible(a); addAndMakeVisible(b); addAndMakeVisible(handle); addAndMakeVisible(grid);
    }
    void resized() override {
        auto r = getLocalBounds(); auto right = r.removeFromRight(r.getWidth() * 5 / 12);
        auto top = right.removeFromTop(24); handle.setBounds(top.removeFromLeft(top.getWidth() / 2).reduced(2)); grid.setBounds(top.reduced(2));
        a.setBounds(right.removeFromTop(right.getHeight() / 2).reduced(2)); b.setBounds(right.reduced(2)); sec.setBounds(r);
    }
};
struct SeqRandPanel : public ScrollPage {
    SeqRandPanel(UiContext& ctx) : ScrollPage(2) {
        for (int k = 0; k < 2; ++k) {
            auto* steps = make<StepEditor>(ctx, k);
            auto* h = make<SourceHandle>(ctx, MS_Seq1 + k);
            auto* box = make<juce::Component>();
            box->addAndMakeVisible(steps); box->addAndMakeVisible(h);
            flow.add(box, [steps, h](int w) { h->setBounds(4, 2, 120, 22); steps->setBounds(4, 28, w - 8, 110); return 142; }, 2);
            section(ctx, "Step sequencer " + juce::String(k + 1), allWithPrefix("seq" + std::to_string(k + 1) + "_"), 4, 2);
        }
        for (int k = 0; k < 4; ++k) {
            auto* s = section(ctx, "Random " + juce::String(k + 1), allWithPrefix("rnd" + std::to_string(k + 1) + "_"), 5);
            auto* h = make<SourceHandle>(ctx, MS_Rnd1 + k); h->setSize(100, 18); s->addHeaderComponent(h);
        }
    }
};
struct FunctionRow : public juce::Component, private juce::ChangeListener {
    UiContext& ctx; int k; juce::ComboBox a, b; Section sec; SourceHandle handle; juce::Label la{"", "A"}, lb{"", "B"};
    FunctionRow(UiContext& c, int fn) : ctx(c), k(fn), sec(c, "Function " + juce::String(fn + 1), allWithPrefix("fn" + std::to_string(fn + 1) + "_"), 2), handle(c, MS_Fn1 + fn) {
        for (auto* cb : {&a, &b}) {
            cb->addItem("None", 1);
            for (int s = 0; s < MS_Count; ++s) if (s < MS_Fn1 + fn) cb->addItem(modSourceName(s), s + 2); // only earlier functions can feed this one
            addAndMakeVisible(*cb);
        }
        a.onChange = [this] { int v = a.getSelectedId() - 2; ctx.proc.editStructure("Function input", [this, v](PatchStructure& s) { s.fn[(size_t) k].a = v; }); };
        b.onChange = [this] { int v = b.getSelectedId() - 2; ctx.proc.editStructure("Function input", [this, v](PatchStructure& s) { s.fn[(size_t) k].b = v; }); };
        addAndMakeVisible(sec); addAndMakeVisible(handle); addAndMakeVisible(la); addAndMakeVisible(lb);
        ctx.proc.structureChanged.addChangeListener(this);
        changeListenerCallback(nullptr);
    }
    ~FunctionRow() override { ctx.proc.structureChanged.removeChangeListener(this); }
    void changeListenerCallback(juce::ChangeBroadcaster*) override {
        a.setSelectedId(ctx.proc.structure().fn[(size_t) k].a + 2, juce::dontSendNotification);
        b.setSelectedId(ctx.proc.structure().fn[(size_t) k].b + 2, juce::dontSendNotification);
    }
    void resized() override {
        auto r = getLocalBounds();
        handle.setBounds(r.removeFromLeft(110).reduced(3, 20));
        la.setBounds(r.removeFromLeft(16)); a.setBounds(r.removeFromLeft(150).reduced(3, 22));
        lb.setBounds(r.removeFromLeft(16)); b.setBounds(r.removeFromLeft(150).reduced(3, 22));
        sec.setBounds(r);
    }
};
} // namespace

ModPage::ModPage(UiContext& ctx) {
    auto* envs = new ScrollPage(1);
    for (int k = 0; k < 4; ++k) { auto* p = envs->make<EnvPanel>(ctx, k); envs->flow.add(p, [](int) { return 170; }); }
    tabs.addTab("Envelopes", colours::panel, envs, true);
    auto* lfos = new ScrollPage(1);
    for (int k = 0; k < 6; ++k) { auto* p = lfos->make<LfoPanel>(ctx, k); lfos->flow.add(p, [](int) { return 190; }); }
    tabs.addTab("LFOs", colours::panel, lfos, true);
    tabs.addTab("Step Seq / Random", colours::panel, new SeqRandPanel(ctx), true);
    auto* fns = new ScrollPage(1);
    for (int k = 0; k < 4; ++k) { auto* r = fns->make<FunctionRow>(ctx, k); fns->flow.add(r, [](int) { return 76; }); }
    fns->section(ctx, "Envelope follower / transient detector (sources: Env Follower, Transient)", {"ef_attack", "ef_release", "ef_gain", "tr_sens", "tr_decay", "ef_sidechain", "fxp_env_trigger", "fxp_in_gain"}, 8);
    tabs.addTab("Functions / Follower", colours::panel, fns, true);
    addAndMakeVisible(tabs);
}
void ModPage::resized() { tabs.setBounds(getLocalBounds()); }

// ================= FX PAGE =================
juce::String moduleTitle(int m) { return juce::String::fromUTF8(fxModuleNames()[(size_t) m].c_str()); }
std::vector<std::string> moduleParamIds(int m, bool fxPlugin) {
    std::vector<std::string> v;
    const std::string pre = "fx_" + fxModuleIds()[(size_t) m] + "_";
    for (auto& p : Registry::get().all()) if (p.id.rfind(pre, 0) == 0 && (p.plugins & (fxPlugin ? kFx : kSynth))) v.push_back(p.id);
    return v;
}

class FxPage::SlotList : public juce::Component {
public:
    SlotList(UiContext& c, FxPage& o) : ctx(c), owner(o) {}
    std::function<void(int)> onSelect;
    int selected = -1;
    void paint(juce::Graphics& g) override {
        const auto& st = ctx.proc.structure();
        const int h = rowH();
        for (int s = 0; s < kNumFxSlots; ++s) {
            auto r = juce::Rectangle<int>(0, s * h, getWidth(), h).reduced(2);
            int m = st.fxSlots[(size_t) s];
            bool on = m >= 0 && ctx.proc.state().getRawParameterValue("fx_" + fxModuleIds()[(size_t) m] + "_on")->load() > 0.5f;
            g.setColour(s == dragFrom ? colours::accent2.withAlpha(0.3f) : (m == selected && m >= 0 ? colours::panel2.brighter(0.15f) : colours::panel2));
            g.fillRoundedRectangle(r.toFloat(), 4.f);
            g.setColour(on ? colours::accent : colours::dim);
            g.fillEllipse((float) r.getX() + 6, (float) r.getCentreY() - 4, 8, 8);
            g.setColour(m >= 0 ? colours::text : colours::dim);
            g.setFont(juce::FontOptions(12.f, m >= 0 ? juce::Font::bold : juce::Font::plain));
            g.drawText(juce::String(s + 1) + "  " + (m >= 0 ? moduleTitle(m) : juce::String("+ empty slot")), r.withTrimmedLeft(20), juce::Justification::centredLeft);
        }
        if (dropAt >= 0) { g.setColour(colours::accent2); g.fillRect(0, dropAt * h, getWidth(), 2); }
    }
    int rowH() const { return std::max(18, getHeight() / kNumFxSlots); }
    int slotAt(int y) const { return juce::jlimit(0, kNumFxSlots - 1, y / rowH()); }
    void mouseDown(const juce::MouseEvent& e) override {
        int s = slotAt(e.y);
        int m = ctx.proc.structure().fxSlots[(size_t) s];
        if (e.x < 20 && m >= 0) { // click the dot: toggle on/off
            if (auto* p = ctx.proc.state().getParameter("fx_" + fxModuleIds()[(size_t) m] + "_on")) p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.f : 1.f);
            repaint(); return;
        }
        if (m < 0 || e.mods.isPopupMenu()) { menu(s); return; }
        dragFrom = s; selected = m; if (onSelect) onSelect(m);
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override { if (dragFrom >= 0) { dropAt = slotAt(e.y + rowH() / 2); repaint(); } }
    void mouseUp(const juce::MouseEvent&) override {
        if (dragFrom >= 0 && dropAt >= 0 && dropAt != dragFrom && dropAt != dragFrom + 1) {
            int from = dragFrom, to = dropAt;
            ctx.proc.editStructure("Reorder FX", [from, to](PatchStructure& st) {
                std::vector<int> v(st.fxSlots.begin(), st.fxSlots.end());
                int m = v[(size_t) from]; v.erase(v.begin() + from);
                v.insert(v.begin() + (to > from ? to - 1 : to), m);
                for (int i = 0; i < kNumFxSlots; ++i) st.fxSlots[(size_t) i] = v[(size_t) i];
            });
        }
        dragFrom = dropAt = -1; repaint();
    }
    void menu(int slot) {
        juce::PopupMenu pm;
        const auto& st = ctx.proc.structure();
        int cur = st.fxSlots[(size_t) slot];
        for (int m = 0; m < (int) fxModuleIds().size(); ++m) {
            if (m == 22 && !ctx.proc.isFxPlugin()) continue; // vocoder needs the GNARL FX sidechain
            pm.addItem(m + 1, moduleTitle(m), st.slotOf(m) < 0 || st.slotOf(m) == slot);
        }
        if (cur >= 0) { pm.addSeparator(); pm.addItem(1000, "Remove from chain"); }
        pm.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [this, slot](int r) {
            if (r == 0) return;
            if (r == 1000) { ctx.proc.editStructure("Remove FX", [slot](PatchStructure& st) { st.fxSlots[(size_t) slot] = -1; }); repaint(); return; }
            int m = r - 1;
            ctx.proc.editStructure("Insert FX", [slot, m](PatchStructure& st) { int prev = st.slotOf(m); if (prev >= 0) st.fxSlots[(size_t) prev] = -1; st.fxSlots[(size_t) slot] = m; });
            if (auto* p = ctx.proc.state().getParameter("fx_" + fxModuleIds()[(size_t) m] + "_on")) p->setValueNotifyingHost(1.f);
            selected = m; if (onSelect) onSelect(m);
            repaint();
        });
    }
    UiContext& ctx; FxPage& owner; int dragFrom = -1, dropAt = -1;
};

namespace {
// Module editor with optional extras (draw curves for the distortions, mini-chain order for the 3-band split)
struct ModuleEditor : public ScrollPage {
    ModuleEditor(UiContext& ctx, int m) : ScrollPage(2) {
        auto ids = moduleParamIds(m, ctx.proc.isFxPlugin());
        if (fxModuleIds()[(size_t) m] == "split") {
            section(ctx, "3-Band Split", {"fx_split_on", "fx_split_mix", "fx_split_xlo", "fx_split_xhi"}, 4, 2);
            const char* bn[] = {"Low", "Mid", "High"};
            for (int b = 1; b <= 3; ++b) {
                auto* s = section(ctx, juce::String(bn[b - 1]) + " band chain", allWithPrefix("fx_split_b" + std::to_string(b) + "_"), 6);
                auto* order = make<juce::TextButton>("Order...");
                order->setSize(70, 18);
                s->addHeaderComponent(order);
                const int band = b - 1;
                order->onClick = [&ctx, band, order] {
                    const char* names[kSplitMiniModules] = {"Filter", "Phaser", "Dist", "Comb", "Crush"};
                    juce::PopupMenu pm;
                    const auto& o = ctx.proc.structure().splitOrder[(size_t) band];
                    for (int i = 0; i < kSplitMiniModules; ++i) {
                        juce::PopupMenu sub;
                        sub.addItem(100 + i * 2, "Move earlier", i > 0);
                        sub.addItem(101 + i * 2, "Move later", i < kSplitMiniModules - 1);
                        pm.addSubMenu(juce::String(i + 1) + ". " + names[o[(size_t) i]], sub);
                    }
                    pm.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(order), [&ctx, band](int r) {
                        if (r < 100) return;
                        int i = (r - 100) / 2, dir = ((r - 100) % 2) ? 1 : -1;
                        ctx.proc.editStructure("Split order", [band, i, dir](PatchStructure& st) { std::swap(st.splitOrder[(size_t) band][(size_t) i], st.splitOrder[(size_t) band][(size_t) (i + dir)]); });
                    });
                };
            }
            return;
        }
        section(ctx, moduleTitle(m), ids, 6, 2);
        if (fxModuleIds()[(size_t) m] == "dist1" || fxModuleIds()[(size_t) m] == "dist2" || fxModuleIds()[(size_t) m] == "mbd") {
            for (int c = 0; c < 2; ++c) {
                auto* e = make<ShapeEditor>(ctx, [c](const PatchStructure& s) { return s.draw[(size_t) c].pts; }, [c](PatchStructure& s, const std::vector<ShapePoint>& p) { s.draw[(size_t) c].pts = p; }, -1.f, 1.f, -1.f, 1.f, 8);
                e->caption = c == 0 ? "Draw curve 1 (used by every Draw-type stage)" : "Draw curve 2 (morph target)";
                flow.add(e, [](int) { return 170; });
            }
        }
    }
};
} // namespace

FxPage::FxPage(UiContext& c) : ctx(c) {
    slots = std::make_unique<SlotList>(ctx, *this);
    slots->onSelect = [this](int m) { showModule(m); };
    addAndMakeVisible(*slots);
    for (int m = 0; m < (int) fxModuleIds().size(); ++m) {
        auto* e = new ModuleEditor(ctx, m);
        editors.add(e);
        addChildComponent(e);
    }
    clipper = std::make_unique<Section>(ctx, "Final stage (always last)", std::vector<std::string>{"fx_clip_on", "fx_clip_mode", "fx_clip_ceiling", "fx_clip_drive"}, 4);
    clipper->title = "Final stage (always last)";
    addAndMakeVisible(*clipper);
    ctx.proc.structureChanged.addChangeListener(this);
    int first = -1; for (int s = 0; s < kNumFxSlots && first < 0; ++s) first = ctx.proc.structure().fxSlots[(size_t) s];
    showModule(first >= 0 ? first : 0);
}
FxPage::~FxPage() { ctx.proc.structureChanged.removeChangeListener(this); }
void FxPage::changeListenerCallback(juce::ChangeBroadcaster*) { slots->repaint(); }
void FxPage::showModule(int m) {
    for (int i = 0; i < editors.size(); ++i) editors[i]->setVisible(i == m);
    shown = m; slots->selected = m; slots->repaint();
}
void FxPage::paint(juce::Graphics& g) {
    g.setColour(colours::dim); g.setFont(juce::FontOptions(11.f));
    g.drawText("Drag to reorder - click a dot to switch a module on/off - right-click or empty slot to add", getLocalBounds().removeFromBottom(16).withWidth(260), juce::Justification::left);
}
void FxPage::resized() {
    auto r = getLocalBounds();
    auto left = r.removeFromLeft(290);
    clipper->setBounds(left.removeFromBottom(110));
    left.removeFromBottom(18);
    slots->setBounds(left.reduced(4));
    for (auto* e : editors) e->setBounds(r.reduced(4));
}

// ================= MATRIX PAGE =================
class MatrixPage::Row : public juce::Component {
public:
    Row(UiContext& c, int route) : ctx(c), k(route), depth(c, Registry::get().indexOf("mod" + std::to_string(route + 1) + "_depth"), "Depth") {
        idx.setText(juce::String(k + 1), juce::dontSendNotification); idx.setColour(juce::Label::textColourId, colours::dim);
        src.addItem("-", 1); via.addItem("no via", 1);
        for (int s = 0; s < MS_Count; ++s) { src.addItem(modSourceName(s), s + 2); via.addItem(juce::String("via ") + modSourceName(s), s + 2); }
        for (int cv = 0; cv < RC_Count; ++cv) curve.addItem(routeCurveName(cv), cv + 1);
        dest.setButtonText("-");
        src.onChange = [this] { int v = src.getSelectedId() - 2; ctx.proc.editStructure("Route source", [this, v](PatchStructure& s) { s.routes[(size_t) k].source = v; s.routes[(size_t) k].bipolar = modSourceIsBipolar(v); }); };
        via.onChange = [this] { int v = via.getSelectedId() - 2; ctx.proc.editStructure("Route via", [this, v](PatchStructure& s) { s.routes[(size_t) k].via = v; }); };
        curve.onChange = [this] { int v = curve.getSelectedId() - 1; ctx.proc.editStructure("Route curve", [this, v](PatchStructure& s) { s.routes[(size_t) k].curve = (uint8_t) v; }); };
        bip.onClick = [this] { bool v = bip.getToggleState(); ctx.proc.editStructure("Route polarity", [this, v](PatchStructure& s) { s.routes[(size_t) k].bipolar = v; }); };
        byp.onClick = [this] { bool v = byp.getToggleState(); ctx.proc.editStructure("Route bypass", [this, v](PatchStructure& s) { s.routes[(size_t) k].bypass = v; }); };
        del.onClick = [this] { ctx.proc.removeRoute(k); };
        dest.onClick = [this] { chooseDest(); };
        for (juce::Component* comp : std::initializer_list<juce::Component*>{&idx, &src, &dest, &depth, &via, &curve, &bip, &byp, &del}) addAndMakeVisible(comp);
        refresh();
    }
    void refresh() {
        const auto& r = ctx.proc.structure().routes[(size_t) k];
        src.setSelectedId(r.source + 2, juce::dontSendNotification);
        via.setSelectedId(r.via + 2, juce::dontSendNotification);
        curve.setSelectedId(r.curve + 1, juce::dontSendNotification);
        bip.setToggleState(r.bipolar, juce::dontSendNotification);
        byp.setToggleState(r.bypass, juce::dontSendNotification);
        dest.setButtonText(r.dest >= 0 ? juce::String::fromUTF8(Registry::get()[r.dest].name.c_str()) : juce::String("choose destination"));
    }
    void chooseDest() {
        juce::PopupMenu root;
        std::map<std::string, juce::PopupMenu> secs; std::vector<std::string> order;
        const uint8_t mask = ctx.proc.isFxPlugin() ? kFx : kSynth;
        auto& R = Registry::get();
        for (int i = 0; i < R.size(); ++i) {
            const auto& p = R[i];
            if (!(p.plugins & mask) || !p.modulatable || p.isDiscrete()) continue;
            if (!secs.count(p.section)) order.push_back(p.section);
            secs[p.section].addItem(i + 1, juce::String::fromUTF8(p.name.c_str()));
        }
        for (auto& s : order) root.addSubMenu(juce::String::fromUTF8(s.c_str()), secs[s]);
        root.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&dest), [this](int r) {
            if (r <= 0) return;
            int d = r - 1;
            ctx.proc.editStructure("Route destination", [this, d](PatchStructure& s) { s.routes[(size_t) k].dest = d; });
        });
    }
    void resized() override {
        auto r = getLocalBounds();
        idx.setBounds(r.removeFromLeft(28));
        src.setBounds(r.removeFromLeft(130).reduced(2, 10));
        dest.setBounds(r.removeFromLeft(220).reduced(2, 10));
        depth.setBounds(r.removeFromLeft(60));
        via.setBounds(r.removeFromLeft(140).reduced(2, 10));
        curve.setBounds(r.removeFromLeft(100).reduced(2, 10));
        bip.setBounds(r.removeFromLeft(78).reduced(2, 12));
        byp.setBounds(r.removeFromLeft(78).reduced(2, 12));
        del.setBounds(r.removeFromLeft(28).reduced(2, 12));
    }
    UiContext& ctx; int k;
    juce::Label idx; juce::ComboBox src, via, curve; juce::TextButton dest, del{"x"}; ModKnob depth;
    juce::ToggleButton bip{"Bipolar"}, byp{"Bypass"};
};

MatrixPage::MatrixPage(UiContext& c) : ctx(c) {
    for (int k = 0; k < kNumRoutes; ++k) { auto* r = new Row(ctx, k); rows.add(r); content.addAndMakeVisible(r); }
    view.setViewedComponent(&content, false);
    addAndMakeVisible(view);
    ctx.proc.structureChanged.addChangeListener(this);
}
MatrixPage::~MatrixPage() { ctx.proc.structureChanged.removeChangeListener(this); }
void MatrixPage::changeListenerCallback(juce::ChangeBroadcaster*) { for (auto* r : rows) r->refresh(); }
void MatrixPage::resized() {
    view.setBounds(getLocalBounds());
    const int h = 48;
    content.setSize(getWidth() - 12, h * rows.size());
    for (int i = 0; i < rows.size(); ++i) rows[i]->setBounds(0, i * h, content.getWidth(), h);
}

// ================= PRESET PAGE =================
PresetPage::PresetPage(UiContext& c) : ctx(c) {
    search.setTextToShowWhenEmpty("Search name, category or tag", colours::dim);
    search.onTextChange = [this] { refresh(); };
    bank.addItemList({"All", "Factory", "Content", "User"}, 1); bank.setSelectedId(1, juce::dontSendNotification);
    bank.onChange = category.onChange = [this] { refresh(); };
    favOnly.onClick = [this] { refresh(); };
    listBox.setRowHeight(22);
    saveBtn.onClick = [this] {
        auto* w = new juce::AlertWindow("Save preset", "Saved to your User bank (never touched by updates).", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor("name", ctx.proc.patchName(), "Name");
        w->addTextEditor("category", "User", "Category");
        w->addTextEditor("tags", "", "Tags (comma separated)");
        w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Cancel", 0);
        w->enterModalState(true, juce::ModalCallbackFunction::create([this, w](int r) {
            if (r == 1) {
                juce::String err;
                if (!ctx.proc.presets().saveUser(w->getTextEditorContents("name"), w->getTextEditorContents("category"), juce::StringArray::fromTokens(w->getTextEditorContents("tags"), ",", ""), &err))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Save preset", err);
            }
        }), true);
    };
    initBtn.onClick = [this] { ctx.proc.presets().loadInit(); };
    favBtn.onClick = [this] { int r = listBox.getSelectedRow(); if (r >= 0 && r < list.size()) ctx.proc.presets().setFavourite(list[r], !list[r].favourite); };
    mutateAmount.setSliderStyle(juce::Slider::LinearHorizontal); mutateAmount.setRange(0, 1); mutateAmount.setValue(0.3); mutateAmount.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    mutateAmount.setTooltip("Mutate amount: how far each mutation moves from the current sound.");
    mutateLabel.setText("Amount", juce::dontSendNotification); mutateLabel.setColour(juce::Label::textColourId, colours::dim);
    mutateBtn.setTooltip("Randomise within safe ranges. Hold a note, keep pressing Mutate and record the results. Undo steps back.");
    mutateBtn.onClick = [this] {
        Patch p = ctx.proc.currentPatch();
        MutateLocks l{lockOsc.getToggleState(), lockFilt.getToggleState(), lockMod.getToggleState(), lockFx.getToggleState()};
        mutatePatch(p, (float) mutateAmount.getValue(), l, ctx.proc.isFxPlugin(), (uint32_t) juce::Time::getMillisecondCounter());
        p.name = (ctx.proc.patchName() + " *").toStdString();
        ctx.proc.loadPatch(p, true);
    };
    for (juce::Component* comp : std::initializer_list<juce::Component*>{&search, &category, &bank, &favOnly, &listBox, &saveBtn, &initBtn, &favBtn, &mutateBtn, &mutateAmount, &mutateLabel, &lockOsc, &lockFilt, &lockMod, &lockFx}) addAndMakeVisible(comp);
    if (ctx.proc.isFxPlugin()) lockOsc.setVisible(false);
    ctx.proc.presets().addChangeListener(this);
    refresh();
}
PresetPage::~PresetPage() { ctx.proc.presets().removeChangeListener(this); }
void PresetPage::refresh() {
    auto cats = ctx.proc.presets().categories();
    auto cur = category.getText();
    category.clear(juce::dontSendNotification); category.addItemList(cats, 1);
    category.setText(cur.isNotEmpty() ? cur : "All", juce::dontSendNotification);
    list = ctx.proc.presets().filtered(search.getText(), category.getText(), bank.getText(), favOnly.getToggleState());
    ctx.proc.presets().setLastFilter(list);
    listBox.updateContent(); listBox.repaint();
}
void PresetPage::paint(juce::Graphics& g) { g.fillAll(colours::bg); }
void PresetPage::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool sel) {
    if (row >= list.size()) return;
    const auto& p = list[row];
    if (sel) g.fillAll(colours::accent.withAlpha(0.2f));
    g.setColour(p.favourite ? colours::accent2 : colours::text); g.setFont(juce::FontOptions(12.f, juce::Font::bold));
    g.drawText((p.favourite ? juce::String::fromUTF8("\xe2\x98\x85 ") : juce::String()) + p.name, 8, 0, w / 2, h, juce::Justification::centredLeft);
    g.setColour(colours::dim); g.setFont(juce::FontOptions(11.f));
    g.drawText(p.category + "   " + p.tags.joinIntoString(", ") + "   [" + p.bank + "]", w / 2, 0, w / 2 - 8, h, juce::Justification::centredRight);
}
void PresetPage::listBoxItemDoubleClicked(int row, const juce::MouseEvent&) { if (row < list.size()) ctx.proc.presets().load(list[row]); }
void PresetPage::selectedRowsChanged(int row) { if (row >= 0 && row < list.size()) ctx.proc.presets().load(list[row]); }
void PresetPage::resized() {
    auto r = getLocalBounds().reduced(6);
    auto top = r.removeFromTop(28);
    search.setBounds(top.removeFromLeft(280).reduced(2));
    category.setBounds(top.removeFromLeft(170).reduced(2));
    bank.setBounds(top.removeFromLeft(110).reduced(2));
    favOnly.setBounds(top.removeFromLeft(110).reduced(2));
    favBtn.setBounds(top.removeFromRight(90).reduced(2));
    initBtn.setBounds(top.removeFromRight(60).reduced(2));
    saveBtn.setBounds(top.removeFromRight(80).reduced(2));
    auto bottom = r.removeFromBottom(34);
    mutateBtn.setBounds(bottom.removeFromLeft(100).reduced(2));
    mutateLabel.setBounds(bottom.removeFromLeft(56));
    mutateAmount.setBounds(bottom.removeFromLeft(200).reduced(2, 8));
    for (auto* b : {&lockOsc, &lockFilt, &lockMod, &lockFx}) b->setBounds(bottom.removeFromLeft(110).reduced(2));
    listBox.setBounds(r.reduced(0, 4));
}

// ================= SETTINGS PAGE =================
class SettingsPage::UpdatePanel : public juce::Component {
public:
    explicit UpdatePanel(UiContext& c) : ctx(c) {
        mode.addItemList({"Updates off", "Notify me", "Install automatically"}, 1);
        auto& u = ctx.proc.updater();
        mode.setSelectedId(u.mode() == Updater::Mode::Off ? 1 : (u.mode() == Updater::Mode::Notify ? 2 : 3), juce::dontSendNotification);
        mode.onChange = [this] { int id = mode.getSelectedId(); ctx.proc.updater().setMode(id == 1 ? Updater::Mode::Off : (id == 2 ? Updater::Mode::Notify : Updater::Mode::Auto)); };
        check.onClick = [this] { ctx.proc.updater().checkNow(); };
        install.onClick = [this] { ctx.proc.updater().installUpdate(); };
        rollback.onClick = [this] { ctx.proc.updater().rollBack(); };
        notes.setMultiLine(true); notes.setReadOnly(true);
        for (juce::Component* comp : std::initializer_list<juce::Component*>{&mode, &check, &install, &rollback, &status, &notes, &about}) addAndMakeVisible(comp);
        refresh();
    }
    void refresh() {
        auto& u = ctx.proc.updater();
        status.setText(u.statusText(), juce::dontSendNotification);
        install.setEnabled(u.pluginUpdateAvailable() || u.contentUpdateAvailable());
        rollback.setEnabled(u.hasBackup());
        rollback.setButtonText(u.hasBackup() ? "Roll back to " + u.backupVersion() : "Roll back");
        notes.setText(u.releaseNotes().isNotEmpty() ? "Release notes for " + u.latestVersion() + ":\n" + u.releaseNotes() : juce::String("No release notes loaded yet. Click Check now."), false);
        about.setText("GNARL " + juce::String(GNARL_VERSION_STRING) + "   content " + juce::String(u.installedContentVersion()) + "   plugins install to ~/Library/Audio/Plug-Ins. Plugin updates take effect the next time your DAW starts.", juce::dontSendNotification);
    }
    void paint(juce::Graphics& g) override { g.setColour(colours::panel); g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2), 6.f); g.setColour(colours::text); g.setFont(juce::FontOptions(12.f, juce::Font::bold)); g.drawText("Updates", 10, 4, 200, 16, juce::Justification::left); }
    void resized() override {
        auto r = getLocalBounds().reduced(8); r.removeFromTop(20);
        auto row = r.removeFromTop(28);
        mode.setBounds(row.removeFromLeft(200).reduced(2)); check.setBounds(row.removeFromLeft(110).reduced(2));
        install.setBounds(row.removeFromLeft(110).reduced(2)); rollback.setBounds(row.removeFromLeft(170).reduced(2));
        status.setBounds(r.removeFromTop(22)); about.setBounds(r.removeFromBottom(22)); notes.setBounds(r.reduced(0, 4));
    }
    UiContext& ctx;
    juce::ComboBox mode; juce::TextButton check{"Check now"}, install{"Install update"}, rollback{"Roll back"};
    juce::Label status, about; juce::TextEditor notes;
};

SettingsPage::SettingsPage(UiContext& c) : ScrollPage(2), ctx(c) {
    section(ctx, "Voice", {"voices", "legato", "glide_time", "glide_curve", "glide_legato", "bend_range"}, 6);
    section(ctx, "Quality and output drive", {"oversampling", "render_os", "post_drive_type", "post_drive"}, 4);
    panel = make<UpdatePanel>(ctx);
    flow.add(panel, [](int) { return 260; }, 2);
    ctx.proc.updater().addChangeListener(this);
}
SettingsPage::~SettingsPage() { ctx.proc.updater().removeChangeListener(this); }
void SettingsPage::changeListenerCallback(juce::ChangeBroadcaster*) { if (panel) panel->refresh(); }

// ================= MACRO STRIP =================
MacroStrip::MacroStrip(UiContext& c) : ctx(c) {
    for (int k = 0; k < 8; ++k) {
        auto* h = new SourceHandle(ctx, MS_Macro1 + k, juce::String::fromUTF8(ctx.proc.structure().macroNames[(size_t) k].c_str()));
        h->setTooltip("Macro " + juce::String(k + 1) + ": drag onto any knob to assign. Double-click to rename.");
        h->addMouseListener(this, false);
        handles.add(h); addAndMakeVisible(h);
        auto* kn = new ModKnob(ctx, Registry::get().indexOf("macro" + std::to_string(k + 1)), " ");
        knobs.add(kn); addAndMakeVisible(kn);
    }
    for (int s : {MS_Velocity, MS_KeyTrack, MS_ModWheel, MS_Aftertouch, MS_PitchBend, MS_ReleaseVel, MS_VoiceCount, MS_MpePressure, MS_MpeSlide, MS_Follower, MS_Transient}) {
        if (ctx.proc.isFxPlugin() && (s == MS_VoiceCount || s == MS_MpePressure || s == MS_MpeSlide || s == MS_ReleaseVel)) continue;
        auto* g = new SourceHandle(ctx, s); globals.add(g); addAndMakeVisible(g);
    }
    ctx.proc.structureChanged.addChangeListener(this);
}
MacroStrip::~MacroStrip() { ctx.proc.structureChanged.removeChangeListener(this); for (auto* h : handles) h->removeMouseListener(this); }
void MacroStrip::changeListenerCallback(juce::ChangeBroadcaster*) {
    for (int k = 0; k < 8; ++k) handles[k]->setText(juce::String::fromUTF8(ctx.proc.structure().macroNames[(size_t) k].c_str()));
}
void MacroStrip::mouseDoubleClick(const juce::MouseEvent& e) {
    for (int k = 0; k < handles.size(); ++k) {
        if (e.eventComponent != handles[k]) continue;
        auto* w = new juce::AlertWindow("Rename macro", "Name macro " + juce::String(k + 1) + " for what it does (e.g. Squelch, Tear, Vowel).", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor("name", juce::String::fromUTF8(ctx.proc.structure().macroNames[(size_t) k].c_str()));
        w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Cancel", 0);
        w->enterModalState(true, juce::ModalCallbackFunction::create([this, w, k](int r) {
            if (r != 1) return;
            auto n = w->getTextEditorContents("name").trim().substring(0, 24).toStdString();
            ctx.proc.editStructure("Rename macro", [k, n](PatchStructure& s) { s.macroNames[(size_t) k] = n; });
        }), true);
    }
}

void MacroStrip::paint(juce::Graphics& g) { g.setColour(colours::panel); g.fillRect(getLocalBounds()); g.setColour(colours::edge); g.drawHorizontalLine(0, 0, (float) getWidth()); }
void MacroStrip::resized() {
    auto r = getLocalBounds().reduced(4);
    auto macros = r.removeFromLeft(r.getWidth() * 2 / 3);
    const int w = macros.getWidth() / 8;
    for (int k = 0; k < 8; ++k) {
        auto cell = macros.removeFromLeft(w).reduced(3, 0);
        handles[k]->setBounds(cell.removeFromBottom(22));
        knobs[k]->setBounds(cell.withSizeKeepingCentre(std::min(cell.getWidth(), cell.getHeight() + 14), cell.getHeight()));
    }
    const int gw = r.getWidth() / std::max(1, (globals.size() + 1) / 2);
    for (int i = 0; i < globals.size(); ++i) globals[i]->setBounds(r.getX() + (i / 2) * gw, r.getY() + (i % 2) * (r.getHeight() / 2), gw - 4, r.getHeight() / 2 - 4);
}

} // namespace gnarl::ui
