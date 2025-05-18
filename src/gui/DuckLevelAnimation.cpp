#include "DuckLevelAnimation.h"
#include "BinaryData.h"

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
                     (canvasHeight - scaledHeight);
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
    properties.push_back({configNode, "duckLevel", foleys::SettableProperty::Number, 0.0f, {}});
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

juce::Component *DuckLevelViewItem::getWrappedComponent()
{
    return &duckAnimation;
}