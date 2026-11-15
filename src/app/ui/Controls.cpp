#include "Controls.h"
#include "engine/PatchStructure.h"

namespace gnarl::ui {

juce::String shortLabel(const ParamDef& p) {
    // drop the section prefix ("Osc A ", "F1 ", "LFO 2 ") so labels fit under knobs
    juce::String n = juce::String::fromUTF8(p.name.c_str());
    for (const char* pre : {"Osc A ", "Osc B ", "Osc C ", "F1 ", "F2 ", "Env 1 ", "Env 2 ", "Env 3 ", "Env 4 ", "LFO 1 ", "LFO 2 ", "LFO 3 ", "LFO 4 ", "LFO 5 ", "LFO 6 ",
                            "Seq 1 ", "Seq 2 ", "Random 1 ", "Random 2 ", "Random 3 ", "Random 4 ", "Dist A ", "Dist B ", "MB Dist ", "Phaser ", "Flanger ", "Chorus ",
                            "Comb ", "Vowel ", "EQ ", "Shift ", "Ring ", "Crush ", "MB Comp ", "Comp ", "Delay ", "Reverb ", "Mod Delay ", "Resonator ", "Disperser ",
                            "Spectral ", "Tamer ", "Split ", "Vocoder ", "Function 1 ", "Function 2 ", "Function 3 ", "Function 4 "})
        if (n.startsWith(pre)) { n = n.substring((int) std::strlen(pre)); break; }
    return n;
}

juce::String tooltipFor(int i, juce::RangedAudioParameter* p) {
    const auto& d = Registry::get()[i];
    juce::String t = juce::String::fromUTF8(d.name.c_str());
    if (p) t << ": " << p->getCurrentValueAsText();
    t << "\n" << juce::String::fromUTF8(d.doc.c_str());
    if (!d.isDiscrete()) t << "\nRange " << p->getText(0.f, 32) << " to " << p->getText(1.f, 32) << ". Double-click to type a value, shift-drag for fine control, alt-click to reset.";
    if (d.modulatable && !d.isDiscrete()) t << " Drop a modulation source here to modulate it.";
    return t;
}

// ---------------- FineSlider ----------------
void FineSlider::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(e); return; }
    if (e.mods.isAltDown()) { setValue(getDoubleClickReturnValue(), juce::sendNotificationSync); return; }
    setMouseDragSensitivity(e.mods.isShiftDown() ? 1200 : 220);
    juce::Slider::mouseDown(e);
}
void FineSlider::mouseUp(const juce::MouseEvent& e) { juce::Slider::mouseUp(e); setMouseDragSensitivity(220); }

// ---------------- ModKnob ----------------
ModKnob::ModKnob(UiContext& c, int p, const juce::String& label) : ctx(c), index(p) {
    const auto& d = Registry::get()[p];
    getProperties().set(kParamProperty, juce::String(d.id));
    knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.setMouseDragSensitivity(220);
    knob.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    auto* param = ctx.proc.paramByIndex(p);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(ctx.proc.state(), d.id, knob);
    knob.setDoubleClickReturnValue(true, d.defaultFor(ctx.proc.isFxPlugin()));
    knob.tooltipText = [this, param] { return tooltipFor(index, param); };
    knob.onDoubleClick = [this] { typeValue(); };
    knob.onRightClick = [this](const juce::MouseEvent&) { showMenu(); };
    addAndMakeVisible(knob);
    name.setText(label.isNotEmpty() ? label : shortLabel(d), juce::dontSendNotification);
    name.setJustificationType(juce::Justification::centred);
    name.setColour(juce::Label::textColourId, colours::dim);
    name.setFont(juce::FontOptions(11.f));
    name.setInterceptsMouseClicks(false, false);
    name.setMinimumHorizontalScale(0.6f);
    addAndMakeVisible(name);
    startTimerHz(30);
}
ModKnob::~ModKnob() { stopTimer(); }

