#include "MyceliaView.h"
#include "BinaryData.h"

MyceliaAnimation::MyceliaAnimation()
{
    // make sure you define some default colour, otherwise the juce lookup will choke
    setColour(backgroundColourId, juce::Colours::black);
    setColour(drawColourId, juce::Colours::green);
    setColour(fillColourId, juce::Colours::green.withAlpha(0.5f));

    startTimerHz(30);
}

void MyceliaAnimation::setFactor(float f)
{
    factor = f;
}

void MyceliaAnimation::paint(juce::Graphics &g)
{
    const float radius = std::min(getWidth(), getHeight()) * 0.4f;
    const auto centre = getLocalBounds().getCentre().toFloat();

    g.fillAll(findColour(backgroundColourId));
    juce::Path p;
    p.startNewSubPath(centre + juce::Point<float>(0, std::sin(phase)) * radius);
    for (auto i = 0.1f; i <= juce::MathConstants<float>::twoPi; i += 0.01f)
        p.lineTo(centre + juce::Point<float>(std::sin(i),
                                             std::sin(std::fmod(i * factor + phase,
                                                                juce::MathConstants<float>::twoPi))) *
                                             radius);
    p.closeSubPath();

    g.setColour(findColour(drawColourId));
    g.strokePath(p, juce::PathStrokeType(2.0f));

    const auto fillColour = findColour(fillColourId);
    if (fillColour.isTransparent() == false)
    {
        g.setColour(fillColour);
        g.fillPath(p);
    }
}

void MyceliaAnimation::timerCallback()
{
    phase += 0.1f;
    if (phase >= juce::MathConstants<float>::twoPi)
        phase -= juce::MathConstants<float>::twoPi;

    repaint();
}

//=========================================================================

MyceliaViewItem::MyceliaViewItem(foleys::MagicGUIBuilder &builder, const juce::ValueTree &node) : foleys::GuiItem(builder, node)
{
    // Create the colour names to have them configurable
    setColourTranslation({{"lissajour-background", MyceliaAnimation::backgroundColourId},
                          {"lissajour-draw", MyceliaAnimation::drawColourId},
                          {"lissajour-fill", MyceliaAnimation::fillColourId}});

    addAndMakeVisible(myceliaAnimation);
}

std::vector<foleys::SettableProperty> MyceliaViewItem::getSettableProperties() const
{
    std::vector<foleys::SettableProperty> newProperties;

    newProperties.push_back({configNode, "factor", foleys::SettableProperty::Number, 1.0f, {}});

    return newProperties;
}

// Override update() to set the values to your custom component
void MyceliaViewItem::update()
{
    auto factor = getProperty("factor");
    myceliaAnimation.setFactor(factor.isVoid() ? 3.0f : float(factor));
}

juce::Component *MyceliaViewItem::getWrappedComponent()
{
    return &myceliaAnimation;
}

//=========================================================================