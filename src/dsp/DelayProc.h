#pragma once

#include <juce_dsp/juce_dsp.h>

#include "util/ProcessorChain.h"
#include "DelayStore.h"
#include "Dispersion.h"
#include "EnvelopeFollower.h"
#include "DuckingCompressor.h"
#include "util/ParameterRanges.h"
// #include "PitchShiftWrapper.h"
// #include "Reverser.h"
// #include "TempoSyncUtils.h"
// #include "VariableDelay.h"

/**
 * Audio processor that implements delay line with feedback,
 * including filtering and distortion in the feedback path
 */
class DelayProc
{
    public:
        struct Parameters
        {
            float delayMs;
            float feedback;
            float growthRate;                  // Growth rate for aging (0-100)
            float baseDelayMs;                 // Base delay time for calculating age ramp
            float filterFreq;
            float filterGainDb;
            float revTimeMs;

            // Envelope follower parameters
            EnvelopeFollower::Parameters envParams; // Envelope follower parameters

            // Compressor parameters
            DuckingCompressor::Parameters compressorParams; // Compressor parameters
            bool useExternalSidechain;         // Whether to use cross-band sidechain input
        };

        DelayProc();

        // processing functions
        void prepare(const juce::dsp::ProcessSpec &spec);
        void reset();

        template <typename ProcessContext>
        void process(const ProcessContext &context);
        void updateSmoothParameter(juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> &param,
                                              float targetValue, float rampTimeSec);
        void setParameters(const Parameters &params, bool force = false);

        // Getter for the input level (envelope follower)
        float getInputLevel() const { return inputLevel; }
        float getOutputLevel() const { return outputLevel; }

        // Set external sidechain level for cross-band ducking
        void setExternalSidechainLevel(float level) { externalSidechainLevel = level; }

        float getAge() const { return currentAge.getCurrentValue(); } // Getter for the current age value

    private:
        template <typename SampleType>
        inline SampleType processSample(SampleType x, size_t ch);

        juce::SharedResourcePointer<DelayStore> delayStore;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delay;
        DuckingCompressor compressor;

        float fs = 44100.0f;
        static constexpr float smoothTimeSec = 0.25f;

        // Parameters
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inDelayTime {0.0f};
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inFeedback {0.0f};
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inFilterFreq {0.0f};
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inFilterGainDb {0.0f};
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inGrowthRate {0.0f};
        std::vector<float> state {0.0f, 0.0f}; // Feedback state for each channel

        // Age control parameters
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> currentAge {0.0f};

        // flush delay line state
        void flushDelay();

        void updateFilterCoefficients(bool force = false);
        void updateProcChainParameters(size_t numSamples = 1, bool force = false);
        void updateAgeingRate(size_t numSamples = 1);
        void updateModulationParameters();

        // The base delay (quarter note time) in milliseconds
        float inBaseDelayMs = 0.0f;
        float rampTimeMs = 500.0f; // Default ramp time for gain modulation

        // Envelope follower for input signal
        EnvelopeFollower inEnvelopeFollower;
        EnvelopeFollower outEnvelopeFollower;
        float inputLevel  = 0.0f;
        float outputLevel = 0.0f;
        static constexpr float inputLevelMetabolicThreshold = 0.01f;
        EnvelopeFollower::Parameters inEnvelopeFollowerParams = {
            .attackMs = 250.0f,
            .releaseMs = 150.0f,
            .levelType = juce::dsp::BallisticsFilterLevelCalculationType::RMS
        };

        DuckingCompressor::Parameters inCompressorParams;

        // External sidechain level for cross-band ducking
        float externalSidechainLevel = 0.0f;
        bool inUseExternalSidechain = true;

        enum
        {
            lpfIdx,
            hpfIdx,
            dispersionIdx//,
            // reverserIdx
        };

        MyProcessorChain<
            juce::dsp::IIR::Filter<float>,
            juce::dsp::IIR::Filter<float>,
            Dispersion>//,
            // Reverser>
            procs;

        // Modulation processor chain
        enum
        {
            oscillatorIdx,
            gainIdx
        };

        MyProcessorChain<
            juce::dsp::Oscillator<float>,
            juce::dsp::Gain<float>>
            modProcs;

        // TempoSyncUtils::SyncedLFO modSine;
        float delayModValue = 0.0f;
        float modDepth = 0.0f;
        float modDepthFactor = 1.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayProc)
};
