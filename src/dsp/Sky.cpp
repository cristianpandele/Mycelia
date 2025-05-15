#include "Sky.h"
#include "util/ParameterRanges.h"

Sky::Sky()
{
    // Initialize reverb parameters
    reverbParams[ReverbParams::PREDELAY]   = -4.0f;    // Default pre-delay
    reverbParams[ReverbParams::ROOM_SIZE]  = 0.6f;     // Large room size
    reverbParams[ReverbParams::DECAY_TIME] = 0.75f;    // Default decay time
    reverbParams[ReverbParams::DIFFUSION]  = 1.0f;     // Full diffusion
    reverbParams[ReverbParams::BUILDUP]    = 1.0f;     // Full buildup
    reverbParams[ReverbParams::MODULATION] = 0.5f;     // Medium modulation
    reverbParams[ReverbParams::LF_DAMPING] = 0.2f;     // Some LF damping
    reverbParams[ReverbParams::HF_DAMPING] = 0.35f;    // Some HF damping

    // Initialize the reverb effect
    reverb = std::make_unique<sst::voice_effects::liftbus::LiftedReverb2<VFXConfig>>();
    // Manual parameter initialization - we'll set them directly in paramStorage
    // since there's an issue with setFloatParam
    reverb->initVoiceEffect();
    startTimer(500); // Start the timer for parameter updates
}

Sky::~Sky()
{
    // Ensure proper cleanup of both effects
    reverb.reset();
}

void Sky::prepare(const juce::dsp::ProcessSpec &spec)
{
    fs = static_cast<float>(spec.sampleRate);

    // Initialize the process buffer
    processBuffer.setSize(spec.numChannels, VFXConfig::blockSize);

    // Re-initialize the reverb effect with new sample rate
    reverb = std::make_unique<sst::voice_effects::liftbus::LiftedReverb2<VFXConfig>>();
    // Manual parameter initialization - we'll set them directly in the reverb object
    // through the VFXConfig::setFloatParam method
    reverb->initVoiceEffect();
}

void Sky::reset()
{
    // Suspend processing for the reverb
    reverb->initVoiceEffect(); // Re-initialize the reverb
}

template <typename ProcessContext>
void Sky::process(const ProcessContext &context)
{
    const auto &inputBlock = context.getInputBlock();
    auto &outputBlock = context.getOutputBlock();
    const auto numChannels = inputBlock.getNumChannels();
    const auto numSamples = inputBlock.getNumSamples();

    // Handle bypass
    if (context.isBypassed)
    {
        if (context.usesSeparateInputAndOutputBlocks())
        {
            outputBlock.copyFrom(inputBlock);
        }
        return;
    }

    // Copy input to output if non-replacing
    if (context.usesSeparateInputAndOutputBlocks())
    {
        outputBlock.copyFrom(inputBlock);
    }

    // Process the audio through the Reverb in blocks of VFXConfig::blockSize
    for (size_t pos = 0; pos < numSamples; pos += VFXConfig::blockSize)
    {
        // Calculate how many samples to process in this iteration
        size_t blockSize = std::min((size_t)VFXConfig::blockSize, numSamples - pos);

        if (numChannels >= 2)
        {
            // Copy input audio to process buffer
            for (size_t i = 0; i < blockSize; ++i)
            {
                processBuffer.setSample(0, i, inputBlock.getSample(0, pos + i));
                processBuffer.setSample(1, i, inputBlock.getSample(1, pos + i));
            }

            // Process through Reverb
            reverb->processStereo(
                processBuffer.getReadPointer(0),
                processBuffer.getReadPointer(1),
                processBuffer.getWritePointer(0),
                processBuffer.getWritePointer(1),
                0.0f); // No pitch modulation

            // Copy processed audio back to output
            for (size_t i = 0; i < blockSize; ++i)
            {
                outputBlock.setSample(0, pos + i, processBuffer.getSample(0, i));
                outputBlock.setSample(1, pos + i, processBuffer.getSample(1, i));
            }
        }
    }
}

void Sky::setParameters(const Parameters &params)
{
    // Map humidity (0-100) to density (0-1) and texture (0-1)
    if (std::abs(inHumidity - params.humidity) > 0.01f)
    {
        inHumidity = params.humidity;
        humidityChanged = true;
    }

    // Map height (0-100) to position (0-1) and pitch (-0.5 to 0.5)
    if (std::abs(inHeight - params.height) > 0.01f)
    {
        inHeight = params.height;
        heightChanged = true;
    }
}

void Sky::timerCallback()
{
    if (humidityChanged)
    {
        float normalizedHumidity = ParameterRanges::normalizeParameter(ParameterRanges::skyHumidityRange, inHumidity);
        // Map humidity to diffusion (0.65-1)
        static auto diffusionRange = juce::NormalisableRange<float>(0.65f, 1.0f, 0.01f);
        auto diffusionVal = ParameterRanges::denormalizeParameter(diffusionRange, normalizedHumidity);
        reverbParams[ReverbParams::DIFFUSION] = diffusionVal; // Higher humidity = more diffusion

        // Map humidity to decay time (-4 to 1)
        static auto decayTimeRange = juce::NormalisableRange<float>(-4.0f, 1.0f, 0.01f);
        auto decayTimeVal = ParameterRanges::denormalizeParameter(decayTimeRange, normalizedHumidity);
        reverbParams[ReverbParams::DECAY_TIME] = decayTimeVal; // Higher humidity = longer decay

        // Map humidity to buildup (0.9 to 1.0)
        static auto buildupRange = juce::NormalisableRange<float>(0.9f, 1.0f, 0.01f);
        auto buildupVal = ParameterRanges::denormalizeParameter(buildupRange, normalizedHumidity);
        reverbParams[ReverbParams::BUILDUP] = buildupVal; // Higher humidity = more buildup

        // Update reverb parameters
        VFXConfig::setFloatParam(reverb.get(), ReverbParams::DIFFUSION, reverbParams[ReverbParams::DIFFUSION]);
        VFXConfig::setFloatParam(reverb.get(), ReverbParams::DECAY_TIME, reverbParams[ReverbParams::DECAY_TIME]);
        VFXConfig::setFloatParam(reverb.get(), ReverbParams::BUILDUP, reverbParams[ReverbParams::BUILDUP]);

        humidityChanged = false;
    }

    if (heightChanged)
    {
        float normalizedHeight = ParameterRanges::normalizeParameter(ParameterRanges::skyHeightRange, inHeight);

        // Height affects reverb predelay (from -0.5 to 1)
        static auto predelayRange = juce::NormalisableRange<float>(-0.5f, 1.0f, 0.01f);
        auto predelayVal = ParameterRanges::denormalizeParameter(predelayRange, normalizedHeight);
        reverbParams[ReverbParams::PREDELAY] = predelayVal;

        // Update reverb parameters
        VFXConfig::setFloatParam(reverb.get(), ReverbParams::PREDELAY, reverbParams[ReverbParams::PREDELAY]);

        heightChanged = false;
    }
}

//==================================================
template void Sky::process<juce::dsp::ProcessContextReplacing<float>>(const juce::dsp::ProcessContextReplacing<float> &);
template void Sky::process<juce::dsp::ProcessContextNonReplacing<float>>(const juce::dsp::ProcessContextNonReplacing<float> &);