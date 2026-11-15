// Renders every editor tab (and sub-tab) to PNG for visual review: gnarl_snapshot <outdir> [fx]
#include "GnarlProcessor.h"
#include "ui/Editor.h"
#include <juce_gui_basics/juce_gui_basics.h>
using namespace gnarl;
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    juce::File out(argc > 1 ? argv[1] : "snapshots");
    out.createDirectory();
    const bool fx = argc > 2 && juce::String(argv[2]) == "fx";
    GnarlProcessor proc(fx);
    proc.setPlayConfigDetails(fx ? 2 : 0, 2, 48000, 256);
    proc.prepareToPlay(48000, 256);
    if (argc > 3) proc.presets().loadFile(juce::File(argv[3]));
    juce::MidiBuffer midi; midi.addEvent(juce::MidiMessage::noteOn(1, 48, (juce::uint8) 100), 0);
    juce::AudioBuffer<float> buf(2, 256);
    for (int i = 0; i < 40; ++i) { proc.processBlock(buf, midi); midi.clear(); }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* editor = dynamic_cast<GnarlEditor*>(ed.get());
    editor->setScale(1.0f);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(300);
    std::function<void(juce::Component&, const juce::String&)> walk = [&](juce::Component& c, const juce::String& prefix) {
        for (auto* ch : c.getChildren()) {
            if (auto* t = dynamic_cast<juce::TabbedComponent*>(ch)) {
                for (int i = 0; i < t->getNumTabs(); ++i) {
                    t->setCurrentTabIndex(i);
                    juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
                    auto name = prefix + "_" + t->getTabNames()[i].replaceCharacters(" /", "__");
                    auto img = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
                    juce::PNGImageFormat png; juce::FileOutputStream os(out.getChildFile(name + ".png")); os.setPosition(0); os.truncate(); png.writeImageToStream(img, os);
                    if (auto* page = t->getTabContentComponent(i)) walk(*page, name);
                }
                t->setCurrentTabIndex(0);
            } else walk(*ch, prefix);
        }
    };
    walk(*editor, fx ? "fx" : "synth");
    std::printf("snapshots in %s\n", out.getFullPathName().toRawUTF8());
    return 0;
}
