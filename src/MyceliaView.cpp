#include "MyceliaView.h"

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

// DuckLevelAnimation implementation
DuckLevelAnimation::DuckLevelAnimation()
{
    // Set default background color
    setColour(backgroundColourId, juce::Colours::transparentWhite);

    // Load the duck image from BinaryData
    duckImage = juce::ImageCache::getFromMemory(BinaryData::duckSmallBackgroundRemoved_png, BinaryData::duckSmallBackgroundRemoved_pngSize);
}

DuckLevelAnimation::~DuckLevelAnimation()
{
    // VBlankAttachment will be automatically destroyed by the unique_ptr
}

void DuckLevelAnimation::setDuckLevel(float level)
{
    // Ensure the level is in range [0.0, 1.0]
    duckLevel = juce::jlimit(0.0f, 1.0f, level);
}

void DuckLevelAnimation::setDryWetLevel(float level)
{
    // Ensure the level is in range [0.0, 1.0]
    dryWetLevel = juce::jlimit(0.0f, 1.0f, level);
}

void DuckLevelAnimation::paint(juce::Graphics &g)
{
    // Fill the background
    g.fillAll(findColour(backgroundColourId));

    // Define the animation area
    const float canvasWidth = static_cast<float>(getWidth());
    const float canvasHeight = static_cast<float>(getHeight());

    if (!duckImage.isNull())
    {
        // Calculate scaling factor based on eased duck level
        const float scale = 1.0f / (2.0f - duckLevel);

        // Get original image dimensions
        const float originalWidth = static_cast<float>(duckImage.getWidth());
        const float originalHeight = static_cast<float>(duckImage.getHeight());

        // Calculate new dimensions while preserving aspect ratio
        const float scaledWidth = originalWidth * scale;
        const float scaledHeight = originalHeight * scale;

        // Calculate position so the duck stays centered as it grows
        const float xPos = currentDuckLevel * (canvasWidth - scaledWidth);
        float yPos = (1.0f - currentDryWetLevel) *
                           (canvasHeight - scaledHeight / 2.0f)
                           - scaledHeight / 4.0f; // Place duck with bottom margin
        if (yPos < 0.0f)
        {
            yPos = 0.0f; // Ensure duck is not drawn outside the top
        }

        // Enable antialiasing for smoother scaling
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

        // Resize the duck image according to the current duck level
        g.drawImageTransformed(
            duckImage,
            juce::AffineTransform::scale(scale).translated(xPos, yPos),
            false // Don't use alpha blending
        );
    }
}

// DuckLevelViewItem implementation
DuckLevelViewItem::DuckLevelViewItem(foleys::MagicGUIBuilder &builder, const juce::ValueTree &node)
    : foleys::GuiItem(builder, node)
{
    // Add the animation component to the view
    addAndMakeVisible(duckAnimation);
}

std::vector<foleys::SettableProperty> DuckLevelViewItem::getSettableProperties() const
{
    std::vector<foleys::SettableProperty> properties;

    // Add a property to set the duck level
    properties.push_back({configNode, "duckLevel", foleys::SettableProperty::Number, 0.0f,
                         {}});
    // Add a property to set the dry/wet level
    properties.push_back({configNode, "dryWetLevel", foleys::SettableProperty::Number, 0.0f, {}});

    return properties;
}

void DuckLevelViewItem::update()
{
    auto levelValue = getProperty("duckLevel");
    if (levelValue.isVoid() == false)
    {
        duckAnimation.setDuckLevel(static_cast<float>(levelValue));
    }
    else
    {
        duckAnimation.setDuckLevel(0.0f);
    }

    levelValue = getProperty("dryWetLevel");
    if (levelValue.isVoid() == false)
    {
        duckAnimation.setDryWetLevel(static_cast<float>(levelValue));
    }
    else
    {
        duckAnimation.setDryWetLevel(0.0f);
    }
}

juce::Component* DuckLevelViewItem::getWrappedComponent()
{
    return &duckAnimation;
}
