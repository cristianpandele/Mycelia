#pragma once
#include <juce_dsp/juce_dsp.h>

/**
 * Audio processor that implements input gain and sculpting
 */
class InputNode
{
public:
    InputNode();

    // processing functions
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    template <typename ProcessContext>
    void process (const ProcessContext& context);

    struct Parameters
    {
        float gainLevel;
    };

    void setParameters (const Parameters& params);

private:
    float fs = 44100.0f;

    float inGainLevel;

    juce::dsp::Gain<float> gain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputNode)
};
