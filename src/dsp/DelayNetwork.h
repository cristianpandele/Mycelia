#pragma once

#include "DiffusionControl.h"
#include "DelayNodes.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

class DelayNetwork
    : private juce::Timer
{
    public:
        // Parameters
        struct Parameters
        {
            int   numActiveFilterBands;  // Controls the number of filter bands (0-MAX_NUTRIENT_BANDS)
            float treeDensity;               // Density of the trees (0.0 to 100.0)
            float stretch;                   // Controls the stretch of the delay network (0-100)
            float tempoValue;                // Controls the tempo value (30-300 BPM)
            float scarcityAbundance;         // Controls the Scarcity/Abundance of the delay network (-1-1)
            float scarcityAbundanceOverride; // Controls the Scarcity/Abundance override (0-1)
            float foldPosition;              // Controls the fold position (-1-1)
            float foldWindowShape;           // Controls the fold window shape (-1-1)
            float foldWindowSize;            // Controls the fold window size (0.2-1.0)
            float entanglement;              // Controls the diffusion and cross-feedback (0-100)
            float growthRate;                // Controls the delay network growth (0-100)
        };

        DelayNetwork();
        ~DelayNetwork();

        // processing functions
        void prepare(const juce::dsp::ProcessSpec &spec);
        void reset();

        template <typename ProcessContext>
        void process(const ProcessContext &context,
                     std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &diffusionBandBuffers,
                     std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &delayBandBuffers);

        void setParameters(const Parameters &params);
        void timerCallback();

    private:
        float fs = 44100.0f;

        // Parameters
        int   inActiveFilterBands;
        float inTreeDensity;
        float inStretch;
        float inTempoValue;
        float inScarcityAbundance;
        float inScarcityAbundanceOverride;
        float inFoldPosition;
        float inFoldWindowShape;
        float inFoldWindowSize;
        float inEntanglement;
        float inGrowthRate;
        // Booleans for parameter changes
        bool  numActiveFilterBandsChanged = false;
        bool  treeDensityChanged = false;
        bool  stretchChanged = false;
        bool  tempoValueChanged = false;
        bool  scarcityAbundanceChanged = false;
        bool  scarcityAbundanceOverrideChanged = false;
        bool  foldPositionChanged = false;
        bool  foldWindowShapeChanged = false;
        bool  foldWindowSizeChanged = false;
        bool  entanglementChanged = false;
        bool  growthRateChanged = false;

        // Allocate delay processors and buffers based on the number of colonies and nodes
        void allocateBandBuffers(int numBands);

        // Base delay time in milliseconds (quarter note time)
        float baseDelayMs = 0.0f;

        // Compressor parameters
        DuckingCompressor::Parameters compressorParams =
            {
                .threshold = -12.0f,
                .ratio = 4.0f,
                .attackTime = 100.0f,
                .releaseTime = 25.0f,
                .kneeWidth = 6.0f,
                .makeupGain = 0.0f,
                .enabled = true
            };
        bool  useExternalSidechain = true;

        // Diffusion control
        DiffusionControl diffusionControl;

        // Delay nodes processor
        DelayNodes delayNodes;

        // Output buffers
        std::vector<float>                                     diffusionBandFrequencies;
        std::vector<std::unique_ptr<juce::AudioBuffer<float>>> diffusionBandBuffers;
        std::vector<std::unique_ptr<juce::AudioBuffer<float>>> delayBandBuffers;

        void updateDiffusionDelayNodesParams();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayNetwork)
};