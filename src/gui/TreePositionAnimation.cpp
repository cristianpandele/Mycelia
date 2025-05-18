#include "TreePositionAnimation.h"
#include "BinaryData.h"

// TreePositionAnimation implementation
TreePositionAnimation::TreePositionAnimation()
{
    // Set default background color
    setColour(backgroundColourId, juce::Colours::transparentWhite);

    // Load the tree images from BinaryData
    tree1Image = juce::ImageCache::getFromMemory(BinaryData::tree_1_png, BinaryData::tree_1_pngSize);
    tree2Image = juce::ImageCache::getFromMemory(BinaryData::tree_2_png, BinaryData::tree_2_pngSize);
    tree3Image = juce::ImageCache::getFromMemory(BinaryData::tree_3_png, BinaryData::tree_3_pngSize);
}

void TreePositionAnimation::setTreePositions(const std::vector<int> &positions)
{
    treePositions = positions;
    repaint();
}

void TreePositionAnimation::setTreeSize(float size)
{
    // Ensure the size is in range [0.1, 1.0]
    treeSize = juce::jlimit(0.1f, 1.0f, size);
    repaint();
}

void TreePositionAnimation::setStretch(float stretchFactor)
{
    // Ensure the stretch is in range [0.0, 1.0]
    stretch = juce::jlimit(0.0f, 1.0f, stretchFactor);
    stretch = std::abs(stretch - 0.5f) * 2.0f; // Denormalize to [0.0, 1.0]
    stretch = juce::jlimit(0.0f, 1.0f, stretch);
    repaint();
}

void TreePositionAnimation::calculateSlotDimensions(float canvasWidth, int numSlots, float& slotWidth, float& startX) const
{
    // Keep the slot width constant - base it on a percentage of canvas width
    const float fixedSlotWidthPercentage = 0.06f; // 6% of canvas width
    slotWidth = canvasWidth * fixedSlotWidthPercentage;

    // Calculate spacing between slots based on stretch
    // At stretch = 0, slots are close together
    // At stretch = 1, slots are spread across the full width

    // Calculate the width needed for all slots without any spacing
    float slotsOnlyWidth = slotWidth * numSlots;

    // Calculate maximum available space for spacing (full canvas minus slots)
    float maxSpacingWidth = canvasWidth - slotsOnlyWidth;

    // Calculate actual spacing based on stretch factor
    float totalSpacingWidth = maxSpacingWidth * stretch;

    // Calculate total width including spacing
    float totalWidth = slotsOnlyWidth + totalSpacingWidth;

    // Center the arrangement horizontally
    startX = (canvasWidth - totalWidth) / 2.0f;
}

