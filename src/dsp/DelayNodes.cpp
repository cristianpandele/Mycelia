#include "DelayNodes.h"

DelayNodes::DelayNodes(size_t numBands)
{
    // Ensure we have enough delay processors
    allocateDelayProcessors(inNumColonies, maxNumDelayProcsPerBand);
    updateFoldWindow();
    startTimer(2000); // Start the timer for parameter updates
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

    for (auto &band : bands)
    {
        for (auto &proc : band.delayProcs)
        {
            proc->prepare(spec);
        }
    }

    // Initialize tree positions
    updateTreePositions();

    // Initialize inter-band connections
    updateNodeInterconnections();
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
                auto &currentBuffer = getProcessorBuffer(band, i);
                currentBuffer.setSize(inputBuffer->getNumChannels(), inputBuffer->getNumSamples(), false, false, true);
                currentBuffer.makeCopyOf(getProcessorBuffer(band, i - 1));
            }
        }
    }

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

        // Process through each delay processor with its own persistent context
        for (size_t i = 0; i < bands[band].delayProcs.size(); ++i)
        {
            // Process the current node
            processNode(band, i);

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
                    // Apply gain according to the tree connections and fold window
                    outputBuffer->addFrom(ch, 0, treeBuffer, ch, 0,
                                          outputBuffer->getNumSamples(), connectionGain * foldWindow[treeIdx]);
                }
            }
        }

        // Apply normalization gain
        // juce::dsp::AudioBlock<float> finalBlock(*outputBuffer);
        // finalBlock.multiplyBy(inNumColonies);
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
            if (bands[band].inBandFrequency)
            {
                params.filterFreq = bands[band].inBandFrequency;
            }
            else
            {
                params.filterFreq = 0.0f; // Default to 0.0 if no frequency is set
            }
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

    for (size_t band = 0; band < inNumColonies; ++band)
    {
        // If we find a single difference in the band frequencies, we need to update all of them
        if (bands[band].inBandFrequency != params.bandFrequencies[band])
        {
            for (size_t band = 0; band < inNumColonies; ++band)
            {
                bands[band].inBandFrequency = params.bandFrequencies[band];
            }
            bandFrequenciesChanged = true;
            break;
        }
    }

    if (std::abs(inStretch - params.stretch) > 0.01f)
    {
        stretchChanged = true;
        inStretch = params.stretch;
    }
    if (std::abs(inScarcityAbundance - params.scarcityAbundance) > 0.01f)
    {
        scarcityAbundanceChanged = true;
        inScarcityAbundance = params.scarcityAbundance;
    }
    if (std::abs(inFoldPosition - params.foldPosition) > 0.01f)
    {
        foldPositionChanged = true;
        inFoldPosition = params.foldPosition;
    }
    if (std::abs(inFoldWindowShape - params.foldWindowShape) > 0.01f)
    {
        foldWindowShapeChanged = true;
        inFoldWindowShape = params.foldWindowShape;
    }
    if (std::abs(inFoldWindowSize - params.foldWindowSize) > 0.01f)
    {
        foldWindowSizeChanged = true;
        inFoldWindowSize = params.foldWindowSize;
    }
    if (std::abs(inEntanglement - params.entanglement) > 0.01f)
    {
        entanglementChanged = true;
        inEntanglement = params.entanglement;
    }
    if (std::abs(inGrowthRate - params.growthRate) > 0.01f)
    {
        growthRateChanged = true;
        inGrowthRate = params.growthRate;
    }
    if (std::abs(inBaseDelayMs - params.baseDelayMs) > 0.01f)
    {
        baseDelayChanged = true;
        inBaseDelayMs = params.baseDelayMs;
    }
    if (std::abs(inTreeDensity - params.treeDensity) > 0.01f)
    {
        treeDensityChanged = true;
        inTreeDensity = params.treeDensity;
    }

    if (inCompressorParams.attackTime != params.compressorParams.attackTime ||
        inCompressorParams.releaseTime != params.compressorParams.releaseTime ||
        inCompressorParams.kneeWidth != params.compressorParams.kneeWidth ||
        inCompressorParams.ratio != params.compressorParams.ratio ||
        inCompressorParams.threshold != params.compressorParams.threshold ||
        inCompressorParams.makeupGain != params.compressorParams.makeupGain)
    {
        compressorParamsChanged = true;
        inCompressorParams = params.compressorParams;
    }

    if (inUseExternalSidechain != params.useExternalSidechain)
    {
        useExternalSidechainChanged = true;
        inUseExternalSidechain = params.useExternalSidechain;
    }
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
    if (bands.size())
    {
        if ((numColonies <= bands.size()) || (numNodes <= numActiveProcsPerBand) ||
            (numNodes <= bands[0].processorBuffers.size()) ||
            (numNodes <= bands[0].treeOutputBuffers.size()))
        {
            // No need to resize, just return
            return;
        }
    }

    std::vector<BandResources> newBands(numColonies);

    // Copy existing bands to newBands
    for (int band = 0; band < bands.size(); ++band)
    {
        newBands[band] = std::move(bands[band]);
    }

    // Allocate new delay processors for each band
    for (int band = 0; band < numColonies; ++band)
    {
        for (size_t proc = numActiveProcsPerBand; proc < numNodes; ++proc)
        {
            auto newDelayProc = std::make_unique<DelayProc>();
            newBands[band].delayProcs.push_back(std::move(newDelayProc));
            auto treeConnection = std::make_unique<float>(0.0f); // Initialize tree connections to 0.0
            newBands[band].treeConnections.push_back(std::move(treeConnection));
            auto newBuffer = std::make_unique<juce::AudioBuffer<float>>(numChannels, blockSize);
            newBands[band].processorBuffers.push_back(std::move(newBuffer));
            auto newTreeBuffer = std::make_unique<juce::AudioBuffer<float>>(numChannels, blockSize);
            newBands[band].treeOutputBuffers.push_back(std::move(newTreeBuffer));
            newBands[band].bufferLevels.push_back(0.0f); // Initialize buffer levels to 0.0
            newBands[band].nodeDelayTimes.push_back(0.0f); // Initialize delay times to 0.0

            // Initialize inter-node connection matrices if needed
            auto curProcConnections = std::vector<std::vector<float>>(
                numColonies,
                std::vector<float>(numNodes, 0.0f));
            for (int sourceBand = 0; sourceBand < numColonies; ++sourceBand)
            {
                for (int sourceProc = 0; sourceProc < numNodes; ++sourceProc)
                {
                    // Initialize inter-node connections to 0.0f
                    curProcConnections[sourceBand][sourceProc] = 0.0f;
                }
                // Set connection from [band][proc-1] to 1.0f
                if (sourceBand == band && proc > 0)
                {
                    curProcConnections[sourceBand][proc - 1] = 1.0f;
                }
            }

            newBands[band].interNodeConnections.push_back(std::move(curProcConnections));
        }
    }
    bands.swap(newBands);
    numActiveProcsPerBand = numNodes;
}

