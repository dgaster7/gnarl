#include "Editor.h"
#include "../PresetManager.h"
#include "../Updater.h"

namespace gnarl {

GnarlEditor::GnarlEditor(GnarlProcessor& p) : juce::AudioProcessorEditor(p), proc(p), ctx(p) {
    setLookAndFeel(&theme);
    juce::LookAndFeel::setDefaultLookAndFeel(&theme);
    ctx.repaintControls = [this] { root.repaint(); };
    addAndMakeVisible(root);

    title.setText(proc.isFxPlugin() ? "GNARL FX" : "GNARL", juce::dontSendNotification);
    versionLabel.setText("v" + juce::String(GNARL_VERSION_STRING), juce::dontSendNotification);
    versionLabel.setFont(juce::FontOptions(10.f));
    versionLabel.setColour(juce::Label::textColourId, ui::colours::dim);
    versionLabel.setTooltip("Installed GNARL version. Updates are in the SETTINGS tab.");
    root.addAndMakeVisible(versionLabel);
    title.setFont(juce::FontOptions(22.f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, ui::colours::accent);
    presetName.setFont(juce::FontOptions(15.f, juce::Font::bold));
    presetName.setColour(juce::Label::backgroundColourId, ui::colours::panel2);
    presetName.setJustificationType(juce::Justification::centred);
    presetName.setTooltip("Current preset. Click to open the browser.");
    presetName.addMouseListener(this, false);
    prevBtn.onClick = [this] { proc.presets().step(-1); };
    nextBtn.onClick = [this] { proc.presets().step(1); };
    abBtn.setTooltip("A/B compare: switch between two versions of the patch.");
    abBtn.onClick = [this] { proc.toggleAB(); abBtn.setButtonText(proc.currentAB() == 0 ? "A" : "B"); };
    copyAb.setTooltip("Copy the current version into the other A/B slot.");
    copyAb.onClick = [this] { proc.storeAB(proc.currentAB() ^ 1); };
    undoBtn.onClick = [this] { proc.undo().undo(); };
    redoBtn.onClick = [this] { proc.undo().redo(); };
    mutateBtn.setTooltip("Mutate (30%): randomise within safe ranges. Finer control and locks are in the Presets tab.");
    mutateBtn.onClick = [this] {
        Patch pt = proc.currentPatch();
        mutatePatch(pt, 0.3f, {}, proc.isFxPlugin(), (uint32_t) juce::Time::getMillisecondCounter());
        pt.name = (proc.patchName().upToFirstOccurrenceOf(" *", false, false) + " *").toStdString();
        proc.loadPatch(pt, true);
    };
    updateBadge.setColour(juce::TextButton::buttonColourId, ui::colours::accent2);
    updateBadge.setTooltip("A GNARL update is available. Click for details.");
    updateBadge.onClick = [this] { tabs.setCurrentTabIndex(tabs.getNumTabs() - 1); };
    updateBadge.setVisible(false);
    for (int s : {75, 100, 125, 150, 175, 200}) sizeBox.addItem(juce::String(s) + "%", s);
    sizeBox.onChange = [this] { setScale(sizeBox.getSelectedId() / 100.f); };
    master = std::make_unique<ui::ModKnob>(ctx, Registry::get().indexOf("master_level"), "Master");
    meter = std::make_unique<ui::Meter>(ctx);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&title, &presetName, &prevBtn, &nextBtn, &abBtn, &copyAb, &undoBtn, &redoBtn, &mutateBtn, &updateBadge, &sizeBox, master.get(), meter.get()}) root.addAndMakeVisible(c);

    auto tabCol = ui::colours::panel;
    if (!proc.isFxPlugin()) tabs.addTab("OSC", tabCol, new ui::OscPage(ctx), true);
    tabs.addTab("FILTER", tabCol, new ui::FilterPage(ctx), true);
    tabs.addTab("MOD", tabCol, new ui::ModPage(ctx), true);
    tabs.addTab("FX", tabCol, new ui::FxPage(ctx), true);
    tabs.addTab("MATRIX", tabCol, new ui::MatrixPage(ctx), true);
    tabs.addTab("PRESETS", tabCol, new ui::PresetPage(ctx), true);
    tabs.addTab("SETTINGS", tabCol, new ui::SettingsPage(ctx), true);
    tabs.setTabBarDepth(34);
    tabs.setOutline(0);
    root.addAndMakeVisible(tabs);
    macros = std::make_unique<ui::MacroStrip>(ctx);
    root.addAndMakeVisible(*macros);

    proc.updater().addChangeListener(this);
    proc.updater().maybeCheckDaily();

