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
    for (auto numCh = 0; numCh < spec.numChannels; ++numCh)
    {
        state.push_back (0.0f);
    }

    // Prepare envelope follower
    envelopeFollower.prepare(spec);
    EnvelopeFollower::Parameters envParams;
    envParams.attackMs = envelopeAttackMs;
    envParams.releaseMs = envelopeReleaseMs;
    envelopeFollower.setParameters(envParams, true);

    reset();

    procs.prepare (spec);
    // modSine.prepare (spec);
    modDepthFactor = 0.25f * (float) spec.sampleRate; // max mod depth = 0.25 seconds
}

void DelayProc::reset()
{
    // modSine.reset();
    flushDelay();
    procs.reset();
    envelopeFollower.reset();
    inputLevel = 0.0f;
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

    // Process the input with the envelope follower
    envelopeFollower.process(context);
    inputLevel = envelopeFollower.getAverageLevel();

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

        // Check for changes in filter parameters and update if necessary
        auto filterFreqChanged = (std::abs(inFilterFreq.getNextValue() - inFilterFreq.getCurrentValue()) / inFilterFreq.getCurrentValue() > 0.01f);
        auto filterGainChanged = (std::abs(inFilterGainDb.getNextValue() - inFilterGainDb.getCurrentValue()) / inFilterGainDb.getCurrentValue() > 0.01f);

        updateFilterCoefficients();

        for (size_t i = 0; i < numSamples; ++i)
        {
            // delayModValue = modDepth * modDepthFactor * modSine.processSample();
            // delayModValue = 0.0f;
            if (inDelayTime.isSmoothing())
            {
                delay.setDelay(juce::jmax(0.0f, inDelayTime.getNextValue() /* + delayModValue */)); // Update delay time
            }

            outputSamples[i] = processSample(inputSamples[i], channel);
        }
    }

    procs.get<lpfIdx>().snapToZero();
    procs.get<hpfIdx>().snapToZero();
}

template <typename SampleType>
inline SampleType DelayProc::processSample (SampleType x, size_t ch)
{
    auto input = x;
    // input = procs.processSample(juce::jlimit(-1.0f, 1.0f, input + state[ch])); // Process input + Feedback state
    // input = procs.processSample (input + state[ch]); // Process input + Feedback state
    input = procs.processSample (input);             // Process input
    delay.pushSample ((int) ch, input);              // Push input to delay line
    auto y = delay.popSample ((int) ch);             // Pop output from delay line
    state[ch] = y * inFeedback.getNextValue();       // Save feedback state
    return y;
}

void DelayProc::setParameters (const Parameters& params, bool force)
{
    auto delayVal = (ParameterRanges::delayRange.snapToLegalValue(params.delayMs) / 1000.0f) * fs;
    auto fbVal    = params.feedback >= ParameterRanges::fbRange.end ? 1.0f
                                                                    : std::pow(juce::jmin(params.feedback, 0.95f), 0.9f);
    auto filterFreq   = (ParameterRanges::filterFreqRange.snapToLegalValue(params.filterFreq));
    auto filterGainDb = (ParameterRanges::filterGainRangeDb.snapToLegalValue(params.filterGainDb));

    auto delayChanged = (std::abs(inDelayTime.getTargetValue() - delayVal) / delayVal > 0.01f);
    auto fbChanged = (std::abs(inFeedback.getTargetValue() - fbVal) / fbVal > 0.01f);
    auto filterFreqChanged = (std::abs(inFilterFreq.getTargetValue() - filterFreq) / filterFreq > 0.01f);
    auto filterGainChanged = (std::abs(inFilterGainDb.getTargetValue() - filterGainDb) / filterGainDb > 0.01f);

    auto envAttackChanged = (std::abs(envelopeAttackMs - params.envelopeAttackMs) / params.envelopeAttackMs > 0.01f);
    auto envReleaseChanged = (std::abs(envelopeReleaseMs - params.envelopeReleaseMs) / params.envelopeReleaseMs > 0.01f);

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
            delay.setDelay (delayVal);
            inDelayTime.setCurrentAndTargetValue(delayVal);
        }
        if (fbChanged)
        {
            inFeedback.setCurrentAndTargetValue(fbVal);
        }
        if (filterFreqChanged || filterGainChanged)
        {
            inFilterFreq.setCurrentAndTargetValue(filterFreq);
            inFilterGainDb.setCurrentAndTargetValue(filterGainDb);
            updateFilterCoefficients(force);
        }
    }
    else
    {
        if (delayChanged)
        {
            inDelayTime = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>(inDelayTime.getNextValue());
            inDelayTime.reset(fs, smoothTimeSec);
            inDelayTime.setTargetValue(delayVal);
        }
        if (fbChanged)
        {
            inFeedback = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>(inFeedback.getNextValue());
            inFeedback.reset(fs, smoothTimeSec);
            inFeedback.setTargetValue(fbVal);
        }
        if (filterFreqChanged || filterGainChanged)
        {
            inFilterFreq = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>(inFilterFreq.getNextValue());
            inFilterFreq.reset(fs, smoothTimeSec);
            inFilterFreq.setTargetValue(filterFreq);
            inFilterGainDb = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>(inFilterGainDb.getNextValue());
            inFilterGainDb.reset(fs, smoothTimeSec);
            inFilterGainDb.setTargetValue(filterGainDb);
            updateFilterCoefficients(force);
        }
        if (growthRateChanged)
        {
            inGrowthRate = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>(inGrowthRate.getNextValue());
            inGrowthRate.reset(fs, smoothTimeSec);
            inGrowthRate.setTargetValue(growthRate);
        }
    }

    // Update envelope follower parameters
    if (envAttackChanged || envReleaseChanged || force)
    {
        envelopeAttackMs = params.envelopeAttackMs;
        envelopeReleaseMs = params.envelopeReleaseMs;

        EnvelopeFollower::Parameters envParams;
        envParams.attackMs = envelopeAttackMs;
        envParams.releaseMs = envelopeReleaseMs;
        envelopeFollower.setParameters(envParams, force);
    }

    Dispersion::Parameters dispParams;
    dispParams.dispersionAmount = (ParameterRanges::dispRange.snapToLegalValue(params.dispAmt));
    dispParams.smoothTime = smoothTimeSec;
    dispParams.allpassFreq = filterFreq;
    procs.get<dispersionIdx>().setParameters(dispParams, force);
    // procs.get<distortionIdx>().setGain (19.5f * std::pow (params.distortion, 2.0f) + 0.5f);
    // procs.get<pitchIdx>().setPitchSemitones (params.pitchSt, force);
    // procs.get<reverserIdx>().setReverseTime (params.revTimeMs);
}

void DelayProc::updateFilterCoefficients(bool force)
{
    float filterFreq = inFilterFreq.getNextValue();
    float filterGain = inFilterGainDb.getNextValue();

    if (force)
    {
        filterFreq = inFilterFreq.getTargetValue();
        filterGain = inFilterGainDb.getTargetValue();
    }

    procs.get<lpfIdx>().coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        fs, filterFreq, 0.4f, juce::Decibels::decibelsToGain(filterGain * -1.0f));
    procs.get<hpfIdx>().coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        fs, filterFreq, 0.4f, juce::Decibels::decibelsToGain(filterGain));
}

//==================================================
template void DelayProc::process<juce::dsp::ProcessContextReplacing<float>> (const juce::dsp::ProcessContextReplacing<float>&);
template void DelayProc::process<juce::dsp::ProcessContextNonReplacing<float>> (const juce::dsp::ProcessContextNonReplacing<float>&);
