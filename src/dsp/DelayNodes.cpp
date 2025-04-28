#include "DelayNodes.h"

DelayNodes::DelayNodes(size_t numBands)
{
    // Ensure we have enough delay processors
    if (delayProcs.size() < static_cast<size_t>(inNumColonies))
    {
        const auto currentSize = delayProcs.size();

        // Create the required number of delay processors
        for (size_t i = currentSize; i < static_cast<size_t>(inNumColonies); ++i)
        {
            delayProcs.push_back(std::vector<std::unique_ptr<DelayProc>>());
            for (size_t j = 0; j < maxNumDelayProcsPerBand; ++j)
            {
                auto newDelayProc = std::make_unique<DelayProc>();
                delayProcs[i].push_back(std::move(newDelayProc));
            }
        }
    }
}

DelayNodes::~DelayNodes()
{
    // Clean up resources
}

void DelayNodes::prepare(const juce::dsp::ProcessSpec &spec)
{
    fs = static_cast<float>(spec.sampleRate);

    // Initialize the processor buffer matrix
    processorBuffers.resize(inNumColonies);
    for (int band = 0; band < inNumColonies; ++band)
    {
        processorBuffers[band].resize(maxNumDelayProcsPerBand);
        for (size_t proc = 0; proc < maxNumDelayProcsPerBand; ++proc)
        {
            // Create an empty buffer for each processor
            processorBuffers[band][proc].setSize(spec.numChannels, spec.maximumBlockSize);
            processorBuffers[band][proc].clear();
        }
    }

    // Prepare the array of delay processors
    for (auto &delayProc : delayProcs)
    {
        for (auto &proc : delayProc)
        {
            // Prepare the processor
            proc->prepare(spec);
        }
    }
}

void DelayNodes::reset()
{
    // Reset all delay processors
    for (auto &delayProc : delayProcs)
    {
        for (auto &proc : delayProc)
        {
            proc->reset();
        }
    }

    // Clear all persistent buffers
    for (auto &bandBuffers : processorBuffers)
    {
        for (auto &buffer : bandBuffers)
        {
            buffer.clear();
        }
    }
}

void DelayNodes::process(juce::AudioBuffer<float> *diffusionBandBuffers)
{
    // Update sidechain levels for all processors based on their positions
    updateSidechainLevels();

    // Process each band
    for (int band = 0; band < inNumColonies; ++band)
    {
        // Get the input buffer for this band
        auto& inputBuffer = diffusionBandBuffers[band];

        // Copy input to the first processor buffer
        auto &firstBuffer = getProcessorBuffer(band, 0);
        firstBuffer.setSize(inputBuffer.getNumChannels(), inputBuffer.getNumSamples(), false, false, true);
        firstBuffer.makeCopyOf(inputBuffer);

        // Process through each delay processor with its own persistent context
        for (size_t i = 0; i < delayProcs[band].size(); ++i)
        {
            // If not the first processor, copy from previous processor's buffer
            if (i > 0)
            {
                auto& currentBuffer = processorBuffers[band][i];
                currentBuffer.setSize(inputBuffer.getNumChannels(), inputBuffer.getNumSamples(), false, false, true);
                currentBuffer.makeCopyOf(processorBuffers[band][i-1]);
            }

            // Process the current node
            processNode(band, i, processorBuffers[band][i]);
        }

        // Copy the final processed buffer back to the input buffer
        inputBuffer.makeCopyOf(processorBuffers[band][delayProcs[band].size() - 1]);

        // Compensate for the normalization gain of the final stage
        juce::dsp::AudioBlock<float> finalBlock(inputBuffer);
        finalBlock.multiplyBy(std::sqrt(inNumColonies));
    }
}

void DelayNodes::updateDelayProcParams()
{
    // Update parameters for all delay processors
    for (size_t i = 0; i < delayProcs.size(); ++i)
    {
        DelayProc::Parameters delayParams;

        // Configure initial delay parameters with increasing times
        float delayTimeMs = std::abs(inStretch) * inBaseDelayMs * (1 + 1.0f * i);

        // Set initial parameters
        DelayProc::Parameters params;
        params.delayMs = delayTimeMs / maxNumDelayProcsPerBand;
        params.feedback = 1.0f;
        params.growthRate = inGrowthRate;
        params.baseDelayMs = inBaseDelayMs;
        params.filterFreq = *inBandFrequencies[i];
        params.filterGainDb = 0.0f;
        params.revTimeMs = 0.0f;

        // Set compressor parameters
        params.compressorParams = inCompressorParams;
        params.useExternalSidechain = inUseExternalSidechain;

        for (auto &proc : delayProcs[i])
        {
            // Set the parameters for each delay processor
            proc->setParameters(params, false);
        }
    }
}

