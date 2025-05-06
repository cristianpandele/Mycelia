#include "DelayNodes.h"

DelayNodes::DelayNodes(size_t numBands)
{
    // Ensure we have enough delay processors
    allocateDelayProcessors(inNumColonies, maxNumDelayProcsPerBand);
    updateWindows();
}

DelayNodes::~DelayNodes()
{
    for (auto &band : bands)
    {
        band.clear();
    }
    bands.clear();
}

void DelayNodes::prepare(const juce::dsp::ProcessSpec &spec)
{
    fs = static_cast<float>(spec.sampleRate);
    numChannels = spec.numChannels;
    blockSize = spec.maximumBlockSize;

    // Prepare the delay processors
    allocateDelayProcessors(ParameterRanges::maxNutrientBands, maxNumDelayProcsPerBand);

    // Initialize tree positions
    updateTreePositions();
}

void DelayNodes::reset()
{
    for (auto &band : bands)
    {
        for (auto &proc : band.delayProcs)
        {
            proc->reset();
        }
    }
}

void DelayNodes::process(std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &delayBandBuffers)
{
    // Update sidechain levels for all processors based on their positions
    updateSidechainLevels();

    // Clear all active tree output buffers
    for (int band = 0; band < inNumColonies; ++band)
    {
        for (int tree = 0; tree < numActiveTrees; ++tree)
        {
            if (tree < bands[band].treeOutputBuffers.size() && bands[band].treeOutputBuffers[tree])
                bands[band].treeOutputBuffers[tree]->clear();
        }
    }

    // Process each band
    for (int band = 0; band < inNumColonies; ++band)
    {
        // Get the input buffer for this band
        auto& inputBuffer = delayBandBuffers[band];

        // Copy input to the first processor buffer
        auto &firstBuffer = getProcessorBuffer(band, 0);
        firstBuffer.setSize(inputBuffer->getNumChannels(), inputBuffer->getNumSamples(), false, false, true);
        firstBuffer.makeCopyOf(*inputBuffer);

        // Process through each delay processor with its own persistent context
        for (size_t i = 0; i < bands[band].delayProcs.size(); ++i)
        {
            // If not the first processor, copy from previous processor's buffer
            if (i > 0)
            {
                auto& currentBuffer = getProcessorBuffer(band, i);
                currentBuffer.setSize(inputBuffer->getNumChannels(), inputBuffer->getNumSamples(), false, false, true);
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
        outputBuffer->clear();

        for (int treeIdx = 0; treeIdx < numActiveTrees; ++treeIdx)
        {
            auto connectionGain = getTreeConnection(band, treeIdx);
            if (connectionGain > 0.0f)
            {
                auto& treeBuffer = getTreeBuffer(band, treeIdx);
                for (int ch = 0; ch < outputBuffer->getNumChannels(); ++ch)
                {
                    outputBuffer->addFrom(ch, 0, treeBuffer, ch, 0,
                                        outputBuffer->getNumSamples(), connectionGain);
                }
            }
        }

        // Apply normalization gain
        juce::dsp::AudioBlock<float> finalBlock(*outputBuffer);
        finalBlock.multiplyBy(std::sqrt(inNumColonies) / numActiveTrees);
    }
}

void DelayNodes::updateDelayProcParams()
{
    // Calculate base delay time
    float baseDelayTimeMs = std::abs(inStretch) * inBaseDelayMs;
    float baseNodeDelayTimeMs = baseDelayTimeMs / maxNumDelayProcsPerBand;

    // Initialize random number generator for consistent variations
    juce::Random random(juce::Time::currentTimeMillis());

    // Update parameters for all delay processors
    for (size_t band = 0; band < bands.size(); ++band)
    {
        // Generate delay times with small variations for each processor in this colony
        for (size_t proc = 0; proc < maxNumDelayProcsPerBand; ++proc)
        {
            // 75% chance to have a variation of ±2.5% of the base delay time
            // 15% chance to have a variation of ±5% of the base delay time
            // 7.5% chance to have a variation of ±7.5% of the base delay time
            // 2.5% chance to have a variation of ±25% of the base delay time
            // Generate a random number between 0 and 1
            float randomValue = random.nextFloat();
            float variationFactor;
            if (randomValue < 0.75f)
            {
                // 75% chance for ±2.5%
                variationFactor = 0.975f + random.nextFloat() * 0.05f;
            }
            else if (randomValue < 0.90f)
            {
                // 15% chance for ±5%
                variationFactor = 0.95f + random.nextFloat() * 0.1f;
            }
            else if (randomValue < 0.975f)
            {
                // 7.5% chance for ±7.5%
                variationFactor = 0.925f + random.nextFloat() * 0.15f;
            }
            else
            {
                // 2.5% chance for ±25%
                variationFactor = 0.75f + random.nextFloat() * 0.5f;
            }

            // Apply the variation to the base delay time
            bands[band].nodeDelayTimes[proc] = baseNodeDelayTimeMs * variationFactor;

            // Set parameters for each delay processor in this colony
            // Configure parameters using the delay time from our matrix
            DelayProc::Parameters params;
            params.delayMs = bands[band].nodeDelayTimes[proc];
            params.feedback = 1.0f;
            params.growthRate = inGrowthRate;
            params.baseDelayMs = inBaseDelayMs;
            params.filterFreq = *inBandFrequencies[band];
            params.filterGainDb = 0.0f;
            params.revTimeMs = 0.0f;

            // Set compressor parameters
            params.compressorParams = inCompressorParams;
            params.useExternalSidechain = inUseExternalSidechain;

            // Set the parameters for this delay processor
            bands[band].delayProcs[proc]->setParameters(params, false);
        }
    }
}

void DelayNodes::setParameters(const Parameters &params)
{
    // Check if the number of colonies is within the valid range
    if (params.numColonies < 1 || params.numColonies > ParameterRanges::maxNutrientBands)
    {
        return;
    }

    allocateDelayProcessors(params.numColonies);

    inNumColonies = params.numColonies;
    inBandFrequencies.clear();
    for (const auto& freq : params.bandFrequencies) {
        inBandFrequencies.push_back(std::make_unique<float>(freq));
    }
    inStretch = params.stretch;
    inScarcityAbundance = params.scarcityAbundance;
    inFoldPosition = params.foldPosition;
    inFoldWindowShape = params.foldWindowShape;
    inFoldWindowSize = params.foldWindowSize;
    inGrowthRate = params.growthRate;
    inEntanglement = params.entanglement;
    inBaseDelayMs = params.baseDelayMs;
    inCompressorParams = params.compressorParams;
    inUseExternalSidechain = params.useExternalSidechain;
    inTreeDensity = params.treeDensity;

    updateDelayProcParams();
    updateTreePositions();
    updateWindows();
}

// Allocate delay processors and buffers based on the number of colonies
void DelayNodes::allocateDelayProcessors(int numColonies, int numNodes)
{
    if (numColonies < 1 || numColonies > ParameterRanges::maxNutrientBands)
    {
        return;
    }

    if (numNodes < 1 || numNodes > maxNumDelayProcsPerBand)
    {
        return;
    }

    // Check if we need to resize the delay processors and buffers
    if ((numColonies == bands.size()) && (numNodes == bands[0].delayProcs.size()) &&
        (numNodes == bands[0].processorBuffers.size()) &&
        (numNodes == bands[0].treeOutputBuffers.size()))
    {
        // No need to resize, just return
        return;
    }

    std::vector<BandResources> newBands(numColonies);

    // Clear the current delay processors and buffers
    for (auto &band : bands)
    {
        band.clear();
    }
    bands.clear();

    // Allocate new delay processors for each band
    for (int band = 0; band < numColonies; ++band)
    {
        for (size_t proc = 0; proc < numNodes; ++proc)
        {
            auto newDelayProc = std::make_unique<DelayProc>();
            newBands[band].delayProcs.push_back(std::move(newDelayProc));

            auto newBuffer = std::make_unique<juce::AudioBuffer<float>>(numChannels, blockSize);
            newBands[band].processorBuffers.push_back(std::move(newBuffer));
            auto newTreeBuffer = std::make_unique<juce::AudioBuffer<float>>(numChannels, blockSize);
            newBands[band].treeOutputBuffers.push_back(std::move(newTreeBuffer));
            newBands[band].treeConnections.push_back(0.0f); // Initialize tree connections to 0.0
            newBands[band].bufferLevels.push_back(0.0f); // Initialize buffer levels to 0.0
            newBands[band].nodeDelayTimes.push_back(0.0f); // Initialize delay times to 0.0
        }

    }
    bands.swap(newBands);
}

// Process a specific band and processor stage with its own context
void DelayNodes::processNode(int band, size_t procIdx, juce::AudioBuffer<float>& input)
{
    if (band < 0 || band >= inNumColonies || procIdx >= bands[band].delayProcs.size())
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
    procIdx = juce::jlimit(static_cast<size_t>(0), bands[band].delayProcs.size() - 1, procIdx);

    return *bands[band].processorBuffers[procIdx];
}

// Get the processor node at a specific position in the matrix
DelayProc& DelayNodes::getProcessorNode(int band, size_t procIdx)
{
    // Make sure the indices are valid
    band = juce::jlimit(0, inNumColonies - 1, band);
    procIdx = juce::jlimit(static_cast<size_t>(0), bands[band].delayProcs.size() - 1, procIdx);

    return *bands[band].delayProcs[procIdx];
}

// Get the tree connection at a specific position in the matrix
float DelayNodes::getTreeConnection(int band, size_t procIdx)
{
    // Make sure the indices are valid
    band = juce::jlimit(0, inNumColonies - 1, band);
    procIdx = juce::jlimit(static_cast<size_t>(0), bands[band].treeConnections.size() - 1, procIdx);

    return bands[band].treeConnections[procIdx];
}

// Get the tree buffer for a specific band
juce::AudioBuffer<float>& DelayNodes::getTreeBuffer(int band, size_t treeIdx)
{
    // Make sure the indices are valid
    band = juce::jlimit(0, inNumColonies - 1, band);
    treeIdx = juce::jlimit(static_cast<size_t>(0), bands[band].treeOutputBuffers.size() - 1, treeIdx);

    return *bands[band].treeOutputBuffers[treeIdx];
}

// Update sidechain levels for all processors in the matrix
void DelayNodes::updateSidechainLevels()
{
    // First, gather all output levels from all delay processors into a matrix
    for (int band = 0; band < inNumColonies; ++band)
    {
        // bands[band].bufferLevels.resize(bands[band].delayProcs.size());
        for (size_t proc = 0; proc < bands[band].delayProcs.size(); ++proc)
        {
            bands[band].bufferLevels[proc] = getProcessorNode(band, proc).getOutputLevel();
        }
    }

    // For each band and processor
    for (int band = 0; band < inNumColonies; ++band)
    {
        const size_t numProcs = bands[band].delayProcs.size();

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
                        const size_t otherNumProcs = bands[otherBand].delayProcs.size();
                        combinedLevel += bands[otherBand].bufferLevels[otherNumProcs - 1];
                    }
                }

                // Set the external sidechain level for this end-of-row processor
                getProcessorNode(band, proc).setExternalSidechainLevel(16.0f * combinedLevel);
            }
            else
            {
                // For non-end nodes: use the output level of the next node in same row
                float nextNodeLevel = bands[band].bufferLevels[proc + 1];
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
    for (int band = 0; band < inNumColonies; ++band)
    {
        // treeConnections[band].resize(numActiveTrees);
        for (int tree = 0; tree < numActiveTrees; ++tree)
        {
            // Determine if this band connects to this tree (75% probability)
            // Always connect the last tree (output tree) to all bands
            if (tree == numActiveTrees - 1 || random.nextFloat() < 0.75f)
            {
                bands[band].treeConnections[tree] = 1.0f; // Connected
            }
            else
            {
                bands[band].treeConnections[tree] = 0.0f; // Not connected
            }
        }
    }
}

void DelayNodes::updateWindows()
{
    // Ensure the window sizes are set correctly
    foldWindow.resize(maxNumDelayProcsPerBand);

    // Use Juce windowing functions to create the window shapes...
    juce::AudioBuffer<float> rect(numChannels, maxNumDelayProcsPerBand);
    juce::AudioBuffer<float> hann(numChannels, maxNumDelayProcsPerBand);
    juce::AudioBuffer<float> fold(numChannels, maxNumDelayProcsPerBand);

    // Populate the window buffers with the appropriate windowing functions
    auto winSize = static_cast<size_t>(std::ceil(inFoldWindowSize * maxNumDelayProcsPerBand));
    winSize = std::max(static_cast<size_t>(4), winSize);

    auto winPosition = static_cast<size_t>(std::ceil((maxNumDelayProcsPerBand - winSize) * inFoldPosition));

    juce::dsp::WindowingFunction<float>::fillWindowingTables(
        rect.getWritePointer(0, winPosition),
        winSize,
        juce::dsp::WindowingFunction<float>::rectangular,
        false);

    juce::dsp::WindowingFunction<float>::fillWindowingTables(
        hann.getWritePointer(0, winPosition),
        winSize,
        juce::dsp::WindowingFunction<float>::hann,
        false);

    juce::dsp::AudioBlock<float> foldBlock(fold);
    juce::dsp::AudioBlock<float> rectBlock(rect);
    juce::dsp::AudioBlock<float> hannBlock(hann);

    // Apply the fold window shape to the fold windows
    rectBlock.multiplyBy(inFoldWindowShape);
    hannBlock.multiplyBy(1.0f - inFoldWindowShape);

    // Sum the rectangular and Hann windows to create the fold window
    foldBlock.replaceWithSumOf(rectBlock, hannBlock);

    // ... and copy the window shapes to the member variables
    for (size_t i = 0; i < maxNumDelayProcsPerBand; ++i)
    {
        foldWindow[i] = fold.getSample(0, i);
    }
}
