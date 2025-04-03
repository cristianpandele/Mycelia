#include "InputNode.h"
#include "util/ParameterRanges.h"

InputNode::InputNode()
{
    // Initialize the input nodes
    inputNodeChain.get<0>().setGainLinear(0.0f); // Gain
    inputNodeChain.get<0>().setRampDurationSeconds(0.05f); // Gain ramp time

    // Initialize filter with default values
    updateFilterCoefficients();
}

InputNode::~InputNode()
{
}

void InputNode::prepare(const juce::dsp::ProcessSpec &spec)
{
    fs = (float) spec.sampleRate;

    // Prepare all processors in the chain
    inputNodeChain.prepare(spec);

    // Update filter coefficients with new sample rate
    updateFilterCoefficients();
}

void InputNode::reset()
{
    // Reset the input node chain
    inputNodeChain.reset();
}


template <typename ProcessContext>
void InputNode::process (const ProcessContext& context)
{
    // Process the input signal through the chain
    inputNodeChain.process(context);
}

void InputNode::setParameters (const Parameters& params)
{
    // Set gain level
    inGainLevel = ParameterRanges::preampLevel.snapToLegalValue(params.gainLevel);
    inputNodeChain.get<0>().setGainLinear(inGainLevel);

    // Set reverb mix level
    inReverbMix = ParameterRanges::reverbMix.snapToLegalValue(params.reverbMix);
    // TODO: mix in reverb signal here

    // Handle bandpass parameters - only update if changed to avoid unnecessary recalculations
    bool filterChanged = false;

    if (std::abs(inBandpassFreq - params.bandpassFreq) / params.bandpassFreq > 0.01f)
    {
        inBandpassFreq = ParameterRanges::bandpassFrequency.snapToLegalValue(params.bandpassFreq);
        filterChanged = true;
    }

    if (std::abs(inBandpassWidth - params.bandpassWidth) / params.bandpassWidth > 0.01f)
    {
        inBandpassWidth = ParameterRanges::bandpassWidth.snapToLegalValue(params.bandpassWidth);
        filterChanged = true;
    }

    if (filterChanged)
        updateFilterCoefficients();
}

void InputNode::updateFilterCoefficients()
{
    // Convert width from 0-1 to Q value (1-10 is a good range for Q)
    // Width of 0 means narrow (high Q), width of 1 means wide (low Q)
    const float q = 1.0f + (1.0f - inBandpassWidth) * 9.0f;

    // Create bandpass filter coefficients
    auto &filter = inputNodeChain.get<filterIdx>();
    *filter.state = *FilterCoefs::makeBandPass(fs, inBandpassFreq, q);

    inputNodeChain.get<1>().reset();
}

//==================================================
template void InputNode::process<juce::dsp::ProcessContextReplacing<float>> (const juce::dsp::ProcessContextReplacing<float>&);
template void InputNode::process<juce::dsp::ProcessContextNonReplacing<float>> (const juce::dsp::ProcessContextNonReplacing<float>&);
