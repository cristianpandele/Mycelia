#pragma once

#include "DelayProc.h"
#include "DuckingCompressor.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>

/**
 * Class to process multiple inputs through separate delay processors
 * Each input gets its own DelayProc with different delay parameters
 */
class DelayNodes
{
    public:
        // Parameters
        struct Parameters
        {
            int   numColonies;                        // Controls the number of colonies (delay processor lineages)
            std::vector<float> bandFrequencies;       // Controls the frequency processed by each colony
            float stretch;                            // Controls the stretch of the delay network
            float scarcityAbundance;                  // Controls the Scarcity/Abundance of the delay network
            float growthRate;                         // Controls how nodes age and grow
            float entanglement;                       // Controls feedback interconnections between nodes
            float baseDelayMs;                        // Base delay time in milliseconds (quarter note time)

            // Compressor parameters
            DuckingCompressor::Parameters compressorParams; // Compressor parameters
            bool useExternalSidechain = true;         // Whether to use cross-band sidechain input
        };

        DelayNodes(size_t numBands = 4);
        ~DelayNodes();

        void prepare(const juce::dsp::ProcessSpec& spec);
        void reset();

        // Process each diffusion output with its own delay node
        void process(juce::AudioBuffer<float> *diffusionBandBuffers);

        void setParameters(const Parameters& params);

    private:
        // Array of delay processors, one for each band
        std::vector<std::unique_ptr<DelayProc>> delayProcs;

        // Parameters to control delay network behavior
        float fs = 44100.0f;
        int   inNumColonies = 4;
        std::vector<float> inBandFrequencies;
        float inStretch = 0.0f;
        float inScarcityAbundance = 0.0f;
        float inEntanglement = 0.5f;
        float inGrowthRate = 0.5f;
        float inBaseDelayMs = 500.0f;

        // Compressor parameters
        DuckingCompressor::Parameters inCompressorParams;
        bool  inUseExternalSidechain = true;

        void updateDelayProcParams();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayNodes)
};