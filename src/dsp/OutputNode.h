#pragma once
#include <juce_dsp/juce_dsp.h>

/**
 * Audio processor that implements input gain and sculpting
 */
class OutputNode
{
public:
    OutputNode();

    // processing functions
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    template <typename ProcessContext>
    void process (const ProcessContext &wetContext, const ProcessContext &dryContext);

    struct Parameters
    {
        float dryWetMixLevel;
    };

    void setParameters (const Parameters& params);

private:
    float fs = 44100.0f;

    float inDryWetMixLevel;

    juce::dsp::Gain<float> wetGain;
    juce::dsp::Gain<float> dryGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputNode)
};
