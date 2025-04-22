#pragma once

#include <juce_dsp/juce_dsp.h>
#include "EnvelopeFollower.h"

/**
 * A compressor that uses a sidechain input to "duck" the main signal.
 * The amount of ducking is proportional to the sidechain input level.
 */
class DuckingCompressor
{
    public:

        DuckingCompressor();
        ~DuckingCompressor();

        struct Parameters
        {
            float threshold;   // Threshold in dB, below which no compression is applied
            float ratio;       // Compression ratio (>= 1.0)
            float attackTime;  // Attack time in milliseconds
            float releaseTime; // Release time in milliseconds
            float kneeWidth;   // Width of the soft knee in dB
            float makeupGain;  // Makeup gain in dB (to compensate for compression)
            bool  enabled;     // Whether the compressor is enabled
        };

        // Prepares the compressor with the given spec
        void prepare(const juce::dsp::ProcessSpec &spec);

        // Resets the compressor state
        void reset();

        // Process a single sample with sidechain input
        float processSample(float inputSample, float sidechainLevel, size_t channel);

        // Sets compressor parameters
        void setParameters(const Parameters &newParams, bool force = false);

    private:
        // Default parameters
        static constexpr float defaultThreshold = -20.0f;
        static constexpr float defaultRatio = 4.0f;
        static constexpr float defaultAttackTime = 10.0f;
        static constexpr float defaultReleaseTime = 100.0f;
        static constexpr float defaultKneeWidth = 6.0f;
        static constexpr float defaultMakeupGain = 0.0f;

        // Parameters
        Parameters params =
        {
            defaultThreshold,
            defaultRatio,
            defaultAttackTime,
            defaultReleaseTime,
            defaultKneeWidth,
            defaultMakeupGain,
            true
        };

        // Internal state
        double sampleRate = 44100.0;
        std::vector<float> gainReduction;

        // Attack/release envelope processor
        EnvelopeFollower attackReleaseCalculator;

        // Helper to calculate gain reduction based on sidechain input level
        float calculateGainReduction(float sidechainLevelDb);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckingCompressor)
};