#include "DelayNetwork.h"

DelayNetwork::DelayNetwork()
{
    diffusionBandFrequencies.resize(ParameterRanges::maxNutrientBands);
}

DelayNetwork::~DelayNetwork()
{
    // Clean up any resources
    for (auto &buffer : diffusionBandBuffers)
    {
        buffer->clear();
    }
    for (auto &buffer : delayBandBuffers)
    {
        buffer->clear();
    }
    diffusionBandFrequencies.clear();
}

void DelayNetwork::prepare(const juce::dsp::ProcessSpec &spec)
{
    fs = (float)spec.sampleRate;
    // numChannels = spec.numChannels;
    // blockSize = spec.maximumBlockSize;

    diffusionBandFrequencies.resize(ParameterRanges::maxNutrientBands);

    // Prepare the diffusion control
    diffusionControl.prepare(spec);

    // Prepare the delay nodes
    delayNodes.prepare(spec);

    updateDiffusionDelayNodesParams();
}

void DelayNetwork::reset()
{
    // Reset all internal states
    diffusionControl.reset();
    delayNodes.reset();

    // Clear the diffusion band buffers
    for (auto &buffer : diffusionBandBuffers)
    {
        buffer->clear();
    }

    // Clear the delay band buffers
    for (auto &buffer : delayBandBuffers)
    {
        buffer->clear();
    }
}

template <typename ProcessContext>
void DelayNetwork::process(const ProcessContext &context,
                           std::vector<std::unique_ptr<juce::AudioBuffer<float>>>  &diffusionBandBuffers,
                           std::vector<std::unique_ptr<juce::AudioBuffer<float>>>  &delayBandBuffers)
{
    // Manage audio context
    const auto &inputBlock = context.getInputBlock();
    auto &outputBlock = context.getOutputBlock();
    const auto numChannels = outputBlock.getNumChannels();
    const auto numSamples = outputBlock.getNumSamples();

    jassert(inputBlock.getNumChannels() == numChannels);
    jassert(inputBlock.getNumSamples() == numSamples);

    // Copy input to output if non-replacing
    if (context.usesSeparateInputAndOutputBlocks())
    {
        outputBlock.copyFrom(inputBlock);
    }

    // Skip processing if bypassed
    if (context.isBypassed)
    {
        return;
    }

    // Process through diffusion control
    diffusionControl.process(context, diffusionBandBuffers);

    // Copy diffusion band buffers to delay band buffers
    for (int band = 0; band < inActiveFilterBands; ++band)
    {
        juce::dsp::AudioBlock<float> diffusionBlock(*diffusionBandBuffers[band].get());
        juce::dsp::AudioBlock<float> delayBlock (*delayBandBuffers[band].get());

        delayBlock.copyFrom(diffusionBlock);
    }

    // Process through delay nodes
    delayNodes.process(delayBandBuffers);
}

void DelayNetwork::setParameters(const Parameters &params)
{
    inActiveFilterBands = ParameterRanges::nutrientBandsRange.snapToLegalValue(params.numActiveFilterBands);
    inTreeDensity = ParameterRanges::treeDensityRange.snapToLegalValue(params.treeDensity);
    inStretch = ParameterRanges::stretchRange.snapToLegalValue(params.stretch);
    inTempoValue = ParameterRanges::tempoValueRange.snapToLegalValue(params.tempoValue);
    inScarcityAbundance = ParameterRanges::scarcityAbundanceRange.snapToLegalValue(params.scarcityAbundance);
    inScarcityAbundanceOverride = ParameterRanges::scarcityAbundanceRange.snapToLegalValue(params.scarcityAbundanceOverride);
    inEntanglement = ParameterRanges::entanglementRange.snapToLegalValue(params.entanglement);
    inGrowthRate = ParameterRanges::growthRateRange.snapToLegalValue(params.growthRate);

    diffusionBandFrequencies.resize(inActiveFilterBands);
    updateDiffusionDelayNodesParams();
}

// Allocate buffers based on the number of bands
void DelayNetwork::allocateBandBuffers(int numBands)
{
    diffusionBandFrequencies.resize(numBands);
}

void DelayNetwork::updateDiffusionDelayNodesParams()
{
    // Calculate base delay time from tempo (quarter note time in milliseconds)
    baseDelayMs = (60.0f / inTempoValue) * 1000.0f;

    // Calculate the compression threshold and ratio based on the scarcity/abundance value
    auto normalizedScarAbundance = ParameterRanges::normalizeParameter(ParameterRanges::scarcityAbundanceRange, inScarcityAbundance);

    compressorParams.threshold = -12.0f * (normalizedScarAbundance);
    compressorParams.ratio = 1.0f + (2.0f * normalizedScarAbundance);

    // Update diffusion control parameters
    diffusionControl.setParameters(DiffusionControl::Parameters{.numActiveBands = inActiveFilterBands});

    // Get band frequencies from the diffusion control
    auto dataPtr = diffusionBandFrequencies.data();
    diffusionControl.getBandFrequencies(dataPtr, &inActiveFilterBands);

    // Update delay nodes parameters
    delayNodes.setParameters(DelayNodes::Parameters{.numColonies = inActiveFilterBands,
                                                    .bandFrequencies = std::vector<float>(dataPtr, dataPtr + inActiveFilterBands),
                                                    .stretch = inStretch,
                                                    .scarcityAbundance = inScarcityAbundance,
                                                    .growthRate = inGrowthRate,
                                                    .entanglement = inEntanglement,
                                                    .baseDelayMs = baseDelayMs,
                                                    .treeDensity = inTreeDensity,
                                                    .compressorParams = compressorParams,
                                                    .useExternalSidechain = useExternalSidechain});
}

// Explicitly instantiate the templates for the supported context types
template void DelayNetwork::process<juce::dsp::ProcessContextReplacing<float>>(const juce::dsp::ProcessContextReplacing<float> &,
    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &, std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &);
template void DelayNetwork::process<juce::dsp::ProcessContextNonReplacing<float>>(const juce::dsp::ProcessContextNonReplacing<float> &,
    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &, std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &);
