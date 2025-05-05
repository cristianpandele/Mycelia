#include "DuckingCompressor.h"

DuckingCompressor::DuckingCompressor()
{
}

DuckingCompressor::~DuckingCompressor()
{
    // Clean up any resources
}

void DuckingCompressor::prepare(const juce::dsp::ProcessSpec &spec)
{
    sampleRate = spec.sampleRate;
    numChannels = spec.numChannels;

    // Initialize attack and release smoothing
    attackReleaseCalculator.prepare(spec);
    EnvelopeFollower::Parameters envParams;
    envParams.attackMs = defaultAttackTime;
    envParams.releaseMs = defaultReleaseTime;
    attackReleaseCalculator.setParameters(envParams, false);

    // Initialize state
    allocateVectors(numChannels);
}

void DuckingCompressor::allocateVectors(size_t numChannels)
{
    // Resize gainReduction if needed
    if (gainReduction.size() != numChannels)
    {
        gainReduction.resize(numChannels);
    }
}

void DuckingCompressor::reset()
{
    attackReleaseCalculator.reset();
    for (auto &gr : gainReduction)
        gr = 1.0f;
}

float DuckingCompressor::processSample(float inputSample, float sidechainLevel, size_t channel)
{
    if (channel < 0 || channel >= numChannels)
        return inputSample;
    // Resize gainReduction if needed
    allocateVectors(numChannels);

    // Check if compressor is enabled
    if (!params.enabled /* || sidechainLevel < 0.001f */)
        return inputSample;

    // Convert sidechain level to dB
    auto sidechainDb = juce::Decibels::gainToDecibels(sidechainLevel);

    // Calculate desired gain reduction amount
    auto gainReducDb = calculateGainReduction(sidechainDb);

    // Apply attack/release smoothing
    attackReleaseCalculator.processSample(channel, gainReducDb);
    auto smoothedGainReducDb = attackReleaseCalculator.getAverageLevel(channel);

    // Convert gain reduction from dB to linear and update state
    gainReduction[channel] = juce::Decibels::decibelsToGain(smoothedGainReducDb);

    // Apply gain reduction and makeup gain to input sample
    auto reducedSample = inputSample * gainReduction[channel];
    return reducedSample * juce::Decibels::decibelsToGain(params.makeupGain);
}

void DuckingCompressor::setParameters(const Parameters &newParams, bool force)
{
    auto thresholdChanged = std::abs(newParams.threshold - params.threshold) > 0.01f;
    auto ratioChanged = std::abs(newParams.ratio - params.ratio) > 0.01f;
    auto attackChanged = std::abs(newParams.attackTime - params.attackTime) > 0.01f;
    auto releaseChanged = std::abs(newParams.releaseTime - params.releaseTime) > 0.01f;
    auto kneeChanged = std::abs(newParams.kneeWidth - params.kneeWidth) > 0.01f;
    auto makeupChanged = std::abs(newParams.makeupGain - params.makeupGain) > 0.01f;
    auto enabledChanged = newParams.enabled != params.enabled;

    if (thresholdChanged || force)
        params.threshold = newParams.threshold;

    if (ratioChanged || force)
        params.ratio = juce::jlimit(1.0f, 40.0f, newParams.ratio);

    if (attackChanged || releaseChanged)
    {
        EnvelopeFollower::Parameters envParams;
        envParams.attackMs = newParams.attackTime;
        envParams.releaseMs = newParams.releaseTime;
        attackReleaseCalculator.setParameters(envParams, force);
    }

    if (kneeChanged || force)
        params.kneeWidth = juce::jlimit(0.0f, 20.0f, newParams.kneeWidth);

    if (makeupChanged || force)
        params.makeupGain = newParams.makeupGain;

    if (enabledChanged || force)
        params.enabled = newParams.enabled;
}

float DuckingCompressor::calculateGainReduction(float sidechainLevelDb)
{
    if (sidechainLevelDb <= params.threshold)
        return 0.0f; // No reduction needed

    auto overshootDb = sidechainLevelDb - params.threshold;
    auto slope = (1.0f / params.ratio) - 1.0f;

    if (params.kneeWidth <= 0.0f)
    {
        // Hard knee
        return juce::jmin(
            overshootDb * slope,
            0.0f);
    }
    else
    {
        // Soft knee
        auto kneeDbHalf = params.kneeWidth * 0.5f;

        if ((overshootDb <= kneeDbHalf) && (overshootDb > -kneeDbHalf))
        {
            // In the knee region
            return juce::jmin(
                0.5f * slope * std::pow(overshootDb + kneeDbHalf, 2.0f) / params.kneeWidth,
                0.0f);
        }

        // Above the knee
        return juce::jmin(
            overshootDb * slope,
            0.0f);
    }
}
