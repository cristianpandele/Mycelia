#pragma once

#include "MyceliaModel.h"

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
    private juce::AudioProcessorValueTreeState::Listener,
    private juce::Value::Listener
{
    public:
        Mycelia();
        ~Mycelia() override;

        //==============================================================================
        void initialiseBuilder(foleys::MagicGUIBuilder &builder) override;
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
        // MAGIC GUI: this is a shorthand where the samples to display are fed to
        foleys::MagicPlotSource *oscilloscope = nullptr;
        foleys::MagicAnalyser *inputAnalyser = nullptr;
        foleys::MagicAnalyser *outputAnalyser = nullptr;
        foleys::MagicLevelSource *inputMeter = nullptr;
        foleys::MagicLevelSource *outputMeter = nullptr;

        foleys::MagicGUIBuilder *magicBuilder = nullptr;

        /////////////////////////////////////////////
        // MIDI

        juce::Value midiLabel{"MIDI Clock Sync Inactive"};
        juce::Value midiLabelVisibility{true};
        juce::Value midiClockDetected{false};

        juce::Value scarAbundAuto{"Automated"};
        juce::Value scarAbundAutoVisibility{true};

        juce::Value delayDuckLevel{0.0f};
        juce::Value dryWetLevel{0.0f};

        void updateMidiClockSyncStatus();
        void valueChanged(juce::Value &value);

        // Process MIDI messages
        void processMidiMessages(const juce::MidiBuffer &midiMessages);
        // Process MIDI messages for MIDI clock sync
        void processMidiClockMessage(const juce::MidiMessage &midiMessage, double currentTime);
        // Process MIDI CC messages
        void processMidiCcMessage(const juce::MidiMessage &midiMessage);

        // Check if MIDI clock sync is active
        bool isMidiClockSyncActive() const;

        // MIDI Clock sync variables
        double midiClockTempo = 0.0;
        double lastMidiClockTime = 0.0;
        int midiClockCounter = 0;
        static constexpr int kDefaultTempo = 100;         // Default tempo in BPM
        static constexpr int kClockCountReset = 24;       // MIDI sends 24 clock messages per quarter note
        static constexpr double kMidiClockTimeout = 15.0; // Reset MIDI clock detection after 15 seconds of no messages

        // MIDI CC values
        int midiCC16Value = 0;
        int midiCC17Value = 0;
        int midiCC18Value = 0;
        int midiCC19Value = 0;

        //////////////////////////////////////////////
        // GUI variables
        // juce::Value xyRadiusValue{0.0f};

        //////////////////////////////////////////////
        // The underlying model used to perform the DSP processing
        MyceliaModel myceliaModel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Mycelia)
};
