#include "GnarlProcessor.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new gnarl::GnarlProcessor(true); }
