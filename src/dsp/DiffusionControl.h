#include <juce_dsp/juce_dsp.h>

#define MAX_BANDS 16

class DiffusionControl
{
    public:
        // Parameters
        struct Parameters
        {
            int numActiveBands;  // Controls the number of filter bands to use (0-16)
        };

        DiffusionControl();
        ~DiffusionControl();

        void prepare(const juce::dsp::ProcessSpec &spec);
        void reset();

        // Process the input context and output through multiple filter banks
        // Returns an array of processed audio blocks
        template <typename ProcessContext>
        void process(const ProcessContext &context,
                     std::array<juce::AudioBuffer<float>, MAX_BANDS> &outputs);

        void setParameters(const Parameters &params);

    private:
        // Using ProcessorDuplicator for proper stereo processing
        using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                    juce::dsp::IIR::Coefficients<float>>;

        // Filter bank implementation with proper stereo support
        std::array<Filter, MAX_BANDS> filters;
        std::array<float, MAX_BANDS> bandFrequencies;
        std::array<float, MAX_BANDS> bandGains;

        // Diffusion parameters
        int inNumActiveBands = 4;
        double fs = 44100.0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiffusionControl)
};