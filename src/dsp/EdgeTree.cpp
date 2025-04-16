#include "EdgeTree.h"
#include "util/ParameterRanges.h"

EdgeTree::EdgeTree()
{
}

EdgeTree::~EdgeTree()
{
}

void EdgeTree::prepare(const juce::dsp::ProcessSpec &spec)
{
    // Set up the envelope follower with our parameters
    EnvelopeFollower::Parameters params;
    params.attackMs = attackMs;
    params.releaseMs = releaseMs;
    params.levelType = juce::dsp::BallisticsFilterLevelCalculationType::RMS;

    envelopeFollower.prepare(spec);
    envelopeFollower.setParameters(params);
}

template <typename ProcessContext>
void EdgeTree::process(const ProcessContext &context)
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

    // Create a copy of the input signal
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.setSize(numChannels, numSamples);

    // Copy input data to our buffer
    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        dryBuffer.copyFrom(channel, 0, inputBlock.getChannelPointer(channel), numSamples);
    }

    // Context for the envelope follower
    juce::dsp::AudioBlock<float> analyzeBlock(dryBuffer);
    juce::dsp::ProcessContextReplacing<float> analyzeContext(analyzeBlock);

    // Process the analysis context using the EnvelopeFollower
    envelopeFollower.process(analyzeContext);

    // Normalize the envelope output
    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        auto* buffer = dryBuffer.getWritePointer(channel);
        for (size_t i = 0; i < numSamples; ++i)
        {
            buffer[i] *= 8.0f;
        }
    }

    // Apply envelope modulation (VCA)
    outputBlock.multiplyBy(analyzeBlock);
}

void EdgeTree::reset()
{
    envelopeFollower.reset();
}

void EdgeTree::setParameters(const Parameters &params)
{
    // Set attack and release times if the tree size has changed significantly
    if (std::abs(inTreeSize - params.treeSize) / params.treeSize > 0.01f)
    {
        // Set tree size
        inTreeSize = ParameterRanges::treeSizeRange.snapToLegalValue(params.treeSize);

        auto temp = ParameterRanges::normalizeParameter(ParameterRanges::treeSizeRange, inTreeSize);
        attackMs  = ParameterRanges::denormalizeParameter(ParameterRanges::attackTimeRange, temp);
        releaseMs = ParameterRanges::denormalizeParameter(ParameterRanges::releaseTimeRange, temp);

        // Update envelope follower parameters
        EnvelopeFollower::Parameters envParams;
        envParams.attackMs = attackMs;
        envParams.releaseMs = releaseMs;
        envParams.levelType = juce::dsp::BallisticsFilterLevelCalculationType::RMS;
        envelopeFollower.setParameters(envParams);
    }
}

//==================================================
template void EdgeTree::process<juce::dsp::ProcessContextReplacing<float>>(const juce::dsp::ProcessContextReplacing<float> &);
template void EdgeTree::process<juce::dsp::ProcessContextNonReplacing<float>>(const juce::dsp::ProcessContextNonReplacing<float> &);