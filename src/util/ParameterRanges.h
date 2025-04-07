#pragma once

#include <juce_dsp/juce_dsp.h>

// Functions to convert between 0-1 and the actual range for ranges that are inverted
static constexpr auto invertedConvertFrom0To1Func = [](float start, float end, float value)
{
    return  (start - value) * (end - start) + start;
};
static constexpr auto invertedConvertTo0To1Func = [](float start, float end, float value)
{
    return (start / (end - start)) * (end - value);
};
static constexpr auto invertedSnapToLegalValueFunction = [](float start, float end, float value)
{
    if (value <= start || end <= start)
        return start;
    if (value >= end)
        return end;
    return value;
};

namespace ParameterRanges
{
    // Input parameters
    inline const juce::NormalisableRange<float> preampLevel(0.0f, 120.0f, 0.1f);
    inline const juce::NormalisableRange<float> preampOverdrive(100.0f, 120.0f, 0.1f);
    inline const juce::NormalisableRange<float> waveshaperGain(1.0f, 12.0f, 0.01f);
    inline const juce::NormalisableRange<float> reverbMix(0.0f, 100.0f, 0.1f);

    // Input sculpting
    inline const juce::NormalisableRange<float> bandpassFrequency(20.0f, 20000.0f,
        [](float start, float end, float normalised)
            { return start + (std::pow(2.0f, normalised * 10.0f) - 1.0f) * (end - start) / 1023.0f; },
        [](float start, float end, float value)
            { return (std::log(((value - start) * 1023.0f / (end - start)) + 1.0f) / std::log(2.0f)) / 10.0f; },
        [](float start, float end, float value)
            {
                if (value > 3000.0f)
                    return juce::jlimit(start, end, 100.0f * juce::roundToInt(value / 100.0f));
                if (value > 1000.0f)
                    return juce::jlimit(start, end, 10.0f * juce::roundToInt(value / 10.0f));
                return juce::jlimit(start, end, float(juce::roundToInt(value)));
            }
    );
    inline const juce::NormalisableRange<float> bandpassWidth(100.0f, 20000.0f,
        [](float start, float end, float normalised)
            { return start + (std::pow(2.0f, normalised * 10.0f) - 1.0f) * (end - start) / 1023.0f; },
        [](float start, float end, float value)
            { return (std::log(((value - start) * 1023.0f / (end - start)) + 1.0f) / std::log(2.0f)) / 10.0f; },
        [](float start, float end, float value)
            {
                if (value > 3000.0f)
                    return juce::jlimit(start, end, 100.0f * juce::roundToInt(value / 100.0f));
                if (value > 1000.0f)
                    return juce::jlimit(start, end, 10.0f * juce::roundToInt(value / 10.0f));
                return juce::jlimit(start, end, float(juce::roundToInt(value)));
            }
    );

    // Tree parameters
    inline const juce::NormalisableRange<float> treeSize(0.2f, 1.8f, 0.01f);
    inline const juce::NormalisableRange<float> attackTime(1.0f, 1500.0f, 0.01f);   // TODO: this should be a multiplication factor on the tempo
    inline const juce::NormalisableRange<float> releaseTime(1.0f,                // TODO: this should be a multiplication factor on the tempo
                                                            2000.0f,
                                                            invertedConvertFrom0To1Func,
                                                            invertedConvertTo0To1Func,
                                                            invertedSnapToLegalValueFunction);
    inline const juce::NormalisableRange<float> treeDensity(0.0f, 100.0f, 0.1f);

    // Universe controls
    inline const juce::NormalisableRange<float> stretch(-80.0f, 400.0f, 0.1f);
    inline const juce::NormalisableRange<float> abundanceScarcity(-80.0f, 400.0f, 0.1f);
    inline const juce::NormalisableRange<float> foldPosition(-1.0f, 1.0f, 0.01f);
    inline const juce::NormalisableRange<float> foldWindowShape(-1.0f, 1.0f, 0.01f);
    inline const juce::NormalisableRange<float> foldWindowSize(0.1f, 0.8f, 0.01f);

    // Mycelia parameters
    inline const juce::NormalisableRange<float> entanglement(0.0f, 100.0f, 0.01f);
    inline const juce::NormalisableRange<float> growthRate(0.0f, 100.0f, 0.01f);

    // Sky parameters
    inline const juce::NormalisableRange<float> skyHumidity(0.0f, 100.0f, 0.01f);
    inline const juce::NormalisableRange<float> skyHeight(0.0f, 100.0f, 0.01f);

    // Output parameters
    inline const juce::NormalisableRange<float> dryWet(-1.0f, 1.0f, 0.01f);
    inline const juce::NormalisableRange<float> delayDuck(0.0f, 100.0f, 0.01f);

    // Utility functions
    inline float normalizeParameter(const juce::NormalisableRange<float>& range, float value)
    {
        return range.convertTo0to1(value);
    }

    inline float denormalizeParameter(const juce::NormalisableRange<float>& range, float normalized)
    {
        return range.convertFrom0to1(normalized);
    }
}