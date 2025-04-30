#pragma once

#include <juce_dsp/juce_dsp.h>

/**
 * EnvelopeFollower processes audio to extract amplitude envelope information
 * Useful for level detection, dynamics processing, and modulation
 */
class EnvelopeFollower
{
public:
    EnvelopeFollower();
    ~EnvelopeFollower() = default;

    struct Parameters
    {
        float attackMs = 20.0f;
        float releaseMs = 100.0f;
        juce::dsp::BallisticsFilterLevelCalculationType levelType;
    };

    void prepare(const juce::dsp::ProcessSpec &spec);
    void allocateVectors(size_t numChannels);
    void reset();

    template <typename ProcessContext>
    void process(const ProcessContext &context);
    void processSample(int ch, float sample);

    void setParameters(const Parameters &params, bool force = false);

    // Get the average level across all channels
    float getAverageLevel(int channel = 0) const;

private:
    template <typename SampleType>
    void gainInterpolator(const juce::dsp::AudioBlock<SampleType> &inputBlock, size_t numSamples);
    void setInterpolationParameters();

    struct EnvelopeState
    {
        float envelope = 0.0f;
        float rmsSum = 0.0f;
        int rmsSamples = 0;
    };

    std::vector<EnvelopeState> envelopeStates {};

    float inAttackMs  = 20.0f;
    float inReleaseMs = 100.0f;
    float epsilonAt   = 0.0f;
    float epsilonRe   = 0.0f;
    juce::dsp::BallisticsFilterLevelCalculationType inLevelType = juce::dsp::BallisticsFilterLevelCalculationType::RMS;
    int numChannels = 2;

    // Coefficients for attack and release
    double attackCoef = 0.0;
    double releaseCoef = 0.0;
    float sampleRate = 44100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeFollower)
};