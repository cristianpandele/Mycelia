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
    scarcityAbundance = treeState.getRawParameterValue(IDs::scarcityAbundance);
    jassert(scarcityAbundance != nullptr);
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
    addParamListener(IDs::tempoValue, this);
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

    // Initialize Sky parameters
    currentSkyParams.humidity = *skyHumidity;
    currentSkyParams.height = *skyHeight;

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
    treeState.removeParameterListener(IDs::tempoValue, this);
    treeState.removeParameterListener(IDs::entanglement, this);
    treeState.removeParameterListener(IDs::growthRate, this);
    treeState.removeParameterListener(IDs::dryWet, this);
    treeState.removeParameterListener(IDs::delayDuck, this);

    for (auto& buffer : diffusionBandBuffers)
    {
        buffer.reset();
    }
    diffusionBandBuffers.clear();
    for (auto& buffer : delayBandBuffers)
    {
        buffer.reset();
    }
    delayBandBuffers.clear();
}

juce::AudioProcessorValueTreeState::ParameterLayout MyceliaModel::createParameterLayout()
{
    auto inputLevels = std::make_unique<juce::AudioProcessorParameterGroup>("Input Levels", juce::translate("Input Levels"), "|");
    inputLevels->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::preampLevel, 1), "Preamp Level", ParameterRanges::preampLevelRange, 0.8f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::reverbMix, 1), "Reverb Mix", ParameterRanges::reverbMixRange, 0.0f));
    //
    auto inputSculpt = std::make_unique<juce::AudioProcessorParameterGroup>("Input Sculpt", juce::translate("Input Sculpt"), "|");

    auto bandpassFreqLabel = std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(IDs::bandpassFreq, 1),
        "Freq",
        ParameterRanges::bandpassFrequencyRange,
        ParameterRanges::defaultBandpassFrequency,
        "Hz",
        juce::AudioProcessorParameter::genericParameter,
        // Custom string function to limit decimal places for all values
        [](float value, int)
        {
            return juce::String(value, 2);
        });

    auto bandpassWidthLabel = std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(IDs::bandpassWidth, 1),
        "Width",
        ParameterRanges::bandpassWidthRange,
        ParameterRanges::defaultBandpassWidth,
        "Hz",
        juce::AudioProcessorParameter::genericParameter,
        // Custom string function to limit decimal places for all values
        [](float value, int)
        {
            return juce::String(value, 2);
        });

    inputSculpt->addChild(
        std::move(bandpassFreqLabel),
        std::move(bandpassWidthLabel));
    //
    auto trees = std::make_unique<juce::AudioProcessorParameterGroup>("Trees", juce::translate("Trees"), "|");
    trees->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::treeSize, 1), "Size", ParameterRanges::treeSizeRange, 1.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::treeDensity, 1), "Density", ParameterRanges::treeDensityRange, 50.0f));
    //
    auto universeCtrls = std::make_unique<juce::AudioProcessorParameterGroup>("Universe Controls", juce::translate("Universe Controls"), "|");

    // Create the stretch parameter with a custom value string function
    auto stretchParamLabel = std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(IDs::stretch, 1),
        "Stretch",
        ParameterRanges::stretchRange,
        1.0f,
        "x",
        juce::AudioProcessorParameter::genericParameter,
        // Custom string function to display musical divisions for negative values
        // and limit decimal places for all values
        [](float value, int) {
            // For negative values (quantized musical intervals), show the rhythm label
            if (value < 0.0f) {
                float absValue = std::abs(value);

                // Find the rhythm with the closest tempo factor
                for (const auto& rhythm : TempoSyncUtils::rhythms) {
                    if (std::abs(static_cast<float>(rhythm.tempoFactor) - absValue) < 0.01f) {
                        return rhythm.getLabel();
                    }
                }

                // If no match found (unlikely), display the value with 1 decimal place
                return juce::String(value, 1);
            }
            // For positive values (continuous stretch), show with 2 decimal places
            else {
                return juce::String(value, 2);
            }
        }
    );

    universeCtrls->addChild(
        std::move(stretchParamLabel),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::tempoValue, 1), "Tempo Value", ParameterRanges::tempoValueRange, 120.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::scarcityAbundance, 1), "Scarcity/Abundance", ParameterRanges::scarcityAbundanceRange, 0.0f),
        std::make_unique<juce::AudioParameterBool>(juce::ParameterID(IDs::scarcityAbundanceOverride, 1), "Override", false),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::foldPosition, 1), "Fold Position", ParameterRanges::foldPositionRange, 0.5f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::foldWindowShape, 1), "Fold Window Shape", ParameterRanges::foldWindowShapeRange, 0.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::foldWindowSize, 1), "Fold Window Size", ParameterRanges::foldWindowSizeRange,  1.0f));
    // TODO: Add checkboxes
    //
    auto mycelia = std::make_unique<juce::AudioProcessorParameterGroup>("Mycelia", juce::translate("Mycelia"), "|");
    mycelia->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::entanglement, 1), "Entanglement", ParameterRanges::entanglementRange, 50.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::growthRate, 1), "Growth Rate", ParameterRanges::growthRateRange, 50.0f));
    //
    auto sky = std::make_unique<juce::AudioProcessorParameterGroup>("Sky", juce::translate("Sky"), "|");
    sky->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::skyHumidity, 1), "Humidity", ParameterRanges::skyHumidityRange, 50.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::skyHeight, 1), "Height", ParameterRanges::skyHeightRange, 75.0f));
    //
    auto outputSculpt = std::make_unique<juce::AudioProcessorParameterGroup>("Output Sculpt", juce::translate("Output Sculpt"), "|");
    outputSculpt->addChild(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::dryWet, 1), "Dry/Wet", ParameterRanges::dryWetRange, 0.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(IDs::delayDuck, 1), "Delay Duck", ParameterRanges::delayDuckRange, 33.33f));

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
        currentSkyParams.humidity = newValue;
        currentSkyParams.height = (1.0f - newValue);
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
    else if (parameterID == IDs::treeDensity)
    {
        currentDelayNetworkParams.treeDensity = newValue;
        delayNetwork.setParameters(currentDelayNetworkParams);
    }
    else if (parameterID == IDs::stretch)
    {
        currentDelayNetworkParams.stretch = newValue;
        delayNetwork.setParameters(currentDelayNetworkParams);
    }
    else if (parameterID == IDs::tempoValue)
    {
        currentDelayNetworkParams.tempoValue = newValue;
        delayNetwork.setParameters(currentDelayNetworkParams);
    }
    else if (parameterID == IDs::scarcityAbundance)
    {
        currentDelayNetworkParams.scarcityAbundance = newValue;
        delayNetwork.setParameters(currentDelayNetworkParams);
    }

    else if (parameterID == IDs::skyHumidity)
    {
        currentSkyParams.humidity = newValue;
        sky.setParameters(currentSkyParams);
    }
    else if (parameterID == IDs::skyHeight)
    {
        currentSkyParams.height = newValue;
        sky.setParameters(currentSkyParams);
    }

    else if (parameterID == IDs::foldPosition)
    {
        currentDelayNetworkParams.foldPosition = newValue;
        delayNetwork.setParameters(currentDelayNetworkParams);
    }
    else if (parameterID == IDs::foldWindowShape)
    {
        currentDelayNetworkParams.foldWindowShape = newValue;
        delayNetwork.setParameters(currentDelayNetworkParams);
    }
    else if (parameterID == IDs::foldWindowSize)
    {
        currentDelayNetworkParams.foldWindowSize = newValue;
        delayNetwork.setParameters(currentDelayNetworkParams);
    }
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

