#include "GnarlProcessor.h"
#include "params/Registry.h"

namespace gnarl {

juce::AudioProcessor::BusesProperties GnarlProcessor::busesFor(bool fx) {
    if (fx)
        return juce::AudioProcessor::BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true)
            .withInput("Sidechain", juce::AudioChannelSet::stereo(), false);
    return juce::AudioProcessor::BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

GnarlProcessor::GnarlProcessor(bool fxPlugin)
    : juce::AudioProcessor(busesFor(fxPlugin)), fx(fxPlugin), engine(fxPlugin),
      apvts(*this, nullptr, "GNARL", makeLayout()) {
    auto& R = Registry::get();
    for (int i : R.forPlugin(fx ? kFx : kSynth))
        if (auto* p = apvts.getRawParameterValue(R[i].id)) paramLinks.emplace_back(i, p);
    midiScratch.reserve(4096);
}

GnarlProcessor::~GnarlProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout GnarlProcessor::makeLayout() const {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto& R = Registry::get();
    for (int i : R.forPlugin(fx ? kFx : kSynth)) {
        const auto& p = R[i];
        juce::ParameterID pid{p.id, 1};
        if (p.role == Role::Choice) {
            juce::StringArray c; for (auto& s : p.choices) c.add(s);
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid, p.name, c, (int) p.def));
        } else if (p.role == Role::Toggle) {
            layout.add(std::make_unique<juce::AudioParameterBool>(pid, p.name, p.def > 0.5f));
        } else if (p.role == Role::Integer) {
            layout.add(std::make_unique<juce::AudioParameterInt>(pid, p.name, (int) p.min, (int) p.max, (int) p.def));
        } else {
            juce::NormalisableRange<float> r(p.min, p.max, p.interval, p.skew);
            layout.add(std::make_unique<juce::AudioParameterFloat>(pid, p.name, r, p.def));
        }
    }
    return layout;
}

bool GnarlProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (fx) {
        if (l.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) return false;
        if (l.inputBuses.size() > 1) {
            auto sc = l.getChannelSet(true, 1);
            if (!sc.isDisabled() && sc != juce::AudioChannelSet::stereo() && sc != juce::AudioChannelSet::mono()) return false;
        }
    }
    return true;
}

void GnarlProcessor::prepareToPlay(double sr, int block) {
    engine.prepare(sr, block);
    scratch.setSize(2, block);
    lastLatency = engine.latencySamples();
    setLatencySamples(lastLatency);
}

void GnarlProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    for (auto& [idx, ptr] : paramLinks) engine.setParam(idx, ptr->load(std::memory_order_relaxed));
    midiScratch.clear();
    for (const auto m : midi) {
        auto msg = m.getMessage();
        if (msg.getRawDataSize() < 1 || msg.getRawDataSize() > 3 || midiScratch.size() == midiScratch.capacity()) continue;
        MidiEvent e; e.offset = m.samplePosition;
        auto* raw = msg.getRawData();
        e.status = raw[0]; e.d1 = msg.getRawDataSize() > 1 ? raw[1] : 0; e.d2 = msg.getRawDataSize() > 2 ? raw[2] : 0;
        midiScratch.push_back(e);
    }
    HostInfo hi;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm()) hi.bpm = *b;
            if (auto p = pos->getPpqPosition()) hi.ppq = *p;
            hi.playing = pos->getIsPlaying();
        }
    hi.offline = isNonRealtime();
    const int n = buffer.getNumSamples();
    auto mainIn = getBusBuffer(buffer, true, 0);
    auto out = getBusBuffer(buffer, false, 0);
    const float *scL = nullptr, *scR = nullptr;
    if (fx && getBusCount(true) > 1 && getBus(true, 1)->isEnabled()) {
        auto sc = getBusBuffer(buffer, true, 1);
        if (sc.getNumChannels() > 0) { scL = sc.getReadPointer(0); scR = sc.getReadPointer(sc.getNumChannels() > 1 ? 1 : 0); }
    }
    const float* inL = fx ? mainIn.getReadPointer(0) : nullptr;
    const float* inR = fx ? mainIn.getReadPointer(1) : nullptr;
    engine.process(inL, inR, out.getWritePointer(0), out.getWritePointer(1), n, midiScratch.data(), (int) midiScratch.size(), hi, scL, scR);
    const int lat = engine.latencySamples();
    if (lat != lastLatency) { lastLatency = lat; setLatencySamples(lat); }
}

void GnarlProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto st = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(st.createXml());
    copyXmlToBinary(*xml, dest);
}

void GnarlProcessor::setStateInformation(const void* data, int size) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* GnarlProcessor::createEditor() { return new juce::GenericAudioProcessorEditor(*this); }

} // namespace gnarl
