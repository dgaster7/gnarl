#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../GnarlProcessor.h"
#include "Theme.h"
#include "params/Registry.h"

namespace gnarl::ui {

// Shared UI state (one per editor).
struct UiContext {
    GnarlProcessor& proc;
    int hoverSource = -1;   // modulation source under the mouse (highlights its targets)
    bool dragging = false;  // a source is being dragged (valid targets light up)
    std::function<void()> repaintControls;
    explicit UiContext(GnarlProcessor& p) : proc(p) {}
};

// Every control bound to a parameter carries this property; the Check-3 binding audit walks the tree for it.
constexpr const char* kParamProperty = "gnarlParam";

juce::String tooltipFor(int paramIndex, juce::RangedAudioParameter* p);

class FineSlider : public juce::Slider {
public:
    std::function<void()> onDoubleClick;
    std::function<void(const juce::MouseEvent&)> onRightClick;
    juce::String getTooltip() override { return tooltipText ? tooltipText() : juce::String(); }
    std::function<juce::String()> tooltipText;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override { if (onDoubleClick) onDoubleClick(); else juce::Slider::mouseDoubleClick(e); }
};

class ModKnob : public juce::Component, public juce::DragAndDropTarget, private juce::Timer {
public:
    ModKnob(UiContext& ctx, int paramIndex, const juce::String& label = {});
    ~ModKnob() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDragEnter(const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit(const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped(const SourceDetails&) override;
    int param() const { return index; }
    FineSlider& slider() { return knob; }
private:
    void timerCallback() override;
    void showMenu();
    void typeValue();
    int routeAt(juce::Point<float> p) const;
    juce::Rectangle<float> knobArea() const;
    UiContext& ctx;
    int index;
    FineSlider knob;
    juce::Label name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    bool dropHover = false;
    int dragRoute = -1; float dragStartDepth = 0;
    float liveNorm = -1;
    std::unique_ptr<juce::TextEditor> editor;
};

class ParamCombo : public juce::Component {
public:
    ParamCombo(UiContext& ctx, int paramIndex, const juce::String& label = {});
    void resized() override;
    juce::ComboBox box;
private:
    juce::Label name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

class ParamToggle : public juce::Component {
public:
    ParamToggle(UiContext& ctx, int paramIndex, const juce::String& label = {});
    void resized() override { button.setBounds(getLocalBounds()); }
    juce::ToggleButton button;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// Draggable modulation source handle.
class SourceHandle : public juce::Component, public juce::SettableTooltipClient {
public:
    SourceHandle(UiContext& ctx, int source, const juce::String& text = {});
    void paint(juce::Graphics&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    int source() const { return src; }
    void setText(const juce::String& t) { label = t; repaint(); }
private:
    UiContext& ctx;
    int src;
    juce::String label;
};

// A titled panel holding auto-generated controls for a list of parameter IDs.
class Section : public juce::Component {
public:
    Section(UiContext& ctx, const juce::String& title, const std::vector<std::string>& ids, int columns = 6, bool fxPluginFilter = true);
    void paint(juce::Graphics&) override;
    void resized() override;
    void addHeaderComponent(juce::Component* c) { header.add(c); addAndMakeVisible(c); }
    int preferredHeight(int width) const;
    juce::String title;
    std::vector<juce::Component*> controls;
private:
    juce::OwnedArray<juce::Component> owned;
    juce::Array<juce::Component*> header;
    int cols;
};

std::unique_ptr<juce::Component> makeControl(UiContext& ctx, int paramIndex, const juce::String& label = {});
juce::String shortLabel(const ParamDef& p);

} // namespace gnarl::ui
