#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <foleys_gui_magic/foleys_gui_magic.h>
#include "dsp/DelayNodes.h"

// Network Graph Animation component that displays the delay nodes network
class NetworkGraphAnimation : public juce::Component
{
    public:
        enum ColourIDs
        {
            backgroundColourId,
            nodeBaseColourId,
            nodeHighAgeColourId,
            lineLowWeightColourId,
            lineHighWeightColourId,
            nodeBorderLowLevelColourId,
            nodeBorderHighLevelColourId
        };

        NetworkGraphAnimation();
        ~NetworkGraphAnimation() override = default;

        // Update with new stretch level
        void setStretch(float stretch);

        // Update with number of active bands
        void setNumActiveBands(int numActiveBands);

        // Update the tree positions
        void setTreePositions(std::vector<int> &treePositions);

        // Update with new band states
        void setBandStates(const std::vector<DelayNodes::BandResources> &states);

        // Paint the network graph
        void paint(juce::Graphics &g) override;

    private:
        // Helper to draw a curved connection between nodes
        void drawNodeConnection(juce::Graphics &g, juce::Point<float> start, juce::Point<float> end,
                                    float weight, float startLevel, float endLevel);

        // Helper to map a value to a color gradient
        juce::Colour mapValueToColour(float value, juce::Colour startColour, juce::Colour endColour);

        // Helper to get the vertical position of a band
        inline float getBandY(int bandIdx, int numAllocatedBands);

        // Helper to get the width of the plot for a band
        inline float getBandUsedWidth(const float canvasWidth);

        // Helper to get the left margin for a band
        inline float getBandMargin(const float canvasWidth, int numBands, int bandIdx);

        // Helper to get the x-position of a node
        inline float getNodeX(float positionProportion, float bandLeftMargin, float canvasWidth);

        // Structure to store only what we need for rendering
        struct BandStateSnapshot
        {
            std::vector<float> bufferLevels;
            std::vector<float> nodeDelayTimes;
            std::vector<float> treeConnections;
            std::vector<std::vector<std::vector<float>>> interNodeConnections;

            // Helper method to extract just what we need from BandResources
            static BandStateSnapshot fromBandResources(const DelayNodes::BandResources &resource)
            {
                BandStateSnapshot snapshot;
                snapshot.bufferLevels = resource.bufferLevels;
                snapshot.nodeDelayTimes = resource.nodeDelayTimes;
                snapshot.treeConnections = resource.treeConnections;
                snapshot.interNodeConnections = resource.interNodeConnections;
                return snapshot;
            }
        };

        // Store the band states data
        std::vector<BandStateSnapshot> bandStateSnapshots;
        // Stretch level for the network
        float stretch = 0.0f;
        // Number of active bands
        int numActiveBands = 0;
        // Tree positions in the network
        std::vector<int> treePositions;

        // Constants for visualization
        const float nodeRadius = 10.0f;

        // VBlank attachment for smooth animation using compositor sync
        juce::VBlankAttachment vBlankAttachment{
            this,
            [&]
            {
                repaint();
            }};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NetworkGraphAnimation)
};

// This class creates and configures the network graph animation component
class NetworkGraphViewItem : public foleys::GuiItem
{
    public:
        FOLEYS_DECLARE_GUI_FACTORY(NetworkGraphViewItem)

        NetworkGraphViewItem(foleys::MagicGUIBuilder &builder, const juce::ValueTree &node);

        std::vector<foleys::SettableProperty> getSettableProperties() const override;
        void update() override;
        juce::Component *getWrappedComponent() override;

    private:
        NetworkGraphAnimation networkAnimation;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NetworkGraphViewItem)
};
