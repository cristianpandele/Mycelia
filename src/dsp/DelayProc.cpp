#include "DelayProc.h"
#include "util/ParameterRanges.h"

DelayProc::DelayProc()
{
    delay = *delayStore->getNextDelay();
    delay.reset();

    // Configure the modulation oscillator to be a sine wave
    modProcs.get<oscillatorIdx>().initialise([](float x) { return std::sin(x); });
}

void DelayProc::prepare (const juce::dsp::ProcessSpec& spec)
{
    delay.prepare (spec);
    fs = (float) spec.sampleRate;

    inFeedback.reset(fs, smoothTimeSec);
    inFilterFreq.reset(fs, smoothTimeSec);
    inFilterGainDb.reset(fs, smoothTimeSec);
    inDelayTime.reset(fs, 2*smoothTimeSec);
    state.resize(spec.numChannels);
    for (auto numCh = 0; numCh < spec.numChannels; ++numCh)
    {
        state[numCh].resize(spec.maximumBlockSize, 0.0f);
    }

    // Prepare envelope follower
    inEnvelopeFollower.prepare(spec);
    outEnvelopeFollower.prepare(spec);
    inEnvelopeFollower.setParameters(inEnvelopeFollowerParams, true);
    outEnvelopeFollower.setParameters(inEnvelopeFollowerParams, true);

    // Prepare compressor
    compressor.prepare(spec);

    reset();

    procs.prepare (spec);
    modProcs.prepare (spec); // Prepare the modulation processor chain

    // Initialize oscillator and gain
    modProcs.get<oscillatorIdx>().setFrequency(0.2f); // Default frequency, will be updated later
    modProcs.get<gainIdx>().setGainLinear(1.0f);      // Start at full gain
}

void DelayProc::reset()
{
    // modSine.reset();
    inputLevel = 0.0f;
    outputLevel = 0.0f;
    flushDelay();
    procs.reset();
    modProcs.reset();
    inEnvelopeFollower.reset();
    outEnvelopeFollower.reset();
    compressor.reset();
}

void DelayProc::flushDelay()
{
    delay.reset();
    for (auto& channel : state)
    {
        std::fill(channel.begin(), channel.end(), 0.0f);
    }
    procs.reset();
    modProcs.reset();
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
    inEnvelopeFollower.process(context);
    outEnvelopeFollower.process(context);
    inputLevel = inEnvelopeFollower.getAverageLevel();
    outputLevel = outEnvelopeFollower.getAverageLevel();

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

    // Update current aging rate and modulation parameters
    if (inGrowthRate.isSmoothing() || (inputLevel > inputLevelMetabolicThreshold))
    {
        updateAgeingRate();
        updateModulationParameters();
    }

    // Update filter coefficients
    if (inFilterFreq.isSmoothing() || inFilterGainDb.isSmoothing() || currentAge.isSmoothing())
    {
        updateFilterCoefficients();
    }

    // Update delay time
    if (inDelayTime.isSmoothing())
    {
        delay.setDelay(juce::jmax(0.0f, inDelayTime.skip(numSamples) /* + delayModValue */));
    }

    // Update filter coefficients and modulation parameters
    if (currentAge.isSmoothing())
    {
        updateProcChainParameters(numSamples);
        updateModulationParameters();
    }

    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        auto *inputSamples = inputBlock.getChannelPointer(channel);
        auto *outputSamples = outputBlock.getChannelPointer(channel);
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputSamples[i] = processSample(inputSamples[i], channel, i);
        }
    }

    procs.get<lpfIdx>().snapToZero();
    procs.get<hpfIdx>().snapToZero();
}

