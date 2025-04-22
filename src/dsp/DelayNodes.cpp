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
            auto newDelayProc = std::make_unique<DelayProc>();
            delayProcs.push_back(std::move(newDelayProc));
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
        delayProc->reset();
    }
}

void DelayNodes::process(juce::AudioBuffer<float> *diffusionBandBuffers)
{
    // First, gather all input levels from all delay processors
    std::vector<float> bandLevels(inNumColonies, 0.0f);
    for (int band = 0; band < inNumColonies; ++band)
    {
        bandLevels[band] = delayProcs[band]->getOutputLevel();
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

        // Set the external sidechain level for this band's processor
        delayProcs[band]->setExternalSidechainLevel(8 * combinedLevel);

        // Create audio blocks for processing
        juce::dsp::AudioBlock<float> inputBlock(const_cast<juce::AudioBuffer<float> &>(input));

        // Process through the delay
        juce::dsp::ProcessContextReplacing<float> context(inputBlock);
        delayProcs[band]->process(context);
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
        params.delayMs = delayTimeMs;
        params.feedback = 1.0f - (0.05f * i); // Decreasing feedback for higher bands
        params.growthRate = inGrowthRate;
        params.baseDelayMs = inBaseDelayMs;
        params.filterFreq = inBandFrequencies[i];
        params.filterGainDb = 0.0f;
        // params.distortion = 0.1f;
        // params.pitchSt = 0.0f;
        params.revTimeMs = 0.0f;
        // params.modFreq = nullptr;
        // params.modDepth = 0.0f;
        // params.tempoBPM = 120.0f;
        // params.lfoSynced = false;
        params.playhead = nullptr;

        // Set compressor parameters
        params.compressorParams = inCompressorParams;
        params.useExternalSidechain = inUseExternalSidechain;

        delayProcs[i]->setParameters(params, false);
    }
}

void DelayNodes::setParameters(const Parameters &params)
{
    inNumColonies = params.numColonies;
    inBandFrequencies = params.bandFrequencies;
    inStretch = params.stretch;
    inScarcityAbundance = params.scarcityAbundance;
    inGrowthRate = params.growthRate;
    inEntanglement = params.entanglement;
    inBaseDelayMs = params.baseDelayMs;
    inCompressorParams = params.compressorParams;
    inUseExternalSidechain = params.useExternalSidechain;

    updateDelayProcParams();
}
