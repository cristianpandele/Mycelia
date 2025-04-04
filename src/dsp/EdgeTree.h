#pragma once

#include <juce_dsp/juce_dsp.h>

/**
 * EdgeTree processes audio to extract envelope information
 * and generate tree edge data based on audio dynamics
 */
class EdgeTree
{
    public:
        EdgeTree();
        ~EdgeTree();

        // Parameters
        struct Parameters
        {
            float treeSize;         // Size of the tree (0.2 to 1.8)
        };

        void prepare(const juce::dsp::ProcessSpec &spec);
        void reset();

        template <typename ProcessContext>
        void process(const ProcessContext &context);

        void setParameters(const Parameters &params);

    private:
        std::unique_ptr<juce::dsp::BallisticsFilter<float>> envelopeFollower;

        float inTreeSize = 1.0f;

        // Parameters
        float attackMs = 250.0f;
        float releaseMs = 150.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EdgeTree)
};