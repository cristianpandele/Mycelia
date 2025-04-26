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
    analysisBuffer.setSize(numChannels, spec.maximumBlockSize);
    reset();
}

void EnvelopeFollower::reset()
{
    filter->reset();
    currentLevel = 0.0f;
    analysisBuffer.clear();
}

template <typename ProcessContext>
void EnvelopeFollower::process(const ProcessContext &context)
{
    // Get input block
    const auto &inputBlock = context.getInputBlock();
    const auto numSamples = inputBlock.getNumSamples();

    // Resize analysis buffer if needed
    if (analysisBuffer.getNumSamples() < numSamples)
    {
        analysisBuffer.setSize(numChannels, numSamples, true, true, true);
    }

    // Copy input data to analysis buffer
    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        analysisBuffer.copyFrom(channel, 0, inputBlock.getChannelPointer(channel), numSamples);
    }

    // Create audio block from analysis buffer
    juce::dsp::AudioBlock<float> analyzeBlock(analysisBuffer);
    juce::dsp::ProcessContextReplacing<float> analyzeContext(analyzeBlock);

    // Process the envelope follower
    filter->process(analyzeContext);

    // Calculate average level across channels
    currentLevel = 0.0f;
    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        currentLevel += analysisBuffer.getSample(channel, numSamples - 1);
    }
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
}

float EnvelopeFollower::getCurrentLevel(int channel) const
{
    if (channel < 0 || channel >= numChannels || analysisBuffer.getNumSamples() == 0)
        return 0.0f;

    return analysisBuffer.getSample(channel, analysisBuffer.getNumSamples() - 1);
}

float EnvelopeFollower::getAverageLevel() const
{
    return currentLevel;
}

// Explicit template instantiations
template void EnvelopeFollower::process<juce::dsp::ProcessContextReplacing<float>>(const juce::dsp::ProcessContextReplacing<float> &);
template void EnvelopeFollower::process<juce::dsp::ProcessContextNonReplacing<float>>(const juce::dsp::ProcessContextNonReplacing<float> &);