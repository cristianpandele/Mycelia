#pragma once

#include <juce_dsp/juce_dsp.h>

#include "util/ProcessorChain.h"
#include "DelayStore.h"
#include "Dispersion.h"
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
            float filterFreq;
            float filterGainDb;
            float distortion;
            float pitchSt;
            float dispAmt;
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
        void updateFilterCoefficients();

        // float getModDepth() const noexcept { return 1000.0f * delayModValue / fs; }

    private:
        template <typename SampleType>
        inline SampleType processSample(SampleType x, size_t ch);

        template <typename SampleType>
        inline SampleType processSampleSmooth(SampleType x, size_t ch);

        juce::SharedResourcePointer<DelayStore> delayStore;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delay;

        float fs = 44100.0f;

        // Parameters
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inDelayTime {0.0f};
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inFeedback {0.0f};
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inFilterFreq {0.0f};
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inFilterGainDb {0.0f};
        std::vector<float> state;

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
