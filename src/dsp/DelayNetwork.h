#pragma once

#include "DiffusionControl.h"
#include "DelayNodes.h"
#include <juce_dsp/juce_dsp.h>
#include <array>

class DelayNetwork
{
    public:
        // Parameters
        struct Parameters
        {
            int   numActiveFilterBands = 4;  // Controls the number of filter bands (0-MAX_NUTRIENT_BANDS)
            float stretch;                   // Controls the stretch of the delay network (0-100)
            float tempoValue;                // Controls the tempo value (30-300 BPM)
            float scarcityAbundance;         // Controls the Scarcity/Abundance of the delay network (-1-1)
            float scarcityAbundanceOverride; // Controls the Scarcity/Abundance override (0-1)
            float entanglement;              // Controls the diffusion and cross-feedback (0-100)
            float growthRate;                // Controls the delay network growth (0-100)
            float baseDelayMs;               // Base delay time in milliseconds
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
        int   inActiveFilterBands = 4;
        float inStretch = 0.0f;
        float inTempoValue = 120.0f;
        float inScarcityAbundance = 0.0f;
        float inScarcityAbundanceOverride = 0.0f;
        float inEntanglement = 50.0f;
        float inGrowthRate = 50.0f;
        float baseDelayMs = 60.0f / inTempoValue * 1000.0f;

        // Diffusion control
        DiffusionControl diffusionControl;

        // Delay nodes processor
        DelayNodes delayNodes;

        // Output buffers
        std::array<float, ParameterRanges::maxNutrientBands>                    diffusionBandFrequencies;
        std::array<juce::AudioBuffer<float>, ParameterRanges::maxNutrientBands> diffusionBandBuffers;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayNetwork)
};