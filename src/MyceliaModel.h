#pragma once

#include "BinaryData.h"
#include "dsp/InputNode.h"
#include "dsp/EdgeTree.h"
#include "dsp/Sky.h"
#include "dsp/OutputNode.h"
#include "dsp/DelayNetwork.h"

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
    static juce::String tempoValue{"tempovalue"};
    static juce::String scarcityAbundance{"scarcityabundance"};
    static juce::String scarcityAbundanceOverride{"scarcityabundanceoverride"};
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
    static juce::Identifier inputAnalyser{"input"};
    static juce::Identifier outputAnalyser{"output"};
    static juce::Identifier inputMeter{"inputMeter"};
    static juce::Identifier outputMeter{"outputMeter"};
    static juce::Identifier midiClockStatus{"midiClockStatus"};
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

        template <typename ProcessContext>
        void process(const ProcessContext &context);

        void releaseResources();

        // Change parameters programmatically, ensuring listeners are notified
        void setParameterExplicitly(const juce::String& paramId, float newValue);

        float getParameterValue(const juce::String &paramId);

        float getAverageScarcityAbundance() const { return delayNetwork.getAverageScarcityAbundance(); }

    private:
        size_t numChannels = 2;
        size_t blockSize = 512;

        void allocateBandBuffers(int numBands);

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
        std::atomic<float>* scarcityAbundance = nullptr;
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
        juce::AudioBuffer<float> skyBuffer;

        std::vector<std::unique_ptr<juce::AudioBuffer<float>>> diffusionBandBuffers;
        std::vector<std::unique_ptr<juce::AudioBuffer<float>>> delayBandBuffers;

        // Audio Processors: Input, Sky, EdgeTree, DelayNetwork, Output
        InputNode inputNode;
        Sky sky;
        EdgeTree edgeTree;
        DelayNetwork delayNetwork;
        OutputNode outputNode;

        // Parameters for processors
        InputNode::Parameters currentInputParams;
        Sky::Parameters currentSkyParams;
        EdgeTree::Parameters currentEdgeTreeParams;
        // Parameters for DelayNetwork
        DelayNetwork::Parameters currentDelayNetworkParams =
        {
            .numActiveFilterBands = 4,
            .stretch = 0.0f,
            .tempoValue = 120.0f,
            .scarcityAbundance = 0.0f,
            .scarcityAbundanceOverride = 0.0f,
            .foldPosition = 0.5f,
            .foldWindowShape = 1.0f,
            .foldWindowSize = 1.0f,
            .entanglement = 50.0f,
            .growthRate = 50.0f
        };
        // Parameters for OutputNode
        OutputNode::Parameters currentOutputParams =
        {
            .dryWetMixLevel = 0.0f,
            .delayDuckLevel = 0.0f,
            .numActiveBands = 4,
            .envelopeFollowerParams =
            {
                .attackMs = 200.0f,
                .releaseMs = 100.0f,
                .levelType = juce::dsp::BallisticsFilterLevelCalculationType::RMS
            },
        };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MyceliaModel)
};
