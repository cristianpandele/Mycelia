#include "DelayProc.h"
#include "util/ParameterRanges.h"

DelayProc::DelayProc()
{
    delay = *delayStore->getNextDelay();
    delay.reset ();
}

void DelayProc::prepare (const juce::dsp::ProcessSpec& spec)
{
    delay.prepare (spec);
    fs = (float) spec.sampleRate;

    inFeedback.reset(fs, 0.05 * spec.numChannels);
    inFilterFreq.reset(fs, 0.05 * spec.numChannels);
    inFilterGainDb.reset(fs, 0.05 * spec.numChannels);
    inDelayTime.reset(fs, 0.15 * spec.numChannels);
    state.resize (spec.numChannels, 0.0f);

    reset();

    procs.prepare (spec);
    // modSine.prepare (spec);
    modDepthFactor = 0.25f * (float) spec.sampleRate; // max mod depth = 0.25 seconds
}

void DelayProc::reset()
{
    // modSine.reset();
    flushDelay();
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

        // Update delay time
        delay.setDelay (inDelayTime.getNextValue());

        // Check for changes in filter parameters and update if necessary
        auto filterFreqChanged = (std::abs(inFilterFreq.getNextValue() - inFilterFreq.getCurrentValue()) / inFilterFreq.getCurrentValue() > 0.01f);
        auto filterGainChanged = (std::abs(inFilterGainDb.getNextValue() - inFilterGainDb.getCurrentValue()) / inFilterGainDb.getCurrentValue() > 0.01f);
        if (filterFreqChanged || filterGainChanged)
        {
            updateFilterCoefficients();
        }

        if (inDelayTime.isSmoothing())
        {
            for (size_t i = 0; i < numSamples; ++i)
            {
                // delayModValue = modDepth * modDepthFactor * modSine.processSample();
                delayModValue = 0.0f;
                delay.setDelay (juce::jmax (0.0f, inDelayTime.getNextValue() + delayModValue));
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
    auto input = x;
    input = procs.processSample (input + state[ch]); // Process input + Feedback state
    delay.pushSample ((int) ch, input);              // Push input to delay line
    auto y = delay.popSample ((int) ch);             // Pop output from delay line
    state[ch] = y * inFeedback.getNextValue();       // Save feedback state
    return y;
}

template <typename SampleType>
inline SampleType DelayProc::processSampleSmooth (SampleType x, size_t ch)
{
    auto input = x;
    input = procs.processSample(input + state[ch]);  // Process input + feedback state
    delay.setDelay(inDelayTime.getNextValue());      // Update delay time
    delay.pushSample ((int) ch, input);              // Push input to delay line
    auto y = delay.popSample ((int) ch);             // Pop output from delay line
    state[ch] = y * inFeedback.getNextValue();       // Save feedback state
    return y;
}

void DelayProc::setParameters (const Parameters& params, bool force)
{
    auto delayVal = (ParameterRanges::delayRange.snapToLegalValue(params.delayMs) / 1000.0f) * fs;
    auto fbVal = params.feedback >= ParameterRanges::fbRange.end ? 1.0f
                                                                 : std::pow(juce::jmin(params.feedback, 0.95f), 0.9f);

    auto delayChanged = (std::abs(inDelayTime.getTargetValue() - delayVal) / delayVal > 0.01f);
    auto fbChanged = (std::abs(inFeedback.getTargetValue() - fbVal) / fbVal > 0.01f);
    auto filterFreqChanged = (std::abs(inFilterFreq.getTargetValue() - params.filterFreq) / params.filterFreq > 0.01f);
    auto filterGainChanged = (std::abs(inFilterGainDb.getTargetValue() - params.filterGainDb) / params.filterGainDb > 0.01f);
    // modDepth = std::pow (params.modDepth, 2.5f);
    // if (params.lfoSynced)
    //     modSine.setFreqSynced (params.modFreq, params.tempoBPM);
    // else
    //     modSine.setFrequency (*params.modFreq);
    // modSine.setPlayHead (params.playhead);

    if (force)
    {
        if (delayChanged)
        {
            inDelayTime.setCurrentAndTargetValue(delayVal);
            delay.setDelay (delayVal);
        }
        if (fbChanged)
        {
            inFeedback.setCurrentAndTargetValue(fbVal);
        }
        if (filterFreqChanged || filterGainChanged)
        {
            inFilterFreq.setCurrentAndTargetValue(params.filterFreq);
            inFilterGainDb.setCurrentAndTargetValue(params.filterGainDb);
            updateFilterCoefficients();
        }
    }
    else
    {
        if (delayChanged)
        {
            inDelayTime.setTargetValue(delayVal);
        }
        if (fbChanged)
        {
            inFeedback.setTargetValue(fbVal);
        }
        if (filterFreqChanged || filterGainChanged)
        {
            inFilterFreq.setTargetValue(params.filterFreq);
            inFilterGainDb.setTargetValue(params.filterGainDb);
            updateFilterCoefficients();
        }
    }

    // procs.get<diffusionIdx>().setDepth (params.dispAmt, force);
    // procs.get<distortionIdx>().setGain (19.5f * std::pow (params.distortion, 2.0f) + 0.5f);
    // procs.get<pitchIdx>().setPitchSemitones (params.pitchSt, force);
    // procs.get<reverserIdx>().setReverseTime (params.revTimeMs);
}

void DelayProc::updateFilterCoefficients()
{
    procs.get<lpfIdx>().coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        fs, inFilterFreq.getNextValue(), 0.4f, juce::Decibels::decibelsToGain(inFilterGainDb.getNextValue() * -1.0f));
    procs.get<hpfIdx>().coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        fs, inFilterFreq.getNextValue(), 0.4f, juce::Decibels::decibelsToGain(inFilterGainDb.getNextValue()));
}

//==================================================
template void DelayProc::process<juce::dsp::ProcessContextReplacing<float>> (const juce::dsp::ProcessContextReplacing<float>&);
template void DelayProc::process<juce::dsp::ProcessContextNonReplacing<float>> (const juce::dsp::ProcessContextNonReplacing<float>&);
