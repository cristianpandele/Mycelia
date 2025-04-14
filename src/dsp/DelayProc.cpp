#include "DelayProc.h"

DelayProc::DelayProc()
{
    delay = *delayStore->getNextDelay();
    delay.reset ();
}

void DelayProc::prepare (const juce::dsp::ProcessSpec& spec)
{
    delay.prepare (spec);
    fs = (float) spec.sampleRate;

    feedback.reset (spec.sampleRate, 0.05 * spec.numChannels);
    inGain.reset (spec.sampleRate, 0.05 * spec.numChannels);
    delaySmooth.reset (spec.sampleRate, 0.15 * spec.numChannels);
    state.resize (spec.numChannels, 0.0f);

    reset();

    procs.prepare (spec);
    // modSine.prepare (spec);
    modDepthFactor = 0.25f * (float) spec.sampleRate; // max mod depth = 0.25 seconds
}

void DelayProc::reset()
{
    delay.reset();
    procs.reset();
    // modSine.reset();
    std::fill (state.begin(), state.end(), 0.0f);
}

void DelayProc::flushDelay()
{
    delay.reset();
    std::fill (state.begin(), state.end(), 0.0f);
    procs.reset();
}

template <typename ProcessContext>
void DelayProc::process (const ProcessContext& context)
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

    // Process context
    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        auto* inputSamples = inputBlock.getChannelPointer (channel);
        auto* outputSamples = outputBlock.getChannelPointer (channel);
        delay.setDelay (delaySmooth.getNextValue());

        if (delaySmooth.isSmoothing())
        {
            for (size_t i = 0; i < numSamples; ++i)
            {
                // delayModValue = modDepth * modDepthFactor * modSine.processSample();
                delayModValue = 0.0f;
                delay.setDelay (juce::jmax (0.0f, delaySmooth.getNextValue() + delayModValue));
                outputSamples[i] = processSampleSmooth (inputSamples[i], channel);
            }
        }
        else
        {
            // modSine.reset();
            delayModValue = 0.0f;
            for (size_t i = 0; i < numSamples; ++i)
                outputSamples[i] = processSample (inputSamples[i], channel);
        }
    }

    procs.get<lpfIdx>().snapToZero();
    procs.get<hpfIdx>().snapToZero();
}

template <typename SampleType>
inline SampleType DelayProc::processSample (SampleType x, size_t ch)
{
    auto input = inGain.getNextValue() * x;
    input = procs.processSample (input + state[ch]); // process input + feedback state
    delay.pushSample ((int) ch, input); // push input to delay line
    auto y = delay.popSample ((int) ch); // pop output from delay line
    state[ch] = y * feedback.getNextValue(); // save feedback state
    return y;
}

template <typename SampleType>
inline SampleType DelayProc::processSampleSmooth (SampleType x, size_t ch)
{
    auto input = /*inGain.getNextValue() * */ x;
    input = procs.processSample(input + state[ch]); // process input + feedback state //TODO: Change to sample by sample
    delay.pushSample ((int) ch, input); // push input to delay line
    auto y = delay.popSample ((int) ch); // pop output from delay line
    state[ch] = y * feedback.getNextValue(); // save feedback state
    return y;
}

using IIRCoefs = juce::dsp::IIR::Coefficients<float>;

void DelayProc::setParameters (const Parameters& params, bool force)
{
    // using namespace ParamHelpers;
    auto delayVal = (params.delayMs / 1000.0f) * fs;
    auto gainVal = params.feedback >= maxFeedback ? 0.0f : 1.0f;
    auto fbVal = params.feedback >= maxFeedback ? 1.0f
                                                : std::pow (juce::jmin (params.feedback, 0.95f), 0.9f);

    // modDepth = std::pow (params.modDepth, 2.5f);
    // if (params.lfoSynced)
    //     modSine.setFreqSynced (params.modFreq, params.tempoBPM);
    // else
    //     modSine.setFrequency (*params.modFreq);
    // modSine.setPlayHead (params.playhead);

    if (force)
    {
        delaySmooth.setCurrentAndTargetValue (delayVal);
        inGain.setCurrentAndTargetValue (gainVal);
        feedback.setCurrentAndTargetValue (fbVal);
        delay.setDelay (delayVal);
    }
    else
    {
        delaySmooth.setTargetValue (delayVal);
        inGain.setTargetValue (gainVal);
        feedback.setTargetValue (fbVal);
    }

    procs.get<lpfIdx>().coefficients = IIRCoefs::makeFirstOrderLowPass ((double) fs, params.lpfFreq);
    procs.get<hpfIdx>().coefficients = IIRCoefs::makeFirstOrderHighPass ((double) fs, params.hpfFreq);
    // procs.get<diffusionIdx>().setDepth (params.dispAmt, force);
    // procs.get<distortionIdx>().setGain (19.5f * std::pow (params.distortion, 2.0f) + 0.5f);
    // procs.get<pitchIdx>().setPitchSemitones (params.pitchSt, force);
    // procs.get<reverserIdx>().setReverseTime (params.revTimeMs);
}

//==================================================
template void DelayProc::process<juce::dsp::ProcessContextReplacing<float>> (const juce::dsp::ProcessContextReplacing<float>&);
template void DelayProc::process<juce::dsp::ProcessContextNonReplacing<float>> (const juce::dsp::ProcessContextNonReplacing<float>&);
