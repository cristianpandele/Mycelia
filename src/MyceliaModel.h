#pragma once

#include "BinaryData.h"
#include "dsp/InputNode.h"
#include "dsp/OutputNode.h"

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================

// Forward declaration of Mycelia
class Mycelia;
namespace IDs
{
    static juce::String preampLevel{"preamplevel"};
    static juce::String reverbMix{"reverbmix"};
    //
    static juce::String bandpassFreq{"bandpassfreq"};
    static juce::String bandpassWidth{"bandpasswidth"};
    //
    static juce::String treeSize{"treesize"};
    static juce::String treeDensity{"treedensity"};
    //
    static juce::String stretch{"stretch"};
    static juce::String abundanceScarcity{"abundancescarcity"};
    static juce::String foldPosition{"foldposition"};
    static juce::String foldWindowShape{"foldwindowshape"};
    static juce::String foldWindowSize{"foldwindowsize"};
    //
    static juce::String entanglement{"entanglement"};
    static juce::String growthRate{"growthrate"};
    //
    static juce::String skyHumidity{"skyhumidity"};
    static juce::String skyHeight{"skyheight"};
    //
    static juce::String dryWet{"drywet"};
    static juce::String delayDuck{"delayduck"};

    static juce::Identifier oscilloscope{"oscilloscope"};
} // namespace IDs

class MyceliaModel :
    private juce::AudioProcessorValueTreeState::Listener
{
    public:
        explicit MyceliaModel(Mycelia &);

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

        void parameterChanged(const juce::String &parameterID, float newValue);

        void prepareToPlay(juce::dsp::ProcessSpec spec);

        void process(juce::AudioBuffer<float> &buffer);

        void releaseResources();

    private:
        // Parameters
        juce::AudioProcessorValueTreeState treeState;

        std::atomic<float>* preampLevel = nullptr;
        std::atomic<float>* reverbMix = nullptr;
        //
        std::atomic<float>* bandpassFreq = nullptr;
        std::atomic<float>* bandpassWidth = nullptr;
        //
        std::atomic<float>* treeSize = nullptr;
        std::atomic<float>* treeDensity = nullptr;
        //
        std::atomic<float>* stretch = nullptr;
        std::atomic<float>* abundanceScarcity = nullptr;
        std::atomic<float>* foldPosition = nullptr;
        std::atomic<float>* foldWindowShape = nullptr;
        std::atomic<float>* foldWindowSize = nullptr;
        //
        std::atomic<float>* entanglement = nullptr;
        std::atomic<float>* growthRate = nullptr;
        //
        std::atomic<float>* skyHumidity = nullptr;
        std::atomic<float>* skyHeight = nullptr;
        //
        std::atomic<float>* dryWet = nullptr;
        std::atomic<float>* delayDuck = nullptr;

        // Buffers for processing
        juce::AudioBuffer<float> dryBuffer;

        // Audio Processors: Input, Delay Network, Reverb, Output
        InputNode inputNode;
        // DelayNetwork delayNetwork;
        // Reverb reverb;
        OutputNode outputNode;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MyceliaModel)
};
