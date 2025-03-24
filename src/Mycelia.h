#pragma once

// #include <JuceHeader.h>
#include "juce_core/juce_core.h"

// #include "juce_audio_processors/processors/juce_AudioProcessor.h"
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_cryptography/juce_cryptography.h>
#include <foleys_gui_magic/foleys_gui_magic.h>

using namespace juce;

class Mycelia :
    public foleys::MagicProcessor,
    private juce::AudioProcessorValueTreeState::Listener
{
    public:
        enum WaveType
        {
            None = 0,
            Sine,
            Triangle,
            Square
        };

        //==============================================================================
        Mycelia();
        ~Mycelia() override;

        //==============================================================================

        void prepareToPlay(double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;

        void parameterChanged(const juce::String &param, float value) override;

        bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

        void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

        const juce::String getName() const override;

        bool acceptsMidi() const override;
        bool producesMidi() const override;
        bool isMidiEffect() const override;
        double getTailLengthSeconds() const override;

        int getNumPrograms() override;
        int getCurrentProgram() override;
        void setCurrentProgram(int index) override;
        const juce::String getProgramName(int index) override;
        void changeProgramName(int index, const juce::String &newName) override;

        void getStateInformation(juce::MemoryBlock &destData) override;
        void setStateInformation(const void *data, int sizeInBytes) override;

        //==============================================================================
    private:

        void setOscillator(juce::dsp::Oscillator<float> &osc, WaveType type);

        std::atomic<float> *frequency = nullptr;
        std::atomic<float> *level = nullptr;

        std::atomic<float> *lfoFrequency = nullptr;
        std::atomic<float> *lfoLevel = nullptr;

        std::atomic<float> *vfoFrequency = nullptr;
        std::atomic<float> *vfoLevel = nullptr;

        juce::dsp::Oscillator<float> mainOSC;
        juce::dsp::Oscillator<float> lfoOSC;
        juce::dsp::Oscillator<float> vfoOSC;

        juce::AudioProcessorValueTreeState treeState;

        // MAGIC GUI: this is a shorthand where the samples to display are fed to
        foleys::MagicPlotSource *oscilloscope = nullptr;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Mycelia)
};
