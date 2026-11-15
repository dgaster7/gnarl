#include "Theme.h"
#include "engine/PatchStructure.h"

namespace gnarl::ui {

juce::Colour colours::forSource(int s) {
    if (s >= MS_Env1 && s <= MS_Env4) return juce::Colour(0xff4cc9f0);
    if (s >= MS_Lfo1 && s <= MS_Lfo6) return juce::Colour(0xffb6ff3b);
    if (s >= MS_Seq1 && s <= MS_Seq2) return juce::Colour(0xffffd166);
    if (s >= MS_Rnd1 && s <= MS_Rnd4) return juce::Colour(0xfff78c6b);
    if (s >= MS_Macro1 && s <= MS_Macro8) return juce::Colour(0xffff4fd8);
    if (s >= MS_OscA && s <= MS_OscC) return juce::Colour(0xff06d6a0);
    if (s >= MS_Fn1 && s <= MS_Fn4) return juce::Colour(0xffb388ff);
    return juce::Colour(0xffe0e0e0);
}

Theme::Theme() {
    using namespace colours;
    setColour(juce::ResizableWindow::backgroundColourId, bg);
    setColour(juce::Label::textColourId, text);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, panel2);
    setColour(juce::ComboBox::backgroundColourId, panel2);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, edge);
    setColour(juce::ComboBox::arrowColourId, dim);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha(0.25f));
    setColour(juce::PopupMenu::highlightedTextColourId, text);
    setColour(juce::TextButton::buttonColourId, panel2);
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TextButton::textColourOnId, bg);
    setColour(juce::TextButton::buttonOnColourId, accent);
    setColour(juce::TextEditor::backgroundColourId, panel2);
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::TextEditor::outlineColourId, edge);
    setColour(juce::TextEditor::focusedOutlineColourId, accent);
    setColour(juce::ListBox::backgroundColourId, panel);
    setColour(juce::ScrollBar::thumbColourId, edge.brighter(0.3f));
    setColour(juce::TooltipWindow::backgroundColourId, panel2);
    setColour(juce::TooltipWindow::textColourId, text);
    setColour(juce::TooltipWindow::outlineColourId, edge);
    setColour(juce::ToggleButton::textColourId, text);
    setColour(juce::ToggleButton::tickColourId, accent);
}

void Theme::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) {
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(4);
    const float r = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float a = start + pos * (end - start);
    juce::Path track; track.addCentredArc(c.x, c.y, r, r, 0, start, end, true);
    g.setColour(colours::knobTrack); g.strokePath(track, juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // bipolar controls fill from the centre
    const bool bip = s.getMinimum() < 0 && s.getMaximum() > 0;
    float from = bip ? start + (float) ((0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) * (end - start) : start;
    if (bip && s.getSkewFactor() != 1.0) from = start + (float) s.valueToProportionOfLength(0) * (end - start);
    juce::Path val; val.addCentredArc(c.x, c.y, r, r, 0, std::min(from, a), std::max(from, a), true);
    g.setColour(s.isEnabled() ? colours::accent : colours::dim); g.strokePath(val, juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(colours::panel2); g.fillEllipse(c.x - r * 0.62f, c.y - r * 0.62f, r * 1.24f, r * 1.24f);
    juce::Path p; p.addRoundedRectangle(-1.5f, -r * 0.6f, 3.f, r * 0.42f, 1.5f);
    g.setColour(colours::text); g.fillPath(p, juce::AffineTransform::rotation(a).translated(c.x, c.y));
}

void Theme::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle st, juce::Slider& s) {
    juce::Rectangle<float> r((float) x, (float) y, (float) w, (float) h);
    g.setColour(colours::knobTrack); g.fillRoundedRectangle(r.reduced(1), 3.f);
    g.setColour(colours::accent.withAlpha(s.isEnabled() ? 0.85f : 0.3f));
    if (st == juce::Slider::LinearVertical) g.fillRoundedRectangle(r.withTop(pos).reduced(1), 3.f);
    else g.fillRoundedRectangle(r.withRight(pos).reduced(1), 3.f);
}

void Theme::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool hl, bool) {
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft(std::min(r.getHeight(), 22.f)).reduced(4);
    g.setColour(b.getToggleState() ? colours::accent : colours::knobTrack);
    g.fillRoundedRectangle(box, 3.f);
    if (hl) { g.setColour(colours::text.withAlpha(0.3f)); g.drawRoundedRectangle(box, 3.f, 1.f); }
    g.setColour(colours::text);
    g.setFont(juce::FontOptions(11.f));
    g.drawFittedText(b.getButtonText(), r.toNearestInt().withTrimmedLeft(2), juce::Justification::centredLeft, 2);
}

void Theme::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool hl, bool down) {
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    auto c = b.getToggleState() ? colours::accent : base;
    if (down) c = c.brighter(0.2f); else if (hl) c = c.brighter(0.08f);
    g.setColour(c); g.fillRoundedRectangle(r, 4.f);
    g.setColour(colours::edge); g.drawRoundedRectangle(r, 4.f, 1.f);
}

void Theme::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb) {
    auto r = juce::Rectangle<float>(0, 0, (float) w, (float) h).reduced(0.5f);
    g.setColour(cb.findColour(juce::ComboBox::backgroundColourId)); g.fillRoundedRectangle(r, 3.f);
    g.setColour(colours::edge); g.drawRoundedRectangle(r, 3.f, 1.f);
    juce::Path arrow; float ax = (float) w - 10, ay = (float) h * 0.5f;
    arrow.addTriangle(ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour(colours::dim); g.fillPath(arrow);
}

void Theme::drawTabButton(juce::TabBarButton& b, juce::Graphics& g, bool over, bool) {
    auto r = b.getLocalBounds().toFloat().reduced(2, 4);
    const bool front = b.isFrontTab();
    g.setColour(front ? colours::accent : (over ? colours::panel2.brighter(0.1f) : colours::panel2));
    g.fillRoundedRectangle(r, 4.f);
    g.setColour(front ? colours::bg : colours::text);
    g.setFont(juce::FontOptions(12.f, juce::Font::bold));
    g.drawText(b.getButtonText(), r, juce::Justification::centred);
}

} // namespace gnarl::ui
