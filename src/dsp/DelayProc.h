#pragma once

#include <juce_dsp/juce_dsp.h>

#include "util/ProcessorChain.h"
#include "DelayStore.h"
#include "Dispersion.h"
#include "EnvelopeFollower.h"
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
            float envelopeAttackMs;            // Attack time for envelope follower
            float envelopeReleaseMs;           // Release time for envelope follower
            float growthRate;                  // Growth rate for aging (0-100)
            float baseDelayMs;                 // Base delay time for calculating age ramp
            float filterFreq;
            float filterGainDb;
            // float distortion;
            // float pitchSt;
            float revTimeMs;
            // const AudioProcessorValueTreeState::Parameter* modFreq;
            float modDepth;
            float tempoBPM;
            bool lfoSynced;
            juce::AudioPlayHead *playhead;
        };

        DelayProc();

        // processing functions
        void prepare(const juce::dsp::ProcessSpec &spec);
        void reset();

        template <typename ProcessContext>
        void process(const ProcessContext &context);

        // flush delay line state
        void flushDelay();

        void setParameters(const Parameters &params, bool force = false);
        void updateFilterCoefficients(bool force = false);

        float getInputLevel() const { return inputLevel; }
        float getAge() const { return currentAge.getCurrentValue(); } // Getter for the current age value

    private:
        template <typename SampleType>
        inline SampleType processSample(SampleType x, size_t ch);

        juce::SharedResourcePointer<DelayStore> delayStore;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delay;

        float fs = 44100.0f;
        static constexpr float smoothTimeSec = 0.1f;

        // Parameters
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inDelayTime {0.0f};
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inFeedback {0.0f};
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inFilterFreq {0.0f};
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inFilterGainDb {0.0f};
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inGrowthRate {0.0f};
        std::vector<float> state;

        // Age control parameters
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> currentAge {0.0f};

        // The base delay (quarter note time) in milliseconds
        float inBaseDelayMs = 0.0f;

        // Envelope follower for input signal
        EnvelopeFollower envelopeFollower;
        float inputLevel = 0.0f;
        static constexpr float inputLevelThreshold = 0.01f;
        float envelopeAttackMs = 20.0f;
        float envelopeReleaseMs = 100.0f;

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

        // TempoSyncUtils::SyncedLFO modSine;
        float delayModValue = 0.0f;
        float modDepth = 0.0f;
        float modDepthFactor = 1.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayProc)
};