// Process a specific band and processor stage with its own context
void DelayNodes::processNode(int band, size_t procIdx)
{
    if (band < 0 || band >= inNumColonies || procIdx >= numActiveProcsPerBand)
        return;

    // Make sure our processor buffer is the right size and initialized with the input
    auto &procBuffer = getProcessorBuffer(band, procIdx);

    // If this is the first processor, data has been already copied from the input buffer
    // Otherwise, clear the buffer to avoid garbage data and set it to the size of the previous processor
    if (procIdx > 0)
    {
        procBuffer.setSize(getProcessorBuffer(band, procIdx - 1).getNumChannels(), getProcessorBuffer(band, procIdx - 1).getNumSamples(), false, false, true);
        procBuffer.clear();
    }
    else
    {
        procBuffer.setSize(getProcessorBuffer(band, procIdx - 1).getNumChannels(), getProcessorBuffer(band, procIdx).getNumSamples(), false, false, true);
    }

    // Mix in signals from other bands based on inter-band connections
    for (int sourceBand = 0; sourceBand < inNumColonies; ++sourceBand)
    {
        for (size_t sourceProc = 0; sourceProc < numActiveProcsPerBand; ++sourceProc)
        {
            // Check if there's a connection from the source band to this band at this position
            // Note: interNodeConnections[sourceBand][procIdx] - first index is target band
            float connectionStrength = bands[band].interNodeConnections[procIdx][sourceBand][sourceProc];

            if (connectionStrength > 0.0f)
            {
                // DBG("Processing node " << band << ", " << procIdx << " with connection from " << sourceBand << ", " << sourceProc << " with strength " << connectionStrength);
                // Get the buffer from the source band's processor at srcPos
                auto &srcBuffer = getProcessorBuffer(sourceBand, sourceProc);

                // Add the signal from the source band to our input with the connection gain
                for (int ch = 0; ch < procBuffer.getNumChannels(); ++ch)
                {
                    if (ch < srcBuffer.getNumChannels())
                    {
                        procBuffer.addFrom(
                            ch, 0, srcBuffer,
                            ch, 0, procBuffer.getNumSamples(),
                            connectionStrength);
                    }
                }
            }
        }
    }

    // Create a dedicated audio block and context for this processor
    juce::dsp::AudioBlock<float> block(procBuffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    // Process with this delay processor
    getProcessorNode(band, procIdx).process(context);
}

// Update sidechain levels for all processors in the matrix
void DelayNodes::updateSidechainLevels()
{
    averageScarcityAbundance = 0.0f;
    // First, gather all output levels from all delay processors into a matrix
    for (int band = 0; band < inNumColonies; ++band)
    {
        for (size_t proc = 0; proc < numActiveProcsPerBand; ++proc)
        {
            auto outputLevel = getProcessorNode(band, proc).getOutputLevel();
            auto normScarcityAbundance = ParameterRanges::normalizeParameter(ParameterRanges::scarcityAbundanceRange, inScarcityAbundance);
            bands[band].bufferLevels[proc] = juce::jlimit(0.0f, 1.0f, outputLevel + (normScarcityAbundance));
            averageScarcityAbundance += outputLevel;
        }
    }

    averageScarcityAbundance = -1.0f + (averageScarcityAbundance * inNumColonies) + inScarcityAbundance;

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
                getProcessorNode(band, proc).setExternalSidechainLevel(combinedLevel);
            }
            else
            {
                // For non-end nodes: use the output level of the next node in same row
                float nextNodeLevel = getSiblingFlow(band, proc + 1);
                // Substract own level from the next node level
                nextNodeLevel -= bands[band].bufferLevels[proc];
                getProcessorNode(band, proc).setExternalSidechainLevel(nextNodeLevel);
            }
        }
    }
}

