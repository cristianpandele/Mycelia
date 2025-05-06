#include "EdgeTree.h"
#include "util/ParameterRanges.h"

EdgeTree::EdgeTree()
{
    startTimerHz(2); // Start the timer for parameter updates
}

EdgeTree::~EdgeTree()
{
}

void EdgeTree::prepare(const juce::dsp::ProcessSpec &spec)
{
    // Set up the envelope follower with our parameters
    envelopeFollower.prepare(spec);
    envelopeFollower.setParameters(envelopeFollowerParams);
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

    // Process the context using the EnvelopeFollower
    envelopeFollower.process(context);

    // Apply envelope modulation (VCA)
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float averageLevel = envelopeFollower.getAverageLevel(ch);
        auto *channelData = outputBlock.getChannelPointer(ch);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            channelData[sample] *= averageLevel;
        }
    }
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
        envelopeFollowerParams.attackMs = ParameterRanges::denormalizeParameter(ParameterRanges::attackTimeRange, temp);
        envelopeFollowerParams.releaseMs = ParameterRanges::denormalizeParameter(ParameterRanges::releaseTimeRange, temp);

        treeSizeChanged = true;
    }
}

void EdgeTree::timerCallback()
{
    if (treeSizeChanged)
    {
        // Update envelope follower parameters
        envelopeFollower.setParameters(envelopeFollowerParams);
        treeSizeChanged = false;
    }
}

//==================================================
template void EdgeTree::process<juce::dsp::ProcessContextReplacing<float>>(const juce::dsp::ProcessContextReplacing<float> &);
template void EdgeTree::process<juce::dsp::ProcessContextNonReplacing<float>>(const juce::dsp::ProcessContextNonReplacing<float> &);