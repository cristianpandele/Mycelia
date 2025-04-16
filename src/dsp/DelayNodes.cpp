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
    // Process each band through its corresponding delay processor
    for (int band = 0; band < inNumColonies; ++band)
    {
        auto &input = diffusionBandBuffers[band];

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
        float delayTimeMs = std::abs(inStretch) * baseDelayMs * (1 + 1.0f * i);

        // Set initial parameters
        DelayProc::Parameters params;
        params.delayMs = delayTimeMs;
        params.feedback = 0.5f - (0.05f * i); // Decreasing feedback for higher bands
        params.filterFreq = inBandFrequencies[i];
        params.filterGainDb = 0.0f;
        // params.distortion = 0.1f;
        // params.pitchSt = 0.0f;
        params.dispAmt = 0.2f;
        params.revTimeMs = 0.0f;
        // params.modFreq = nullptr;
        // params.modDepth = 0.0f;
        params.tempoBPM = 120.0f;
        // params.lfoSynced = false;
        params.playhead = nullptr;

        delayProcs[i]->setParameters(params, false);
    }
}

void DelayNodes::setParameters(const Parameters &params)
{
    inGrowthRate = params.growthRate;
    inEntanglement = params.entanglement;
    inNumColonies = params.numColonies;
    inBandFrequencies = params.bandFrequencies;
    inStretch = params.stretch;

    updateDelayProcParams();
}