juce::Rectangle<float> ModKnob::knobArea() const {
    auto r = getLocalBounds().toFloat().withTrimmedBottom(14);
    float s = std::min(r.getWidth(), r.getHeight());
    return r.withSizeKeepingCentre(s, s);
}

void ModKnob::resized() {
    auto k = knobArea();
    knob.setBounds(k.reduced(k.getWidth() * 0.12f).toNearestInt());
    name.setBounds(getLocalBounds().removeFromBottom(14));
}

void ModKnob::timerCallback() {
    const auto& d = Registry::get()[index];
    float v = ctx.proc.getEngine().displayValues()[index].load(std::memory_order_relaxed);
    float n = d.toNorm(v);
    if (std::fabs(n - liveNorm) > 0.002f) { liveNorm = n; if (!ctx.proc.routesForParam(index).empty()) repaint(); }
}

void ModKnob::paint(juce::Graphics& g) {
    auto a = knobArea();
    if (dropHover || (ctx.dragging && Registry::get()[index].modulatable && !Registry::get()[index].isDiscrete())) {
        g.setColour(colours::accent2.withAlpha(dropHover ? 0.45f : 0.18f));
        g.fillEllipse(a.reduced(1));
    }
}

void ModKnob::paintOverChildren(juce::Graphics& g) {
    const auto& d = Registry::get()[index];
    auto routes = ctx.proc.routesForParam(index);
    if (routes.empty()) return;
    auto a = knobArea();
    const auto c = a.getCentre();
    const float start = juce::MathConstants<float>::pi * 1.25f, end = juce::MathConstants<float>::pi * 2.75f;
    auto* param = ctx.proc.paramByIndex(index);
    const float base = param ? param->getValue() : 0.f;
    float r = a.getWidth() * 0.5f - 1.5f;
    const auto& st = ctx.proc.structure();
    for (int k : routes) {
        const auto& rt = st.routes[(size_t) k];
        float depth = ctx.proc.routeDepth(k);
        float lo = base, hi = juce::jlimit(0.f, 1.f, base + depth);
        if (rt.bipolar) lo = juce::jlimit(0.f, 1.f, base - std::fabs(depth));
        if (hi < lo) std::swap(lo, hi);
        juce::Path arc; arc.addCentredArc(c.x, c.y, r, r, 0, start + lo * (end - start), start + hi * (end - start), true);
        auto col = colours::forSource(rt.source);
        if (rt.bypass) col = col.withAlpha(0.3f);
        if (ctx.hoverSource == rt.source) { g.setColour(col.withAlpha(0.35f)); g.strokePath(arc, juce::PathStrokeType(6.f)); }
        g.setColour(col); g.strokePath(arc, juce::PathStrokeType(2.2f));
        r -= 3.f;
    }
    if (liveNorm >= 0) {
        float ang = start + juce::jlimit(0.f, 1.f, liveNorm) * (end - start);
        float rr = a.getWidth() * 0.5f - 1.5f;
        g.setColour(colours::text);
        g.fillEllipse(c.x + rr * std::sin(ang) - 2.5f, c.y - rr * std::cos(ang) - 2.5f, 5.f, 5.f);
    }
    (void) d;
}

int ModKnob::routeAt(juce::Point<float> p) const {
    auto routes = ctx.proc.routesForParam(index);
    if (routes.empty()) return -1;
    auto a = knobArea();
    float dist = p.getDistanceFrom(a.getCentre());
    float r = a.getWidth() * 0.5f - 1.5f;
    // the arcs occupy the ring outside the knob body
    if (dist < r - 3.f * (float) routes.size() - 2.f || dist > r + 4.f) return -1;
    int i = juce::jlimit(0, (int) routes.size() - 1, (int) ((r + 2.f - dist) / 3.f));
    return routes[(size_t) i];
}

