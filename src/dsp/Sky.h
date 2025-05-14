#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "sst/voice-effects/lifted_bus_effects/LiftedReverb2.h"
#include "sst/voice-effects/lifted_bus_effects/FXConfigFromVFXConfig.h"
#include "sst/effects/Reverb2.h"
/**
 * Sky processor that uses Nimbus granular effect from SST
 */
class Sky
    : private juce::AsyncUpdater
{
    public:
        struct Parameters
        {
            float humidity;      // Humidity affects density and texture (0-100)
            float height;        // Height affects position and pitch (0-100)
        };

        Sky();
        ~Sky();

        // processing functions
        void prepare(const juce::dsp::ProcessSpec& spec);
        void reset();

        template <typename ProcessContext>
        void process(const ProcessContext& context);

        void setParameters(const Parameters& params);

    private:
        float fs = 44100.0f;

        float inHumidity = 50.0f;
        float inHeight = 75.0f;

        // Define reverb parameter indices
        enum ReverbParams
        {
            PREDELAY = 0,
            ROOM_SIZE,
            DECAY_TIME,
            DIFFUSION,
            BUILDUP,
            MODULATION,
            LF_DAMPING,
            HF_DAMPING,
            WIDTH,
            MIX,
            NUM_PARAMS
        };

        void handleAsyncUpdate() override;

        // Booleans for parameter changes
        bool  humidityChanged = false;
        bool  heightChanged = false;

        struct DbToLinearProvider
        {
            static constexpr size_t nPoints{512};
            float table_dB[nPoints];

            void init()
            {
                for (auto i = 0U; i < nPoints; i++)
                    table_dB[i] = powf(10.f, 0.05f * ((float)i - 384.f));
            }

            float dbToLinear(float db) const
            {
                db += 384;
                int e = (int)db;
                float a = db - (float)e;
                return (1.f - a) * table_dB[e & (nPoints - 1)] + a * table_dB[(e + 1) & (nPoints - 1)];
            }
        };

        struct VFXConfig
        {
            using config_t = VFXConfig;

            struct BC
            {
                DbToLinearProvider dbtlp;
                static constexpr uint16_t maxParamCount{8};
                float paramStorage[maxParamCount];
                double sampleRate = 44100.0; // Add the sampleRate field here
                double getSampleRate() const { return sampleRate; }
                float equalNoteToPitch(float p) const { return pow(2.0, p / 12); }
                float dbToLinear(float f) const { return dbtlp.dbToLinear(f); }
                template <typename... Types>
                BC(Types...) { dbtlp.init(); }
            };
            struct GS
            {
                double sampleRate;
                GS(double sr) : sampleRate(sr) { }

                float envelope_rate_linear_nowrap(float f) const
                {
                    return 1.f * blockSize / (sampleRate) * pow(-2, f);
                }

                float getTempoSyncRatio() const { return 1.0f; }


            };
            struct ES
            {
            };

            using BaseClass = BC;
            using GlobalStorage = GS;
            using EffectStorage = ES;
            using ValueStorage = float *;
            static constexpr int blockSize{16};

            static void setFloatParam(BC *b, int i, float f) { b->paramStorage[i] = f; }
            static float getFloatParam(const BC *b, int i) { return b->paramStorage[i]; }

            static void setIntParam(BC *b, int i, int v) { b->paramStorage[i] = static_cast<float>(v); }
            static int getIntParam(const BC *b, int i) { return static_cast<int>(std::round(b->paramStorage[i])); }

            static inline float dbToLinear(const BaseClass *s, float f)
            {
                return s->dbtlp.dbToLinear(f);
            }
            static inline float equalNoteToPitch(const BaseClass *s, float p)
            {
                return s->equalNoteToPitch(p);
            }
            static inline float getSampleRate(const BaseClass *s)
            {
                return s->getSampleRate();
            }
            static inline float getSampleRateInv(const BaseClass *s)
            {
                // Cannot use static_cast since the types aren't related by inheritance
                return 1.0f / s->getSampleRate();
            }

            static uint8_t *checkoutBlock(BaseClass *, size_t n)
            {
                return new uint8_t[n];
            }

            static void returnBlock(BaseClass *, uint8_t *ptr, size_t)
            {
                delete[] ptr;
            }

            static void preReservePool(BaseClass *, size_t) {}
            static void preReserveSingleInstancePool(BaseClass *, size_t) {}
        };

        std::unique_ptr<sst::voice_effects::liftbus::LiftedReverb2<VFXConfig>> reverb;
        std::array<float, sst::voice_effects::liftbus::LiftedReverb2<VFXConfig>::numFloatParams> reverbParams;

        // Buffer for handling different block sizes
        juce::AudioBuffer<float> processBuffer;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Sky)
};