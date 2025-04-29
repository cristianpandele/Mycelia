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

    // Initialize tree output buffers - start with maxNumDelayProcsPerBand buffers
    treeOutputBuffers.resize(inNumColonies);
    for (int i = 0; i < inNumColonies; ++i)
    {
        treeOutputBuffers[i].resize(maxNumDelayProcsPerBand);
        // Create an empty buffer for each tree output
        for (int j = 0; j < maxNumDelayProcsPerBand; ++j)
        {
            treeOutputBuffers[i][j] = std::make_unique<juce::AudioBuffer<float>>(spec.numChannels, spec.maximumBlockSize);
            treeOutputBuffers[i][j]->clear();
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

    // Initialize tree positions
    updateTreePositions();
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

    // Clear all tree output buffers
    for (auto &band : treeOutputBuffers)
    {
        for (auto &buffer : band)
        {
            if (buffer)
                buffer->clear();
        }
    }
}

void DelayNodes::process(juce::AudioBuffer<float> *delayBandBuffers)
{
    // Update sidechain levels for all processors based on their positions
    updateSidechainLevels();

    // Clear all active tree output buffers
    for (int band = 0; band < inNumColonies; ++band)
    {
        for (int tree = 0; tree < numActiveTrees; ++tree)
        {
            if (tree < treeOutputBuffers[band].size() && treeOutputBuffers[band][tree])
                treeOutputBuffers[band][tree]->clear();
        }
    }

    // Process each band
    for (int band = 0; band < inNumColonies; ++band)
    {
        // Get the input buffer for this band
        auto& inputBuffer = delayBandBuffers[band];

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
                auto& currentBuffer = getProcessorBuffer(band, i);
                currentBuffer.setSize(inputBuffer.getNumChannels(), inputBuffer.getNumSamples(), false, false, true);
                currentBuffer.makeCopyOf(getProcessorBuffer(band, i-1));
            }

            // Process the current node
            processNode(band, i, getProcessorBuffer(band, i));

            // Check if this node position is a tree tap point
            for (int treeIdx = 0; treeIdx < numActiveTrees; ++treeIdx)
            {
                auto connectionGain = getTreeConnection(band, treeIdx);
                if (i == static_cast<size_t>(treePositions[treeIdx]) && connectionGain > 0.0f)
                {
                    // This node is connected to a tree - route the audio to the tree output buffer
                    auto& procBuffer = getProcessorBuffer(band, i);
                    auto& treeBuffer = getTreeBuffer(band, treeIdx);

                    // Ensure the tree buffer is sized correctly
                    if (treeBuffer.getNumChannels() != procBuffer.getNumChannels() || 
                        treeBuffer.getNumSamples() != procBuffer.getNumSamples())
                    {
                        treeBuffer.setSize(procBuffer.getNumChannels(), procBuffer.getNumSamples(), false, false, true);
                    }
                    treeBuffer.clear();

                    // Add to the tree buffer with the connection gain
                    for (int ch = 0; ch < procBuffer.getNumChannels(); ++ch)
                    {
                        treeBuffer.addFrom(
                            ch, 0, procBuffer,
                            ch, 0, procBuffer.getNumSamples(),
                            connectionGain);
                    }
                }
            }
        }
    }

    // Now that all bands have been processed, combine tree outputs into the delay band buffers
    for (int band = 0; band < inNumColonies; ++band)
    {
        auto& outputBuffer = delayBandBuffers[band];
        outputBuffer.clear();

        for (int treeIdx = 0; treeIdx < numActiveTrees; ++treeIdx)
        {
            auto connectionGain = getTreeConnection(band, treeIdx);
            if (connectionGain > 0.0f)
            {
                auto& treeBuffer = getTreeBuffer(band, treeIdx);
                for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
                {
                    outputBuffer.addFrom(ch, 0, treeBuffer, ch, 0,
                                        outputBuffer.getNumSamples(), connectionGain);
                }
            }
        }

        // Apply normalization gain
        juce::dsp::AudioBlock<float> finalBlock(outputBuffer);
        finalBlock.multiplyBy(std::sqrt(inNumColonies) / numActiveTrees);
    }
}

