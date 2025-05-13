#include "DelayNetwork.h"

DelayNetwork::DelayNetwork()
{
    diffusionBandFrequencies.resize(ParameterRanges::maxNutrientBands);
    startTimerHz(2); // Start the timer for parameter updates
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
    if (std::abs(inActiveFilterBands - params.numActiveFilterBands) > 0.01f)
    {
        inActiveFilterBands = ParameterRanges::nutrientBandsRange.snapToLegalValue(params.numActiveFilterBands);
        numActiveFilterBandsChanged = true;
    }
    if (std::abs(inTreeDensity - params.treeDensity) > 0.01f)
    {
        inTreeDensity = ParameterRanges::treeDensityRange.snapToLegalValue(params.treeDensity);
        treeDensityChanged = true;
    }
    if (std::abs(inStretch - params.stretch) > 0.01f)
    {
        inStretch = ParameterRanges::stretchRange.snapToLegalValue(params.stretch);
        stretchChanged = true;
    }
    if (std::abs(inTempoValue - params.tempoValue) > 0.01f)
    {
        inTempoValue = ParameterRanges::tempoValueRange.snapToLegalValue(params.tempoValue);
        tempoValueChanged = true;
    }
    if (std::abs(inScarcityAbundance - params.scarcityAbundance) > 0.01f)
    {
        inScarcityAbundance = ParameterRanges::scarcityAbundanceRange.snapToLegalValue(params.scarcityAbundance);
        scarcityAbundanceChanged = true;
    }
    if (std::abs(inScarcityAbundanceOverride - params.scarcityAbundanceOverride) > 0.01f)
    {
        inScarcityAbundanceOverride = ParameterRanges::scarcityAbundanceRange.snapToLegalValue(params.scarcityAbundanceOverride);
        scarcityAbundanceOverrideChanged = true;
    }
    if (std::abs(inFoldPosition - params.foldPosition) > 0.01f)
    {
        inFoldPosition = ParameterRanges::foldPositionRange.snapToLegalValue(params.foldPosition);
        foldPositionChanged = true;
    }
    if (std::abs(inFoldWindowShape - params.foldWindowShape) > 0.01f)
    {
        inFoldWindowShape = ParameterRanges::foldWindowShapeRange.snapToLegalValue(params.foldWindowShape);
        foldWindowShapeChanged = true;
    }
    if (std::abs(inFoldWindowSize - params.foldWindowSize) > 0.01f)
    {
        inFoldWindowSize = ParameterRanges::foldWindowSizeRange.snapToLegalValue(params.foldWindowSize);
        foldWindowSizeChanged = true;
    }
    if (std::abs(inEntanglement - params.entanglement) > 0.01f)
    {
        inEntanglement = ParameterRanges::entanglementRange.snapToLegalValue(params.entanglement);
        entanglementChanged = true;
    }
    if (std::abs(inGrowthRate - params.growthRate) > 0.01f)
    {
        inGrowthRate = ParameterRanges::growthRateRange.snapToLegalValue(params.growthRate);
        growthRateChanged = true;
    }
}

void DelayNetwork::timerCallback()
{
    // Update parameters if they have changed
    if (numActiveFilterBandsChanged || treeDensityChanged || stretchChanged ||
        tempoValueChanged || scarcityAbundanceChanged || scarcityAbundanceOverrideChanged ||
        foldPositionChanged || foldWindowShapeChanged || foldWindowSizeChanged ||
        entanglementChanged || growthRateChanged)
    {
        allocateBandBuffers(inActiveFilterBands);
        updateDiffusionDelayNodesParams();

        numActiveFilterBandsChanged = false;
        treeDensityChanged = false;
        stretchChanged = false;
        tempoValueChanged = false;
        scarcityAbundanceChanged = false;
        scarcityAbundanceOverrideChanged = false;
        foldPositionChanged = false;
        foldWindowShapeChanged = false;
        foldWindowSizeChanged = false;
        entanglementChanged = false;
        growthRateChanged = false;
    }
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

    compressorParams.threshold = -6.0f * (normalizedScarAbundance);
    compressorParams.ratio = 1.0f + (3.0f * normalizedScarAbundance);

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
                                                    .foldPosition = inFoldPosition,
                                                    .foldWindowShape = inFoldWindowShape,
                                                    .foldWindowSize = inFoldWindowSize,
                                                    .entanglement = inEntanglement,
                                                    .growthRate = inGrowthRate,
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
