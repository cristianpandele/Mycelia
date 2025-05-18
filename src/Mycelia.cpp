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

    // Create analyzers and meters
    oscilloscope = magicState.createAndAddObject<foleys::MagicOscilloscope>(IDs::oscilloscope);
    inputAnalyser = magicState.createAndAddObject<foleys::MagicAnalyser>(IDs::inputAnalyser);
    outputAnalyser = magicState.createAndAddObject<foleys::MagicAnalyser>(IDs::outputAnalyser);
    inputMeter = magicState.createAndAddObject<foleys::MagicLevelSource>(IDs::inputMeter);
    outputMeter = magicState.createAndAddObject<foleys::MagicLevelSource>(IDs::outputMeter);

    // Create oscilloscopes for delay bands (initially create 4 bands)
    for (int i = 0; i < 4; i++) {
        auto oscId = juce::String("delayBand") + juce::String(i);
        delayBandOscilloscopes.push_back(magicState.createAndAddObject<foleys::MagicOscilloscope>(oscId));
    }

    midiLabel.referTo(magicState.getPropertyAsValue("midiClockStatus"));
    midiLabelVisibility.referTo(magicState.getPropertyAsValue("midiClockStatusVisibility"));
    midiClockDetected.addListener(this);

    scarAbundAuto.referTo(magicState.getPropertyAsValue("scarcityAbundanceAuto"));
    scarAbundAuto.addListener(this);
    scarAbundAutoVisibility.referTo(magicState.getPropertyAsValue("scarcityAbundanceAutoVisibility"));
    scarAbundAutoVisibility.addListener(this);

    delayDuckLevel.addListener(this);
    dryWetLevel.addListener(this);

    windowSizeVal.addListener(this);
    windowShapeVal.addListener(this);
    windowPosVal.addListener(this);

    treePositionsVal.addListener(this);
    treeSizeVal.addListener(this);
    treeStretchVal.addListener(this);

    magicState.setGuiValueTree(BinaryData::sporadic_xml, BinaryData::sporadic_xmlSize);

    midiLabel.setValue("MIDI Clock Sync Inactive");
    midiLabelVisibility.setValue(true);
    midiClockDetected.setValue(false);
    scarAbundAuto.setValue("Automated");
    scarAbundAutoVisibility.setValue(true);

    startTimer(kGuiTimerId, 15);
    startTimer(kScarcityTimerId, 2000);
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
    if (param == IDs::scarcityAbundance)
    {
        scarAbundAuto.setValue("Overridden");
        scarAbundAutoVisibility.setValue(false);
    }
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
    builder.registerFactory("DuckLevelAnimation", &DuckLevelViewItem::factory);
    builder.registerFactory("FoldWindowAnimation", &FoldWindowViewItem::factory);
    builder.registerFactory("TreePositionAnimation", &TreePositionViewItem::factory);
    builder.registerFactory("NetworkGraphAnimation", &NetworkGraphViewItem::factory);

    // Save a reference to the builder for later use
    magicBuilder = &builder;
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
    oscilloscope->prepareToPlay(sampleRate, samplesPerBlock);
    inputAnalyser->prepareToPlay(sampleRate, samplesPerBlock);
    outputAnalyser->prepareToPlay(sampleRate, samplesPerBlock);

    // Prepare delay band oscilloscopes
    for (auto* oscope : delayBandOscilloscopes) {
        if (oscope != nullptr)
            oscope->prepareToPlay(sampleRate, samplesPerBlock);
    }

    // Ensure we have the right number of oscilloscopes for the current number of bands
    int numActiveBands = myceliaModel.getNumActiveFilterBands();
    while (delayBandOscilloscopes.size() < numActiveBands) {
        auto oscId = juce::String("delayBand") + juce::String(delayBandOscilloscopes.size());
        auto* newOsc = magicState.createAndAddObject<foleys::MagicOscilloscope>(oscId);
        newOsc->prepareToPlay(sampleRate, samplesPerBlock);
        delayBandOscilloscopes.push_back(newOsc);
    }

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
    {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    juce::dsp::AudioBlock<float> attBlock(buffer);
    attBlock.multiplyBy(0.5f);

    // Copy the samples to the input buffer
    inputBuffer = buffer;

    // Process audio block
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    myceliaModel.process(context);

    // Copy the processed samples to the output buffer
    outputBuffer = buffer;

    // GUI Magic: update the input/output meters and analyzers
    inputAnalyser->pushSamples(inputBuffer);
    inputMeter->pushSamples(inputBuffer);

    outputMeter->pushSamples(outputBuffer);
    outputAnalyser->pushSamples(outputBuffer);
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
    if (isMidiClockSyncActive() && (currentTime - lastMidiClockTime) > kMidiClockTimeout)
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
                midiClockDetected.setValue(false);
                midiClockCounter = 0;
            }
            // Handle MIDI CC messages
            else if (message.isController())
            {
                processMidiCcMessage(message);
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
        if (!isMidiClockSyncActive())
        {
            midiClockDetected.setValue(true);
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

void Mycelia::processMidiCcMessage(const juce::MidiMessage &midiMessage)
{
    // Get the controller number and value
    int ccNumber = midiMessage.getControllerNumber();
    int ccValue = midiMessage.getControllerValue();

    // Store values for CCs 0-13, 16, 17, 18, and 19
    switch (ccNumber)
    {
        case 0:
        {
            // CC0 mapped to Bandpass Filter Frequency
            midiCc0Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc0Value);
            auto bandpassFreqVal = ParameterRanges::denormalizeParameter(ParameterRanges::bandpassFrequencyRange, normCcValue);
            myceliaModel.setParameterExplicitly(IDs::bandpassFreq, bandpassFreqVal);
            magicState.getPropertyAsValue("bandpassFreq").setValue(bandpassFreqVal);
            break;
        }
        case 1:
        {
            // CC1 mapped to Bandpass Filter Width
            midiCc1Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc1Value);
            auto bandpassWidthVal = ParameterRanges::denormalizeParameter(ParameterRanges::bandpassWidthRange, normCcValue);
            myceliaModel.setParameterExplicitly(IDs::bandpassWidth, bandpassWidthVal);
            magicState.getPropertyAsValue("bandpassWidth").setValue(bandpassWidthVal);
            break;
        }
        case 2:
        {
            // CC2 mapped to Preamp Level
            midiCc2Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc2Value);
            auto preampLevelVal = ParameterRanges::denormalizeParameter(ParameterRanges::preampLevelRange, normCcValue);
            myceliaModel.setParameterExplicitly(IDs::preampLevel, preampLevelVal);
            magicState.getPropertyAsValue("preampLevel").setValue(preampLevelVal);
            break;
        }
        case 3:
        {
            // CC3 mapped to Reverb Mix
            midiCc3Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc3Value);
            auto reverbMixVal = ParameterRanges::denormalizeParameter(ParameterRanges::reverbMixRange, normCcValue);
            myceliaModel.setParameterExplicitly(IDs::reverbMix, reverbMixVal);
            magicState.getPropertyAsValue("reverbMix").setValue(reverbMixVal);
            myceliaModel.setParameterExplicitly(IDs::skyHumidity, reverbMixVal);
            magicState.getPropertyAsValue("skyHumidity").setValue(reverbMixVal);
            myceliaModel.setParameterExplicitly(IDs::skyHeight, (1.0f - reverbMixVal));
            magicState.getPropertyAsValue("skyHeight").setValue((1.0f - reverbMixVal));
            break;
        }
        case 4:
        {
            // CC4 mapped to Tree Size
            midiCc4Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc4Value);
            auto treeSizeVal = ParameterRanges::denormalizeParameter(ParameterRanges::treeSizeRange, normCcValue);
            myceliaModel.setParameterExplicitly(IDs::treeSize, treeSizeVal);
            magicState.getPropertyAsValue("treeSize").setValue(treeSizeVal);
            break;
        }
        case 5:
        {
            // CC5 mapped to Tree Density
            midiCc5Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc5Value);
            auto treeDensityVal = ParameterRanges::denormalizeParameter(ParameterRanges::treeDensityRange, normCcValue);
            myceliaModel.setParameterExplicitly(IDs::treeDensity, treeDensityVal);
            magicState.getPropertyAsValue("treeDensity").setValue(treeDensityVal);
            break;
        }
        case 6:
        {
            // CC6 mapped to Stretch
            midiCc6Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc6Value);
            auto stretchVal = ParameterRanges::denormalizeParameter(ParameterRanges::stretchRange, normCcValue);
            myceliaModel.setParameterExplicitly(IDs::stretch, stretchVal);
            magicState.getPropertyAsValue("stretch").setValue(stretchVal);
            break;
        }
        case 7:
        {
            // CC7 mapped to Scarcity/Abundance
            midiCc7Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc7Value);
            auto scarcityAbundanceVal = ParameterRanges::denormalizeParameter(ParameterRanges::scarcityAbundanceRange, normCcValue);
            myceliaModel.setParameterExplicitly(IDs::scarcityAbundance, scarcityAbundanceVal);
            scarAbundAuto.setValue("Overridden");
            magicState.getPropertyAsValue("scarcityAbundance").setValue(scarcityAbundanceVal);
            break;
        }
        case 8:
        {
            // CC8 mapped to Entanglement
            midiCc8Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc8Value);
            auto entanglementVal = ParameterRanges::denormalizeParameter(ParameterRanges::entanglementRange, normCcValue);
            myceliaModel.setParameterExplicitly(IDs::entanglement, entanglementVal);
            magicState.getPropertyAsValue("entanglement").setValue(entanglementVal);
            break;
        }
        case 9:
        {
            // CC9 mapped to Growth Rate
            midiCc9Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc9Value);
            auto growthRateVal = ParameterRanges::denormalizeParameter(ParameterRanges::growthRateRange, normCcValue);
            myceliaModel.setParameterExplicitly(IDs::growthRate, growthRateVal);
            magicState.getPropertyAsValue("growthRate").setValue(growthRateVal);
            break;
        }
        case 10:
        {
            // CC10 mapped to Sky Humidity
            midiCc10Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc10Value);
            auto skyHumidityVal = ParameterRanges::denormalizeParameter(ParameterRanges::skyHumidityRange, normCcValue);
            myceliaModel.setParameterExplicitly(IDs::skyHumidity, skyHumidityVal);
            magicState.getPropertyAsValue("skyHumidity").setValue(skyHumidityVal);
            break;
        }
        //
        case 11:
        {
            // CC11 mapped to Sky Humidity
            midiCc11Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc11Value);
            auto skyHumidityVal = ParameterRanges::denormalizeParameter(ParameterRanges::skyHumidityRange, normCcValue);
            myceliaModel.setParameterExplicitly(IDs::skyHumidity, skyHumidityVal);
            magicState.getPropertyAsValue("skyHumidity").setValue(skyHumidityVal);
            break;
        }
        //
        case 12:
        {
            // CC12 mapped to Dry/Wet Mix
            midiCc12Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc12Value);
            auto dryWetMixVal = ParameterRanges::denormalizeParameter(ParameterRanges::dryWetRange, normCcValue);
            myceliaModel.setParameterExplicitly(IDs::dryWet, dryWetMixVal);
            magicState.getPropertyAsValue("dryWetMix").setValue(dryWetMixVal);
            break;
        }
        //
        case 13:
        {
            // CC13 mapped to Delay Duck
            midiCc13Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc13Value);
            auto delayDuckVal = ParameterRanges::denormalizeParameter(ParameterRanges::delayDuckRange, normCcValue);
            // Save the current delay duck level for GUI updates
            myceliaModel.setParameterExplicitly(IDs::delayDuck, delayDuckVal);
            magicState.getPropertyAsValue("delayDuck").setValue(delayDuckVal);
            break;
        }
        //
        case 16:
        {
            // CC16 mapped to Fold Position
            midiCc16Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc16Value);
            auto foldPositionVal = ParameterRanges::denormalizeParameter(ParameterRanges::foldPositionRange, normCcValue);
            myceliaModel.setParameterExplicitly(IDs::foldPosition, foldPositionVal);
            magicState.getPropertyAsValue("foldPosition").setValue(foldPositionVal);
            break;
        }
        //
        case 17:
        {
            // CC17 mapped to Fold Window Shape
            midiCc17Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc17Value);
            auto windowShapeVal = ParameterRanges::denormalizeParameter(ParameterRanges::foldWindowShapeRange, normCcValue);
            myceliaModel.setParameterExplicitly(IDs::foldWindowShape, windowShapeVal);
            magicState.getPropertyAsValue("foldWindowShape").setValue(windowShapeVal);
            break;
        }
        //
        case 18:
        {
            // CC18 mapped to Fold Window Size
            midiCc18Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            auto normCcValue = ParameterRanges::normalizeParameter(ParameterRanges::midiCcValueRange, midiCc18Value);
            // Map the normalized value in opposite direction
            auto windowSizeVal = ParameterRanges::denormalizeParameter(ParameterRanges::foldPositionRange, (1.0f - normCcValue));
            myceliaModel.setParameterExplicitly(IDs::foldWindowSize, windowSizeVal);
            magicState.getPropertyAsValue("foldWindowSize").setValue(windowSizeVal);
            break;
        }
        case 19:
        {
            midiCc19Value = ParameterRanges::midiCcValueRange.snapToLegalValue(ccValue);
            // DBG("MIDI CC 19 received: " + juce::String(ccValue));
            break;
        }
        default:
            // Handle other CC messages if needed
            break;
    }
}