template <typename SampleType>
inline SampleType DelayProc::processSample(SampleType x, size_t ch, size_t sample)
{
    auto input = x;
    // input = procs.processSample(juce::jlimit(-1.0f, 1.0f, input + state[ch])); // Process input + Feedback state
    input = procs.processSample (input + state[ch][sample]); // Process input + Feedback state
    // input = procs.processSample (input);             // Process input
    delay.pushSample ((int) ch, input);              // Push input to delay line
    auto delayOut = delay.popSample ((int) ch);             // Pop output from delay line

    // Apply ducking compressor using either the input level or external sidechain
    float sidechainLevel = inUseExternalSidechain ? externalSidechainLevel : inputLevel;
    auto y = compressor.processSample(delayOut, sidechainLevel, ch);

    // state[ch] = y * inFeedback.getNextValue();       // Save feedback state
    state[ch][sample] = (y - delayOut) * inFeedback.getNextValue(); // Save feedback state
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

    auto envAttackChanged = (std::abs(inEnvelopeFollowerParams.attackMs - params.envParams.attackMs) / params.envParams.attackMs > 0.01f);
    auto envReleaseChanged = (std::abs(inEnvelopeFollowerParams.releaseMs - params.envParams.releaseMs) / params.envParams.releaseMs > 0.01f);
    auto levelTypeChanged = (inEnvelopeFollowerParams.levelType != params.envParams.levelType);
    auto envParamsChanged = (envAttackChanged || envReleaseChanged || levelTypeChanged);

    auto growthRateChanged  = (std::abs(inGrowthRate.getTargetValue() - growthRate) / growthRate > 0.01f);
    auto baseDelayMsChanged = (std::abs(inBaseDelayMs - baseDelayMs) / baseDelayMs > 0.01f);

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
    if (envParamsChanged)
    {
        inEnvelopeFollowerParams = params.envParams;
        inEnvelopeFollower.setParameters(inEnvelopeFollowerParams, force);
        outEnvelopeFollower.setParameters(inEnvelopeFollowerParams, force);
    }

    if (baseDelayMsChanged)
    {
        inBaseDelayMs = params.baseDelayMs;
    }

    // Update age parameter
    if (growthRateChanged)
    {
        updateAgeingRate();
        updateModulationParameters();
    }

    // Update delay processor parameters
    updateProcChainParameters(force);

    // Update compressor parameters
    inCompressorParams = params.compressorParams;
    compressor.setParameters(inCompressorParams, force);

    // Update external sidechain usage flag
    inUseExternalSidechain = params.useExternalSidechain;

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

    // Apply modulation processing chain to the output
    float x = 0.0f;
    x = modProcs.processSample(x);
    // Modulate the filter tilt
    filterGain += x * 3.0f;
    filterGain = juce::jlimit(-12.0f, 12.0f, filterGain);

    procs.get<lpfIdx>().coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        fs, filterFreq, 0.4f, juce::Decibels::decibelsToGain(filterGain * -1.0f));
    procs.get<hpfIdx>().coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        fs, filterFreq, 0.4f, juce::Decibels::decibelsToGain(filterGain));
}

void DelayProc::updateProcChainParameters(bool force)
{
    Dispersion::Parameters dispParams;
    dispParams.allpassFreq = inFilterFreq.getNextValue();
    if (inputLevel > inputLevelMetabolicThreshold)
    {
        dispParams.dispersionAmount = currentAge.getNextValue();
    }
    else
    {
        dispParams.dispersionAmount = currentAge.getCurrentValue();
    }
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
    rampTimeMs = inBaseDelayMs * 100.0f / juce::jmax(0.001f, inGrowthRate.getNextValue());
    if (inputLevel > inputLevelMetabolicThreshold)
    {
        float rampTimeSec = rampTimeMs / 1000.0f;
        currentAge = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>(currentAge.getNextValue());
        currentAge.reset(fs, rampTimeSec);
        currentAge.setTargetValue(ParameterRanges::maxAge);
    }
}

void DelayProc::updateModulationParameters()
{
    // Calculate oscillator frequency: 1/10 of frequency of inBaseDelay
    float oscFreq = 0.1f;
    if (inBaseDelayMs > 0.0f)
    {
        oscFreq = 100.0f / inBaseDelayMs;
    }

    // Set the oscillator frequency
    modProcs.get<oscillatorIdx>().setFrequency(oscFreq);
    // Set the ramp time for gain modulation
    modProcs.get<gainIdx>().setGainLinear(1-currentAge.getCurrentValue());
}

//==================================================
template void DelayProc::process<juce::dsp::ProcessContextReplacing<float>> (const juce::dsp::ProcessContextReplacing<float>&);
template void DelayProc::process<juce::dsp::ProcessContextNonReplacing<float>> (const juce::dsp::ProcessContextNonReplacing<float>&);
