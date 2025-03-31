#include "MyceliaModel.h"
#include "Mycelia.h"

MyceliaModel::MyceliaModel(Mycelia &p)
    : treeState(p, nullptr, "PARAMETERS", MyceliaModel::createParameterLayout())
{
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
}

MyceliaModel::~MyceliaModel()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MyceliaModel::createParameterLayout()
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

    auto generator = std::make_unique<juce::AudioProcessorParameterGroup>("Generator", juce::translate("Generator"), "|");
    generator->addChild(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(IDs::mainType, 1), "Type",
                                                                     juce::StringArray("None", "Sine", "Triangle", "Square"), 1),
                        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::mainFreq, 1), "Frequency", freqRange, 440.0f),
                        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::mainLevel, 1), "Level",
                                                                    juce::NormalisableRange<float>(-100.0f, 0.0f, 1.0f), -96.0f));

    auto lfo = std::make_unique<juce::AudioProcessorParameterGroup>("lfo", juce::translate("LFO"), "|");
    lfo->addChild(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(IDs::lfoType, 1), "LFO-Type", juce::StringArray("None", "Sine", "Triangle", "Square"), 0),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::lfoFreq, 1), "Frequency", juce::NormalisableRange<float>(0.25f, 10.0f), 2.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::lfoLevel, 1), "Level", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    auto vfo = std::make_unique<juce::AudioProcessorParameterGroup>("vfo", juce::translate("VFO"), "|");
    vfo->addChild(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(IDs::vfoType, 1), "VFO-Type", juce::StringArray("None", "Sine", "Triangle", "Square"), 0),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::vfoFreq, 1), "Frequency", juce::NormalisableRange<float>(0.5f, 10.0f), 2.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::vfoLevel, 1), "Level", juce::NormalisableRange<float>(0.0f, 1.0), 0.0f));

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::move(generator), std::move(lfo), std::move(vfo));

    return layout;
}

void MyceliaModel::getStateInformation(juce::MemoryBlock &destData)
{
    juce::MemoryOutputStream stream(destData, true);
    treeState.state.writeToStream(stream);
}

void MyceliaModel::setStateInformation(const void *data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    treeState.state = juce::ValueTree::readFromStream(stream);
}

void MyceliaModel::addParamListener(juce::String id, juce::AudioProcessorValueTreeState::Listener *listener)
{
    treeState.addParameterListener(id, listener);
}

void MyceliaModel::setOscillator(const juce::String &id, MyceliaModel::WaveType type)
{
    juce::dsp::Oscillator<float> *osc = nullptr;
    if (id == IDs::mainType)
        osc = &mainOSC;
    else if (id == IDs::lfoType)
        osc = &lfoOSC;
    else if (id == IDs::vfoType)
        osc = &vfoOSC;

    if (osc != nullptr) // Check if the pointer is valid before using it
    {
        if (type == MyceliaModel::WaveType::Sine)
            osc->initialise([](auto in)
                           { return std::sin(in); });
        else if (type == MyceliaModel::WaveType::Triangle)
            osc->initialise([](auto in)
                           { return in / juce::MathConstants<float>::pi; });
        else if (type == MyceliaModel::WaveType::Square)
            osc->initialise([](auto in)
                           { return in < 0 ? 1.0f : -1.0f; });
        else
            osc->initialise([](auto)
                           { return 0.0f; });
    }
}

void MyceliaModel::prepareToPlay(juce::dsp::ProcessSpec spec)
{
    mainOSC.prepare(spec);
    lfoOSC.prepare(spec);
    vfoOSC.prepare(spec);

    for (auto type_id : {IDs::mainType, IDs::lfoType, IDs::vfoType})
    {
        auto type = juce::roundToInt(treeState.getRawParameterValue(type_id)->load());
        setOscillator(type_id, WaveType(type));
    }
}

void MyceliaModel::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc. (although this may never be called, depending on the
    // host's settings)
    mainOSC.reset();
    lfoOSC.reset();
    vfoOSC.reset();

    frequency = nullptr;
    level = nullptr;
    lfoFrequency = nullptr;
    lfoLevel = nullptr;
    vfoFrequency = nullptr;
    vfoLevel = nullptr;
}

void MyceliaModel::process(juce::AudioBuffer<float> &buffer)
{
    auto gain = juce::Decibels::decibelsToGain(level->load());

    lfoOSC.setFrequency(*lfoFrequency);
    vfoOSC.setFrequency(*vfoFrequency);

    auto *channelData = buffer.getWritePointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        mainOSC.setFrequency(frequency->load() * (1.0f + vfoOSC.processSample(0.0f) * vfoLevel->load()));
        channelData[i] = juce::jlimit(-1.0f, 1.0f, mainOSC.processSample(0.0f) * gain * (1.0f - (lfoLevel->load() * lfoOSC.processSample(0.0f))));
    }
}

//==============================================================================