void TreePositionAnimation::paint(juce::Graphics &g)
{
    // Fill the background
    g.fillAll(findColour(backgroundColourId));

    // Define the animation area
    const float canvasWidth = static_cast<float>(getWidth());
    const float canvasHeight = static_cast<float>(getHeight());

    // Number of slots
    const int numSlots = 8;

    // Calculate slot width and starting X position based on stretch
    float slotWidth = 0.0f;
    float startX = 0.0f;
    calculateSlotDimensions(canvasWidth, numSlots, slotWidth, startX);

    // Calculate spacing between slots
    float slotsOnlyWidth = slotWidth * numSlots;
    float maxSpacingWidth = canvasWidth - slotsOnlyWidth;
    float totalSpacingWidth = maxSpacingWidth * stretch;
    float spacingPerSlot = (numSlots > 1) ? (totalSpacingWidth / (numSlots - 1)) : 0.0f;

    // Enable high-quality image rendering
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

    // Determine which tree image to use (randomized but consistent for each position)
    random.setSeed(4321); // Set seed for consistent randomness

    // Draw trees in their positions
    for (int treePos : treePositions)
    {
        // Skip invalid positions
        if (treePos < 0 || treePos >= numSlots)
            continue;

        // Calculate slot position with spacing
        float x;
        if (treePos == 0)
        {
            // First slot starts at startX
            x = startX;
        }
        else
        {
            // Other slots include spacing
            x = startX + (treePos * slotWidth) + (treePos * spacingPerSlot);
        }

        // Get center of slot for positioning the tree
        float xPos = x + (slotWidth / 2.0f);

        int treeType = random.nextInt(3); // 0, 1, or 2

        juce::Image &treeImage = treeType == 0 ? tree1Image : (treeType == 1 ? tree2Image : tree3Image);

        if (!treeImage.isNull())
        {
            // Calculate image dimensions based on tree size
            float baseScale = juce::jmin(
                                slotWidth / treeImage.getWidth(),
                                canvasHeight / treeImage.getHeight());

            // Increase the base scale based on tree size
            float scale = baseScale * (2.0f + 1.5f * treeSize);

            // Apply random variation of +/-25% to make trees look less uniform
            float scaleVariation = random.nextFloat() * 0.5f - 0.25f; // -25% to +25%

            // Apply variation to scale
            scale *= (1.0f + scaleVariation);

            float scaledWidth = treeImage.getWidth() * scale;
            float scaledHeight = treeImage.getHeight() * scale;

            // Center tree in the slot horizontally and place at the bottom vertically
            x = xPos - (scaledWidth / 2.0f);
            // Add a small variation to the x position to make it look less uniform
            float positionVariation = random.nextFloat() * 0.1f - 0.05f; // +/- 5% variation
            x *= (1.0f + positionVariation);
            // Position the tree at the bottom of the canvas
            float y = canvasHeight - scaledHeight;

            // Draw the tree
            g.drawImageTransformed(
                treeImage,
                juce::AffineTransform::scale(scale).translated(x, y),
                false // Don't use alpha blending
            );
        }
    }
}

// TreePositionViewItem implementation
TreePositionViewItem::TreePositionViewItem(foleys::MagicGUIBuilder &builder, const juce::ValueTree &node)
    : foleys::GuiItem(builder, node)
{
    // Create the colour names to have them configurable
    setColourTranslation({{"tree-position-background", TreePositionAnimation::backgroundColourId}});

    // Add the animation component to the view
    addAndMakeVisible(treeAnimation);
}

std::vector<foleys::SettableProperty> TreePositionViewItem::getSettableProperties() const
{
    std::vector<foleys::SettableProperty> properties;

    // Add a property for tree positions (text representation: "1,3,5,7")
    properties.push_back({configNode, "treePositions", foleys::SettableProperty::Text, "0,1,2,3,4,5,6,7", {}});

    // Add a property for tree size (0.1-1.0)
    properties.push_back({configNode, "treeSize", foleys::SettableProperty::Number, 0.1f, {}});

    // Add a property for stretch factor (0.0-1.0)
    properties.push_back({configNode, "stretch", foleys::SettableProperty::Number, 0.5f, {}});

    return properties;
}

void TreePositionViewItem::update()
{
    // Get the tree positions property
    auto positionsValue = getProperty("treePositions");
    if (!positionsValue.isVoid())
    {
        // Parse the comma-separated list of positions
        juce::String positionsStr = positionsValue.toString();
        std::vector<int> positions;

        juce::StringArray tokens;
        tokens.addTokens(positionsStr, ",", "");

        for (const auto &token : tokens)
        {
            positions.push_back(token.getIntValue());
        }

        treeAnimation.setTreePositions(positions);
    }

    // Get the tree size property
    auto treeSizeValue = getProperty("treeSize");
    if (!treeSizeValue.isVoid())
    {
        treeAnimation.setTreeSize(static_cast<float>(treeSizeValue));
    }

    // Get the stretch property
    auto stretchValue = getProperty("stretch");
    if (!stretchValue.isVoid())
    {
        treeAnimation.setStretch(static_cast<float>(stretchValue));
    }
}

juce::Component* TreePositionViewItem::getWrappedComponent()
{
    return &treeAnimation;
}
