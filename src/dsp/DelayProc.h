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
        DelayProc();

        // processing functions
        void prepare (const juce::dsp::ProcessSpec& spec);
        void reset();

        template <typename ProcessContext>
        void process (const ProcessContext& context);

        // flush delay line state
        void flushDelay();

        struct Parameters
        {
            float delayMs;
            float feedback;
            float lpfFreq;
            float hpfFreq;
            float distortion;
            float pitchSt;
            float dispAmt;
            float revTimeMs;
            // const AudioProcessorValueTreeState::Parameter* modFreq;
            float modDepth;
            float tempoBPM;
            bool lfoSynced;
            juce::AudioPlayHead* playhead;
        };

        void setParameters (const Parameters& params, bool force = false);
        // void setDelayType(juce::dsp::DelayLine::DelayType type) { delay->setDelayType(type); }
        // float getModDepth() const noexcept { return 1000.0f * delayModValue / fs; }

    private:
        template <typename SampleType>
        inline SampleType processSample (SampleType x, size_t ch);

        template <typename SampleType>
        inline SampleType processSampleSmooth (SampleType x, size_t ch);

        juce::SharedResourcePointer<DelayStore> delayStore;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delay;

        float fs = 44100.0f;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delaySmooth;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> feedback;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inGain;
        std::vector<float> state;

        enum
        {
            lpfIdx,
            hpfIdx//,
            // dispersionIdx,
            // reverserIdx
        };

        MyProcessorChain<
            juce::dsp::IIR::Filter<float>,
            juce::dsp::IIR::Filter<float>>//,
            // Dispersion,
            // Reverser>
            procs;

        // TempoSyncUtils::SyncedLFO modSine;
        float delayModValue = 0.0f;
        float modDepth = 0.0f;
        float modDepthFactor = 1.0f;

        static constexpr float minDelay = 0.0f;
        static constexpr float maxDelay = 1500.0f;
        static constexpr float centreDelay = 200.0f;
        static constexpr float maxPan = 1.0f;
        static constexpr float minFeedback = 0.0f;
        static constexpr float maxFeedback = 0.99f;
        static constexpr float maxGain = 12.0f;
        static constexpr float minLPF = 200.0f;
        static constexpr float maxLPF = 20000.0f;
        static constexpr float minHPF = 20.0f;
        static constexpr float maxHPF = 2000.0f;
        static constexpr float minDispersion = 0.0f;
        static constexpr float maxDispersion = 1.0f;
        static constexpr float minReverse = 0.0f;
        static constexpr float maxReverse = 1000.0f;
        // static constexpr float minModFreq = 0.0f;
        // static constexpr float maxModFreq = 5.0f;
        static constexpr int numParams = 13;

        inline juce::NormalisableRange<float> rangeWithSkewForCentre(float min, float max, float centre)
        {
            juce::NormalisableRange<float> range{min, max};
            range.setSkewForCentre(centre);
            return range;
        }
        const juce::NormalisableRange<float> delayRange = rangeWithSkewForCentre(minDelay, maxDelay, centreDelay);

        // set up panner
        const juce::NormalisableRange<float> panRange{-maxPan, maxPan};

        // set up feedback amount
        const juce::NormalisableRange<float> fbRange{minFeedback, maxFeedback};

        // set up gain
        const juce::NormalisableRange<float> gainRange{-maxGain, maxGain};

        // set up LPF
        const juce::NormalisableRange<float> lpfRange = rangeWithSkewForCentre(minLPF, maxLPF, std::sqrt(minLPF * maxLPF));

        // set up HPF
        const juce::NormalisableRange<float> hpfRange{minHPF, maxHPF};

        // set up dispersion amount
        const juce::NormalisableRange<float> dispRange{minDispersion, maxDispersion};

        // set up reverse
        const juce::NormalisableRange<float> revRange{minReverse, maxReverse};

        // set up mod frequency
        // juce::NormalisableRange<float> modFreqRange{minModFreq, maxModFreq};
        // modFreqRange.setSkewForCentre(2.0f);

        // set up delay mod depth
        // juce::NormalisableRange<float> delayModRange{0.0f, 1.0f};

        // set up pan mod depth
        const juce::NormalisableRange<float> panModRange{-maxPan, maxPan};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayProc)
};
