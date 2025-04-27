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
            for (size_t j = 0; j < 4; ++j)
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

    // Prepare the array of delay processors
    for (auto &delayProc : delayProcs)
    {
        // Prepare the processor
        delayProc->prepare(spec);
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
}

void DelayNodes::process(juce::AudioBuffer<float> *diffusionBandBuffers)
{
    // First, gather all output levels from all delay processors
    std::vector<float> bandLevels(inNumColonies, 0.0f);
    for (int band = 0; band < inNumColonies; ++band)
    {
        // Get the output level from each final delay processor
        bandLevels[band] = delayProcs[band].back()->getOutputLevel();
    }

    // Process each band through its corresponding delay processor
    for (int band = 0; band < inNumColonies; ++band)
    {
        auto &input = diffusionBandBuffers[band];

        // Calculate the combined sidechain level from all OTHER bands
        float combinedLevel = 0.0f;
        int otherBandsCount = 0;

        for (int otherBand = 0; otherBand < inNumColonies; ++otherBand)
        {
            if (otherBand != band)  // Skip the current band
            {
                combinedLevel += bandLevels[otherBand];
                otherBandsCount++;
            }
        }

        // Set the external sidechain level for this band's final processor
        delayProcs[band].back()->setExternalSidechainLevel(/*8 * */ combinedLevel);

        // Create audio blocks for processing
        juce::dsp::AudioBlock<float> inputBlock(const_cast<juce::AudioBuffer<float> &>(input));
        juce::dsp::ProcessContextReplacing<float> context(inputBlock);

        // Process through the delay processors
        const float normalizationGain = /*std::sqrt*/(inNumColonies);
        for (auto &proc : delayProcs[band])
        {
            proc->process(context);
        }
        // Compensate for the normalization gain of the final stage
        inputBlock.multiplyBy(normalizationGain);
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
        params.delayMs = delayTimeMs/4.0f;
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
