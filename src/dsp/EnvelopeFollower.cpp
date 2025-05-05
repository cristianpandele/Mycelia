#include "EnvelopeFollower.h"

EnvelopeFollower::EnvelopeFollower()
{
    filter = std::make_unique<juce::dsp::BallisticsFilter<float>>();
}

void EnvelopeFollower::prepare(const juce::dsp::ProcessSpec &spec)
{
    filter->prepare(spec);
    filter->setLevelCalculationType(inLevelType);
    filter->setAttackTime(inAttackMs);
    filter->setReleaseTime(inReleaseMs);

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
        analysisBuffer->setSize(numChannels, numSamples, true, true, true);
    }

    // Copy input data to analysis buffer
    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        analysisBuffer->copyFrom(channel, 0, inputBlock.getChannelPointer(channel), numSamples);
    }

    // Create audio block from analysis buffer
    juce::dsp::AudioBlock<float> analyzeBlock(*analysisBuffer);
    juce::dsp::ProcessContextReplacing<float> analyzeContext(analyzeBlock);

    // Process the envelope follower
    filter->process(analyzeContext);

    // Calculate average level across channels
    currentLevel = 0.0f;
    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            currentLevel += analysisBuffer->getSample(channel, sample);
        }
    }
    currentLevel /= static_cast<float>(numSamples);
    currentLevel /= static_cast<float>(numChannels);
}

float EnvelopeFollower::processSample(int ch, float sample)
{
    // Process a single sample through the envelope follower
    filter->processSample(ch, sample);

    return sample;
}

void EnvelopeFollower::setParameters(const Parameters &params, bool force)
{
    bool attackChanged = std::abs(inAttackMs - params.attackMs) > 0.01f;
    bool releaseChanged = std::abs(inReleaseMs - params.releaseMs) > 0.01f;
    bool levelTypeChanged = inLevelType != params.levelType;

    if (attackChanged || force)
    {
        inAttackMs = params.attackMs;
        filter->setAttackTime(inAttackMs);
    }

    if (releaseChanged || force)
    {
        inReleaseMs = params.releaseMs;
        filter->setReleaseTime(inReleaseMs);
    }

    if (levelTypeChanged || force)
    {
        inLevelType = params.levelType;
        filter->setLevelCalculationType(inLevelType);
    }

    filter->snapToZero();
}

float EnvelopeFollower::getCurrentLevel(int channel) const
{
    if (channel < 0 || channel >= numChannels || analysisBuffer->getNumSamples() == 0)
        return 0.0f;

    return analysisBuffer->getSample(channel, analysisBuffer->getNumSamples() - 1);
}

float EnvelopeFollower::getAverageLevel() const
{
    return currentLevel;
}

// Explicit template instantiations
template void EnvelopeFollower::process<juce::dsp::ProcessContextReplacing<float>>(const juce::dsp::ProcessContextReplacing<float> &);
template void EnvelopeFollower::process<juce::dsp::ProcessContextNonReplacing<float>>(const juce::dsp::ProcessContextNonReplacing<float> &);