void ModKnob::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) { showMenu(); return; }
    dragRoute = routeAt(e.position);
    if (dragRoute >= 0) {
        if (e.mods.isAltDown()) { // modifier-click toggles bipolar
            int k = dragRoute;
            ctx.proc.editStructure("Route polarity", [k](PatchStructure& s) { s.routes[(size_t) k].bipolar = !s.routes[(size_t) k].bipolar; });
            dragRoute = -1; repaint(); return;
        }
        dragStartDepth = ctx.proc.routeDepth(dragRoute);
        ctx.proc.undo().beginNewTransaction("Modulation depth");
    }
}
void ModKnob::mouseDrag(const juce::MouseEvent& e) {
    if (dragRoute < 0) return;
    float delta = (float) (-e.getDistanceFromDragStartY() + e.getDistanceFromDragStartX()) / (e.mods.isShiftDown() ? 600.f : 150.f);
    ctx.proc.setRouteDepth(dragRoute, dragStartDepth + delta);
    repaint();
}
void ModKnob::mouseUp(const juce::MouseEvent&) { dragRoute = -1; }
void ModKnob::mouseDoubleClick(const juce::MouseEvent& e) {
    int k = routeAt(e.position);
    if (k >= 0) { ctx.proc.setRouteDepth(k, 0); repaint(); }
}

bool ModKnob::isInterestedInDragSource(const SourceDetails& s) {
    const auto& d = Registry::get()[index];
    return s.description.toString().startsWith("modsrc:") && d.modulatable && !d.isDiscrete();
}
void ModKnob::itemDropped(const SourceDetails& s) {
    dropHover = false;
    int src = s.description.toString().fromFirstOccurrenceOf("modsrc:", false, false).getIntValue();
    ctx.proc.addRoute(src, index, 0.35f);
    if (ctx.repaintControls) ctx.repaintControls();
    repaint();
}

void ModKnob::typeValue() {
    auto* p = ctx.proc.paramByIndex(index);
    if (!p) return;
    editor = std::make_unique<juce::TextEditor>();
    editor->setText(p->getCurrentValueAsText(), false);
    editor->setJustification(juce::Justification::centred);
    editor->setBounds(getLocalBounds().withSizeKeepingCentre(std::max(getWidth(), 60), 20));
    addAndMakeVisible(*editor);
    editor->selectAll(); editor->grabKeyboardFocus();
    auto commit = [this, p] {
        if (!editor) return;
        auto t = editor->getText();
        p->beginChangeGesture(); p->setValueNotifyingHost(p->getValueForText(t)); p->endChangeGesture();
        juce::MessageManager::callAsync([sp = juce::Component::SafePointer<ModKnob>(this)] { if (sp) sp->editor.reset(); });
    };
    editor->onReturnKey = commit;
    editor->onFocusLost = commit;
    editor->onEscapeKey = [this] { juce::MessageManager::callAsync([sp = juce::Component::SafePointer<ModKnob>(this)] { if (sp) sp->editor.reset(); }); };
}

