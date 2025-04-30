#include "EnvelopeFollower.h"

EnvelopeFollower::EnvelopeFollower()
{
    // Initialize with default values
}

void EnvelopeFollower::prepare(const juce::dsp::ProcessSpec &spec)
{
    sampleRate = static_cast<float>(spec.sampleRate);

    // Calculate coefficients for attack and release
    setInterpolationParameters();

    numChannels = spec.numChannels;

    // Resize envelopeStates to match the number of channels
    allocateVectors(numChannels);

    reset();
}

void EnvelopeFollower::allocateVectors(size_t numChannels)
{
    // Resize envelopeStates if needed
    if (envelopeStates.size() != numChannels)
    {
        envelopeStates.resize(numChannels);
    }
}

void EnvelopeFollower::reset()
{
    // Reset all envelope states
    for (auto &state : envelopeStates)
    {
        state.envelope = 0.0f;
        state.rmsSum = 0.0f;
        state.rmsSamples = 0;
    }
}

template <typename ProcessContext>
void EnvelopeFollower::process(const ProcessContext &context)
{
    // Get input block
    const auto &inputBlock = context.getInputBlock();
    const auto numSamples = inputBlock.getNumSamples();

    // Resize envelopeStates if needed
    allocateVectors(numChannels);

    // Process each channel with the envelope follower
    gainInterpolator(inputBlock, numSamples);
}

void EnvelopeFollower::processSample(int channel, float sample)
{
    if (channel < 0 || channel >= static_cast<int>(envelopeStates.size()))
        return;

    // Resize envelopeStates if needed
    allocateVectors(numChannels);

    // Process a single sample through the envelope follower
    auto &envelope = envelopeStates[channel].envelope;

    if (envelope < sample)
    {
        // Attack phase
        envelope = std::min(envelope + epsilonAt * ((sample - envelope)), sample);
    }
    else if (envelope > sample)
    {
        // Release phase
        envelope = std::max(envelope + epsilonRe * ((sample - envelope)), sample);
    }
}

template <typename SampleType>
void EnvelopeFollower::gainInterpolator(const juce::dsp::AudioBlock<SampleType> &inputBlock, size_t numSamples)
{
    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        auto &envelope = envelopeStates[channel].envelope;

        float min = inputBlock.getSample(channel, 0);
        float max = min;
        for (size_t i = 0; i < numSamples; ++i)
        {
            auto sample = inputBlock.getSample(channel, i);
            if (inLevelType == juce::dsp::BallisticsFilterLevelCalculationType::RMS)
            {
                sample *= sample;
            }
            else
            {
                sample = std::abs(sample);
            }
            min = std::min(min, sample);
            max = std::max(max, sample);
        }

        if (envelope < max)
        {
            // Attack phase
            envelope = std::min(envelope + numSamples * epsilonAt * ((max - envelope)), max);
        }

        else if (envelope > max)
        {
            // Release phase
            envelope = std::max(envelope + numSamples * epsilonRe * ((max - envelope)), min);
        }

        envelopeStates[channel].rmsSamples += numSamples;
        if (inLevelType == juce::dsp::BallisticsFilterLevelCalculationType::RMS)
        {
            envelopeStates[channel].rmsSum += envelope;
        }
        else
        {
            envelopeStates[channel].rmsSum += envelope * envelope;
        }
    }
}

void EnvelopeFollower::setParameters(const Parameters &params, bool force)
{
    bool attackChanged = std::abs(inAttackMs - params.attackMs) > 0.01f;
    bool releaseChanged = std::abs(inReleaseMs - params.releaseMs) > 0.01f;
    bool levelTypeChanged = inLevelType != params.levelType;

    if (attackChanged || force)
    {
        inAttackMs = params.attackMs;
        epsilonAt = 1.0f / ((inAttackMs / 1000.0f) * sampleRate);
        setInterpolationParameters();
    }

    if (releaseChanged || force)
    {
        inReleaseMs = params.releaseMs;
        epsilonRe = 1.0f / ((inReleaseMs / 1000.0f) * sampleRate);
        setInterpolationParameters();
    }

    if (levelTypeChanged || force)
    {
        inLevelType = params.levelType;
    }
}

void EnvelopeFollower::setInterpolationParameters()
{
    attackCoef = std::exp(std::log(0.01) / (inAttackMs * sampleRate * 0.001));
    releaseCoef = std::exp(std::log(0.01) / (inReleaseMs * sampleRate * 0.001));
}

float EnvelopeFollower::getAverageLevel(int channel) const
{
    if (channel < 0 || channel >= static_cast<int>(envelopeStates.size()))
        return 0.0f;

    if (inLevelType == juce::dsp::BallisticsFilterLevelCalculationType::RMS)
    {
        auto &state = envelopeStates[channel];
        if (state.rmsSamples > 0)
        {
            return std::sqrt(state.rmsSum / state.rmsSamples);
        }
    }
    return 4.0f * envelopeStates[channel].envelope;
}

// Explicit template instantiations
template void EnvelopeFollower::process<juce::dsp::ProcessContextReplacing<float>>(const juce::dsp::ProcessContextReplacing<float> &);
template void EnvelopeFollower::process<juce::dsp::ProcessContextNonReplacing<float>>(const juce::dsp::ProcessContextNonReplacing<float> &);

template void EnvelopeFollower::gainInterpolator(const juce::dsp::AudioBlock<float> &inputBlock, size_t numSamples);