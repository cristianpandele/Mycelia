#pragma once

/**
 * Class for delay line dispersion processing
 * 
 * The DSP is made up of many stages of first-order allpass
 * filters, with linear fading between stages.
 */
#include <stddef.h>
#include <juce_dsp/juce_dsp.h>

class Dispersion
{
    public:
        struct Parameters
        {
            float dispersionAmount;
            float allpassFreq;
        };

        Dispersion();

        // void setDepth (float depth, bool force);
        void prepare (const juce::dsp::ProcessSpec& spec);
        void reset();

        float processSample(float x);
        void  setParameters(const Parameters &params);

    private:
        void  updateAllpassCoefficients();
        float processStage(float x, size_t stage);

        // Parameters
        float inDispersionAmount = 0.0f;
        float inAllpassFreq = 800.0f;

        static constexpr size_t maxNumStages = 100;

        float fs   = 44100.0f;
        float a[2] = { 0.0f};
        float y1   = 0.0f;
        float stageFb[maxNumStages + 1];

        juce::dsp::IIR::Filter<float> allPassFilter;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Dispersion)
};
