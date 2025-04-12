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

static constexpr float minPreampLevel = 0.0f;
static constexpr float maxPreampLevel = 120.0f;
static constexpr float minPreampOverdrive = 100.0f;
static constexpr float maxPreampOverdrive = 120.0f;
static constexpr float minWaveshaperGain = 1.0f;
static constexpr float maxWaveshaperGain = 12.0f;
static constexpr float minReverbMix = 0.0f;
static constexpr float maxReverbMix = 100.0f;
static constexpr float minBandpassFrequency = 20.0f;
static constexpr float maxBandpassFrequency = 20000.0f;
static constexpr float minBandpassWidth = 100.0f;
static constexpr float maxBandpassWidth = 20000.0f;
//
static constexpr float minTreeSize = 0.2f;
static constexpr float maxTreeSize = 1.8f;
static constexpr float minAttackTime = 1.0f; // TODO: this should be a multiplication factor on the tempo
static constexpr float maxAttackTime = 1500.0f;
static constexpr float minReleaseTime = 1.0f; // TODO: this should be a multiplication factor on the tempo
static constexpr float maxReleaseTime = 2000.0f;
static constexpr float minTreeDensity = 0.0f;
static constexpr float maxTreeDensity = 100.0f;
//
static constexpr float minStretch = -80.0f;
static constexpr float maxStretch = 400.0f;
static constexpr float minAbundanceScarcity = -80.0f;
static constexpr float maxAbundanceScarcity = 400.0f;
static constexpr float minFoldPosition = -1.0f;
static constexpr float maxFoldPosition = 1.0f;
static constexpr float minFoldWindowShape = -1.0f;
static constexpr float maxFoldWindowShape = 1.0f;
static constexpr float minFoldWindowSize = 0.1f;
static constexpr float maxFoldWindowSize = 0.8f;
//
static constexpr float minEntanglement = 0.0f;
static constexpr float maxEntanglement = 100.0f;
static constexpr float minGrowthRate = 0.0f;
static constexpr float maxGrowthRate = 100.0f;
// Delay Processor constants
static constexpr float minDelay = 0.0f;
static constexpr float maxDelay = 1500.0f;
static constexpr float centreDelay = 200.0f;
static constexpr float maxPan = 1.0f;
static constexpr float minFeedback = 0.0f;
static constexpr float maxFeedback = 0.99f;
static constexpr float maxGain = 12.0f;
static constexpr float minFilterFreq = 200.0f;
static constexpr float maxFilterFreq = 20000.0f;
static constexpr float minFilterGainDb = 20.0f;
static constexpr float maxFilterGainDb = 2000.0f;
static constexpr float minDispersion = 0.0f;
static constexpr float maxDispersion = 1.0f;
static constexpr float minReverse = 0.0f;
static constexpr float maxReverse = 1000.0f;
// static constexpr float minModFreq = 0.0f;
// static constexpr float maxModFreq = 5.0f;

// Sky constants
static constexpr float minSkyHumidity = 0.0f;
static constexpr float maxSkyHumidity = 100.0f;
static constexpr float minSkyHeight = 0.0f;
static constexpr float maxSkyHeight = 100.0f;

static constexpr float minDryWet = -1.0f;
static constexpr float maxDryWet = 1.0f;
static constexpr float minDelayDuck = 0.0f;
static constexpr float maxDelayDuck = 100.0f;

namespace ParameterRanges
{
    // Input parameters
    inline const juce::NormalisableRange<float> preampLevel(minPreampLevel, maxPreampLevel, 0.1f);
    inline const juce::NormalisableRange<float> preampOverdrive(minPreampOverdrive, maxPreampOverdrive, 0.1f);
    inline const juce::NormalisableRange<float> waveshaperGain(minWaveshaperGain, maxWaveshaperGain, 0.01f);
    inline const juce::NormalisableRange<float> reverbMix(minReverbMix, maxReverbMix, 0.1f);