//==============================================================================

bool Mycelia::isMidiClockSyncActive() const
{
    // Forward the request to the DelayNetwork
    return static_cast<bool>(midiClockDetected.getValue());
}

bool Mycelia::isScarcityAbundanceOverridden() const
{
    // Check if the scarcity/abundance parameter is overridden
    return static_cast<bool>(scarAbundOverridden.getValue());
}

void Mycelia::valueChanged(juce::Value &value)
{
    if (value == midiClockDetected)
    {
        // Update the MIDI clock sync status property using the ValueTree API
        if (isMidiClockSyncActive())
        {
            midiLabel.setValue("MIDI Clock Sync On");
            midiLabelVisibility.setValue(true);
        }
        else
        {
            midiLabel.setValue("MIDI Clock Sync Off");
            midiLabelVisibility.setValue(true);
        }

        // Update the GUI to reflect the MIDI clock sync status
        auto tree = magicBuilder->getGuiRootNode();
        auto id = foleys::IDs::caption;

        // Set the background colour of the label
        auto val = juce::String("Mycelial Delay Controls");
        auto child = tree.getChildWithProperty(id, val);

        if (child.isValid())
        {
            val = juce::String("Universe Controls");
            child = child.getChildWithProperty(id, val);

            if (child.isValid())
            {
                id = juce::Identifier("id");
                val = juce::String("MIDI Sync");
                child = child.getChildWithProperty(id, val);

                if (child.isValid())
                {
                    if (isMidiClockSyncActive())
                    {
                        child.setProperty(foleys::IDs::backgroundColour, "FF008800", nullptr);
                    }
                    else
                    {
                        child.setProperty(foleys::IDs::backgroundColour, "FFFF8800", nullptr);
                    }
                }
            }
        }
    }

    if ((value == delayDuckLevel) || (value == dryWetLevel))
    {
        // Update the GUI to reflect the delay duck level
        auto tree = magicBuilder->getGuiRootNode();
        auto id = foleys::IDs::caption;

        // Set the background colour of the label
        auto val = juce::String("XY Controls");
        auto child = tree.getChildWithProperty(id, val);

        if (child.isValid())
        {
            val = juce::String("Output Sculpt");
            child = child.getChildWithProperty(id, val);

            if (child.isValid())
            {
                id = juce::Identifier("title");
                val = juce::String("Delay Duck Level");
                child = child.getChildWithProperty(id, val);

                if (child.isValid())
                {
                    if (value == delayDuckLevel)
                    {
                        child.setProperty("duckLevel", value.getValue(), nullptr);
                    }
                    else if (value == dryWetLevel)
                    {
                        child.setProperty("dryWetLevel", value.getValue(), nullptr);
                    }
                }
            }
        }
    }

    // Update the GUI to handle tree positions and size
    if ((value == treePositionsVal) || (value == treeSizeVal) || (value == treeStretchVal))
    {
        updateTreePositionInfo();
    }

    if (value == scarAbundAuto)
    {
        scarAbundOverridden.setValue(true);
        updateScarcityAbundanceLabel();
        }
    if ((value == windowSizeVal) || (value == windowShapeVal) || (value == windowPosVal))
    {
        // Update the GUI to reflect the fold window size
        auto tree = magicBuilder->getGuiRootNode();
        auto id = foleys::IDs::caption;

        // Set the background colour of the label
        auto val = juce::String("XY Controls");
        auto child = tree.getChildWithProperty(id, val);

        if (child.isValid())
        {
            id = juce::Identifier("title");
            val = juce::String("Fold XY");
            child = child.getChildWithProperty(id, val);

            if (child.isValid())
            {
                val = juce::String("Fold Window Display");
                child = child.getChildWithProperty(id, val);

                if (child.isValid())
                {
                    if (value == windowSizeVal)
                    {
                        child.setProperty("windowSize", value.getValue(), nullptr);
                    }
                    else if (value == windowShapeVal)
                    {
                        child.setProperty("windowShape", value.getValue(), nullptr);
                    }
                    else if (value == windowPosVal)
                    {
                        child.setProperty("windowPos", value.getValue(), nullptr);
                    }
                }
            }
        }
    }
}

