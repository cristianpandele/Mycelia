#include <juce_dsp/juce_dsp.h>
#include "sst/filters.h"
#include "sst/filters/FilterCoefficientMaker_Impl.h"

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
        static constexpr int maxNutrientBands = 16;
        template <typename ProcessContext>
        void process(const ProcessContext &context,
                     std::array<juce::AudioBuffer<float>, DiffusionControl::maxNutrientBands> &outputs);

        void setParameters(const Parameters &params);

    private:
        // Diffusion parameters
        int inNumActiveBands = 4;
        double fs = 44100.0;

        // Filter bank implementation
        std::array<sst::filters::FilterCoefficientMaker<>, DiffusionControl::maxNutrientBands> coeffMaker;
        std::array<sst::filters::QuadFilterUnitState, DiffusionControl::maxNutrientBands> filterState;
        std::array<sst::filters::FilterUnitQFPtr, DiffusionControl::maxNutrientBands> filters;
        std::array<float, DiffusionControl::maxNutrientBands> bandFrequencies;
        void prepareCoefficients();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiffusionControl)
};