// Update tree positions and connections based on treeDensity
void DelayNodes::updateTreePositions()
{
    // Calculate number of active trees based on treeDensity (0-100)
    const juce::NormalisableRange<float> activeTreeRange{1.0f, static_cast<float>(numActiveProcsPerBand)};

    auto normTreeDensity = ParameterRanges::normalizeParameter(ParameterRanges::treeDensityRange, inTreeDensity);
    numActiveTrees = static_cast<int>(ParameterRanges::denormalizeParameter(activeTreeRange, normTreeDensity));
    numActiveTrees = juce::jlimit(1, static_cast<int>(numActiveProcsPerBand), numActiveTrees);

    // Initialize random number generator for consistent variations
    juce::Random random(juce::Time::currentTimeMillis());

    // Resize and clear the tree positions array
    treePositions.resize(numActiveTrees);

    // Always place the first tree at the output (last position)
    treePositions[0] = static_cast<int>(numActiveProcsPerBand) - 1;

    // Place remaining trees symmetrically with small variations
    if (numActiveTrees > 1)
    {
        for (int i = 1; i < numActiveTrees; ++i)
        {
            // Calculate the ideal position for even distribution
            float idealPosition = static_cast<float>(numActiveProcsPerBand - 1) * static_cast<float>(i) / static_cast<float>(numActiveTrees - 1);

            // Add small variation (+/- 2)
            int variation = random.nextInt(4) - 2;
            int position = static_cast<int>(idealPosition) + variation;

            // Ensure position is valid and not the last position (reserved for the output)
            position = juce::jlimit(0, static_cast<int>(numActiveProcsPerBand) - 2, position);

            // Ensure we don't duplicate positions
            bool isDuplicate;
            do
            {
                isDuplicate = false;
                for (int j = 0; j < i; ++j)
                {
                    if (treePositions[j] == position)
                    {
                        isDuplicate = true;
                        position = (position + 1) % (static_cast<int>(numActiveProcsPerBand) - 2); // -1 is the output tree
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
            // Determine if this band connects to this tree (25% probability)
            // Always connect the last tree (output tree) to all bands
            if (tree == numActiveTrees - 1 || random.nextFloat() < 0.25f)
            {
                *bands[band].treeConnections[tree] = 1.0f; // Connected
            }
            else
            {
                *bands[band].treeConnections[tree] = 0.0f; // Not connected
            }
        }
    }
}

void DelayNodes::updateFoldWindow()
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
        true);

    juce::dsp::WindowingFunction<float>::fillWindowingTables(
        hann.getWritePointer(0, winPosition),
        winSize,
        juce::dsp::WindowingFunction<float>::hann,
        true);

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
        // Gain to match the potential reduction in window size
        foldWindow[i] = fold.getSample(0, i) *
                        (maxNumDelayProcsPerBand / static_cast<float>(winSize));
    }
}

