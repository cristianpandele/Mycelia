#include "OutputNode.h"
#include "util/ParameterRanges.h"

OutputNode::OutputNode()
{
    // Initialize the output nodes
    for (auto *gain : {&wetGain, &dryGain})
    {
        gain->setRampDurationSeconds(0.05f);
        gain->setGainLinear(0.0f);
    }

    tempBuffer = std::make_unique<juce::AudioBuffer<float>>();

    startTimerHz(2); // Start the timer for parameter updates
}

OutputNode::~OutputNode()
{
    // Clean up any resources
    tempBuffer->clear();
}

void OutputNode::prepare(const juce::dsp::ProcessSpec& spec)
{
    fs = (float) spec.sampleRate;

    // Prepare the gain modules
    for (auto* gain : {&wetGain, &dryGain})
    {
        gain->prepare(spec);
    }

    // Prepare the ducking compressors
    for (auto& compressor : duckingCompressors)
    {
        compressor.prepare(spec);
    }

    // Prepare the envelope followers
    EnvelopeFollower::Parameters envParams;
    envParams.attackMs = inEnvelopeFollowerParams.attackMs;
    envParams.releaseMs = inEnvelopeFollowerParams.releaseMs;
    envParams.levelType = inEnvelopeFollowerParams.levelType;
    for (auto& follower : envelopeFollowers)
    {
        follower.prepare(spec);
        follower.setParameters(envParams, true);
    }

    // Initialize temporary buffer
    tempBuffer = std::make_unique<juce::AudioBuffer<float>>(spec.numChannels, spec.maximumBlockSize);
}

void OutputNode::reset()
{
    // Reset all ducking compressors
    for (auto& compressor : duckingCompressors)
    {
        compressor.reset();
    }

    // Reset all envelope followers
    for (auto& follower : envelopeFollowers)
    {
        follower.reset();
    }

    // Reset all gain modules
    for (auto *gain : {&wetGain, &dryGain})
    {
        gain->reset();
    }

    // Clear temporary buffer
    tempBuffer->clear();
}

template <typename ProcessContext>
void OutputNode::process(const ProcessContext &wetContext,
                         const ProcessContext &dryContext,
                         std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &diffusionBandBuffers,
                         std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &delayBandBuffers)
{
    // Manage audio context
    const auto &inputDryBlock = dryContext.getInputBlock();
    auto &outputDryBlock = dryContext.getOutputBlock();
    const auto numDryChannels = outputDryBlock.getNumChannels();
    const auto numDrySamples  = outputDryBlock.getNumSamples();
    const auto &inputWetBlock = wetContext.getInputBlock();
    auto &outputWetBlock = wetContext.getOutputBlock();
    const auto numWetChannels = outputWetBlock.getNumChannels();
    const auto numWetSamples = outputWetBlock.getNumSamples();

    jassert(inputDryBlock.getNumChannels() == numDryChannels);
    jassert(inputDryBlock.getNumSamples() == numDrySamples);
    jassert(inputWetBlock.getNumChannels() == numWetChannels);
    jassert(inputWetBlock.getNumSamples() == numWetSamples);

    // Copy input to output if non-replacing
    if (wetContext.usesSeparateInputAndOutputBlocks())
    {
        outputWetBlock.copyFrom(inputDryBlock);
    }

    // Skip processing if bypassed
    if (wetContext.isBypassed)
    {
        return;
    }

    // Clear the wet context, as we will be adding to it
    outputWetBlock.clear();

    // Process each active band
    for (int band = 0; band < inNumActiveBands; ++band)
    {
        // Get references to this band's buffers
        auto& diffusionBuffer = diffusionBandBuffers[band];
        juce::dsp::AudioBlock<float> diffusionBlock(*diffusionBuffer.get());
        const auto& diffusionContext = juce::dsp::ProcessContextReplacing<float>(diffusionBlock);
        const auto& delayBuffer = delayBandBuffers[band];

        // Get the diffusion sample level using envelope follower
        envelopeFollowers[band].process(diffusionContext);

        // Process each channel
        for (int channel = 0; channel < numWetChannels; ++channel)
        {
            float diffusionLevel = useExternalSidechain ? envelopeFollowers[band].getAverageLevel(channel) : 0.0f;
            // Get raw pointers to data
            const float* diffusionData = diffusionBandBuffers[band]->getReadPointer(channel);
            const float* delayData = delayBandBuffers[band]->getReadPointer(channel);
            float* outputData = tempBuffer->getWritePointer(channel);

            // Process each sample
            for (int sample = 0; sample < numWetSamples; ++sample)
            {
                // Use diffusion signal level as sidechain input to compress the delay signal
                outputData[sample] = duckingCompressors[band].processSample(delayData[sample], diffusionLevel, channel);

                // outputData[sample] = delayData[sample];
            }
        }

        // Add the processed band to the output
        juce::dsp::AudioBlock<float> tempBlock(*tempBuffer);
        outputWetBlock.add(tempBlock);
    }

    // Apply wet gain to the output
    wetGain.process(wetContext);

    dryGain.process(dryContext);

    // Mix the wet and dry signals
    auto wetBlock = wetContext.getOutputBlock();
    auto dryBlock = dryContext.getOutputBlock();

    wetBlock.replaceWithSumOf(wetBlock, dryBlock);
}

