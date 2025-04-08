#pragma once

#include "DiffusionControl.h"
#include <juce_dsp/juce_dsp.h>
#include <array>

class DelayNetwork
{
    public:
        // Parameters
        struct Parameters
        {
            float growthRate;   // Controls the delay network growth (0-100)
            float entanglement; // Controls the diffusion and cross-feedback (0-100)
        };

        DelayNetwork();
        ~DelayNetwork();

        // processing functions
        void prepare(const juce::dsp::ProcessSpec &spec);
        void reset();

        template <typename ProcessContext>
        void process(const ProcessContext &context);

        void setParameters(const Parameters &params);

    private:
        float fs = 44100.0f;

        // Parameters
        float inGrowthRate = 50.0f;
        float inEntanglement = 50.0f;

        // Diffusion control
        DiffusionControl diffusionControl;

        // Output from diffusion control
        std::array<juce::AudioBuffer<float>, 16> diffusionOutputs;
        int activeFilterBands = 4;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayNetwork)
};