void DelayNodes::timerCallback()
{
    if (baseDelayChanged || bandFrequenciesChanged || stretchChanged || growthRateChanged || useExternalSidechainChanged || compressorParamsChanged)
    {
        updateDelayProcParams();
        useExternalSidechainChanged = false;
        compressorParamsChanged = false;
        bandFrequenciesChanged = false;
        stretchChanged = false;
        growthRateChanged = false;
        baseDelayChanged = false;
    }

    if (treeDensityChanged)
    {
        updateTreePositions();
        treeDensityChanged = false;
    }
    if (bands[0].delayProcs[0])
    {
        if ((bands[0].delayProcs[0]->getAge() > 0.001f) || entanglementChanged)
        {
            updateNodeInterconnections();
            entanglementChanged = false;
        }
    }
    if (foldPositionChanged || foldWindowShapeChanged || foldWindowSizeChanged)
    {
        updateFoldWindow();
        foldPositionChanged = false;
        foldWindowShapeChanged = false;
        foldWindowSizeChanged = false;
    }
}

// Get the flow of sibling nodes into a specific band and processor
float DelayNodes::getSiblingFlow(int targetBand, size_t targetProcIdx)
{
    if (targetBand < 0 || targetBand >= inNumColonies || targetProcIdx >= numActiveProcsPerBand)
        return 0.0f;

    float incomingFlow = 0.0f;

    // Sum incoming connections from all other processors to this processor
    for (int sourceBand = 0; sourceBand < inNumColonies; ++sourceBand)
    {
        for (size_t sourceProc = 0; sourceProc < numActiveProcsPerBand; ++sourceProc)
        {
            if (bands[targetBand].interNodeConnections[targetProcIdx][sourceBand][sourceProc] > 0.0f)
            {
                // Add the connection strength to the incoming flow
                incomingFlow += bands[targetBand].interNodeConnections[targetProcIdx][sourceBand][sourceProc] *
                                bands[sourceBand].bufferLevels[sourceProc];
            }
        }
    }
    // DBG("Sibling flow for band " << targetBand << " proc " << targetProcIdx << ": " << incomingFlow);
    return incomingFlow;
}

// Update sum of outgoing connections from a particular processor
void DelayNodes::normalizeOutgoingConnections(int band, size_t procIdx)
{
    if (band < 0 || band >= inNumColonies || procIdx >= numActiveProcsPerBand)
        return;

    // Sum outgoing connections from all other processors to this processor
    float sum = 0.0f;
    for (int targetBand = 0; targetBand < inNumColonies; ++targetBand)
    {
        for (size_t targetProc = 0; targetProc < numActiveProcsPerBand; ++targetProc)
        {
            if (bands[targetBand].interNodeConnections[targetProc][band][procIdx] > 0.0f)
            {
                // Sum the outgoing connections to all other processors
                sum += bands[targetBand].interNodeConnections[targetProc][band][procIdx];
            }
        }
    }


    // DBG("Normalizing outgoing connections for band " << band << " proc " << procIdx << " with sum " << sum);
    // If updated connections over 100%, normalize the connections to ensure they sum up to 0.9f
    for (int targetBand = 0; targetBand < inNumColonies; ++targetBand)
    {
        for (size_t targetProc = 0; targetProc < numActiveProcsPerBand; ++targetProc)
        {
            // if (bands[targetBand].interNodeConnections[targetProc][band][procIdx] > 0.0f)
            // {
            bands[targetBand].interNodeConnections[targetProc][band][procIdx] /= (sum + 0.1f);
            // }
        }
    }
}

