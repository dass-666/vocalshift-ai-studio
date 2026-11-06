#include "PluginProcessor.h"

// Entry point used by the JUCE plugin wrappers (VST3). Kept separate so tests and
// the bench can link the processor without a second definition.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new vs::VocalShiftProcessor();
}
