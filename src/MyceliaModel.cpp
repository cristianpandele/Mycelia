#include "MyceliaModel.h"
#include "Mycelia.h"

MyceliaModel::MyceliaModel(Mycelia &p)
    : treeState(p, nullptr, "PARAMETERS", MyceliaModel::createParameterLayout())
{
    preampLevel = treeState.getRawParameterValue(IDs::preampLevel);
    jassert(preampLevel != nullptr);
    reverbMix = treeState.getRawParameterValue(IDs::reverbMix);
    jassert(reverbMix != nullptr);
    //
    bandpassFreq = treeState.getRawParameterValue(IDs::bandpassFreq);
    jassert(bandpassFreq != nullptr);
    bandpassWidth = treeState.getRawParameterValue(IDs::bandpassWidth);
    jassert(bandpassWidth != nullptr);
    //
    treeSize = treeState.getRawParameterValue(IDs::treeSize);
    jassert(treeSize != nullptr);
    treeDensity = treeState.getRawParameterValue(IDs::treeDensity);
    jassert(treeDensity != nullptr);
    //
    stretch = treeState.getRawParameterValue(IDs::stretch);
    jassert(stretch != nullptr);
    abundanceScarcity = treeState.getRawParameterValue(IDs::abundanceScarcity);
    jassert(abundanceScarcity != nullptr);
    foldPosition = treeState.getRawParameterValue(IDs::foldPosition);
    jassert(foldPosition != nullptr);
    foldWindowShape = treeState.getRawParameterValue(IDs::foldWindowShape);
    jassert(foldWindowShape != nullptr);
    foldWindowSize = treeState.getRawParameterValue(IDs::foldWindowSize);
    jassert(foldWindowSize != nullptr);
    //
    entanglement = treeState.getRawParameterValue(IDs::entanglement);
    jassert(entanglement != nullptr);
    growthRate = treeState.getRawParameterValue(IDs::growthRate);
    jassert(growthRate != nullptr);
    //
    skyHumidity = treeState.getRawParameterValue(IDs::skyHumidity);
    jassert(skyHumidity != nullptr);
    skyHeight = treeState.getRawParameterValue(IDs::skyHeight);
    jassert(skyHeight != nullptr);
    //
    dryWet = treeState.getRawParameterValue(IDs::dryWet);
    jassert(dryWet != nullptr);
    delayDuck = treeState.getRawParameterValue(IDs::delayDuck);
    jassert(delayDuck != nullptr);
}

