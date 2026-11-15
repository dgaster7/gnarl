#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace gnarl::ui {

namespace colours {
inline const juce::Colour bg{0xff0e1114}, panel{0xff161b20}, panel2{0xff1c232a}, edge{0xff2a333c}, text{0xffe6edf2}, dim{0xff8a9aa6},
    accent{0xffb6ff3b}, accent2{0xffff4fd8}, warn{0xffffb347}, knobTrack{0xff2b3540};
// one colour per modulation source family
juce::Colour forSource(int src);
} // namespace colours

class Theme : public juce::LookAndFeel_V4 {
public:
    Theme();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(juce::FontOptions(12.f)); }
    juce::Font getLabelFont(juce::Label& l) override { return l.getFont(); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return juce::Font(juce::FontOptions(12.f, juce::Font::bold)); }
    void drawTabButton(juce::TabBarButton&, juce::Graphics&, bool over, bool down) override;
    int getTabButtonBestWidth(juce::TabBarButton&, int) override { return 92; }
    void drawTabAreaBehindFrontButton(juce::TabbedButtonBar&, juce::Graphics&, int, int) override {}
};

} // namespace gnarl::ui