    // persistent UI scale
    auto settings = juce::JSON::parse(GnarlProcessor::appDataDir().getChildFile("ui.json"));
    float s = (float) (double) settings.getProperty("scale", 1.0);
    constrainer.setFixedAspectRatio((double) kBaseW / kBaseH);
    constrainer.setSizeLimits((int) (kBaseW * 0.75f), (int) (kBaseH * 0.75f), kBaseW * 2, kBaseH * 2);
    setConstrainer(&constrainer);
    setResizable(true, true);
    setScale(juce::jlimit(0.75f, 2.f, s));
    startTimerHz(10);
}

GnarlEditor::~GnarlEditor() {
    proc.updater().removeChangeListener(this);
    presetName.removeMouseListener(this);
    setLookAndFeel(nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void GnarlEditor::setScale(float s) {
    scale = juce::jlimit(0.75f, 2.f, s);
    sizeBox.setSelectedId((int) std::lround(scale * 100 / 25) * 25, juce::dontSendNotification);
    setSize((int) std::lround(kBaseW * scale), (int) std::lround(kBaseH * scale));
    auto f = GnarlProcessor::appDataDir().getChildFile("ui.json");
    f.getParentDirectory().createDirectory();
    f.replaceWithText("{\"scale\": " + juce::String(scale) + "}");
}

void GnarlEditor::paint(juce::Graphics& g) { g.fillAll(ui::colours::bg); }

void GnarlEditor::resized() {
    scale = (float) getWidth() / (float) kBaseW;
    root.setTransform(juce::AffineTransform::scale(scale));
    root.setBounds(0, 0, kBaseW, kBaseH);
    auto r = juce::Rectangle<int>(0, 0, kBaseW, kBaseH);
    auto header = r.removeFromTop(46).reduced(8, 6);
    title.setBounds(header.removeFromLeft(proc.isFxPlugin() ? 112 : 82));
    versionLabel.setBounds(header.removeFromLeft(44).withTrimmedTop(14));
    prevBtn.setBounds(header.removeFromLeft(28).reduced(1));
    presetName.setBounds(header.removeFromLeft(300).reduced(2, 0));
    nextBtn.setBounds(header.removeFromLeft(28).reduced(1));
    header.removeFromLeft(10);
    abBtn.setBounds(header.removeFromLeft(34).reduced(1));
    copyAb.setBounds(header.removeFromLeft(44).reduced(1));
    undoBtn.setBounds(header.removeFromLeft(54).reduced(1));
    redoBtn.setBounds(header.removeFromLeft(54).reduced(1));
    mutateBtn.setBounds(header.removeFromLeft(70).reduced(1));
    sizeBox.setBounds(header.removeFromRight(80).reduced(1, 4));
    meter->setBounds(header.removeFromRight(18).reduced(0, 0));
    header.removeFromRight(6);
    master->setBounds(header.removeFromRight(70).withTrimmedTop(-6));
    updateBadge.setBounds(header.removeFromRight(80).reduced(1));
    macros->setBounds(r.removeFromBottom(96));
    tabs.setBounds(r);
}

void GnarlEditor::timerCallback() {
    presetName.setText(proc.patchName(), juce::dontSendNotification);
    undoBtn.setEnabled(proc.undo().canUndo());
    redoBtn.setEnabled(proc.undo().canRedo());
}

void GnarlEditor::changeListenerCallback(juce::ChangeBroadcaster*) {
    auto& u = proc.updater();
    updateBadge.setVisible(u.status() == Updater::Status::UpdateAvailable || (u.status() == Updater::Status::Installed && u.restartNeeded()));
    updateBadge.setButtonText(u.status() == Updater::Status::Installed ? "Restart DAW" : "Update");
    if (u.status() == Updater::Status::Installed && !u.restartNeeded()) proc.presets().rescan();
}

void GnarlEditor::mouseDown(const juce::MouseEvent& e) {
    if (e.eventComponent == &presetName)
        for (int i = 0; i < tabs.getNumTabs(); ++i) if (tabs.getTabNames()[i] == "PRESETS") tabs.setCurrentTabIndex(i);
}

void GnarlEditor::dragOperationEnded(const juce::DragAndDropTarget::SourceDetails&) { ctx.dragging = false; root.repaint(); }

void GnarlEditor::collectBound(juce::Component& c, std::vector<juce::Component*>& out) {
    if (c.getProperties().contains(ui::kParamProperty)) out.push_back(&c);
    if (auto* t = dynamic_cast<juce::TabbedComponent*>(&c)) {
        for (int i = 0; i < t->getNumTabs(); ++i) if (auto* page = t->getTabContentComponent(i)) collectBound(*page, out);
        return;
    }
    for (auto* ch : c.getChildren()) collectBound(*ch, out);
}

} // namespace gnarl