void DelayNodes::updateDelayProcParams()
{
    // Calculate base delay time
    float baseDelayTime = std::abs(inStretch) * inBaseDelayMs;
    float baseNodeDelayTime = baseDelayTime / maxNumDelayProcsPerBand;

    // Resize the delay time matrix if needed
    nodeDelayTimes.resize(delayProcs.size());

    // Initialize random number generator for consistent variations
    juce::Random random(juce::Time::currentTimeMillis());

    // Update parameters for all delay processors
    for (size_t i = 0; i < delayProcs.size(); ++i)
    {
        // Resize this colony's delay time vector if needed
        nodeDelayTimes[i].resize(maxNumDelayProcsPerBand);

        // Generate delay times with small variations for each processor in this colony
        for (size_t j = 0; j < maxNumDelayProcsPerBand; ++j)
        {
            // Generate a random variation factor between 0.95 and 1.05 (±5%)
            float variationFactor = 0.95f + random.nextFloat() * 0.1f;

            // Apply the variation to the base delay time
            nodeDelayTimes[i][j] = baseNodeDelayTime * variationFactor;
        }

        // Set parameters for each delay processor in this colony
        for (size_t j = 0; j < delayProcs[i].size(); ++j)
        {
            // Configure parameters using the delay time from our matrix
            DelayProc::Parameters params;
            params.delayMs = nodeDelayTimes[i][j];
            params.feedback = 1.0f;
            params.growthRate = inGrowthRate;
            params.baseDelayMs = inBaseDelayMs;
            params.filterFreq = *inBandFrequencies[i];
            params.filterGainDb = 0.0f;
            params.revTimeMs = 0.0f;

            // Set compressor parameters
            params.compressorParams = inCompressorParams;
            params.useExternalSidechain = inUseExternalSidechain;

            // Set the parameters for this delay processor
            delayProcs[i][j]->setParameters(params, false);
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
    inTreeDensity = params.treeDensity;

    updateDelayProcParams();
    updateTreePositions();
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

// Get the tree connection at a specific position in the matrix
float DelayNodes::getTreeConnection(int band, size_t procIdx)
{
    // Make sure the indices are valid
    band = juce::jlimit(0, inNumColonies - 1, band);
    procIdx = juce::jlimit(static_cast<size_t>(0), treeConnections[band].size() - 1, procIdx);

    return treeConnections[band][procIdx];
}

// Get the tree buffer for a specific band
juce::AudioBuffer<float>& DelayNodes::getTreeBuffer(int band, size_t treeIdx)
{
    // Make sure the indices are valid
    band = juce::jlimit(0, inNumColonies - 1, band);
    treeIdx = juce::jlimit(static_cast<size_t>(0), treeOutputBuffers[band].size() - 1, treeIdx);

    return *treeOutputBuffers[band][treeIdx];
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

// Update tree positions and connections based on treeDensity
void DelayNodes::updateTreePositions()
{
    // Calculate number of active trees based on treeDensity (0-100)
    const juce::NormalisableRange<float> activeTreeRange{1.0f, static_cast<float>(maxNumDelayProcsPerBand)};

    auto normTreeDensity = ParameterRanges::normalizeParameter(ParameterRanges::treeDensityRange, inTreeDensity);
    numActiveTrees = static_cast<int>(ParameterRanges::denormalizeParameter(activeTreeRange, normTreeDensity));
    numActiveTrees = juce::jlimit(1, static_cast<int>(maxNumDelayProcsPerBand), numActiveTrees);

    // Initialize random number generator for consistent variations
    juce::Random random(juce::Time::currentTimeMillis());

    // Resize and clear the tree positions array
    treePositions.resize(numActiveTrees);

    // Always place the first tree at the output (last position)
    treePositions[0] = static_cast<int>(maxNumDelayProcsPerBand) - 1;

    // Place remaining trees symmetrically with small variations
    if (numActiveTrees > 1)
    {
        for (int i = 1; i < numActiveTrees; ++i)
        {
            // Calculate the ideal position for even distribution
            float idealPosition = static_cast<float>(maxNumDelayProcsPerBand - 1) * static_cast<float>(i) / static_cast<float>(numActiveTrees - 1);

            // Add small variation (+/- 1)
            int variation = random.nextInt(3) - 1; // -1, 0, or 1
            int position = static_cast<int>(idealPosition) + variation;

            // Ensure position is valid and not the last position (reserved for the output)
            position = juce::jlimit(0, static_cast<int>(maxNumDelayProcsPerBand) - 2, position);

            // Ensure we don't duplicate positions
            bool isDuplicate;
            do {
                isDuplicate = false;
                for (int j = 0; j < i; ++j)
                {
                    if (treePositions[j] == position)
                    {
                        isDuplicate = true;
                        position = (position + 1) % (static_cast<int>(maxNumDelayProcsPerBand) - 2);  // -1 is the output tree
                        break;
                    }
                }
            } while (isDuplicate);

            treePositions[i] = position;
        }
    }

    // Sort the positions in ascending order for easier processing
    std::sort(treePositions.begin(), treePositions.end());

    // Resize and initialize the tree connections matrix
    treeConnections.resize(inNumColonies);
    for (int band = 0; band < inNumColonies; ++band)
    {
        treeConnections[band].resize(numActiveTrees);
        for (int tree = 0; tree < numActiveTrees; ++tree)
        {
            // Determine if this band connects to this tree (75% probability)
            // Always connect the last tree (output tree) to all bands
            if (tree == numActiveTrees - 1 || random.nextFloat() < 0.75f)
            {
                treeConnections[band][tree] = 1.0f; // Connected
            }
            else
            {
                treeConnections[band][tree] = 0.0f; // Not connected
            }
        }
    }
}