MyceliaModel::~MyceliaModel()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MyceliaModel::createParameterLayout()
{

    auto preampLevelRange   = juce::NormalisableRange<float>(0.0f, 1.2f, 0.01f);
    auto reverbMixRange     = juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f);
    //
    auto freqRange          = juce::NormalisableRange<float>{20.0f, 20000.0f, [](float start, float end, float normalised)
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
    auto bandpassWidthRange     = juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f);
    //
    auto treeSizeRange          = juce::NormalisableRange<float>(0.2f, 1.8f, 0.01f);
    auto treeDensityRange       = juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f);
    //
    auto stretchRange           = juce::NormalisableRange<float>(-80.0f, 400.0f, 0.1f);
    auto abundanceScarcityRange = juce::NormalisableRange<float>(-80.0f, 400.0f, 0.1f);
    auto foldPositionRange      = juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f);
    auto foldWindowShapeRange   = juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f);
    auto foldWindowSize         = juce::NormalisableRange<float>(0.1f, 0.8f, 0.01f);
    //
    auto entanglementRange      = juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f);
    auto growthRateRange        = juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f);
    //
    auto skyHumidityRange       = juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f);
    auto skyHeightRange         = juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f);
    //
    auto dryWetRange           = juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f);
    auto delayDuckRange        = juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f);

    auto inputLevels = std::make_unique<juce::AudioProcessorParameterGroup>("Input Levels", juce::translate("Input Levels"), "|");
    inputLevels->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::preampLevel, 1), "Preamp Level", preampLevelRange, 0.8f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::reverbMix, 1), "Reverb Mix", reverbMixRange, 0.0f));
    //
    auto inputSculpt = std::make_unique<juce::AudioProcessorParameterGroup>("Input Sculpt", juce::translate("Input Sculpt"), "|");
    inputSculpt->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::bandpassFreq, 1), "Freq", freqRange, 4000.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::bandpassWidth, 1), "Width", bandpassWidthRange, 0.5f));
    //
    auto trees = std::make_unique<juce::AudioProcessorParameterGroup>("Trees", juce::translate("Trees"), "|");
    trees->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::treeSize, 1), "Size", treeSizeRange, 1.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::treeDensity, 1), "Density", treeDensityRange, 50.0f));
    //
    auto universeCtrls = std::make_unique<juce::AudioProcessorParameterGroup>("Universe Controls", juce::translate("Universe Controls"), "|");
    universeCtrls->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::stretch, 1), "Stretch", stretchRange, 100.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::abundanceScarcity, 1), "Abundance/Scarcity", abundanceScarcityRange, 0.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::foldPosition, 1), "Fold Position", foldPositionRange, 0.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::foldWindowShape, 1), "Fold Window Shape", foldWindowShapeRange, 0.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::foldWindowSize, 1), "Fold Window Size", foldWindowSize, 0.2f));
    // TODO: Add checkboxes
    //
    auto mycelia = std::make_unique<juce::AudioProcessorParameterGroup>("Mycelia", juce::translate("Mycelia"), "|");
    mycelia->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::entanglement, 1), "Entanglement", entanglementRange, 50.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::growthRate, 1), "Growth Rate", growthRateRange, 50.0f));
    //
    auto sky = std::make_unique<juce::AudioProcessorParameterGroup>("Sky", juce::translate("Sky"), "|");
    sky->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::skyHumidity, 1), "Humidity", skyHumidityRange, 50.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::skyHeight, 1), "Height", skyHeightRange, 75.0f));
    //
    auto outputSculpt = std::make_unique<juce::AudioProcessorParameterGroup>("Output Sculpt", juce::translate("Output Sculpt"), "|");
    outputSculpt->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::dryWet, 1), "Dry/Wet", dryWetRange, 0.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::delayDuck, 1), "Delay Duck", delayDuckRange, 33.33f));

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::move(inputLevels), std::move(inputSculpt), std::move(trees), std::move(universeCtrls), std::move(mycelia), std::move(sky), std::move(outputSculpt));

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
    // juce::dsp::Oscillator<float> *osc = nullptr;
    // if (id == IDs::reverbMix)
    //     osc = &mainOSC;
    // else if (id == IDs::lfoType)
    //     osc = &lfoOSC;
    // else if (id == IDs::vfoType)
    //     osc = &vfoOSC;

    // if (osc != nullptr) // Check if the pointer is valid before using it
    // {
    //     if (type == MyceliaModel::WaveType::Sine)
    //         osc->initialise([](auto in)
    //                        { return std::sin(in); });
    //     else if (type == MyceliaModel::WaveType::Triangle)
    //         osc->initialise([](auto in)
    //                        { return in / juce::MathConstants<float>::pi; });
    //     else if (type == MyceliaModel::WaveType::Square)
    //         osc->initialise([](auto in)
    //                        { return in < 0 ? 1.0f : -1.0f; });
    //     else
    //         osc->initialise([](auto)
    //                        { return 0.0f; });
    // }
}

void MyceliaModel::prepareToPlay(juce::dsp::ProcessSpec spec)
{
    // mainOSC.prepare(spec);
    // lfoOSC.prepare(spec);
    // vfoOSC.prepare(spec);

    // for (auto type_id : {IDs::mainType, IDs::lfoType, IDs::vfoType})
    // {
    //     auto type = juce::roundToInt(treeState.getRawParameterValue(type_id)->load());
    //     setOscillator(type_id, WaveType(type));
    // }
}

void MyceliaModel::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc. (although this may never be called, depending on the
    // host's settings)
    // mainOSC.reset();
    // lfoOSC.reset();
    // vfoOSC.reset();

    // frequency = nullptr;
    // level = nullptr;
    // lfoFrequency = nullptr;
    // lfoLevel = nullptr;
    // vfoFrequency = nullptr;
    // vfoLevel = nullptr;

    preampLevel = nullptr;
    reverbMix = nullptr;
    //
    bandpassFreq = nullptr;
    bandpassWidth = nullptr;
    //
    treeSize = nullptr;
    treeDensity = nullptr;
    //
    stretch = nullptr;
    abundanceScarcity = nullptr;
    foldPosition = nullptr;
    foldWindowShape = nullptr;
    foldWindowSize = nullptr;
    //
    entanglement = nullptr;
    growthRate = nullptr;
    //
    skyHumidity = nullptr;
    skyHeight = nullptr;
    //
    dryWet = nullptr;
    delayDuck = nullptr;
}

//==============================================================================

void MyceliaModel::process(juce::AudioBuffer<float> &buffer)
{
    auto gain = juce::Decibels::decibelsToGain(/*level->load()*/ 0.0f);

    // lfoOSC.setFrequency(*lfoFrequency);
    // vfoOSC.setFrequency(*vfoFrequency);

    auto *channelData = buffer.getWritePointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        // mainOSC.setFrequency(frequency->load() * (1.0f + vfoOSC.processSample(0.0f) * vfoLevel->load()));
        channelData[i] = juce::jlimit(-1.0f, 1.0f, /* mainOSC.processSample(0.0f)*/ 0 * gain * (1.0f - (/*lfoLevel->load() *   lfoOSC.processSample(0.0f) */ 0)));
    }
}
