#pragma once

#include "BinaryData.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================

// Forward declaration of Mycelia
class Mycelia;
namespace IDs
{
    static juce::String mainType{"mainType"};
    static juce::String mainFreq{"mainfreq"};
    static juce::String mainLevel{"mainlevel"};
    static juce::String lfoType{"lfoType"};
    static juce::String lfoFreq{"lfofreq"};
    static juce::String lfoLevel{"lfolevel"};
    static juce::String vfoType{"vfoType"};
    static juce::String vfoFreq{"vfofreq"};
    static juce::String vfoLevel{"vfolevel"};

    static juce::Identifier oscilloscope{"oscilloscope"};
} // namespace IDs

class MyceliaModel
{
public:
    explicit MyceliaModel(Mycelia&);

    ~MyceliaModel();

    //==============================================================================

    enum WaveType
    {
        None = 0,
        Sine,
        Triangle,
        Square
    };
    //==============================================================================

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void getStateInformation(juce::MemoryBlock &destData);
    void setStateInformation(const void *data, int sizeInBytes);

    //==============================================================================
    void addParamListener(juce::String id, juce::AudioProcessorValueTreeState::Listener *listener);

    void setOscillator(const juce::String &id, WaveType type);

    void prepareToPlay(juce::dsp::ProcessSpec spec);

    void process(juce::AudioBuffer<float> &buffer);

    void releaseResources();

private:
    juce::AudioProcessorValueTreeState treeState;

    std::atomic<float> *frequency = nullptr;
    std::atomic<float> *level = nullptr;

    std::atomic<float> *lfoFrequency = nullptr;
    std::atomic<float> *lfoLevel = nullptr;

    std::atomic<float> *vfoFrequency = nullptr;
    std::atomic<float> *vfoLevel = nullptr;

    juce::dsp::Oscillator<float> mainOSC;
    juce::dsp::Oscillator<float> lfoOSC;
    juce::dsp::Oscillator<float> vfoOSC;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MyceliaModel)
};