void OutputNode::setParameters(const Parameters &params)
{
    inNumActiveBands = ParameterRanges::nutrientBandsRange.snapToLegalValue(params.numActiveBands);

    // Update ducking parameters
    if ((std::abs(inDelayDuckLevel - params.delayDuckLevel) / params.delayDuckLevel) > 0.01f)
    {
        inDelayDuckLevel = ParameterRanges::delayDuckRange.snapToLegalValue(params.delayDuckLevel);
        inDelayDuckLevel = params.delayDuckLevel;
        duckingChanged = true;
    }

    auto tempWetGain = ParameterRanges::normalizeParameter(ParameterRanges::dryWetRange, params.dryWetMixLevel);
    if ((std::abs(inDryWetMixLevel - tempWetGain) / tempWetGain) > 0.01f)
    {
        // Convert to 0-1 range
        inDryWetMixLevel = tempWetGain;
        gainChanged = true;
    }

    // Update envelope follower parameters
    if ((std::abs(inEnvelopeFollowerParams.attackMs - params.envelopeFollowerParams.attackMs) / params.envelopeFollowerParams.attackMs) > 0.01f)
    {
        inEnvelopeFollowerParams.attackMs = params.envelopeFollowerParams.attackMs;
        envelopeFollowerChanged = true;
    }
    if ((std::abs(inEnvelopeFollowerParams.releaseMs - params.envelopeFollowerParams.releaseMs) / params.envelopeFollowerParams.releaseMs) > 0.01f)
    {
        inEnvelopeFollowerParams.releaseMs = params.envelopeFollowerParams.releaseMs;
        envelopeFollowerChanged = true;
    }
    if (inEnvelopeFollowerParams.levelType != params.envelopeFollowerParams.levelType)
    {
        inEnvelopeFollowerParams.levelType = params.envelopeFollowerParams.levelType;
        envelopeFollowerChanged = true;
    }
}

void OutputNode::timerCallback()
{
    if (gainChanged)
    {
        // Set the gain for the wet and dry signals (linear gain)
        wetGain.setGainLinear(inDryWetMixLevel);
        dryGain.setGainLinear(1.0f - inDryWetMixLevel);
        gainChanged = false;
    }

    if (duckingChanged)
    {
        // Calculate the compression threshold and ratio based on the delay ducking value
        auto normalizedDuckValue = ParameterRanges::normalizeParameter(ParameterRanges::delayDuckRange, inDelayDuckLevel);

        compressorParams.threshold = -12.0f * (4 * normalizedDuckValue);
        compressorParams.ratio = 1.0f + 7.0f * normalizedDuckValue;

        // Update all compressors with new parameters
        for (auto &compressor : duckingCompressors)
        {
            compressor.setParameters(compressorParams);
        }
        duckingChanged = false;
    }

    if (envelopeFollowerChanged)
    {
        // Update envelope follower parameters
        for (auto &follower : envelopeFollowers)
        {
            follower.setParameters(inEnvelopeFollowerParams);
        }
        envelopeFollowerChanged = false;
    }
}

//==================================================
template void OutputNode::process<juce::dsp::ProcessContextReplacing<float>>(
    const juce::dsp::ProcessContextReplacing<float> &,
    const juce::dsp::ProcessContextReplacing<float> &,
    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &,
    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &);
template void OutputNode::process<juce::dsp::ProcessContextNonReplacing<float>>(
    const juce::dsp::ProcessContextNonReplacing<float> &,
    const juce::dsp::ProcessContextNonReplacing<float> &,
    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &,
    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> &);
