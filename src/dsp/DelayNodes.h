#pragma once

#include "DelayProc.h"
#include "DuckingCompressor.h"
#include "util/ParameterRanges.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

/**
 * Class to process multiple inputs through separate delay processors
 * Each input gets its own DelayProc with different delay parameters
 */
class DelayNodes :
    private juce::Timer
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

        struct BandResources
        {
            std::vector<std::unique_ptr<DelayProc>> delayProcs;
            std::vector<std::unique_ptr<juce::AudioBuffer<float>>> processorBuffers;
            std::vector<std::unique_ptr<juce::AudioBuffer<float>>> treeOutputBuffers;
            std::vector<float> treeConnections;
            // Vector to store output levels of each processor
            std::vector<float> bufferLevels;

            // Vector to store delay times for each colony and processor
            std::vector<float> nodeDelayTimes;

            // Band center frequency
            float inBandFrequency = 0.0f;

            // Vector of matrices to store connection strengths between nodes
            std::vector<std::vector<std::vector<float>>> interNodeConnections;

            void clear()
            {
                // Clear in reverse order of dependency
                bufferLevels.clear();
                nodeDelayTimes.clear();

                // Clear inter-node connections
                for (auto &node : interNodeConnections)
                {
                    for (auto &bands : node)
                    {
                        bands.clear();
                    }
                    node.clear();
                }
                interNodeConnections.clear();

                treeConnections.clear();

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

        DelayNodes(size_t numBands = 4);
        ~DelayNodes();

        void prepare(const juce::dsp::ProcessSpec& spec);
        void reset();

        // Process each diffusion output with its own delay node
        void process(std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &diffusionBandBuffers);

        void setParameters(const Parameters& params);
        float getAverageScarcityAbundance() const { return averageScarcityAbundance; }

        // Get the contents of the fold window
        std::vector<BandResources>& getBandState() { return bands; }

        // Get the position of the trees in the network
        std::vector<int>& getTreePositions() { return treePositions; }

    private:
        std::vector<BandResources> bands;

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

        // Average scarcity/abundance value
        float averageScarcityAbundance = 0.0f;

        // Parameters for delay network
        int inNumColonies = ParameterRanges::maxNutrientBands;
        float inStretch = 0.0f;
        float inScarcityAbundance = 0.0f;
        float inFoldPosition = 0.0f;
        float inFoldWindowShape = 0.0f;
        float inFoldWindowSize = 1.0f;
        float inEntanglement = 0.5f;
        float inGrowthRate = 0.5f;
        float inBaseDelayMs = 500.0f;

        // Boolean flags for parameter changes
        bool bandFrequenciesChanged = false;
        bool treeDensityChanged = false;
        bool stretchChanged = false;
        bool scarcityAbundanceChanged = false;
        bool foldPositionChanged = false;
        bool foldWindowShapeChanged = false;
        bool foldWindowSizeChanged = false;
        bool growthRateChanged = false;
        bool useExternalSidechainChanged = false;
        bool compressorParamsChanged = false;
        bool baseDelayChanged = false;

        // Compressor parameters
        DuckingCompressor::Parameters inCompressorParams;
        bool  inUseExternalSidechain = true;

        // Parameters for delay processor
        static constexpr size_t maxNumDelayProcsPerBand = 8;
        size_t numActiveProcsPerBand = 0;

        // Window for folding
        std::vector<float> foldWindow;

        // Timer callback function
        void timerCallback() override;

        // Update sum of outgoing connections
        void normalizeOutgoingConnections(int band, size_t procIdx);

        // Get incoming flow to a specific band and processor
        float getSiblingFlow(int targetBand, size_t targetProcIdx);

        // Update delay processor parameters
        void updateDelayProcParams();

        // Update sidechain levels for all processors in the matrix
        void updateSidechainLevels();

        // Update tree positions and connections based on treeDensity
        void updateTreePositions();

        // Update inter-band connections based on entanglement parameter
        void updateNodeInterconnections();

        // Update fold window for all processors
        void updateFoldWindow();

        // Process a specific band and processor stage
        void processNode(int band, size_t procIdx);

        // Get processor buffer at a specific position in the matrix
        juce::AudioBuffer<float> &getProcessorBuffer(int band, size_t procIdx);

        // Get processor node at a specific position in the matrix
        DelayProc &getProcessorNode(int band, size_t procIdx);

        // Get tree connection at a specific position in the matrix
        float &getTreeConnection(int band, size_t procIdx);

        // Get the tree output buffer for a specific band and tree index
        juce::AudioBuffer<float>& getTreeBuffer(int band, size_t treeIdx);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayNodes)
};