void MyceliaModel::setParameterExplicitly(const juce::String& paramId, float newValue)
{
    // Get the parameter and update its value
    auto* param = treeState.getParameter(paramId);
    if (param != nullptr)
    {
        // Convert normalized value if needed
        float normValue = param->convertTo0to1(newValue);
        param->setValueNotifyingHost(normValue);
    }
}

float MyceliaModel::getParameterValue(const juce::String &paramId)
{
    auto *param = treeState.getParameter(paramId);
    if (param != nullptr)
    {
        return param->getValue();
    }
    return 0.0f;
}

void MyceliaModel::prepareToPlay(juce::dsp::ProcessSpec spec)
{
    numChannels = spec.numChannels;
    blockSize = spec.maximumBlockSize;

    // Prepare all processors
    inputNode.prepare(spec);
    sky.prepare(spec);
    edgeTree.prepare(spec);
    delayNetwork.prepare(spec);
    outputNode.prepare(spec);

    // Initialize buffers
    dryBuffer.setSize(spec.numChannels, spec.maximumBlockSize);
    skyBuffer.setSize(spec.numChannels, spec.maximumBlockSize);
    allocateBandBuffers(currentDelayNetworkParams.numActiveFilterBands);
}

void MyceliaModel::allocateBandBuffers(int numBands)
{
    if (numBands < 1 || numBands > ParameterRanges::maxNutrientBands)
    {
        return;
    }

    // Check if we need to resize the buffers
    if (numBands == diffusionBandBuffers.size() &&
        numBands == delayBandBuffers.size())
    {
        // No need to resize, just return
        return;
    }

    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> newDiffusionBandBuffers;
    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> newDelayBandBuffers;

    // Clear the current buffers
    for (auto &buffer : diffusionBandBuffers)
    {
        buffer->clear();
    }
    for (auto &buffer : delayBandBuffers)
    {
        buffer->clear();
    }

    // Allocate new buffers for each band
    for (int band = 0; band < numBands; ++band)
    {
        auto newDiffusionBuffer = std::make_unique<juce::AudioBuffer<float>>(numChannels, blockSize);
        newDiffusionBandBuffers.push_back(std::move(newDiffusionBuffer));

        auto newDelayBuffer = std::make_unique<juce::AudioBuffer<float>>(numChannels, blockSize);
        newDelayBandBuffers.push_back(std::move(newDelayBuffer));
    }

    // Swap the new buffers with the old ones
    diffusionBandBuffers.swap(newDiffusionBandBuffers);
    delayBandBuffers.swap(newDelayBandBuffers);
}


