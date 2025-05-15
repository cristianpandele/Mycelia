#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "EnvelopeFollower.h"
#include "DuckingCompressor.h"
#include "util/ParameterRanges.h"
#include <array>

/**
 * OutputNode - Handles per-band ducking compression where diffusion bands
 * act as sidechain signals for compressing corresponding delay bands.
 */
class OutputNode
    : private juce::Timer
{
    public:
        // Parameters
        struct Parameters
        {
            float dryWetMixLevel;
            float delayDuckLevel;

            int numActiveBands = 4;  // Number of active bands

            // Envelope follower parameters
            EnvelopeFollower::Parameters envelopeFollowerParams;
        };

        OutputNode();
        ~OutputNode();

        // Processing functions
        void prepare(const juce::dsp::ProcessSpec &spec);
        void reset();

        template <typename ProcessContext>
        void process(
            const ProcessContext &wetContext,
            const ProcessContext &dryContext,
            std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &diffusionBandBuffers,
            std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &delayBandBuffers);

        void setParameters(const Parameters &params);

    private:
        void timerCallback();

        float fs = 44100.0f;

        float inDryWetMixLevel;
        float inDelayDuckLevel;
        // Booleans for parameter changes
        bool gainChanged = false;
        bool duckingChanged = false;
        bool envelopeFollowerChanged = false;

        juce::dsp::Gain<float> wetGain;
        juce::dsp::Gain<float> dryGain;

        // Parameters
        int inNumActiveBands = 4;

        //Envelope follower parameters
        EnvelopeFollower::Parameters inEnvelopeFollowerParams = {
            .attackMs = 2.0f,
            .releaseMs = 1.0f,
            .levelType = juce::dsp::BallisticsFilterLevelCalculationType::RMS
        };

        // Compression parameters
        DuckingCompressor::Parameters compressorParams = {
            .threshold = 0.0f,
            .ratio = 4.0f,
            .attackTime = 100.0f,
            .releaseTime = 25.0f,
            .kneeWidth = 6.0f,
            .makeupGain = 0.0f,
            .enabled = true
        };

        static constexpr bool useExternalSidechain = true;

        // Per-band processing components
        std::array<EnvelopeFollower, ParameterRanges::maxNutrientBands> envelopeFollowers;
        std::array<DuckingCompressor, ParameterRanges::maxNutrientBands> duckingCompressors;

        // Temporary buffers
        std::unique_ptr<juce::AudioBuffer<float>> tempBuffer;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputNode)
};
