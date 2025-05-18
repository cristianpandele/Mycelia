#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <foleys_gui_magic/foleys_gui_magic.h>

// Fold Window animation component that displays the fold window from DelayNodes
class FoldWindowAnimation : public juce::Component
{
    public:
        enum ColourIDs
        {
            backgroundColourId,
            windowColourId,
            gridColourId
        };

        FoldWindowAnimation();
        ~FoldWindowAnimation() override;

        void setWindowSize(float size);
        void setWindowShape(float shape);
        void setWindowPosition(float position);
        void updateFoldWindow();
        void paint(juce::Graphics &g) override;

    private:
        std::vector<float> windowValues;
        float windowSize = 0.0f;
        float windowShape = 0.0f;
        float windowPosition = 0.0f;

        juce::VBlankAttachment vBlankAttachment
        {
            this,
            [&] {
                repaint();
            }
        };


        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FoldWindowAnimation)
};

// This class creates and configures the fold window animation component
class FoldWindowViewItem : public foleys::GuiItem
{
    public:
        FOLEYS_DECLARE_GUI_FACTORY(FoldWindowViewItem)

        FoldWindowViewItem(foleys::MagicGUIBuilder &builder, const juce::ValueTree &node);

        std::vector<foleys::SettableProperty> getSettableProperties() const override;
        void update() override;
        juce::Component *getWrappedComponent() override;

    private:
        FoldWindowAnimation foldAnimation;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FoldWindowViewItem)
};
