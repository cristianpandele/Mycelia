#include "MyceliaModel.h"
#include "Mycelia.h"

#include "dsp/InputNode.h"
#include "dsp/OutputNode.h"
#include "util/ParameterRanges.h"

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

    // Add listeners to parameters that are not processed by the Controller
    addParamListener(IDs::preampLevel, this);
    addParamListener(IDs::reverbMix, this);
    addParamListener(IDs::bandpassFreq, this);
    addParamListener(IDs::bandpassWidth, this);
    addParamListener(IDs::dryWet, this);
    addParamListener(IDs::delayDuck, this);
}

MyceliaModel::~MyceliaModel()
{
    treeState.removeParameterListener(IDs::preampLevel, this);
    treeState.removeParameterListener(IDs::reverbMix, this);
    treeState.removeParameterListener(IDs::bandpassFreq, this);
    treeState.removeParameterListener(IDs::bandpassWidth, this);
    treeState.removeParameterListener(IDs::dryWet, this);
    treeState.removeParameterListener(IDs::delayDuck, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout MyceliaModel::createParameterLayout()
{

    auto preampLevelRange   = juce::NormalisableRange<float>(0.0f, 1.2f, 0.01f);
    auto reverbMixRange     = juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f);

    auto inputLevels = std::make_unique<juce::AudioProcessorParameterGroup>("Input Levels", juce::translate("Input Levels"), "|");
    inputLevels->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::preampLevel, 1), "Preamp Level", ParameterRanges::preampLevel, 0.8f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::reverbMix, 1), "Reverb Mix", ParameterRanges::reverbMix, 0.0f));
    //
    auto inputSculpt = std::make_unique<juce::AudioProcessorParameterGroup>("Input Sculpt", juce::translate("Input Sculpt"), "|");
    inputSculpt->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::bandpassFreq, 1), "Freq", ParameterRanges::bandpassFrequency, 4000.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::bandpassWidth, 1), "Width", ParameterRanges::bandpassWidth, 0.5f));
    //
    auto trees = std::make_unique<juce::AudioProcessorParameterGroup>("Trees", juce::translate("Trees"), "|");
    trees->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::treeSize, 1), "Size", ParameterRanges::treeSize, 1.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::treeDensity, 1), "Density", ParameterRanges::treeDensity, 50.0f));
    //
    auto universeCtrls = std::make_unique<juce::AudioProcessorParameterGroup>("Universe Controls", juce::translate("Universe Controls"), "|");
    universeCtrls->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::stretch, 1), "Stretch", ParameterRanges::stretch, 100.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::abundanceScarcity, 1), "Abundance/Scarcity", ParameterRanges::abundanceScarcity, 0.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::foldPosition, 1), "Fold Position", ParameterRanges::foldPosition, 0.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::foldWindowShape, 1), "Fold Window Shape", ParameterRanges::foldWindowShape, 0.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::foldWindowSize, 1), "Fold Window Size", ParameterRanges::foldWindowSize, 0.2f));
    // TODO: Add checkboxes
    //
    auto mycelia = std::make_unique<juce::AudioProcessorParameterGroup>("Mycelia", juce::translate("Mycelia"), "|");
    mycelia->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::entanglement, 1), "Entanglement", ParameterRanges::entanglement, 50.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::growthRate, 1), "Growth Rate", ParameterRanges::growthRate, 50.0f));
    //
    auto sky = std::make_unique<juce::AudioProcessorParameterGroup>("Sky", juce::translate("Sky"), "|");
    sky->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::skyHumidity, 1), "Humidity", ParameterRanges::skyHumidity, 50.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::skyHeight, 1), "Height", ParameterRanges::skyHeight, 75.0f));
    //
    auto outputSculpt = std::make_unique<juce::AudioProcessorParameterGroup>("Output Sculpt", juce::translate("Output Sculpt"), "|");
    outputSculpt->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::dryWet, 1), "Dry/Wet", ParameterRanges::dryWet, 0.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::delayDuck, 1), "Delay Duck", ParameterRanges::delayDuck, 33.33f));

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

void MyceliaModel::parameterChanged(const juce::String &parameterID, float newValue)
{
    // Handle parameter changes here
    if (parameterID == IDs::preampLevel)
    {
        inputNode.setParameters( (InputNode::Parameters) {newValue});
    }
    else if (parameterID == IDs::reverbMix)
    {
        DBG("Reverb Mix changed to: " << newValue);
    }
    else if (parameterID == IDs::bandpassFreq)
    {
        DBG("Bandpass Frequency changed to: " << newValue);
    }
    else if (parameterID == IDs::bandpassWidth)
    {
        DBG("Bandpass Width changed to: " << newValue);
    }
    else if (parameterID == IDs::dryWet)
    {
        outputNode.setParameters( (OutputNode::Parameters) {newValue});
    }
    else if (parameterID == IDs::delayDuck)
    {
        DBG("Delay Duck changed to: " << newValue);
    }

}

void MyceliaModel::prepareToPlay(juce::dsp::ProcessSpec spec)
{
    inputNode.prepare(spec);
    outputNode.prepare(spec);
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

    inputNode.reset();
    outputNode.reset();

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
    // auto gain = juce::Decibels::decibelsToGain(/*level->load()*/ 0.0f);

    juce::dsp::AudioBlock<float> inBlock(buffer);
    juce::dsp::ProcessContextReplacing<float> inContext(inBlock);
    inputNode.process(inContext);

    // Keep "dry" signal - post input conditioning
    dryBuffer.makeCopyOf(buffer, true);

    juce::dsp::AudioBlock<float> dryBlock(dryBuffer);
    juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);
    juce::dsp::AudioBlock<float> wetBlock(buffer);
    juce::dsp::ProcessContextReplacing<float> wetContext(wetBlock);

    outputNode.process(dryContext, wetContext);

    // lfoOSC.setFrequency(*lfoFrequency);
    // vfoOSC.setFrequency(*vfoFrequency);

    // auto *channelData = buffer.getWritePointer(0);

    // for (int i = 0; i < buffer.getNumSamples(); ++i)
    // {
    //     // mainOSC.setFrequency(frequency->load() * (1.0f + vfoOSC.processSample(0.0f) * vfoLevel->load()));
    //     channelData[i] = juce::jlimit(-1.0f, 1.0f, /* mainOSC.processSample(0.0f)*/ 0 * gain * (1.0f - (/*lfoLevel->load() *   lfoOSC.processSample(0.0f) */ 0)));
    // }
}
