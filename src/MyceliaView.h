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