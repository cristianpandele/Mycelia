#include "Mycelia.h"
#include "BinaryData.h"
#include "MyceliaModel.h"
#include "MyceliaView.h"
#include "foleys_gui_magic/General/foleys_MagicGUIBuilder.h"

//==============================================================================
Mycelia::Mycelia()
    : foleys::MagicProcessor (juce::AudioProcessor::BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
    myceliaModel(*this)
{
    FOLEYS_SET_SOURCE_PATH(RES_FOLDER_PATH);

    myceliaModel.addParamListener(IDs::treeSize, this);
    myceliaModel.addParamListener(IDs::treeDensity, this);
    //
    myceliaModel.addParamListener(IDs::stretch, this);
    myceliaModel.addParamListener(IDs::scarcityAbundance, this);
    myceliaModel.addParamListener(IDs::foldPosition, this);
    myceliaModel.addParamListener(IDs::foldWindowShape, this);
    myceliaModel.addParamListener(IDs::foldWindowSize, this);
    //
    myceliaModel.addParamListener(IDs::entanglement, this);
    myceliaModel.addParamListener(IDs::growthRate, this);
    //
    myceliaModel.addParamListener(IDs::skyHumidity, this);
    myceliaModel.addParamListener(IDs::skyHeight, this);

    // MAGIC GUI: register an oscilloscope to display in the GUI.
    //            We keep a pointer to push samples into in processBlock().
    //            And we are only interested in channel 0
    oscilloscope = magicState.createAndAddObject<foleys::MagicOscilloscope>(IDs::oscilloscope, 0);

    inputMeter = magicState.createAndAddObject<foleys::MagicLevelSource>(IDs::inputMeter);
    outputMeter = magicState.createAndAddObject<foleys::MagicLevelSource>(IDs::outputMeter);

    // Start the timer to update MIDI clock sync status
    midiClockStatusUpdater.startTimerHz(1); // Update 1 time per second

    magicState.setGuiValueTree(BinaryData::sporadic_xml, BinaryData::sporadic_xmlSize);
}

Mycelia::~Mycelia()
{
}

//==============================================================================
// Callback for the Parameter Listeners

void Mycelia::parameterChanged(const juce::String &param, float value)
{
    // Pass the parameter change to the model
    myceliaModel.parameterChanged(param, value);
    // TODO: pass the parameter change to the GUI
}

//==============================================================================

const juce::String Mycelia::getName() const
{
    return JucePlugin_Name;
}

bool Mycelia::acceptsMidi() const
{
    // Always accept MIDI input to support MIDI clock sync
    return true;
}

bool Mycelia::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool Mycelia::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double Mycelia::getTailLengthSeconds() const
{
    return 0.0;
}

int Mycelia::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int Mycelia::getCurrentProgram()
{
    return 0;
}

void Mycelia::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String Mycelia::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void Mycelia::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void Mycelia::initialiseBuilder(foleys::MagicGUIBuilder &builder)
{
    builder.registerJUCEFactories();
    builder.registerJUCELookAndFeels();

    // Register your custom GUI components here
    builder.registerFactory("MyceliaAnimation", &MyceliaViewItem::factory);
}

//==============================================================================

void Mycelia::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    const auto numChannels = getTotalNumOutputChannels();

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = juce::uint32(samplesPerBlock);
    spec.numChannels = juce::uint32(numChannels);

    inputMeter->setNumChannels(numChannels);
    outputMeter->setNumChannels(numChannels);
    myceliaModel.prepareToPlay(spec);

    // MAGIC GUI: this will setup all internals like MagicPlotSources etc.
    magicState.prepareToPlay(sampleRate, samplesPerBlock);
}

void Mycelia::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    myceliaModel.releaseResources();
}

bool Mycelia::isBusesLayoutSupported(const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}

void Mycelia::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    // Process MIDI messages
    processMidiMessages(midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Process audio block
    inputMeter->pushSamples(buffer);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    myceliaModel.process(context);

    for (int i = 1; i < totalNumOutputChannels; ++i)
    {
        buffer.copyFrom(i, 0, buffer.getReadPointer(0), buffer.getNumSamples());
    }
    outputMeter->pushSamples(buffer);

    // MAGIC GUI: push the samples to be displayed
    oscilloscope->pushSamples(buffer);
}

//==============================================================================

void Mycelia::getStateInformation(juce::MemoryBlock &destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    myceliaModel.getStateInformation(destData);
}

void Mycelia::setStateInformation(const void *data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    myceliaModel.setStateInformation(data, sizeInBytes);
}

//==============================================================================

void Mycelia::processMidiMessages(const juce::MidiBuffer &midiMessages)
{
    // Get the current time in seconds
    const double currentTime = juce::Time::getMillisecondCounterHiRes() * 0.001;

    // Check for MIDI clock timeout
    if (midiClockDetected && (currentTime - lastMidiClockTime) > kMidiClockTimeout)
    {
        midiClockDetected = false;
        midiClockCounter = 0;
        myceliaModel.setParameterExplicitly(IDs::tempoValue, kDefaultTempo);
    }

    // Forward MIDI messages to the DelayNetwork for MIDI clock sync processing
    if (!midiMessages.isEmpty())
    {
        for (const auto metadata : midiMessages)
        {
            const auto message = metadata.getMessage();

            // Check for MIDI clock messages
            if (message.isMidiClock())
            {
                processMidiClockMessage(message, currentTime);
            }
            // Also handle MIDI start/stop messages for transport control
            else if (message.isMidiStart() || message.isMidiContinue())
            {
                // Reset counter when transport starts/continues
                midiClockCounter = 0;
            }
            else if (message.isMidiStop())
            {
                // Stop tracking when transport stops
                midiClockDetected = false;
                midiClockCounter = 0;
            }
        }
    }
}

void Mycelia::processMidiClockMessage(const juce::MidiMessage &midiMessage, double currentTime)
{
    // Calculate tempo from the timing of MIDI clock messages
    midiClockCounter++;

    if (midiClockCounter >= kClockCountReset)
    {
        if (!midiClockDetected)
        {
            midiClockDetected = true;
            // First complete set of clock messages received, start tracking from here
        }
        else
        {
            // Calculate tempo based on time it took to receive 24 MIDI clock messages
            midiClockTempo = 60.0 / (currentTime - lastMidiClockTime);
        }
        midiClockCounter = 0;
        lastMidiClockTime = currentTime;
    }
}

bool Mycelia::isMidiClockSyncActive() const
{
    // Forward the request to the DelayNetwork
    return midiClockDetected;
}

void Mycelia::updateMidiClockSyncStatus()
{
    // Update the MIDI clock status property using the ValueTree API
    bool isMidiClockActive = isMidiClockSyncActive();
    if (isMidiClockActive)
    {
        magicState.getPropertyAsValue("midiClockStatus").setValue("MIDI Clock Sync Active");
    }
    else
    {
        magicState.getPropertyAsValue("midiClockStatus").setValue(isMidiClockActive);
    }
}

//==============================================================================

// This creates new instances of the plugin
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new Mycelia();
}
