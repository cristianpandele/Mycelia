#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <foleys_gui_magic/foleys_gui_magic.h>

// This class displays the tree positions as animated trees
class TreePositionAnimation : public juce::Component
{
    public:
        enum ColourIDs
        {
            backgroundColourId,
            gridColourId
        };

        TreePositionAnimation();
        ~TreePositionAnimation() override = default;

        // Set the tree positions
        void setTreePositions(const std::vector<int> &positions);

        // Set the tree size
        void setTreeSize(float size);

        // Set the stretch factor (0.0-1.0) affecting slot width
        void setStretch(float stretchFactor);

        // Paint the trees
        void paint(juce::Graphics &g) override;

    private:
        std::vector<int> treePositions;
        float treeSize = 0.5f;
        float stretch = 0.5f; // Default stretch value

        // Calculate the slot width and position based on stretch
        void calculateSlotDimensions(float canvasWidth, int numSlots, float &slotWidth, float &startX) const;

        // Tree images
        juce::Image tree1Image;
        juce::Image tree2Image;
        juce::Image tree3Image;

        // Random number generator for tree type selection
        juce::Random random;

        juce::VBlankAttachment vBlankAttachment{
            this,
            [&]
            {
                repaint();
            }};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TreePositionAnimation)
};

// This class creates and configures the tree position animation component
class TreePositionViewItem : public foleys::GuiItem
{
    public:
        FOLEYS_DECLARE_GUI_FACTORY(TreePositionViewItem)

        TreePositionViewItem(foleys::MagicGUIBuilder &builder, const juce::ValueTree &node);

        std::vector<foleys::SettableProperty> getSettableProperties() const override;
        void update() override;
        juce::Component *getWrappedComponent() override;

    private:
        TreePositionAnimation treeAnimation;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TreePositionViewItem)
};