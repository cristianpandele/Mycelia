#include "EdgeTree.h"
#include "util/ParameterRanges.h"

EdgeTree::EdgeTree()
{
    envelopeFollower = std::make_unique<juce::dsp::BallisticsFilter<float>>();
}

EdgeTree::~EdgeTree()
{
}

void EdgeTree::prepare(const juce::dsp::ProcessSpec &spec)
{
    envelopeFollower->prepare(spec);
    envelopeFollower->setLevelCalculationType(juce::dsp::BallisticsFilterLevelCalculationType::RMS);
    envelopeFollower->setAttackTime(attackMs);
    envelopeFollower->setReleaseTime(releaseMs);
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

    // Create an audio block for the analysis buffer
    juce::dsp::AudioBlock<float> analyzeBlock(dryBuffer);
    juce::dsp::ProcessContextReplacing<float> analyzeContext(analyzeBlock);

    // Process the envelope follower
    envelopeFollower->process(analyzeContext);
    // Normalize the envelope output
    analyzeBlock.multiplyBy(8.0f);
    // Apply envelope modulation (VCA)
    outputBlock.multiplyBy(analyzeBlock);
}

void EdgeTree::reset()
{
    envelopeFollower->reset();
}

void EdgeTree::setParameters(const Parameters &params)
{
    // Set attack and release times if the tree size has changed significantly
    if (std::abs(inTreeSize - params.treeSize) / params.treeSize > 0.01f)
    {
        // Set tree size
        inTreeSize = ParameterRanges::treeSize.snapToLegalValue(params.treeSize);

        auto temp = ParameterRanges::treeSize.convertTo0to1(inTreeSize);
        attackMs  = ParameterRanges::attackTime.convertFrom0to1(temp);
        releaseMs = ParameterRanges::releaseTime.convertFrom0to1(temp);

        envelopeFollower->setAttackTime(attackMs);
        envelopeFollower->setReleaseTime(releaseMs);
    }
}

//==================================================
template void EdgeTree::process<juce::dsp::ProcessContextReplacing<float>>(const juce::dsp::ProcessContextReplacing<float> &);
template void EdgeTree::process<juce::dsp::ProcessContextNonReplacing<float>>(const juce::dsp::ProcessContextNonReplacing<float> &);