#pragma once
#include "Displays.h"
#include "../PresetManager.h"
#include "../Updater.h"

namespace gnarl::ui {

// Lays out child components in columns; each child reports a height via the `height` callback.
class Flow : public juce::Component {
public:
    struct Item { juce::Component* c; std::function<int(int)> height; int span = 1; };
    explicit Flow(int columns) : cols(columns) {}
    void add(juce::Component* c, std::function<int(int)> h, int span = 1) { items.push_back({c, std::move(h), span}); addAndMakeVisible(c); }
    void addSection(Section* s, int span = 1) { add(s, [s](int w) { return s->preferredHeight(w); }, span); }
    int layout(int width);
    void resized() override { layout(getWidth()); }
    std::vector<Item> items;
    int cols;
};

class ScrollPage : public juce::Component {
public:
    explicit ScrollPage(int columns) : flow(columns) { view.setViewedComponent(&flow, false); view.setScrollBarsShown(true, false); addAndMakeVisible(view); }
    void resized() override { view.setBounds(getLocalBounds()); int h = flow.layout(getWidth() - 10); flow.setSize(getWidth() - 10, h); }
    Flow flow;
    juce::OwnedArray<juce::Component> owned;
    template <typename T, typename... A> T* make(A&&... a) { auto* p = new T(std::forward<A>(a)...); owned.add(p); return p; }
    Section* section(UiContext& ctx, const juce::String& t, const std::vector<std::string>& ids, int cols = 6, int span = 1) { auto* s = make<Section>(ctx, t, ids, cols); flow.addSection(s, span); return s; }
private:
    juce::Viewport view;
};

class OscPage : public juce::Component, private juce::AudioProcessorValueTreeState::Listener, private juce::AsyncUpdater {
public:
    explicit OscPage(UiContext& ctx);
    ~OscPage() override;
    void resized() override;
private:
    void parameterChanged(const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { updateVisibility(); }
    void updateVisibility();
    UiContext& ctx;
    juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop};
    struct OscTab;
    std::vector<OscTab*> oscTabs;
};

class FilterPage : public ScrollPage { public: explicit FilterPage(UiContext& ctx); };
class ModPage : public juce::Component { public: explicit ModPage(UiContext& ctx); void resized() override; private: juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop}; };
class FxPage : public juce::Component, private juce::ChangeListener {
public:
    explicit FxPage(UiContext& ctx);
    ~FxPage() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void showModule(int m);
private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    UiContext& ctx;
    class SlotList; std::unique_ptr<SlotList> slots;
    juce::OwnedArray<juce::Component> editors; // one per module (all exist; only the selected one is visible)
    std::unique_ptr<Section> clipper;
    int shown = -1;
};
class MatrixPage : public juce::Component, private juce::ChangeListener {
public:
    explicit MatrixPage(UiContext& ctx);
    ~MatrixPage() override;
    void resized() override;
private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    UiContext& ctx;
    class Row; juce::OwnedArray<Row> rows;
    juce::Component content; juce::Viewport view;
    juce::OwnedArray<juce::Component> fnRows;
};
class PresetPage : public juce::Component, private juce::ChangeListener, private juce::ListBoxModel {
public:
    explicit PresetPage(UiContext& ctx);
    ~PresetPage() override;
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override { refresh(); }
    int getNumRows() override { return list.size(); }
    void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void selectedRowsChanged(int row) override;
    void refresh();
    UiContext& ctx;
    juce::TextEditor search; juce::ComboBox category, bank; juce::ToggleButton favOnly{"Favourites"};
    juce::ListBox listBox{"presets", this};
    juce::Array<PresetInfo> list;
    juce::TextButton saveBtn{"Save..."}, initBtn{"Init"}, favBtn{"Favourite"}, mutateBtn{"Mutate"};
    juce::Slider mutateAmount; juce::Label mutateLabel;
    juce::ToggleButton lockOsc{"Lock osc"}, lockFilt{"Lock filters"}, lockMod{"Lock mod"}, lockFx{"Lock FX"};
};
class SettingsPage : public ScrollPage, private juce::ChangeListener {
public:
    explicit SettingsPage(UiContext& ctx);
    ~SettingsPage() override;
private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    class UpdatePanel; UpdatePanel* panel = nullptr;
    UiContext& ctx;
};

// Bottom strip: macros (renameable, draggable) and global performance sources.
class MacroStrip : public juce::Component, private juce::ChangeListener {
public:
    explicit MacroStrip(UiContext& ctx);
    ~MacroStrip() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    UiContext& ctx;
    juce::OwnedArray<SourceHandle> handles; juce::OwnedArray<ModKnob> knobs; juce::OwnedArray<SourceHandle> globals;
};

juce::String moduleTitle(int m);
std::vector<std::string> moduleParamIds(int m, bool fxPlugin);

} // namespace gnarl::ui