void ModKnob::showMenu() {
    juce::PopupMenu m;
    auto routes = ctx.proc.routesForParam(index);
    const auto& st = ctx.proc.structure();
    m.addItem(1, "Type value...");
    m.addItem(2, "Reset to default");
    if (!routes.empty()) {
        m.addSeparator();
        m.addSectionHeader("Modulation");
        for (int k : routes) {
            const auto& r = st.routes[(size_t) k];
            juce::PopupMenu sub;
            sub.addItem(100 + k * 4 + 0, "Remove");
            sub.addItem(100 + k * 4 + 1, "Bypass", true, r.bypass);
            sub.addItem(100 + k * 4 + 2, "Invert depth");
            sub.addItem(100 + k * 4 + 3, "Bipolar", true, r.bipolar);
            m.addSubMenu(juce::String(modSourceName(r.source)) + "  " + juce::String(ctx.proc.routeDepth(k) * 100.f, 0) + "%", sub);
        }
        m.addItem(3, "Remove all modulation");
    }
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [sp = juce::Component::SafePointer<ModKnob>(this)](int r) {
        if (!sp || r == 0) return;
        auto& self = *sp;
        if (r == 1) self.typeValue();
        else if (r == 2) { if (auto* p = self.ctx.proc.paramByIndex(self.index)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->getDefaultValue()); p->endChangeGesture(); } }
        else if (r == 3) { for (int k : self.ctx.proc.routesForParam(self.index)) self.ctx.proc.removeRoute(k); }
        else if (r >= 100) {
            int k = (r - 100) / 4, op = (r - 100) % 4;
            if (op == 0) self.ctx.proc.removeRoute(k);
            else if (op == 1) self.ctx.proc.editStructure("Bypass route", [k](PatchStructure& s) { s.routes[(size_t) k].bypass = !s.routes[(size_t) k].bypass; });
            else if (op == 2) self.ctx.proc.setRouteDepth(k, -self.ctx.proc.routeDepth(k));
            else self.ctx.proc.editStructure("Route polarity", [k](PatchStructure& s) { s.routes[(size_t) k].bipolar = !s.routes[(size_t) k].bipolar; });
        }
        if (self.ctx.repaintControls) self.ctx.repaintControls();
        self.repaint();
    });
}

// ---------------- combo / toggle ----------------
ParamCombo::ParamCombo(UiContext& ctx, int p, const juce::String& label) {
    const auto& d = Registry::get()[p];
    getProperties().set(kParamProperty, juce::String(d.id));
    if (d.role == Role::Choice) { int i = 1; for (auto& c : d.choices) box.addItem(juce::String::fromUTF8(c.c_str()), i++); }
    else for (int v = (int) d.min; v <= (int) d.max; ++v) box.addItem(juce::String(v), v - (int) d.min + 1);
    box.setTooltip(tooltipFor(p, ctx.proc.paramByIndex(p)));
    addAndMakeVisible(box);
    name.setText(label.isNotEmpty() ? label : shortLabel(d), juce::dontSendNotification);
    name.setJustificationType(juce::Justification::centred);
    name.setColour(juce::Label::textColourId, colours::dim);
    name.setFont(juce::FontOptions(11.f));
    name.setMinimumHorizontalScale(0.6f);
    addAndMakeVisible(name);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(ctx.proc.state(), d.id, box);
}
void ParamCombo::resized() {
    auto r = getLocalBounds();
    name.setBounds(r.removeFromBottom(14));
    box.setBounds(r.withSizeKeepingCentre(r.getWidth() - 2, std::min(22, r.getHeight())));
}

ParamToggle::ParamToggle(UiContext& ctx, int p, const juce::String& label) {
    const auto& d = Registry::get()[p];
    getProperties().set(kParamProperty, juce::String(d.id));
    button.setButtonText(label.isNotEmpty() ? label : shortLabel(d));
    button.setTooltip(tooltipFor(p, ctx.proc.paramByIndex(p)));
    addAndMakeVisible(button);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(ctx.proc.state(), d.id, button);
}

std::unique_ptr<juce::Component> makeControl(UiContext& ctx, int p, const juce::String& label) {
    const auto& d = Registry::get()[p];
    if (d.role == Role::Choice || (d.role == Role::Integer && d.max - d.min <= 12)) return std::make_unique<ParamCombo>(ctx, p, label);
    if (d.role == Role::Toggle) return std::make_unique<ParamToggle>(ctx, p, label);
    return std::make_unique<ModKnob>(ctx, p, label);
}

