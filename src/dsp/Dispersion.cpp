#include "Dispersion.h"
#include "util/ParameterRanges.h"

Dispersion::Dispersion()
{
    reset();
}

void Dispersion::reset()
{
    std::fill (stageFb, &stageFb[maxNumStages + 1], 0.0f);
}

float Dispersion::processSample(float x)
{
    auto numStages = inDispersionAmount.getNextValue() * maxNumStages;
    const auto numStagesInt = static_cast<size_t>(numStages);
    float y = x;

    // process integer stages
    for (size_t stage = 0; stage < numStagesInt; ++stage)
        y = processStage(y, stage);

    // process fractional stage
    float stageFrac = numStages - numStagesInt;
    y = stageFrac * processStage(y, numStagesInt) + (1.0f - stageFrac) * y;

    // Save the allpass path output for the next call
    y1 = y;

    // Add the direct path + allpass path
    y = 0.5f * (x + y);
    // Apply the feedback path
    y = y - 0.4f * y1;

    return y;
}

float Dispersion::processStage(float x, size_t stage)
{
    float y = a[1] * x + stageFb[stage];
    stageFb[stage] = x * a[0] - y * a[1];
    return y;
}

void Dispersion::setParameters(const Parameters &params, bool force)
{
    auto dispAmtVal = (ParameterRanges::dispRange.snapToLegalValue(params.dispersionAmount) / 100.0f) * maxNumStages;
    auto dispAmtChanged = (std::abs(inDispersionAmount.getTargetValue() - dispAmtVal) / dispAmtVal > 0.01f);
    auto smoothTimeChanged = (std::abs(inSmoothTime - params.smoothTime) / params.smoothTime > 0.01f);
    auto allpassFreqChanged = (std::abs(inAllpassFreq.getTargetValue() - params.allpassFreq) / params.allpassFreq > 0.01f);

    if (smoothTimeChanged)
    {
        inSmoothTime = params.smoothTime;
    }

    if (dispAmtChanged)
    {
        if (force)
        {
            inDispersionAmount.setCurrentAndTargetValue(dispAmtVal);
        }
        else
        {
            inDispersionAmount.reset(fs, inSmoothTime);
            inDispersionAmount.setTargetValue(dispAmtVal);
        }
    }

    if (allpassFreqChanged)
    {
        if (force)
        {
            inAllpassFreq.setCurrentAndTargetValue(params.allpassFreq);
        }
        else
        {
            inAllpassFreq.reset(fs, inSmoothTime);
            inAllpassFreq.setTargetValue(params.allpassFreq);
        }
        updateFilterCoefficients(force);
    }
}

void Dispersion::prepare (const juce::dsp::ProcessSpec& spec)
{
    fs = (float) spec.sampleRate;
    inDispersionAmount.setCurrentAndTargetValue(0.0f);
    inDispersionAmount.reset (fs, inSmoothTime);
    // inDispersionAmount.setTargetValue(1000.0f);

    // updateFilterCoefficients();
    reset();
}

void Dispersion::updateFilterCoefficients(bool force)
{
    a[0] = 1.0f;
    float freq = inAllpassFreq.getNextValue();
    if (force)
    {
        freq = inAllpassFreq.getTargetValue();
    }

    float wT = juce::MathConstants<float>::twoPi * freq / fs;
    a[1] = -1.0f * wT;
}
