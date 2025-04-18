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

    inFeedback.reset(fs, smoothTimeSec);
    inFilterFreq.reset(fs, smoothTimeSec);
    inFilterGainDb.reset(fs, smoothTimeSec);
    inDelayTime.reset(fs, 2*smoothTimeSec);
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
    inputLevel = 0.0f;
    flushDelay();
    procs.reset();
    envelopeFollower.reset();
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

    // The first time the input level exceeds a threshold, set the target age to 1.0f
    if ((inputLevel > inputLevelThreshold) && (currentAge.getTargetValue() < 0.1f))
    {
        updateAgeingRate();
    }

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

        // Update current aging rate and filter coefficients
        if (inGrowthRate.isSmoothing())
        {
            updateAgeingRate();
        }

        if (inFilterFreq.isSmoothing() || inFilterGainDb.isSmoothing())
        {
            updateFilterCoefficients();
        }

        for (size_t i = 0; i < numSamples; ++i)
        {
            // delayModValue = modDepth * modDepthFactor * modSine.processSample();
            // delayModValue = 0.0f;
            if (inDelayTime.isSmoothing())
            {
                delay.setDelay(juce::jmax(0.0f, inDelayTime.getNextValue() /* + delayModValue */)); // Update delay time
            }

            if (currentAge.isSmoothing())
            {
                updateProcChainParameters();
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
    auto delaySamples = (ParameterRanges::delayRange.snapToLegalValue(params.delayMs) / 1000.0f) * fs;
    auto fbVal        = params.feedback >= ParameterRanges::fbRange.end ? 1.0f
                                                                    : std::pow(juce::jmin(params.feedback, 0.95f), 0.9f);
    auto filterFreq   = (ParameterRanges::filterFreqRange.snapToLegalValue(params.filterFreq));
    auto filterGainDb = (ParameterRanges::filterGainRangeDb.snapToLegalValue(params.filterGainDb));

    auto growthRate  = (ParameterRanges::growthRateRange.snapToLegalValue(params.growthRate));
    auto baseDelayMs = (ParameterRanges::delayRange.snapToLegalValue(params.baseDelayMs));

    auto delayChanged = (std::abs(inDelayTime.getTargetValue() - delaySamples) / delaySamples > 0.01f);
    auto fbChanged = (std::abs(inFeedback.getTargetValue() - fbVal) / fbVal > 0.01f);
    auto filterFreqChanged = (std::abs(inFilterFreq.getTargetValue() - filterFreq) / filterFreq > 0.01f);
    auto filterGainChanged = (std::abs(inFilterGainDb.getTargetValue() - filterGainDb) / filterGainDb > 0.01f);

    auto envAttackChanged = (std::abs(envelopeAttackMs - params.envelopeAttackMs) / params.envelopeAttackMs > 0.01f);
    auto envReleaseChanged = (std::abs(envelopeReleaseMs - params.envelopeReleaseMs) / params.envelopeReleaseMs > 0.01f);

    auto growthRateChanged  = (std::abs(inGrowthRate.getTargetValue() - growthRate) / growthRate > 0.01f);
    auto baseDelayMsChanged = (std::abs(inBaseDelayMs - baseDelayMs) / baseDelayMs > 0.01f);

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
            delay.setDelay (delaySamples);
            inDelayTime.setCurrentAndTargetValue(delaySamples);
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
        if (growthRateChanged)
        {
            inGrowthRate.setCurrentAndTargetValue(growthRate);
        }
    }
    else
    {
        if (delayChanged)
        {
            inDelayTime = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>(inDelayTime.getNextValue());
            inDelayTime.reset(fs, smoothTimeSec);
            inDelayTime.setTargetValue(delaySamples);
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
    if (envAttackChanged || envReleaseChanged)
    {
        envelopeAttackMs = params.envelopeAttackMs;
        envelopeReleaseMs = params.envelopeReleaseMs;

        EnvelopeFollower::Parameters envParams;
        envParams.attackMs = envelopeAttackMs;
        envParams.releaseMs = envelopeReleaseMs;
        envelopeFollower.setParameters(envParams, force);
    }

    if (baseDelayMsChanged)
    {
        inBaseDelayMs = params.baseDelayMs;
    }

    // Update age parameter
    if (growthRateChanged)
    {
        updateAgeingRate();
    }

    // Update delay processor parameters
    updateProcChainParameters(force);
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

void DelayProc::updateProcChainParameters(bool force)
{
    Dispersion::Parameters dispParams;
    dispParams.allpassFreq = inFilterFreq.getNextValue();
    dispParams.dispersionAmount = currentAge.getNextValue();
    if (force)
    {
        dispParams.allpassFreq = inFilterFreq.getTargetValue();
        dispParams.dispersionAmount = currentAge.getTargetValue();
    }
    procs.get<dispersionIdx>().setParameters(dispParams);

    // procs.get<distortionIdx>().setGain (19.5f * std::pow (params.distortion, 2.0f) + 0.5f);
    // procs.get<pitchIdx>().setPitchSemitones (params.pitchSt, force);
    // procs.get<reverserIdx>().setReverseTime (params.revTimeMs);
}

void DelayProc::updateAgeingRate()
{
    float rampTimeSec = (inBaseDelayMs * 100.0f / juce::jmax(0.1f, inGrowthRate.getNextValue())) / 1000.0f;
    currentAge = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>(currentAge.getNextValue());
    currentAge.reset(fs, rampTimeSec);
    currentAge.setTargetValue(ParameterRanges::maxAge);
}

//==================================================
template void DelayProc::process<juce::dsp::ProcessContextReplacing<float>> (const juce::dsp::ProcessContextReplacing<float>&);
template void DelayProc::process<juce::dsp::ProcessContextNonReplacing<float>> (const juce::dsp::ProcessContextNonReplacing<float>&);