void Mycelia::timerCallback(const int timerID)
{
    if (timerID == kGuiTimerId)
    {
        // Get the current delay duck and dry/wet level (valueChanged() will trigger updating the GUI)
        delayDuckLevel.setValue(myceliaModel.getParameterValue(IDs::delayDuck));
        dryWetLevel.setValue(myceliaModel.getParameterValue(IDs::dryWet));

        // Get the current fold window parameters (valueChanged() will trigger updating the GUI)
        windowSizeVal.setValue(myceliaModel.getParameterValue(IDs::foldWindowSize));
        windowShapeVal.setValue(myceliaModel.getParameterValue(IDs::foldWindowShape));
        windowPosVal.setValue(myceliaModel.getParameterValue(IDs::foldPosition));

        //////////////
        // Get the current band states
        auto& bandStates = myceliaModel.getBandStates();

        // Update the delay band oscilloscopes
        for (int i = 0; i < delayBandOscilloscopes.size(); ++i)
        {
            if (i < bandStates.size())
            {
                auto* oscope = delayBandOscilloscopes[i];
                if (oscope != nullptr)
                {
                    // Get the current band state and push it to the corresponding oscilloscope
                    auto& bandState = bandStates[i];
                    oscope->pushSamples(*bandState.processorBuffers[0]);
                }
            }
        }

        // Update network graph animation with current band states
        if (auto* item = magicBuilder->findGuiItemWithId("networkGraphId"))
        {
            if (item != nullptr)
            {
                auto *networkGraph = dynamic_cast<NetworkGraphAnimation *>(item->getWrappedComponent());
                // Update the network graph with the current band states
                if (networkGraph != nullptr)
                {
                    networkGraph->setStretch(stretchLevel);
                    networkGraph->setBandStates(bandStates);
                }
            }
        }

        //////////////
        // Get the tree positions and push them to the GUI
        auto& treePos = myceliaModel.getTreePositions();

        // Convert tree positions to a comma-separated string
        juce::String treePositionsStr;
        for (int i = 0; i < treePos.size(); ++i)
        {
            treePositionsStr += juce::String(treePos[i]);
            if (i < treePos.size() - 1)
                treePositionsStr += ",";
        }

        // Update the tree positions value (valueChanged() will trigger updating the GUI)
        treePositionsVal.setValue(treePositionsStr);

        // Update the tree size
        treeSizeVal.setValue(myceliaModel.getParameterValue(IDs::treeSize));

        // Update the tree stretch
        treeStretchVal.setValue(myceliaModel.getParameterValue(IDs::stretch));

        /////////////
        // MAGIC GUI: push the input samples to be displayed in the output sculpt visualization
        // Make a copy of the output buffer and normalize it to at most 10% of dynamic range before sending it to the oscilloscope
        juce::AudioBuffer<float> oscilloscopeBuffer = outputBuffer;

        auto bufferRange = oscilloscopeBuffer.findMinMax(0, 0, oscilloscopeBuffer.getNumSamples());
        float normFactor = 0.0f;
        if (bufferRange.getEnd() < 0.001f)
        {
            normFactor = 1.0f;
        }
        else
        {
            normFactor = 0.1f / bufferRange.getEnd();
        }

        juce::dsp::AudioBlock<float> oscilloscopeBlock(oscilloscopeBuffer);
        oscilloscopeBlock.multiplyBy(normFactor);

        // Add the value of the dry/wet mix (scaled to 0.15-0.9) to the output block for visualization
        auto dryWetMix = myceliaModel.getParameterValue(IDs::dryWet);
        const juce::NormalisableRange<float> waterLevelRange(0.15f, 0.9f, 0.01f);
        dryWetMix = ParameterRanges::denormalizeParameter(waterLevelRange, dryWetMix);
        dryWetMix = ParameterRanges::denormalizeParameter(ParameterRanges::dryWetRange, dryWetMix);
        oscilloscopeBlock.replaceWithSumOf(oscilloscopeBlock, dryWetMix);

        // MAGIC GUI: push the output samples to be displayed
        oscilloscope->pushSamples(oscilloscopeBuffer);
    }
    else if (timerID == kScarcityTimerId)
    {
        if (!isScarcityAbundanceOverridden())
        {
            // Reset the scarcity/abundance parameter to its default value
            auto scarAbundanceVal = myceliaModel.getAverageScarcityAbundance();
            if (std::abs(scarAbundanceVal - myceliaModel.getParameterValue(IDs::scarcityAbundance)) > 0.01f)
            {
                // Update the GUI to reflect the scarcity/abundance level
                if (auto *item = magicBuilder->findGuiItemWithId("scarabundid"))
                {
                    if (auto *slider = dynamic_cast<juce::Slider *>(item->getWrappedComponent()))
                    {
                        // DBG("Slider value: " + juce::String(slider->getValue()));
                        if (scarAbundanceVal)
                        {
                            scarAbundanceVal = ParameterRanges::scarcityAbundanceRange.snapToLegalValue(scarAbundanceVal);
                            slider->setValue(scarAbundanceVal, juce::dontSendNotification);
                        }
                    }
                }
            }
        }
        else
        {
            // Reset the scarcity/abundance parameter to its set value
            scarAbundAuto.setValue("Automated");
            scarAbundOverridden.setValue(false);
        }
        // Update the scarcity/abundance label
        updateScarcityAbundanceLabel();
    }
}

