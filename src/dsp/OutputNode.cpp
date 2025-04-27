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
}

OutputNode::~OutputNode()
{
    // Clean up any resources
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
                         juce::AudioBuffer<float> *diffusionBandBuffers,
                         juce::AudioBuffer<float> *delayBandBuffers)
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
        juce::dsp::AudioBlock<float> diffusionBlock(diffusionBuffer);
        const auto& diffusionContext = juce::dsp::ProcessContextReplacing<float>(diffusionBlock);
        const auto& delayBuffer = delayBandBuffers[band];

        // Get the diffusion sample level using envelope follower
        envelopeFollowers[band].process(diffusionContext);
        float diffusionLevel = useExternalSidechain ? envelopeFollowers[band].getAverageLevel() : 0.0f;

        // Process each channel
        for (int channel = 0; channel < numWetChannels; ++channel)
        {
            // Get raw pointers to data
            const float* diffusionData = diffusionBuffer.getReadPointer(channel);
            const float* delayData = delayBuffer.getReadPointer(channel);
            float* outputData = tempBuffer->getWritePointer(channel);

            // Process each sample
            for (int sample = 0; sample < numWetSamples; ++sample)
            {
                // Use diffusion signal level as sidechain input to compress the delay signal
                outputData[sample] = duckingCompressors[band].processSample(delayData[sample], 8*diffusionLevel, channel);

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

    // Update mix parameters
    inDryWetMixLevel = ParameterRanges::dryWetRange.snapToLegalValue(params.dryWetMixLevel);
    inDelayDuckLevel = ParameterRanges::delayDuckRange.snapToLegalValue(params.delayDuckLevel);

    // Convert to 0-1 range
    inDryWetMixLevel = ParameterRanges::normalizeParameter(ParameterRanges::dryWetRange, inDryWetMixLevel);

    // Set the gain for the wet and dry signals (linear gain)
    wetGain.setGainLinear(inDryWetMixLevel);
    dryGain.setGainLinear(1.0f - inDryWetMixLevel);

    // Calculate the compression threshold and ratio based on the delay ducking value
    auto normalizedDuckValue = ParameterRanges::normalizeParameter(ParameterRanges::delayDuckRange, inDelayDuckLevel);

    compressorParams.threshold = -12.0f * (4 * normalizedDuckValue);
    compressorParams.ratio = 1.0f + 7.0f * normalizedDuckValue;

    // Update all compressors with new parameters
    for (auto &compressor : duckingCompressors)
    {
        compressor.setParameters(compressorParams);
    }

    // Update envelope followers with appropriate attack/release
    for (auto& follower : envelopeFollowers)
    {
        follower.setParameters(inEnvelopeFollowerParams);
    }
}

//==================================================
template void OutputNode::process<juce::dsp::ProcessContextReplacing<float>>(
    const juce::dsp::ProcessContextReplacing<float> &,
    const juce::dsp::ProcessContextReplacing<float> &,
    juce::AudioBuffer<float> *,
    juce::AudioBuffer<float> *);
template void OutputNode::process<juce::dsp::ProcessContextNonReplacing<float>>(
    const juce::dsp::ProcessContextNonReplacing<float> &,
    const juce::dsp::ProcessContextNonReplacing<float> &,
    juce::AudioBuffer<float> *,
    juce::AudioBuffer<float> *);