void MyceliaModel::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc. (although this may never be called, depending on the
    // host's settings)

    inputNode.reset();
    sky.reset();
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
    scarcityAbundance = nullptr;
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

    // Set up processing contexts
    juce::dsp::AudioBlock<float> dryBlock(dryBuffer);
    juce::dsp::AudioBlock<float> skyBlock(skyBuffer);
    juce::dsp::AudioBlock<float> wetBlock(context.getOutputBlock());

    juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);
    juce::dsp::ProcessContextReplacing<float> skyContext(skyBlock);
    juce::dsp::ProcessContextReplacing<float> wetContext(wetBlock);

    // Allocate buffers if needed
    allocateBandBuffers(currentDelayNetworkParams.numActiveFilterBands);

    // Process through input node
    inputNode.process(context);

    // Keep "dry" signal - post input conditioning
    dryBuffer.setSize(numChannels, numSamples, false, false, true);
    dryBlock.copyFrom(outputBlock);

    // Mix in the reverb signal (with gain of 0.45f * the reverb mix parameter)
    auto reverbMix = ParameterRanges::normalizeParameter(ParameterRanges::reverbMixRange, currentInputParams.reverbMix);
    skyBlock.multiplyBy(0.45f * reverbMix);
    wetBlock.replaceWithSumOf(skyBlock, dryBlock);

    // Process "dry" (+ reverb) signal through EdgeTree
    edgeTree.process(wetContext);

    // Process through the DelayNetwork
    delayNetwork.process(wetContext, diffusionBandBuffers, delayBandBuffers);

    // Make another copy for sky processing
    skyBuffer.setSize(numChannels, numSamples, false, false, true);
    skyBlock.copyFrom(wetBlock);

    // Process through the Sky processor
    sky.process(skyContext);

    // Output mixing stage
    outputNode.process(wetContext, dryContext, diffusionBandBuffers, delayBandBuffers);
}

//==================================================
template void MyceliaModel::process<juce::dsp::ProcessContextReplacing<float>>(const juce::dsp::ProcessContextReplacing<float> &);
template void MyceliaModel::process<juce::dsp::ProcessContextNonReplacing<float>>(const juce::dsp::ProcessContextNonReplacing<float> &);