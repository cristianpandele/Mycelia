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

void OutputNode::prepare (const juce::dsp::ProcessSpec& spec)
{
    fs = (float) spec.sampleRate;
    for (auto *gain : {&wetGain, &dryGain})
    {
        gain->prepare(spec);
    }
}

void OutputNode::reset()
{
    wetGain.reset();
    dryGain.reset();
    // std::fill(state.begin(), state.end(), 0.0f);
}

template <typename ProcessContext>
void OutputNode::process(const ProcessContext &dryContext, const ProcessContext &wetContext)
{
    wetGain.process(wetContext);
    dryGain.process(dryContext);

    // Mix the wet and dry signals
    auto wetBlock = wetContext.getOutputBlock();
    auto dryBlock = dryContext.getOutputBlock();
    auto numChannels = wetBlock.getNumChannels();

    wetBlock.replaceWithSumOf (wetBlock, dryBlock);
}

void OutputNode::setParameters(const Parameters &params)
{
    inDryWetMixLevel = ParameterRanges::normalizeParameter(ParameterRanges::dryWet, params.dryWetMixLevel);
    // Set the gain for the wet and dry signals (linear gain)
    wetGain.setGainLinear(inDryWetMixLevel);
    dryGain.setGainLinear(1.0f - inDryWetMixLevel);
}

//==================================================
template void OutputNode::process<juce::dsp::ProcessContextReplacing<float>>(
    const juce::dsp::ProcessContextReplacing<float> &,
    const juce::dsp::ProcessContextReplacing<float> &);

template void OutputNode::process<juce::dsp::ProcessContextNonReplacing<float>>(
    const juce::dsp::ProcessContextNonReplacing<float> &,
    const juce::dsp::ProcessContextNonReplacing<float> &);
