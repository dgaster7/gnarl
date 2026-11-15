#pragma once
#include "Pages.h"

namespace gnarl {

class GnarlEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer, private juce::ChangeListener {
public:
    explicit GnarlEditor(GnarlProcessor& p);
    ~GnarlEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void dragOperationEnded(const juce::DragAndDropTarget::SourceDetails&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void setScale(float s);
    juce::Component& content() { return root; }
    static constexpr int kBaseW = 1280, kBaseH = 820;
    // For the Check-3 binding audit: every component (including tab pages not currently shown)
    static void collectBound(juce::Component& c, std::vector<juce::Component*>& out);
private:
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    GnarlProcessor& proc;
    ui::Theme theme;
    ui::UiContext ctx;
    juce::Component root;
    juce::TooltipWindow tooltips{this, 600};
    juce::Label title, presetName;
    juce::TextButton prevBtn{"<"}, nextBtn{">"}, abBtn{"A"}, copyAb{"A>B"}, undoBtn{"Undo"}, redoBtn{"Redo"}, mutateBtn{"Mutate"}, updateBadge{"Update"};
    juce::ComboBox sizeBox;
    std::unique_ptr<ui::ModKnob> master;
    std::unique_ptr<ui::Meter> meter;
    juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop};
    std::unique_ptr<ui::MacroStrip> macros;
    float scale = 1.f;
    juce::ComponentBoundsConstrainer constrainer;
};

} // namespace gnarl
