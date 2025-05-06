#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <sst/voice-effects/waveshaper/WaveShaper.h>

/**
 * Audio processor that implements input gain and sculpting
 */
class InputNode
    : private juce::Timer
{
    public:
        struct Parameters
        {
            float gainLevel;       // Gain level (0.0 to 120.0)
            float bandpassFreq;    // Bandpass center frequency in Hz
            float bandpassWidth;   // Bandpass width (100.0 to 20000.0)
            float reverbMix;       // Reverb mix level (0.0 to 1.0)
        };

        InputNode();
        ~InputNode();

        // processing functions
        void prepare(const juce::dsp::ProcessSpec &spec);
        void reset();

        template <typename ProcessContext>
        void process(const ProcessContext &context);

        void setParameters(const Parameters &params);
        void timerCallback();

    private:
        float fs = 44100.0f;

        float inGainLevel = 0.0f;
        float inBandpassFreq = 2070.0f;
        float inBandpassWidth = 4000.0f;
        float inReverbMix = 0.0f;
        float waveshaperDrive = 50.0f;
        float waveshaperLowpass = 100.0f;
        float waveshaperHighpass = 8000.0f;
        static constexpr float waveshaperBias = 0.0f;
        static constexpr float waveshaperPostgain = 0.0f;
        static constexpr int waveshaperType = (int)sst::waveshapers::WaveshaperType::wst_ojd;
        // Booleans for parameter changes
        bool gainChanged = false;
        bool filterChanged = false;
        bool reverbMixChanged = false;

        // Input gain
        juce::dsp::Gain<float> gain;

        struct WaveshaperConfig
        {
            struct BaseClass
            {
                std::array<float, 256> fb{};
                std::array<int, 256> ib{};
            };
            static constexpr int blockSize{16};
            static void setFloatParam(BaseClass *b, int i, float f) { b->fb[i] = f; }
            static float getFloatParam(const BaseClass *b, int i) { return b->fb[i]; }

            static void setIntParam(BaseClass *b, int i, int v) { b->ib[i] = v; }
            static int getIntParam(const BaseClass *b, int i) { return b->ib[i]; }

            static float dbToLinear(const BaseClass *, float db) { return std::pow(10, db / 20.0); }
            static float equalNoteToPitch(const BaseClass *, float f) { return pow(2.f, (f + 69) / 12.f); }
            static float getSampleRate(const BaseClass *) { return 48000.f; }
            static float getSampleRateInv(const BaseClass *) { return 1.0 / 48000.f; }

            static void preReservePool(BaseClass *, size_t) {}
            static void preReserveSingleInstancePool(BaseClass *, size_t) {}
            static uint8_t *checkoutBlock(BaseClass *, size_t) { return nullptr; }
            static void returnBlock(BaseClass *, uint8_t *, size_t) {}
        };

        // Waveshaper implementation
        using MyShaperType = sst::voice_effects::waveshaper::WaveShaper<WaveshaperConfig>;
        std::unique_ptr<MyShaperType> waveShaper;
        bool waveshaperBypass = false;

        // Waveshaper filter
        void updateFilterCoefficients();

        // Process audio through the waveshaper
        void processWaveShaper(juce::dsp::AudioBlock<float> &buffer);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputNode)
};
