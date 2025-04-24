#pragma once

#include <juce_dsp/juce_dsp.h>
#include "dsp/TempoSyncUtils.h"

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
    //////////////////////// CONSTANTS ////////////////////////////////
    // Input Processor constants
    static constexpr float minPreampLevel = 0.0f;
    static constexpr float maxPreampLevel = 120.0f;
    static constexpr float minPreampOverdrive = 100.0f;
    static constexpr float maxPreampOverdrive = 120.0f;
    static constexpr float minWaveshaperDrive = 1.0f;
    static constexpr float maxWaveshaperDrive = 6.0f;
    static constexpr float minReverbMix = 0.0f;
    static constexpr float maxReverbMix = 100.0f;
    static constexpr float minBandpassFrequency = 20.0f;
    static constexpr float maxBandpassFrequency = 20000.0f;
    static constexpr float defaultBandpassFrequency = 4000.0f;
    static constexpr float minBandpassWidth = 200.0f;
    static constexpr float maxBandpassWidth = 10000.0f;
    static constexpr float defaultBandpassWidth = 1000.0f;
    // Tree Processor constants
    static constexpr float minTreeSize = 0.2f;
    static constexpr float maxTreeSize = 1.8f;
    static constexpr float minAttackTime = 1.0f; // TODO: this should be a multiplication factor on the tempo
    static constexpr float maxAttackTime = 2500.0f;
    static constexpr float minReleaseTime = 1.0f; // TODO: this should be a multiplication factor on the tempo
    static constexpr float maxReleaseTime = 500.0f;
    static constexpr float minTreeDensity = 0.0f;
    static constexpr float maxTreeDensity = 100.0f;
    // Universe Control constants
    static constexpr float minStretch = -4.0f;
    static constexpr float maxStretch = 16.0f;
    static constexpr float centreStretch = 0.25f;
    static constexpr float minTempoValue = 30.0f;
    static constexpr float maxTempoValue = 300.0f;
    static constexpr float defaultTempoValue = 120.0f;
    static constexpr float minScarcityAbundance = -1.0f;
    static constexpr float maxScarcityAbundance = 1.0f;
    static constexpr float minFoldPosition = -1.0f;
    static constexpr float maxFoldPosition = 1.0f;
    static constexpr float minFoldWindowShape = -1.0f;
    static constexpr float maxFoldWindowShape = 1.0f;
    static constexpr float minFoldWindowSize = 0.1f;
    static constexpr float maxFoldWindowSize = 0.8f;
    // Mycelia Processor constants
    static constexpr float minEntanglement = 0.0f;
    static constexpr float maxEntanglement = 100.0f;
    static constexpr float minGrowthRate = 0.0f;
    static constexpr float maxGrowthRate = 100.0f;
    static constexpr float centerGrowthRate = 20.0f;
    static constexpr int   minNutrientBands = 1;
    static constexpr int   maxNutrientBands = 16;
    // DelayProc constants
    static constexpr float minAge = 0.0f;
    static constexpr float maxAge = 1.0f;
    static constexpr float defaultAge = 0.0f;
    // Delay Processor constants
    static constexpr float minDelayMs = 0.0f;
    static constexpr float maxDelayMs = 4000.0f;
    static constexpr float centreDelay = 200.0f;
    static constexpr float maxPan = 1.0f;
    static constexpr float minFeedback = 0.0f;
    static constexpr float maxFeedback = 0.99f;
    static constexpr float maxGain = 12.0f;
    static constexpr float minFilterFreq = 200.0f;
    static constexpr float maxFilterFreq = 20000.0f;
    static constexpr float minFilterSmoothTimeSec = 0.05f;
    static constexpr float maxFilterSmoothTimeSec = 1800.0f;
    static constexpr float minFilterGainDb = 0.0f;
    static constexpr float maxFilterGainDb = 24.0f;
    static constexpr float minDispersion = 0.0f;
    static constexpr float maxDispersion = 1.0f;
    static constexpr float minReverse = 0.0f;
    static constexpr float maxReverse = 1000.0f;
    // Sky constants
    static constexpr float minSkyHumidity = 0.0f;
    static constexpr float maxSkyHumidity = 100.0f;
    static constexpr float minSkyHeight = 0.0f;
    static constexpr float maxSkyHeight = 100.0f;
    // Output Processor constants
    static constexpr float minDryWet = -1.0f;
    static constexpr float maxDryWet = 1.0f;
    static constexpr float minDelayMsDuck = 0.0f;
    static constexpr float maxDelayMsDuck = 100.0f;
    // MIDI Constants
    static constexpr int   minMidiCcValue = 0;
    static constexpr int   maxMidiCcValue = 127;

    /////////////////////////// RANGES ////////////////////////////////
    // Input parameters
    inline const juce::NormalisableRange<float> preampLevelRange(minPreampLevel, maxPreampLevel, 0.1f);
    inline const juce::NormalisableRange<float> preampOverdriveRange(minPreampOverdrive, maxPreampOverdrive, 0.1f);
    inline const juce::NormalisableRange<float> waveshaperDriveRange(minWaveshaperDrive, maxWaveshaperDrive, 0.01f);
    inline const juce::NormalisableRange<float> reverbMixRange(minReverbMix, maxReverbMix, 0.1f);

    // Input sculpting
    inline const juce::NormalisableRange<float> bandpassFrequencyRange(minBandpassFrequency, maxBandpassFrequency,
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
    inline const juce::NormalisableRange<float> bandpassWidthRange(minBandpassWidth, maxBandpassWidth,
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

    inline const juce::NormalisableRange<float> rangeWithSkewForCentre(float min, float max, float centre)
    {
        juce::NormalisableRange<float> range{min, max, 0.01f};
        range.setSkewForCentre(centre);
        return range;
    }

    // Tree parameters
    inline const juce::NormalisableRange<float> treeSizeRange(minTreeSize, maxTreeSize, 0.01f);
    inline const juce::NormalisableRange<float> attackTimeRange(minAttackTime, maxAttackTime, 0.01f);   // TODO: this should be a multiplication factor on the tempo
    inline const juce::NormalisableRange<float> releaseTimeRange(minReleaseTime,                // TODO: this should be a multiplication factor on the tempo
                                                                 maxReleaseTime,
                                                                 invertedConvertFrom0To1Func,
                                                                 invertedConvertTo0To1Func,
                                                                 invertedSnapToLegalValueFunction);
    inline const juce::NormalisableRange<float> treeDensityRange(minTreeDensity, maxTreeDensity, 0.1f);

    // Helper functions for quantized stretch values
    inline float getQuantizedStretchValue(float normalizedValue)
    {
        // This function handles the bottom half of the dial (0.0 - 0.5 normalized)
        // Maps to quantized musical intervals as negative values

        // Scale the 0.0-0.5 range to 0.0-1.0 to use with TempoSyncUtils
        float tempoParam = 1.0f - (normalizedValue * 2.0f);

        // Get the corresponding rhythm from TempoSyncUtils
        const auto& rhythm = TempoSyncUtils::getRhythmForParam(tempoParam);

        // Convert the rhythm's tempo factor to our stretch scale but make it negative
        // Tempo factors in TempoSyncUtils range from 0.125 (1/32) to 8.0 (2/1)
        return -static_cast<float>(rhythm.tempoFactor);
    }

    // Universe controls
    inline const juce::NormalisableRange<float> stretchRange(minStretch, maxStretch,
        // Convert from normalized 0-1 to actual value
        [](float start, float end, float normalised) {
            if (normalised > 0.5f) {
                // Top half: continuous values from center to max
                // Map 0.5-1.0 to center-max
                float scaledNormalized = (normalised - 0.5f) * 2.0f;
                return centreStretch + scaledNormalized * (end - centreStretch);
            } else {
                // Bottom half: quantized musical values as negative values
                return getQuantizedStretchValue(normalised);
            }
        },
        // Convert from actual value to normalized 0-1
        [](float start, float end, float value) {
            if (value >= centreStretch) {
                // Values above center: map center-max to 0.5-1.0
                return 0.5f + 0.5f * (value - centreStretch) / (end - centreStretch);
            } else {
                // For negative values, find the closest rhythm in our array
                float absValue = std::abs(value);

                // Find the rhythm with the closest tempo factor to our value
                size_t closestIndex = 0;
                float closestDiff = std::numeric_limits<float>::max();

                for (size_t i = 0; i < TempoSyncUtils::rhythms.size(); ++i) {
                    float diff = std::abs(static_cast<float>(TempoSyncUtils::rhythms[i].tempoFactor) - absValue);
                    if (diff < closestDiff) {
                        closestDiff = diff;
                        closestIndex = i;
                    }
                }

                // Inverting the formula from getRhythmForParam:
                // idx = (rhythms.size() - 1) * std::pow(param01, 1.5f)
                // param01 = std::pow(idx / (rhythms.size() - 1), 1.0f/1.5f)
                float param01 = std::pow(static_cast<float>(closestIndex) /
                                         static_cast<float>(TempoSyncUtils::rhythms.size() - 1),
                                         1.0f / 1.5f);

                return param01 * 0.5f; // Scale to 0-0.5 range
            }
        },
        // Snap to legal value function
        [](float start, float end, float value) {
            // Always snap to exact quantized values in bottom half (negative values)
            if (value < centreStretch) {
                // Find the closest quantized musical interval
                for (const auto& rhythm : TempoSyncUtils::rhythms) {
                    if (std::abs(static_cast<float>(-rhythm.tempoFactor) - value) < 0.01f)
                        return static_cast<float>(-rhythm.tempoFactor);
                }

                // If no close match, find closest rhythm
                float closestDiff = std::numeric_limits<float>::max();
                float closestValue = centreStretch;

                for (const auto& rhythm : TempoSyncUtils::rhythms) {
                    float rhythmValue = static_cast<float>(-rhythm.tempoFactor);
                    float diff = std::abs(rhythmValue - value);
                    if (diff < closestDiff) {
                        closestDiff = diff;
                        closestValue = rhythmValue;
                    }
                }

                return closestValue;
            }

            // Top half is continuous, just constrain to range
            return juce::jlimit(centreStretch, end, value);
        }
    );

    inline const juce::NormalisableRange<float> tempoValueRange(minTempoValue, maxTempoValue, 1.0f);
    inline const juce::NormalisableRange<float> midiCcValueRange(minMidiCcValue, maxMidiCcValue, 0.01f);

    // Mycelia parameters
    inline const juce::NormalisableRange<float> entanglementRange(minEntanglement, maxEntanglement, 0.01f);
    inline const juce::NormalisableRange<float> growthRateRange = rangeWithSkewForCentre(minGrowthRate, maxGrowthRate, centerGrowthRate);
    inline const juce::NormalisableRange<int>   nutrientBandsRange(minNutrientBands, maxNutrientBands, 1.0f);

    // Delay Processor parameters
    inline const juce::NormalisableRange<float> delayRange = rangeWithSkewForCentre(minDelayMs, maxDelayMs, centreDelay);
    inline const juce::NormalisableRange<float> panRange{-maxPan, maxPan};
    inline const juce::NormalisableRange<float> fbRange{minFeedback, maxFeedback};
    inline const juce::NormalisableRange<float> gainRange{-maxGain, maxGain};
    inline const juce::NormalisableRange<float> filterFreqRange = rangeWithSkewForCentre(minFilterFreq, maxFilterFreq, std::sqrt(minFilterFreq *maxFilterFreq));
    inline const juce::NormalisableRange<float> filterSmoothTimeRange{minFilterSmoothTimeSec, maxFilterSmoothTimeSec};
    inline const juce::NormalisableRange<float> filterGainRangeDb{minFilterGainDb, maxFilterGainDb};
    inline const juce::NormalisableRange<float> dispRange{minDispersion, maxDispersion};
    inline const juce::NormalisableRange<float> revRange{minReverse, maxReverse};
    inline const juce::NormalisableRange<float> panModRange{-maxPan, maxPan};

    // Universe parameters
    inline const juce::NormalisableRange<float> scarcityAbundanceRange(minScarcityAbundance, maxScarcityAbundance, 0.1f);
    inline const juce::NormalisableRange<float> foldPositionRange(minFoldPosition, maxFoldPosition, 0.01f);
    inline const juce::NormalisableRange<float> foldWindowShapeRange(minFoldWindowShape, maxFoldWindowShape, 0.01f);
    inline const juce::NormalisableRange<float> foldWindowSizeRange(minFoldWindowSize, maxFoldWindowSize, 0.01f);

    // Sky parameters
    inline const juce::NormalisableRange<float> skyHumidityRange(minSkyHumidity, maxSkyHumidity, 0.01f);
    inline const juce::NormalisableRange<float> skyHeightRange(minSkyHeight, maxSkyHeight, 0.01f);

    // Output parameters
    inline const juce::NormalisableRange<float> dryWetRange(minDryWet, maxDryWet, 0.01f);
    inline const juce::NormalisableRange<float> delayDuckRange(minDelayMsDuck, maxDelayMsDuck, 0.01f);

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