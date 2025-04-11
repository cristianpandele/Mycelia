#include "DiffusionControl.h"

DiffusionControl::DiffusionControl()
{
    // Set up default frequency bands - logarithmically spaced between 20Hz and 20kHz
    const double minFreq = 250.0;
    const double maxFreq = 3000.0;

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
        coeffMaker[i] = sst::filters::FilterCoefficientMaker<>();
        coeffMaker[i].setSampleRateAndBlockSize((float)fs, spec.maximumBlockSize);

        filters[i] = sst::filters::GetQFPtrFilterUnit(sst::filters::fut_bp24, sst::filters::st_Standard);
    }

    prepareCoefficients();
}

void DiffusionControl::reset()
{
    // Reset all filters
    for (size_t i = 0; i < inNumActiveBands; ++i)
    {
        // Reset filter state
        memset(&filterState[i], 0, sizeof(sst::filters::QuadFilterUnitState));
        // Reset the filter maker
        coeffMaker[i].Reset();
    }
}

template <typename ProcessContext>
void DiffusionControl::process(
    const ProcessContext &inContext,
    juce::AudioBuffer<float> *outputBuffers)
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
        juce::dsp::AudioBlock<float> outputBandBlock(outputBuffers[band]);
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
        juce::dsp::AudioBlock<float> outputBandBlock(outputBuffers[band]);

        // Ensure filter bands are active
        for (int ch = 0; ch < numChannels; ++ch)
        {
            filterState[band].active[ch] = 0xFFFFFFFF;
        }

        // Apply filter
        for (int i = 0; i < numSamples; ++i)
        {
            float r alignas(16)[4];
            r[0] = outputBandBlock.getChannelPointer(0)[i];
            r[1] = outputBandBlock.getChannelPointer(1)[i];
            r[2] = 0.f;
            r[3] = 0.f;
            auto yVec = filters[band](&filterState[band], SIMD_MM(load_ps)(r));

            float yArr alignas(16)[4];
            SIMD_MM(store_ps)(yArr, yVec);

            outputBandBlock.getChannelPointer(0)[i] = yArr[0];
            outputBandBlock.getChannelPointer(1)[i] = yArr[1];
        }
    }
}

inline float freq_hz_to_note_num(float freqHz)
{
    return 12.0f * std::log2(freqHz / 440.0f);
}

void DiffusionControl::prepareCoefficients()
{
    for (size_t i = 0; i < inNumActiveBands; ++i)
    {
        // Reset filter state
        memset(&filterState[i], 0, sizeof(sst::filters::QuadFilterUnitState));
        // Reset the filter maker
        coeffMaker[i].Reset();
        // Get the center frequency for this band
        float centerFreq = bandFrequencies[i];

        coeffMaker[i].MakeCoeffs(freq_hz_to_note_num(centerFreq), 0.5f, sst::filters::fut_bp24, sst::filters::st_Standard, nullptr, false);

        coeffMaker[i].updateState(filterState[i]);
    }
}

void DiffusionControl::setParameters(const Parameters &params)
{
    // diffusionAmount = params.diffusion;
    inNumActiveBands = params.numActiveBands;
}

template void DiffusionControl::process<juce::dsp::ProcessContextReplacing<float>>(const juce::dsp::ProcessContextReplacing<float> &, juce::AudioBuffer<float> *);
template void DiffusionControl::process<juce::dsp::ProcessContextNonReplacing<float>>(const juce::dsp::ProcessContextNonReplacing<float> &, juce::AudioBuffer<float> *);