void DelayNodes::setParameters(const Parameters &params)
{
    inNumColonies = params.numColonies;
    inBandFrequencies.clear();
    for (const auto& freq : params.bandFrequencies) {
        inBandFrequencies.push_back(std::make_unique<float>(freq));
    }
    inStretch = params.stretch;
    inScarcityAbundance = params.scarcityAbundance;
    inGrowthRate = params.growthRate;
    inEntanglement = params.entanglement;
    inBaseDelayMs = params.baseDelayMs;
    inCompressorParams = params.compressorParams;
    inUseExternalSidechain = params.useExternalSidechain;

    updateDelayProcParams();
}

// Process a specific band and processor stage with its own context
void DelayNodes::processNode(int band, size_t procIdx, juce::AudioBuffer<float>& input)
{
    if (band < 0 || band >= inNumColonies || procIdx >= delayProcs[band].size())
        return;

    // Make sure our processor buffer is the right size and initialized with the input
    auto &procBuffer = getProcessorBuffer(band, procIdx);
    procBuffer.setSize(input.getNumChannels(), input.getNumSamples(), false, false, true);
    procBuffer.makeCopyOf(input);

    // Create a dedicated audio block and context for this processor
    juce::dsp::AudioBlock<float> block(procBuffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    // Process with this delay processor
    getProcessorNode(band, procIdx).process(context);
}

// Get the processor buffer at a specific position in the matrix
juce::AudioBuffer<float>& DelayNodes::getProcessorBuffer(int band, size_t procIdx)
{
    // Make sure the indices are valid
    band = juce::jlimit(0, inNumColonies - 1, band);
    procIdx = juce::jlimit(static_cast<size_t>(0), delayProcs[band].size() - 1, procIdx);

    return processorBuffers[band][procIdx];
}

// Get the processor node at a specific position in the matrix
DelayProc& DelayNodes::getProcessorNode(int band, size_t procIdx)
{
    // Make sure the indices are valid
    band = juce::jlimit(0, inNumColonies - 1, band);
    procIdx = juce::jlimit(static_cast<size_t>(0), delayProcs[band].size() - 1, procIdx);

    return *delayProcs[band][procIdx];
}

// Update sidechain levels for all processors in the matrix
void DelayNodes::updateSidechainLevels()
{
    // First, gather all output levels from all delay processors into a matrix
    bufferLevels.resize(inNumColonies);
    for (int band = 0; band < inNumColonies; ++band)
    {
        bufferLevels[band].resize(delayProcs[band].size());
        for (size_t proc = 0; proc < delayProcs[band].size(); ++proc)
        {
            bufferLevels[band][proc] = getProcessorNode(band, proc).getOutputLevel();
        }
    }

    // For each band and processor
    for (int band = 0; band < inNumColonies; ++band)
    {
        const size_t numProcs = delayProcs[band].size();

        for (size_t proc = 0; proc < numProcs; ++proc)
        {
            // Is this a node at the end of the row?
            if (proc == numProcs - 1)
            {
                // For end-of-row nodes: sum levels from all OTHER row ends
                float combinedLevel = 0.0f;

                for (int otherBand = 0; otherBand < inNumColonies; ++otherBand)
                {
                    if (otherBand != band)  // Skip the current band
                    {
                        const size_t otherNumProcs = delayProcs[otherBand].size();
                        combinedLevel += bufferLevels[otherBand][otherNumProcs - 1];
                    }
                }

                // Set the external sidechain level for this end-of-row processor
                getProcessorNode(band, proc).setExternalSidechainLevel(16.0f * combinedLevel);
            }
            else
            {
                // For non-end nodes: use the output level of the next node in same row
                float nextNodeLevel = bufferLevels[band][proc + 1];
                getProcessorNode(band, proc).setExternalSidechainLevel(16.0f * nextNodeLevel);
            }
        }
    }
}