// Update inter-node connections based on entanglement parameter
void DelayNodes::updateNodeInterconnections()
{
    if (inNumColonies <= 1)
        return; // No inter-band connections possible with only one band

    // Initialize random number generator for consistent variations
    juce::Random random(juce::Time::currentTimeMillis());

    for (int band1 = 0; band1 < inNumColonies; ++band1)
    {
        for (int band2 = 0; band2 < inNumColonies; ++band2)
        {
            // Skip the first processor on each band (input node)
            for (size_t proc1 = 1; proc1 < numActiveProcsPerBand; ++proc1)
            {
                for (size_t proc2 = 1; proc2 < numActiveProcsPerBand; ++proc2)
                {
                    // Skip self-connections
                    if (band1 == band2 && proc1 == proc2)
                    {
                        continue;
                    }

                    // Skip connections to the previous processor on the same band
                    if (band1 == band2 && std::abs(static_cast<int>(proc1) - static_cast<int>(proc2)) == 1)
                    {
                        continue;
                    }

                    // Check if there's a connection between band1 and band2 at proc1 and proc2, respectively
                    float connectionStrength = bands[band1].interNodeConnections[proc1][band2][proc2];
                    auto pairMinAge = std::min(bands[band1].delayProcs[proc1]->getAge(), bands[band2].delayProcs[proc2]->getAge());

                    // If the connection strength is greater than 0.0f, we need to update it based on entanglement and age
                    if (connectionStrength > 0.0f)
                    {
                        auto normEntanglement = ParameterRanges::normalizeParameter(ParameterRanges::entanglementRange, inEntanglement);
                        auto pairEntanglementDelta = random.nextFloat() * normEntanglement * 0.5f * (0.5f - pairMinAge);

                        // DBG("Updating connection strength: " << connectionStrength << " for proc1: " << proc1 << " band1: " << band1 << " proc2: " << proc2 << " band2: " << band2);
                        // Update the connection strength based on entanglement and age
                        bands[band1].interNodeConnections[proc1][band2][proc2] += connectionStrength * pairEntanglementDelta;
                        // Update the connection strength for the reverse direction
                        bands[band2].interNodeConnections[proc2][band1][proc1] += connectionStrength * pairEntanglementDelta;
                        // Ensure that the connection strength does not drop below 0.0f
                        bands[band1].interNodeConnections[proc1][band2][proc2] = std::max(0.0f, bands[band1].interNodeConnections[proc1][band2][proc2]);
                        bands[band2].interNodeConnections[proc2][band1][proc1] = std::max(0.0f, bands[band2].interNodeConnections[proc2][band1][proc1]);

                        // Ensure that the sum of connections from this node to all other nodes is 1.0f
                        normalizeOutgoingConnections(band1, proc1);
                        normalizeOutgoingConnections(band2, proc2);
                    }
                    // If the connection strength is 0.0f, attempt to create a new connection based on entanglement
                    else
                    {
                        auto normEntanglement = ParameterRanges::normalizeParameter(ParameterRanges::entanglementRange, inEntanglement);
                        auto pairEntanglementProbability = normEntanglement * (1.0f - pairMinAge);

                        // Test for creating a connection
                        if (random.nextFloat() < pairEntanglementProbability)
                        {
                            // Determine a connection strength (0.2-0.55)
                            auto connectionStrength = 0.2f + random.nextFloat() / (5.0f + (1.0f - normEntanglement) + pairMinAge);
                            // DBG("Creating new connection between band " << band1 << " proc " << proc1 << " and band " << band2 << " proc " << proc2 << " with strength " << connectionStrength);
                            bands[band1].interNodeConnections[proc1][band2][proc2] = connectionStrength;
                            bands[band2].interNodeConnections[proc2][band1][proc1] = connectionStrength;

                            // Ensure that the sum of connections from this node to all other nodes is 1.0f
                            normalizeOutgoingConnections(band1, proc1);
                            normalizeOutgoingConnections(band2, proc2);
                        }
                    }
                }
            }
        }
    }
}

///////////////////////////
// Getter functions

// Get the processor buffer at a specific position in the matrix
juce::AudioBuffer<float> &DelayNodes::getProcessorBuffer(int band, size_t procIdx)
{
    // Make sure the indices are valid
    band = juce::jlimit(0, inNumColonies - 1, band);
    procIdx = juce::jlimit(static_cast<size_t>(0), bands[band].delayProcs.size() - 1, procIdx);

    return *bands[band].processorBuffers[procIdx];
}

// Get the processor node at a specific position in the matrix
DelayProc &DelayNodes::getProcessorNode(int band, size_t procIdx)
{
    // Make sure the indices are valid
    band = juce::jlimit(0, inNumColonies - 1, band);
    procIdx = juce::jlimit(static_cast<size_t>(0), bands[band].delayProcs.size() - 1, procIdx);

    return *bands[band].delayProcs[procIdx];
}

// Get the tree connection at a specific position in the matrix
float &DelayNodes::getTreeConnection(int band, size_t procIdx)
{
    // Make sure the indices are valid
    band = juce::jlimit(0, inNumColonies - 1, band);
    procIdx = juce::jlimit(static_cast<size_t>(0), bands[band].treeConnections.size() - 1, procIdx);

    return *bands[band].treeConnections[procIdx];
}

// Get the tree buffer for a specific band
juce::AudioBuffer<float> &DelayNodes::getTreeBuffer(int band, size_t treeIdx)
{
    // Make sure the indices are valid
    band = juce::jlimit(0, inNumColonies - 1, band);
    treeIdx = juce::jlimit(static_cast<size_t>(0), bands[band].treeOutputBuffers.size() - 1, treeIdx);

    return *bands[band].treeOutputBuffers[treeIdx];
}