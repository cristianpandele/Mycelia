#pragma once

#include <juce_dsp/juce_dsp.h>

// Utility functions for Mycelia
namespace Utils
{
    inline void updateSmoothParameter(juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> &param,
                                      float fs, float targetValue, float rampTimeSec)
    {
        param = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>(param.getNextValue());
        param.reset(fs, rampTimeSec);
        param.setTargetValue(targetValue);
    }
}