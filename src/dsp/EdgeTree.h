#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "EnvelopeFollower.h"

/**
 * EdgeTree processes audio to extract envelope information
 * and generate tree edge data based on audio dynamics
 */
class EdgeTree
    : private juce::Timer
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
        EnvelopeFollower envelopeFollower;

        void timerCallback();

        float inTreeSize = 1.0f;
        float inTreeDensity = 0.0f;
        // Booleans for parameter changes
        bool treeSizeChanged = false;

        float compGain = 1.0f;

        // Parameters
        EnvelopeFollower::Parameters envelopeFollowerParams =
        {
            .attackMs = 250.0f,
            .releaseMs = 150.0f,
            .levelType = juce::dsp::BallisticsFilterLevelCalculationType::peak
        };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EdgeTree)
};