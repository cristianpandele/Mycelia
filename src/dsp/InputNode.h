#pragma once
#include <juce_dsp/juce_dsp.h>

/**
 * Audio processor that implements input gain and sculpting
 */
class InputNode
{
public:
    struct Parameters
    {
        float gainLevel;       // Gain level (0.0 to 1.2)
        float bandpassFreq;    // Bandpass center frequency in Hz
        float bandpassWidth;   // Bandpass width (0.0 to 1.0)
        float reverbMix;       // Reverb mix level (0.0 to 1.0)
    };

    InputNode();

    // processing functions
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    template <typename ProcessContext>
    void process (const ProcessContext& context);

    void setParameters (const Parameters& params);

private:
    float fs = 44100.0f;

    float inGainLevel = 0.0f;
    float inBandpassFreq = 1000.0f;
    float inBandpassWidth = 0.5f;
    float inReverbMix = 0.0f;

    enum
    {
        gainIdx,
        filterIdx,
    };

    // Input gain
    juce::dsp::Gain<float> gain;

    // Bandpass filter
    using FilterCoefs = juce::dsp::IIR::Coefficients<float>;
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;

    // Chain
    juce::dsp::ProcessorChain<juce::dsp::Gain<float>, Filter> inputNodeChain;

    // Update filter coefficients based on current parameters
    void updateFilterCoefficients();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputNode)
};
