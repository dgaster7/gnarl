#pragma once
#include "Controls.h"
#include "osc/Cycle.h"
#include "filter/Filter.h"

namespace gnarl::ui {

// Live single-cycle view of an oscillator (computed with the same CycleBuilder the DSP uses).
class WaveDisplay : public juce::Component, private juce::Timer {
public:
    WaveDisplay(UiContext& ctx, int osc);
    void paint(juce::Graphics&) override;
private:
    void timerCallback() override;
    UiContext& ctx; int osc;
    CycleBuilder builder;
    std::vector<float> table = std::vector<float>(CycleBuilder::kPadded, 0.f);
    std::vector<float> shown;
    CycleInputs last; int lastEngine = -1; float lastKey = 0;
};

// Magnitude response of both filters from the real coefficients, with draggable nodes.
class FilterDisplay : public juce::Component, private juce::Timer {
public:
    explicit FilterDisplay(UiContext& ctx);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    FilterSettings settingsFor(int k) const;
    struct Node { int filter; int kind; juce::Point<float> pos; }; // kind 0 cutoff/reso, 1 lower shoulder, 2 upper shoulder
    std::vector<Node> nodes() const;
    float xForHz(float hz) const; float hzForX(float x) const; float yForDb(float db) const;
    void setParam(const std::string& id, float value);
    UiContext& ctx;
    int dragNode = -1, hoverNode = -1;
    std::vector<Node> dragNodes;
    juce::Point<float> dragStart; float startA = 0, startB = 0;
};

// Breakpoint editor for LFO shapes, drawn envelopes and Draw distortion curves.
class ShapeEditor : public juce::Component, public juce::SettableTooltipClient {
public:
    using Getter = std::function<std::vector<ShapePoint>(const PatchStructure&)>;
    using Setter = std::function<void(PatchStructure&, const std::vector<ShapePoint>&)>;
    ShapeEditor(UiContext& ctx, Getter g, Setter s, float xmin, float xmax, float ymin, float ymax, int grid = 8);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void setGrid(int g) { grid = g; repaint(); }
    std::function<float()> playhead; // optional 0..1 position marker
    juce::String caption;
private:
    juce::Point<float> toScreen(const ShapePoint& p) const;
    ShapePoint fromScreen(juce::Point<float> s, bool snap) const;
    void commit(bool undoable);
    UiContext& ctx; Getter get; Setter set;
    float x0, x1, y0, y1; int grid;
    std::vector<ShapePoint> pts;
    int drag = -1; bool curveDrag = false; float curveStart = 0; juce::Point<float> downPos;
};

class StepEditor : public juce::Component, public juce::SettableTooltipClient {
public:
    StepEditor(UiContext& ctx, int seq);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    void setAt(juce::Point<float> p);
    UiContext& ctx; int seq;
    StepSeqData data; bool editing = false;
};

class Meter : public juce::Component, private juce::Timer {
public:
    explicit Meter(UiContext& c) : ctx(c) { startTimerHz(30); }
    void paint(juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    UiContext& ctx;
};

} // namespace gnarl::ui
