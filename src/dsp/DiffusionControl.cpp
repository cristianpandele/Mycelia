#include "DiffusionControl.h"

DiffusionControl::DiffusionControl()
{
    // Set up default frequency bands - logarithmically spaced between 20Hz and 20kHz
    const double minFreq = 80.0;
    const double maxFreq = 8000.0;

    for (size_t i = 0; i < inNumActiveBands; ++i)
    {
        // Calculate logarithmically spaced frequency
        double t = static_cast<double>(i) / static_cast<double>(inNumActiveBands - 1);
        bandFrequencies[i] = static_cast<float>(minFreq * std::pow(maxFreq / minFreq, t));
    }
}

DiffusionControl::~DiffusionControl()
{
    // Clean up any resources
}

void DiffusionControl::prepare(const juce::dsp::ProcessSpec &spec)
{
    // Store sample rate for coefficient updates
    fs = spec.sampleRate;

    // Prepare all filters
    for (size_t i = 0; i < inNumActiveBands; ++i)
    {
        // Create bandpass filters for each band
        auto &filter = filters[i];

        // Create bandpass filter coefficients
        auto coefficients =
            juce::dsp::IIR::Coefficients<float>::makeBandPass(
                fs,
                bandFrequencies[i],
                0.7f
            );

        // Update the filter with the new coefficients
        filter.state = coefficients;

        // Prepare the filter for processing
        filter.prepare(spec);
    }
}

void DiffusionControl::reset()
{
    // Reset all filters
    for (auto &filter : filters)
    {
        filter.reset();
    }
}

template <typename ProcessContext>
void DiffusionControl::process(
    const ProcessContext &inContext,
    std::array<juce::AudioBuffer<float>, MAX_BANDS> &outputs)
{
    // Manage audio context
    const auto &inputBlock = inContext.getInputBlock();
    auto &outputBlock = inContext.getOutputBlock();
    const auto numChannels = inputBlock.getNumChannels();
    const auto numSamples = inputBlock.getNumSamples();

    jassert(inputBlock.getNumChannels() == numChannels);
    jassert(inputBlock.getNumSamples() == numSamples);

    // Copy input to all output bands
    for (int band = 0; band < inNumActiveBands; ++band)
    {
        juce::dsp::AudioBlock<float> outputBandBlock(outputs[band]);
        outputBandBlock.copyFrom(inputBlock);
    }

    // Skip processing if bypassed
    if (inContext.isBypassed)
    {
        return;
    }

    // Apply diffusion across bands
    for (int band = 0; band < inNumActiveBands; ++band)
    {
        // Create AudioBlock for filter processing
        juce::dsp::AudioBlock<float> outputBandBlock(outputs[band]);

        // Create Context for filter processing
        juce::dsp::ProcessContextReplacing<float> bandContext(outputBandBlock);

        // Apply filter
        filters[band].process(bandContext);
    }
}

void DiffusionControl::setParameters(const Parameters &params)
{
    // diffusionAmount = params.diffusion;
    inNumActiveBands = params.numActiveBands;

    // Update filter coefficients if we have a valid sample rate
    for (size_t i = 0; i < inNumActiveBands; ++i)
    {
        // Create new filter coefficients with updated Q based on diffusion
        auto coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(
            fs,
            bandFrequencies[i],
            0.7f);

        // Update the filter with the new coefficients
        filters[i].state = coefficients;
    }
}
template void DiffusionControl::process<juce::dsp::ProcessContextReplacing<float>>(const juce::dsp::ProcessContextReplacing<float> &, std::array<juce::AudioBuffer<float>, MAX_BANDS> &);
template void DiffusionControl::process<juce::dsp::ProcessContextNonReplacing<float>>(const juce::dsp::ProcessContextNonReplacing<float> &, std::array<juce::AudioBuffer<float>, MAX_BANDS> &);