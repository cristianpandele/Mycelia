#include "EnvelopeFollower.h"

EnvelopeFollower::EnvelopeFollower()
{
    filter = std::make_unique<juce::dsp::BallisticsFilter<float>>();
}

void EnvelopeFollower::prepare(const juce::dsp::ProcessSpec& spec)
{
    filter->prepare(spec);
    filter->setLevelCalculationType(juce::dsp::BallisticsFilterLevelCalculationType::RMS);
    filter->setAttackTime(attackMs);
    filter->setReleaseTime(releaseMs);
    
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
void EnvelopeFollower::process(const ProcessContext& context)
{
    // Get input block
    const auto& inputBlock = context.getInputBlock();
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

void EnvelopeFollower::setParameters(const Parameters& params, bool force)
{
    bool attackChanged = std::abs(attackMs - params.attackMs) > 0.01f;
    bool releaseChanged = std::abs(releaseMs - params.releaseMs) > 0.01f;
    
    if (attackChanged || force)
    {
        attackMs = params.attackMs;
        filter->setAttackTime(attackMs);
    }
    
    if (releaseChanged || force)
    {
        releaseMs = params.releaseMs;
        filter->setReleaseTime(releaseMs);
    }
    
    filter->setLevelCalculationType(params.levelType);
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
template void EnvelopeFollower::process<juce::dsp::ProcessContextReplacing<float>>(const juce::dsp::ProcessContextReplacing<float>&);
template void EnvelopeFollower::process<juce::dsp::ProcessContextNonReplacing<float>>(const juce::dsp::ProcessContextNonReplacing<float>&);