    // Input sculpting
    inline const juce::NormalisableRange<float> bandpassFrequency(minBandpassFrequency, maxBandpassFrequency,
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
    inline const juce::NormalisableRange<float> bandpassWidth(minBandpassWidth, maxBandpassWidth,
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
    inline const juce::NormalisableRange<float> treeSize(minTreeSize, maxTreeSize, 0.01f);
    inline const juce::NormalisableRange<float> attackTime(minAttackTime, maxAttackTime, 0.01f);   // TODO: this should be a multiplication factor on the tempo
    inline const juce::NormalisableRange<float> releaseTime(minReleaseTime,                // TODO: this should be a multiplication factor on the tempo
                                                            maxReleaseTime,
                                                            invertedConvertFrom0To1Func,
                                                            invertedConvertTo0To1Func,
                                                            invertedSnapToLegalValueFunction);
    inline const juce::NormalisableRange<float> treeDensity(minTreeDensity, maxTreeDensity, 0.1f);

    // Universe controls
    inline const juce::NormalisableRange<float> stretch(minStretch, maxStretch, 0.1f);
    inline const juce::NormalisableRange<float> abundanceScarcity(minAbundanceScarcity, maxAbundanceScarcity, 0.1f);
    inline const juce::NormalisableRange<float> foldPosition(minFoldPosition, maxFoldPosition, 0.01f);
    inline const juce::NormalisableRange<float> foldWindowShape(minFoldWindowShape, maxFoldWindowShape, 0.01f);
    inline const juce::NormalisableRange<float> foldWindowSize(minFoldWindowSize, maxFoldWindowSize, 0.01f);

    // Mycelia parameters
    inline const juce::NormalisableRange<float> entanglement(minEntanglement, maxEntanglement, 0.01f);
    inline const juce::NormalisableRange<float> growthRate(minGrowthRate, maxGrowthRate, 0.01f);

    // Delay Processor parameters
    inline const juce::NormalisableRange<float> rangeWithSkewForCentre(float min, float max, float centre)
    {
        juce::NormalisableRange<float> range{min, max};
        range.setSkewForCentre(centre);
        return range;
    }
    inline const juce::NormalisableRange<float> delayRange = rangeWithSkewForCentre(minDelay, maxDelay, centreDelay);
    inline const juce::NormalisableRange<float> panRange{-maxPan, maxPan};
    inline const juce::NormalisableRange<float> fbRange{minFeedback, maxFeedback};
    inline const juce::NormalisableRange<float> gainRange{-maxGain, maxGain};
    inline const juce::NormalisableRange<float> filterFreqRange = rangeWithSkewForCentre(minFilterFreq, maxFilterFreq, std::sqrt(minFilterFreq *maxFilterFreq));
    inline const juce::NormalisableRange<float> filterGainRangeDb{minFilterGainDb, maxFilterGainDb};
    inline const juce::NormalisableRange<float> dispRange{minDispersion, maxDispersion};
    inline const juce::NormalisableRange<float> revRange{minReverse, maxReverse};
    // juce::NormalisableRange<float> modFreqRange{minModFreq, maxModFreq};
    // modFreqRange.setSkewForCentre(2.0f);
    // juce::NormalisableRange<float> delayModRange{0.0f, 1.0f};
    inline const juce::NormalisableRange<float> panModRange{-maxPan, maxPan};

    // Sky parameters
    inline const juce::NormalisableRange<float> skyHumidity(minSkyHumidity, maxSkyHumidity, 0.01f);
    inline const juce::NormalisableRange<float> skyHeight(minSkyHeight, maxSkyHeight, 0.01f);

    // Output parameters
    inline const juce::NormalisableRange<float> dryWet(minDryWet, maxDryWet, 0.01f);
    inline const juce::NormalisableRange<float> delayDuck(minDelayDuck, maxDelayDuck, 0.01f);

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