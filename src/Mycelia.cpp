#include "Mycelia.h"
#include "BinaryData.h"
// #include "MyceliaEditor.h"

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

static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    auto freqRange = juce::NormalisableRange<float>{20.0f, 20000.0f, [](float start, float end, float normalised)
                                                    { return start + (std::pow(2.0f, normalised * 10.0f) - 1.0f) * (end - start) / 1023.0f; },
                                                    [](float start, float end, float value)
                                                    { return (std::log(((value - start) * 1023.0f / (end - start)) + 1.0f) / std::log(2.0f)) / 10.0f; },
                                                    [](float start, float end, float value)
                                                    {
                                                        if (value > 3000.0f)
                                                            return juce::jlimit(start, end, 100.0f * juce::roundToInt(value / 100.0f));

                                                        if (value > 1000.0f)
                                                            return juce::jlimit(start, end, 10.0f * juce::roundToInt(value / 10.0f));

                                                        return juce::jlimit(start, end, float(juce::roundToInt(value)));
                                                    }};

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto generator = std::make_unique<juce::AudioProcessorParameterGroup>("Generator", TRANS("Generator"), "|");
    generator->addChild(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(IDs::mainType, 1), "Type",
                                                                     juce::StringArray("None", "Sine", "Triangle", "Square"), 1),
                        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::mainFreq, 1), "Frequency", freqRange, 440.0f),
                        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::mainLevel, 1), "Level",
                                                                    juce::NormalisableRange<float>(-100.0f, 0.0f, 1.0f), -96.0f));

    auto lfo = std::make_unique<juce::AudioProcessorParameterGroup>("lfo", TRANS("LFO"), "|");
    lfo->addChild(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(IDs::lfoType, 1), "LFO-Type", juce::StringArray("None", "Sine", "Triangle", "Square"), 0),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::lfoFreq, 1), "Frequency", juce::NormalisableRange<float>(0.25f, 10.0f), 2.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::lfoLevel, 1), "Level", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    auto vfo = std::make_unique<juce::AudioProcessorParameterGroup>("vfo", TRANS("VFO"), "|");
    vfo->addChild(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(IDs::vfoType, 1), "VFO-Type", juce::StringArray("None", "Sine", "Triangle", "Square"), 0),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::vfoFreq, 1), "Frequency", juce::NormalisableRange<float>(0.5f, 10.0f), 2.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::vfoLevel, 1), "Level", juce::NormalisableRange<float>(0.0f, 1.0), 0.0f));

    layout.add(std::move(generator), std::move(lfo), std::move(vfo));

    return layout;
}

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
    treeState (*this, nullptr, "PARAMETERS", createParameterLayout())

{
    FOLEYS_SET_SOURCE_PATH(__FILE__);

    frequency = treeState.getRawParameterValue(IDs::mainFreq);
    jassert(frequency != nullptr);
    level = treeState.getRawParameterValue(IDs::mainLevel);
    jassert(level != nullptr);

    lfoFrequency = treeState.getRawParameterValue(IDs::lfoFreq);
    jassert(lfoFrequency != nullptr);
    lfoLevel = treeState.getRawParameterValue(IDs::lfoLevel);
    jassert(lfoLevel != nullptr);

    vfoFrequency = treeState.getRawParameterValue(IDs::vfoFreq);
    jassert(vfoFrequency != nullptr);
    vfoLevel = treeState.getRawParameterValue(IDs::vfoLevel);
    jassert(vfoLevel != nullptr);

    treeState.addParameterListener(IDs::mainType, this);
    treeState.addParameterListener(IDs::lfoType, this);
    treeState.addParameterListener(IDs::vfoType, this);

    // MAGIC GUI: register an oscilloscope to display in the GUI.
    //            We keep a pointer to push samples into in processBlock().
    //            And we are only interested in channel 0
    oscilloscope = magicState.createAndAddObject<foleys::MagicOscilloscope>(IDs::oscilloscope, 0);

    magicState.setGuiValueTree(BinaryData::magic_xml, BinaryData::magic_xmlSize);

}

Mycelia::~Mycelia()
{
}

void Mycelia::setOscillator(juce::dsp::Oscillator<float> &osc, WaveType type)
{
    if (type == WaveType::Sine)
        osc.initialise([](auto in)
                       { return std::sin(in); });
    else if (type == WaveType::Triangle)
        osc.initialise([](auto in)
                       { return in / juce::MathConstants<float>::pi; });
    else if (type == WaveType::Square)
        osc.initialise([](auto in)
                       { return in < 0 ? 1.0f : -1.0f; });
    else
        osc.initialise([](auto)
                       { return 0.0f; });
}

void Mycelia::parameterChanged(const juce::String &param, float value)
{
    if (param == IDs::mainType)
        setOscillator(mainOSC, WaveType(juce::roundToInt(value)));
    else if (param == IDs::lfoType)
        setOscillator(lfoOSC, WaveType(juce::roundToInt(value)));
    else if (param == IDs::vfoType)
        setOscillator(vfoOSC, WaveType(juce::roundToInt(value)));
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

    mainOSC.prepare(spec);
    lfoOSC.prepare(spec);
    vfoOSC.prepare(spec);

    setOscillator(mainOSC, WaveType(juce::roundToInt(treeState.getRawParameterValue(IDs::mainType)->load())));
    setOscillator(lfoOSC, WaveType(juce::roundToInt(treeState.getRawParameterValue(IDs::lfoType)->load())));
    setOscillator(vfoOSC, WaveType(juce::roundToInt(treeState.getRawParameterValue(IDs::vfoType)->load())));

    // MAGIC GUI: this will setup all internals like MagicPlotSources etc.
    magicState.prepareToPlay(sampleRate, samplesPerBlock);
}

void Mycelia::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
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

    auto gain = juce::Decibels::decibelsToGain(level->load());

    lfoOSC.setFrequency(*lfoFrequency);
    vfoOSC.setFrequency(*vfoFrequency);

    auto *channelData = buffer.getWritePointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        mainOSC.setFrequency(frequency->load() * (1.0f + vfoOSC.processSample(0.0f) * vfoLevel->load()));
        channelData[i] = juce::jlimit(-1.0f, 1.0f, mainOSC.processSample(0.0f) * gain * (1.0f - (lfoLevel->load() * lfoOSC.processSample(0.0f))));
    }

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
    juce::ignoreUnused(destData);
}

void Mycelia::setStateInformation(const void *data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused(data, sizeInBytes);
}

//==============================================================================

// This creates new instances of the plugin
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new Mycelia();
}
