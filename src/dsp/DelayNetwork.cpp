#include "DelayNetwork.h"

DelayNetwork::DelayNetwork()
{
}

DelayNetwork::~DelayNetwork()
{
    // Clean up any resources
}

void DelayNetwork::prepare(const juce::dsp::ProcessSpec &spec)
{
    fs = (float)spec.sampleRate;

    // Initialize diffusion band buffers
    for (auto &buffer : diffusionBandBuffers)
    {
        buffer.setSize(spec.numChannels, spec.maximumBlockSize);
        buffer.clear();
    }

    // Prepare the diffusion control
    diffusionControl.prepare(spec);

    // Prepare the delay nodes
    delayNodes.prepare(spec);

    // Initialize default parameters
    diffusionControl.setParameters(DiffusionControl::Parameters{.numActiveBands = activeFilterBands});

    // Initialize delay nodes with default parameters
    delayNodes.setParameters(DelayNodes::Parameters{.growthRate = inGrowthRate,
                                                    .entanglement = inEntanglement,
                                                    .numColonies = activeFilterBands});
}

void DelayNetwork::reset()
{
    // Reset all internal states
    diffusionControl.reset();
    delayNodes.reset();

    // Clear the diffusion band buffers
    for (auto &buffer : diffusionBandBuffers)
    {
        buffer.clear();
    }
}

template <typename ProcessContext>
void DelayNetwork::process(const ProcessContext &context)
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
    diffusionControl.process(context, diffusionBandBuffers.data());

    // Process through delay nodes
    delayNodes.process(diffusionBandBuffers.data());

    // Clear output before summing
    outputBlock.clear();

    // Sum all active band outputs into the output block
    for (int band = 0; band < activeFilterBands; ++band)
    {
        // Create AudioBlock for band output
        juce::dsp::AudioBlock<float> bandBlock(diffusionBandBuffers[band]);
        // Sum band output to the main output block
        outputBlock.add(bandBlock);
    }

    // Apply normalization based on the number of bands
    // const float normalizationGain = 1.0f / std::sqrtf(static_cast<float>(activeFilterBands));
    // outputBlock.multiplyBy(normalizationGain);
}

void DelayNetwork::setParameters(const Parameters &params)
{
    inGrowthRate = params.growthRate;
    inEntanglement = params.entanglement;

    // Update diffusion control parameters
    diffusionControl.setParameters(DiffusionControl::Parameters{.numActiveBands = activeFilterBands});

    // Update delay nodes parameters
    delayNodes.setParameters(DelayNodes::Parameters{.growthRate = inGrowthRate,
                                                    .entanglement = inEntanglement,
                                                    .numColonies = activeFilterBands});
}

// Explicitly instantiate the templates for the supported context types
template void DelayNetwork::process<juce::dsp::ProcessContextReplacing<float>>(const juce::dsp::ProcessContextReplacing<float> &);
template void DelayNetwork::process<juce::dsp::ProcessContextNonReplacing<float>>(const juce::dsp::ProcessContextNonReplacing<float> &);
