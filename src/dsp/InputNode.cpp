#include "InputNode.h"

InputNode::InputNode()
{
    // Initialize the input nodes
    gain.setRampDurationSeconds(0.05f);
    gain.setGainLinear(0.0f);
}

InputNode::~InputNode()
{
}

void InputNode::prepare(const juce::dsp::ProcessSpec &spec)
{
    fs = (float) spec.sampleRate;
    gain.prepare(spec);
}

void InputNode::reset()
{
    gain.reset();
    // std::fill(state.begin(), state.end(), 0.0f);
}


template <typename ProcessContext>
void InputNode::process (const ProcessContext& context)
{
    gain.process(context);
}

void InputNode::setParameters (const Parameters& params)
{
    const auto maxGain = 1.2f; //TODO
    // using namespace ParamHelpers;

    inGainLevel = params.gainLevel >= maxGain ? maxGain : params.gainLevel;

    gain.setGainLinear (inGainLevel);
}

//==================================================
template void InputNode::process<juce::dsp::ProcessContextReplacing<float>> (const juce::dsp::ProcessContextReplacing<float>&);
template void InputNode::process<juce::dsp::ProcessContextNonReplacing<float>> (const juce::dsp::ProcessContextNonReplacing<float>&);
