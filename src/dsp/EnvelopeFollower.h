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
        juce::dsp::BallisticsFilterLevelCalculationType levelType = juce::dsp::BallisticsFilterLevelCalculationType::RMS;
    };

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    
    template <typename ProcessContext>
    void process(const ProcessContext& context);
    
    void setParameters(const Parameters& params, bool force = false);
    
    // Get the current envelope value for a specific channel
    float getCurrentLevel(int channel = 0) const;
    
    // Get the average level across all channels
    float getAverageLevel() const;
    
private:
    std::unique_ptr<juce::dsp::BallisticsFilter<float>> filter;
    juce::AudioBuffer<float> analysisBuffer;
    
    float attackMs = 20.0f;
    float releaseMs = 100.0f;
    float currentLevel = 0.0f;
    int numChannels = 2;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeFollower)
};