void Mycelia::updateTreePositionInfo()
{
    // Update the GUI to reflect the tree positions and size
    auto tree = magicBuilder->getGuiRootNode();
    auto id = foleys::IDs::caption;

    // Search for the component that displays tree positions
    auto val = juce::String("XY Controls");
    auto child = tree.getChildWithProperty(id, val);

    if (child.isValid())
    {
        id = juce::Identifier("title");
        val = juce::String("Fold XY");
        child = child.getChildWithProperty(id, val);

        if (child.isValid())
        {
            val = juce::String("Tree Display");
            child = child.getChildWithProperty(id, val);

            if (child.isValid())
            {
                child.setProperty("treePositions", treePositionsVal.getValue(), nullptr);
                child.setProperty("treeSize", treeSizeVal.getValue(), nullptr);
                child.setProperty("stretch", treeStretchVal.getValue(), nullptr);
            }
        }
    }
}

void Mycelia::updateScarcityAbundanceLabel()
{
    // Update the GUI label to reflect the scarcity/abundance state
    auto tree = magicBuilder->getGuiRootNode();
    auto id = foleys::IDs::caption;

    // Set the background colour of the label
    auto val = juce::String("Mycelial Delay Controls");
    auto child = tree.getChildWithProperty(id, val);

    if (child.isValid())
    {
        val = juce::String("Universe Controls");
        child = child.getChildWithProperty(id, val);

        if (child.isValid())
        {
            id = juce::Identifier("id");
            val = juce::String("Scar/Abundance Automation");
            child = child.getChildWithProperty(id, val);

            if (child.isValid())
            {
                if (isScarcityAbundanceOverridden())
                {
                    child.setProperty(foleys::IDs::backgroundColour, "FFFF8800", nullptr);
                }
                else
                {
                    child.setProperty(foleys::IDs::backgroundColour, "FF008800", nullptr);
                }
            }
        }
    }
}

//==============================================================================

// This creates new instances of the plugin
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new Mycelia();
}
