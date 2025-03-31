#include "Mycelia.h"
#include "BinaryData.h"
#include "MyceliaModel.h"
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
    myceliaModel(*this)//,
{
    FOLEYS_SET_SOURCE_PATH(__FILE__);

    myceliaModel.addParamListener(IDs::mainType, this);
    myceliaModel.addParamListener(IDs::lfoType, this);
    myceliaModel.addParamListener(IDs::vfoType, this);

    // MAGIC GUI: register an oscilloscope to display in the GUI.
    //            We keep a pointer to push samples into in processBlock().
    //            And we are only interested in channel 0
    oscilloscope = magicState.createAndAddObject<foleys::MagicOscilloscope>(IDs::oscilloscope, 0);

    magicState.setGuiValueTree(BinaryData::sporadic_xml, BinaryData::sporadic_xmlSize);

}

Mycelia::~Mycelia()
{
}

//==============================================================================
// Callback for the Parameter Listeners

void Mycelia::parameterChanged(const juce::String &param, float value)
{
    myceliaModel.setOscillator(param, MyceliaModel::WaveType(juce::roundToInt(value)));
}

//==============================================================================

const juce::String Mycelia::getName() const
{
    return JucePlugin_Name;
}

bool Mycelia::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
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
    juce::ignoreUnused(midiMessages);

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
    myceliaModel.process(buffer);

    for (int i = 1; i < getTotalNumOutputChannels(); ++i)
        buffer.copyFrom(i, 0, buffer.getReadPointer(0), buffer.getNumSamples());

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

// This creates new instances of the plugin
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new Mycelia();
}
