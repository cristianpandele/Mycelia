#include <juce_dsp/juce_dsp.h>
#include "sst/filters.h"
#include "sst/filters/FilterCoefficientMaker_Impl.h"
#include "util/ParameterRanges.h"

class DiffusionControl
{
    public:
        // Parameters
        struct Parameters
        {
            int numActiveBands; // Controls the number of filter bands to use (0-MAX_NUTRIENT_BANDS)
        };

        DiffusionControl();
        ~DiffusionControl();

        void prepare(const juce::dsp::ProcessSpec &spec);
        void reset();

        // Process the input context and output through multiple filter banks
        // Returns an array of processed audio blocks
        template <typename ProcessContext>
        void process(const ProcessContext &context,
                     juce::AudioBuffer<float> *outputBuffers);

        void setParameters(const Parameters &params);
        void getBandFrequencies(float *outBandFrequencies, int *numActiveBands);

    private:
        // Diffusion parameters
        int inNumActiveBands = 4;
        double fs = 44100.0;

        static constexpr double minFreq = 250.0;
        static constexpr double maxFreq = 3000.0;

        // Filter bank implementation
        std::array<sst::filters::FilterCoefficientMaker<>, ParameterRanges::maxNutrientBands> coeffMaker;
        std::array<sst::filters::QuadFilterUnitState, ParameterRanges::maxNutrientBands> filterState;
        std::array<sst::filters::FilterUnitQFPtr, ParameterRanges::maxNutrientBands> filters;
        std::array<float, ParameterRanges::maxNutrientBands> bandFrequencies;
        void prepareCoefficients();
        void updateBandFrequencies(double minFreq, double maxFreq, int numBands);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiffusionControl)
};