// ---------------- SourceHandle ----------------
SourceHandle::SourceHandle(UiContext& c, int s, const juce::String& text) : ctx(c), src(s), label(text.isNotEmpty() ? text : juce::String(modSourceName(s))) {
    setTooltip("Drag " + label + " onto any knob to modulate it. Hover to see what it modulates.");
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
}
void SourceHandle::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(1);
    auto col = colours::forSource(src);
    g.setColour(col.withAlpha(ctx.hoverSource == src ? 0.45f : 0.2f));
    g.fillRoundedRectangle(r, 4.f);
    g.setColour(col); g.drawRoundedRectangle(r, 4.f, 1.f);
    // grip dots
    for (int i = 0; i < 3; ++i) g.fillEllipse(r.getX() + 5.f, r.getCentreY() - 5.f + (float) i * 4.f, 2.5f, 2.5f);
    g.setColour(colours::text);
    g.setFont(juce::FontOptions(11.f, juce::Font::bold));
    g.drawFittedText(label, r.toNearestInt().withTrimmedLeft(12), juce::Justification::centred, 1);
}
void SourceHandle::mouseEnter(const juce::MouseEvent&) { ctx.hoverSource = src; if (ctx.repaintControls) ctx.repaintControls(); }
void SourceHandle::mouseExit(const juce::MouseEvent&) { ctx.hoverSource = -1; if (ctx.repaintControls) ctx.repaintControls(); }
void SourceHandle::mouseDrag(const juce::MouseEvent&) {
    if (auto* dd = juce::DragAndDropContainer::findParentDragContainerFor(this))
        if (!dd->isDragAndDropActive()) {
            ctx.dragging = true;
            if (ctx.repaintControls) ctx.repaintControls();
            dd->startDragging("modsrc:" + juce::String(src), this);
        }
}

// ---------------- Section ----------------
Section::Section(UiContext& ctx, const juce::String& t, const std::vector<std::string>& ids, int columns, bool) : title(t), cols(columns) {
    auto& R = Registry::get();
    const uint8_t mask = ctx.proc.isFxPlugin() ? kFx : kSynth;
    for (auto& id : ids) {
        int i = R.indexOf(id);
        if (i < 0 || !(R[i].plugins & mask)) continue;
        auto c = makeControl(ctx, i);
        controls.push_back(c.get());
        addAndMakeVisible(*c);
        owned.add(c.release());
    }
}
int Section::preferredHeight(int width) const {
    int c = std::max(1, std::min(cols, (int) controls.size()));
    int rows = ((int) controls.size() + c - 1) / c;
    int cell = std::max(40, (width - 12) / std::max(cols, 1));
    return 22 + rows * std::min(cell, 66) + 6;
}
void Section::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(2);
    g.setColour(colours::panel); g.fillRoundedRectangle(r, 6.f);
    g.setColour(colours::edge); g.drawRoundedRectangle(r, 6.f, 1.f);
    g.setColour(colours::text); g.setFont(juce::FontOptions(12.f, juce::Font::bold));
    g.drawText(title, r.reduced(8, 3).removeFromTop(16), juce::Justification::centredLeft);
}
void Section::resized() {
    auto r = getLocalBounds().reduced(6);
    auto top = r.removeFromTop(18);
    for (int i = header.size(); --i >= 0;) header[i]->setBounds(top.removeFromRight(header[i]->getWidth() > 0 ? header[i]->getWidth() : 80).reduced(1));
    if (controls.empty()) return;
    const int c = std::max(1, cols);
    const int cw = r.getWidth() / c;
    const int rows = ((int) controls.size() + c - 1) / c;
    const int rh = std::min(66, std::max(30, r.getHeight() / std::max(rows, 1)));
    for (size_t i = 0; i < controls.size(); ++i) {
        int col = (int) i % c, row = (int) i / c;
        auto cell = juce::Rectangle<int>(r.getX() + col * cw, r.getY() + row * rh, cw, rh).reduced(2);
        auto* comp = controls[i];
        if (dynamic_cast<ParamToggle*>(comp)) cell = cell.withSizeKeepingCentre(cell.getWidth(), 24);
        else if (dynamic_cast<ParamCombo*>(comp)) cell = cell.withSizeKeepingCentre(cell.getWidth(), std::min(cell.getHeight(), 40));
        comp->setBounds(cell);
    }
}

} // namespace gnarl::ui
