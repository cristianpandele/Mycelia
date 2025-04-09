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
    //
    addParamListener(IDs::entanglement, this);
    addParamListener(IDs::growthRate, this);
    //
    addParamListener(IDs::dryWet, this);
    addParamListener(IDs::delayDuck, this);

    // Initialize current parameter values
    currentInputParams.gainLevel = *preampLevel;
    currentInputParams.bandpassFreq = *bandpassFreq;
    currentInputParams.bandpassWidth = *bandpassWidth;

    // Initialize DelayNetwork parameters
    currentDelayNetworkParams.entanglement = *entanglement;
    currentDelayNetworkParams.growthRate = *growthRate;

    // Initialize Output parameters
    currentOutputParams.dryWetMixLevel = *dryWet;
    currentOutputParams.delayDuckLevel = *delayDuck;
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
        currentInputParams.gainLevel = newValue;
        inputNode.setParameters(currentInputParams);
    }
    else if (parameterID == IDs::reverbMix)
    {
        currentInputParams.reverbMix = newValue;
        inputNode.setParameters(currentInputParams);
    }
    else if (parameterID == IDs::bandpassFreq)
    {
        currentInputParams.bandpassFreq = newValue;
        inputNode.setParameters(currentInputParams);
    }
    else if (parameterID == IDs::bandpassWidth)
    {
        currentInputParams.bandpassWidth = newValue;
        inputNode.setParameters(currentInputParams);
    }
    //
    else if (parameterID == IDs::treeSize)
    {
        currentEdgeTreeParams.treeSize = newValue;
        edgeTree.setParameters(currentEdgeTreeParams);
    }
    //
    else if (parameterID == IDs::entanglement)
    {
        currentDelayNetworkParams.entanglement = newValue;
        delayNetwork.setParameters(currentDelayNetworkParams);
    }
    else if (parameterID == IDs::growthRate)
    {
        currentDelayNetworkParams.growthRate = newValue;
        delayNetwork.setParameters(currentDelayNetworkParams);
    }
    //
    else if (parameterID == IDs::dryWet)
    {
        currentOutputParams.dryWetMixLevel = newValue;
        outputNode.setParameters(currentOutputParams);
    }
    else if (parameterID == IDs::delayDuck)
    {
        currentOutputParams.delayDuckLevel = newValue;
        outputNode.setParameters(currentOutputParams);
    }
}

void MyceliaModel::prepareToPlay(juce::dsp::ProcessSpec spec)
{
    // Prepare all processors
    inputNode.prepare(spec);
    edgeTree.prepare(spec);
    delayNetwork.prepare(spec);
    outputNode.prepare(spec);

    // Initialize buffers
    dryBuffer.setSize(spec.numChannels, spec.maximumBlockSize);
}

void MyceliaModel::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc. (although this may never be called, depending on the
    // host's settings)

    inputNode.reset();
    edgeTree.reset();
    outputNode.reset();

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

template <typename ProcessContext>
void MyceliaModel::process(const ProcessContext &context)
{
    // Manage audio context
    const auto &inputBlock = context.getInputBlock();
    auto &outputBlock = context.getOutputBlock();
    const auto numChannels = outputBlock.getNumChannels();
    const auto numSamples = outputBlock.getNumSamples();

    jassert(inputBlock.getNumChannels() == numChannels);
    jassert(inputBlock.getNumSamples() == numSamples);

    // Copy input to output if non-replacing
    if (context.usesSeparateInputAndOutputBlocks())
    {
        outputBlock.copyFrom(inputBlock);
    }

    // Skip processing if bypassed
    if (context.isBypassed)
    {
        return;
    }

    // Process through input node
    inputNode.process(context);

    // Keep "dry" signal - post input conditioning
    dryBuffer.setSize(numChannels, numSamples, false, false, true);
    outputBlock.copyTo(dryBuffer, 0, 0, numSamples);

    // Set up processing contexts
    juce::dsp::AudioBlock<float> dryBlock(dryBuffer);
    juce::dsp::AudioBlock<float> wetBlock(context.getOutputBlock());

    juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);
    juce::dsp::ProcessContextReplacing<float> wetContext(wetBlock);

    // Process "dry" signal through EdgeTree
    edgeTree.process(wetContext);

    // Process through the DelayNetwork
    delayNetwork.process(wetContext);

    // Output mixing stage
    outputNode.process(dryContext, wetContext);
}

//==================================================
template void MyceliaModel::process<juce::dsp::ProcessContextReplacing<float>>(const juce::dsp::ProcessContextReplacing<float> &);
template void MyceliaModel::process<juce::dsp::ProcessContextNonReplacing<float>>(const juce::dsp::ProcessContextNonReplacing<float> &);