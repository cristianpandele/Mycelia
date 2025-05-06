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
            float foldPosition;                       // Controls the fold position (-1-1)
            float foldWindowShape;                    // Controls the fold window shape (-1-1)
            float foldWindowSize;                     // Controls the fold window size (0.2-1.0)
            float entanglement;                       // Controls feedback interconnections between nodes
            float growthRate;                         // Controls how nodes age and grow
            float baseDelayMs;                        // Base delay time in milliseconds (quarter note time)
            float treeDensity;                        // Controls how many trees (taps) are used (0-100)

            // Compressor parameters
            DuckingCompressor::Parameters compressorParams; // Compressor parameters
            bool useExternalSidechain = true;         // Whether to use cross-band sidechain input
        };

        DelayNodes(size_t numBands = 4);
        ~DelayNodes();

        void prepare(const juce::dsp::ProcessSpec& spec);
        void reset();

        // Process each diffusion output with its own delay node
        void process(std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &diffusionBandBuffers);

        void setParameters(const Parameters& params);

    private:
        struct BandResources
        {
            std::vector<std::unique_ptr<DelayProc>> delayProcs;
            std::vector<std::unique_ptr<juce::AudioBuffer<float>>> processorBuffers;
            std::vector<std::unique_ptr<juce::AudioBuffer<float>>> treeOutputBuffers;
            std::vector<float> treeConnections;
            // Matrix to store output levels of each processor
            std::vector<float> bufferLevels;

            // Matrix to store delay times for each colony and processor
            std::vector<float> nodeDelayTimes;

            void clear()
            {
                // Clear in reverse order of dependency
                treeConnections.clear();
                bufferLevels.clear();
                nodeDelayTimes.clear();

                for (auto &buffer : treeOutputBuffers)
                    buffer.reset();
                treeOutputBuffers.clear();

                for (auto &buffer : processorBuffers)
                    buffer.reset();
                processorBuffers.clear();

                for (auto &proc : delayProcs)
                    proc.reset();
                delayProcs.clear();
            }
        };
        std::vector<BandResources> 
        bands;

        // Allocate delay processors and buffers based on the number of colonies and nodes
        void allocateDelayProcessors(int numColonies, int numNodes = maxNumDelayProcsPerBand);

        // Tree-related parameters
        float inTreeDensity = 0.0f;                      // Tree density parameter (0-100)
        int numActiveTrees = 1;                          // Number of active trees (1-8)
        std::vector<int> treePositions;                  // Positions of trees in the network

        // Parameters to control delay network behavior
        float fs = 44100.0f;
        size_t numChannels = 2;
        size_t blockSize = 512;

        int inNumColonies = ParameterRanges::maxNutrientBands;
        std::vector<std::unique_ptr<float>> inBandFrequencies;
        float inStretch = 0.0f;
        float inScarcityAbundance = 0.0f;
        float inFoldPosition = 0.0f;
        float inFoldWindowShape = 0.0f;
        float inFoldWindowSize = 1.0f;
        float inEntanglement = 0.5f;
        float inGrowthRate = 0.5f;
        float inBaseDelayMs = 500.0f;

        // Compressor parameters
        DuckingCompressor::Parameters inCompressorParams;
        bool  inUseExternalSidechain = true;

        static constexpr size_t maxNumDelayProcsPerBand = 8;

        // Window for folding
        std::vector<float> foldWindow;

        // Update delay processor parameters
        void updateDelayProcParams();

        void updateWindows();

        // Process a specific band and processor stage
        void processNode(int band, size_t procIdx, juce::AudioBuffer<float> &input);

        // Get processor buffer at a specific position in the matrix
        juce::AudioBuffer<float> &getProcessorBuffer(int band, size_t procIdx);

        // Get processor node at a specific position in the matrix
        DelayProc &getProcessorNode(int band, size_t procIdx);

        // Get tree connection at a specific position in the matrix
        float getTreeConnection(int band, size_t procIdx);

        // Update sidechain levels for all processors in the matrix
        void updateSidechainLevels();

        // Update tree positions and connections based on treeDensity
        void updateTreePositions();

        // Get the tree output buffer for a specific band and tree index
        juce::AudioBuffer<float>& getTreeBuffer(int band, size_t treeIdx);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayNodes)
};