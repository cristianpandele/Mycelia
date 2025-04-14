#include "InputNode.h"
#include "util/ParameterRanges.h"

InputNode::InputNode()
{
    // Initialize the input nodes
    gain.setGainLinear(inGainLevel);    // Gain
    gain.setRampDurationSeconds(0.05f); // Gain ramp time

    // Initialize the waveshaper
    waveShaper = std::make_unique<MyShaperType>();
    waveShaper->initVoiceEffectParams();

    // Initialize OJD waveshaper type
    waveShaper->setIntParam(0, waveshaperType);

    // Set initial parameters
    using FloatParams = MyShaperType::WaveShaperFloatParams;
    waveShaper->setFloatParam((int)FloatParams::drive, waveshaperDrive); // drive
    waveShaper->setFloatParam((int)FloatParams::bias, waveshaperBias); // bias
    waveShaper->setFloatParam((int)FloatParams::postgain, waveshaperPostgain); // postgain
    // Set waveshaper filter parameters
    updateFilterCoefficients();
}

InputNode::~InputNode()
{
    // Clean up any resources
}

void InputNode::prepare(const juce::dsp::ProcessSpec &spec)
{
    fs = (float)spec.sampleRate;

    // Prepare all processors in the chain
    gain.prepare(spec);

    // Prepare the waveshaper
    updateFilterCoefficients();
}

void InputNode::reset()
{
    gain.reset();
}

template <typename ProcessContext>
void InputNode::process(const ProcessContext &context)
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

    // Process through gain stage
    gain.process(context);


    // Process through waveshaper
    processWaveShaper(outputBlock);

}

void InputNode::setParameters(const Parameters &params)
{
    // Set gain level
    inGainLevel = ParameterRanges::preampLevelRange.snapToLegalValue(params.gainLevel);
    gain.setGainLinear(inGainLevel/100.f);

    // Set waveshaper parameters
    if (params.gainLevel < ParameterRanges::preampOverdriveRange.start)
    {
        waveshaperDrive = ParameterRanges::waveshaperGainRange.start;
    }
    else
    {
        auto normValue = ParameterRanges::normalizeParameter(ParameterRanges::preampOverdriveRange, params.gainLevel);
        waveshaperDrive = ParameterRanges::denormalizeParameter(ParameterRanges::waveshaperGainRange, normValue);
    }
    waveShaper->setFloatParam((int)MyShaperType::WaveShaperFloatParams::drive, waveshaperDrive);

    // Set reverb mix level
    inReverbMix = ParameterRanges::reverbMixRange.snapToLegalValue(params.reverbMix);
    // TODO: mix in reverb signal here

    // Handle bandpass parameters - only update if changed to avoid unnecessary recalculations
    bool filterChanged = false;

    if (std::abs(inBandpassFreq - params.bandpassFreq) / params.bandpassFreq > 0.01f)
    {
        inBandpassFreq = ParameterRanges::bandpassFrequencyRange.snapToLegalValue(params.bandpassFreq);
        filterChanged = true;
    }

    if (std::abs(inBandpassWidth - params.bandpassWidth) / params.bandpassWidth > 0.01f)
    {
        inBandpassWidth = ParameterRanges::bandpassWidthRange.snapToLegalValue(params.bandpassWidth);
        filterChanged = true;
    }

    if (filterChanged)
    {
        updateFilterCoefficients();
    }
}

void InputNode::updateFilterCoefficients()
{
    waveshaperLowpass = std::min(inBandpassFreq + inBandpassWidth * 0.5f,
                                 ParameterRanges::bandpassFrequencyRange.end);
    waveshaperHighpass = std::max(inBandpassFreq - inBandpassWidth * 0.5f,
                                  ParameterRanges::bandpassFrequencyRange.start);

    // Set the low pass and high pass filter coefficients with respect to A4 (MIDI note 69)
    float lowpassPitch = std::round(std::log2(waveshaperLowpass / 440.0f) * 12.0f) - 69;
    float highpassPitch = std::round(std::log2(waveshaperHighpass / 440.0f) * 12.0f) - 69;

    waveShaper->setFloatParam((int)MyShaperType::WaveShaperFloatParams::lowpass, lowpassPitch);
    waveShaper->setFloatParam((int)MyShaperType::WaveShaperFloatParams::highpass, highpassPitch);
}

void InputNode::processWaveShaper(juce::dsp::AudioBlock<float> & buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Use a fixed note number for processing (key tracking not used here)
    const float noteNum = 0.0f;

    // Process the audio through the waveshaper in blocks of WaveShaperConfig::blockSize
    for (int pos = 0; pos < numSamples; pos += WaveshaperConfig::blockSize)
    {
        if (numChannels >= 2)
        {
            auto curSubBlock = buffer.getSubBlock(pos, WaveshaperConfig::blockSize);
            float *leftChannel = curSubBlock.getChannelPointer(0);
            float *rightChannel = curSubBlock.getChannelPointer(1);

            waveShaper->processStereo(leftChannel, rightChannel, leftChannel, rightChannel, noteNum);
        }
    }
}

//==================================================
template void InputNode::process<juce::dsp::ProcessContextReplacing<float>>(const juce::dsp::ProcessContextReplacing<float> &);
template void InputNode::process<juce::dsp::ProcessContextNonReplacing<float>>(const juce::dsp::ProcessContextNonReplacing<float> &);
