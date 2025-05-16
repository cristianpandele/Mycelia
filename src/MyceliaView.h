#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <foleys_gui_magic/foleys_gui_magic.h>

class MyceliaAnimation :
    public juce::Component,
    private juce::Timer
{
    public:
        enum ColourIDs
        {
            // we are safe from collisions, because we set the colours on every component directly from the stylesheet
            backgroundColourId,
            drawColourId,
            fillColourId
        };
        MyceliaAnimation();
        void setFactor(float f);
        void paint(juce::Graphics &g) override;

    private:
        void timerCallback() override;

        float factor = 3.0f;
        float phase = 0.0f;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyceliaAnimation)
};

// Duck animation component that displays a duck that moves based on the delay duck parameter
class DuckLevelAnimation : public juce::Component
{
    public:
        enum ColourIDs
        {
            backgroundColourId
        };

        DuckLevelAnimation();
        ~DuckLevelAnimation() override;
        void setDuckLevel(float level);
        void setDryWetLevel(float level);
        void paint(juce::Graphics &g) override;

    private:
        float duckLevel = 0.0f;          // Target level for size
        float dryWetLevel = 0.0f;        // Target level for position
        float currentDryWetLevel = 0.0f; // Current level with easing
        float currentDuckLevel = 0.0f;   // Current level with easing
        float animSpeed = 0.2f;          // Animation speed factor (0.0-1.0)

        juce::Image duckImage;
        juce::VBlankAttachment vBlankAttachment
        {
            this,
            [&] {
                if (std::abs(currentDryWetLevel - dryWetLevel) > 0.001f)
                {
                    // Interpolate with easing
                    currentDryWetLevel += (dryWetLevel - currentDryWetLevel) * animSpeed;
                }
                if (std::abs(currentDuckLevel - duckLevel) > 0.001f)
                {
                    currentDuckLevel += (duckLevel - currentDuckLevel) * animSpeed;
                }
                repaint();
            }
        };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckLevelAnimation)
};

// This class creates and configures the duck animation component
class DuckLevelViewItem : public foleys::GuiItem
{
    public:
        FOLEYS_DECLARE_GUI_FACTORY(DuckLevelViewItem)

        DuckLevelViewItem(foleys::MagicGUIBuilder &builder, const juce::ValueTree &node);

        std::vector<foleys::SettableProperty> getSettableProperties() const override;
        void update() override;
        juce::Component *getWrappedComponent() override;

    private:
        DuckLevelAnimation duckAnimation;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckLevelViewItem)
};

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

// ==========================================================================

// This class is creating and configuring your custom component
class MyceliaViewItem : public foleys::GuiItem
{
    public:
        FOLEYS_DECLARE_GUI_FACTORY(MyceliaViewItem)

        MyceliaViewItem(foleys::MagicGUIBuilder &builder, const juce::ValueTree &node);

        std::vector<foleys::SettableProperty> getSettableProperties() const override;

        // Override update() to set the values to your custom component
        void update() override;

        juce::Component *getWrappedComponent() override;

    private:
        MyceliaAnimation myceliaAnimation;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyceliaViewItem)
};