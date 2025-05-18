#include "FoldWindowAnimation.h"

// FoldWindowAnimation implementation
FoldWindowAnimation::FoldWindowAnimation()
{
    // Set default background color
    setColour(backgroundColourId, juce::Colours::transparentWhite);
    setColour(windowColourId, juce::Colours::green);
}

FoldWindowAnimation::~FoldWindowAnimation()
{
    // No specific cleanup needed
}

void FoldWindowAnimation::setWindowSize(float size)
{
    windowSize = size;
}
void FoldWindowAnimation::setWindowShape(float shape)
{
    windowShape = shape;
}
void FoldWindowAnimation::setWindowPosition(float position)
{
    windowPosition = position;
}

void FoldWindowAnimation::updateFoldWindow()
{
    static constexpr size_t maxNumDelayProcsPerBand = 8;
    // Ensure the window sizes are set correctly
    windowValues.resize(maxNumDelayProcsPerBand);

    // Use Juce windowing functions to create the window shapes...
    juce::AudioBuffer<float> rect(1, maxNumDelayProcsPerBand);
    juce::AudioBuffer<float> hann(1, maxNumDelayProcsPerBand);
    juce::AudioBuffer<float> fold(1, maxNumDelayProcsPerBand);

    // Populate the window buffers with the appropriate windowing functions
    auto winSize = static_cast<size_t>(std::ceil(windowSize * maxNumDelayProcsPerBand));
    winSize = std::max(static_cast<size_t>(3), winSize);

    auto winPosition = static_cast<size_t>(std::floor((maxNumDelayProcsPerBand - winSize) * windowPosition));

    juce::dsp::WindowingFunction<float>::fillWindowingTables(
        rect.getWritePointer(0, winPosition),
        winSize,
        juce::dsp::WindowingFunction<float>::rectangular,
        false);

    juce::dsp::WindowingFunction<float>::fillWindowingTables(
        hann.getWritePointer(0, winPosition),
        winSize,
        juce::dsp::WindowingFunction<float>::hann,
        false);

    juce::dsp::AudioBlock<float> foldBlock(fold);
    juce::dsp::AudioBlock<float> rectBlock(rect);
    juce::dsp::AudioBlock<float> hannBlock(hann);

    // Apply the fold window shape to the fold windows
    rectBlock.multiplyBy(windowShape);
    hannBlock.multiplyBy(1.0f - windowShape);

    // Sum the rectangular and Hann windows to create the fold window
    foldBlock.replaceWithSumOf(rectBlock, hannBlock);

    // ... and copy the window shapes to the member variables
    for (size_t i = 0; i < maxNumDelayProcsPerBand; ++i)
    {
        // Gain to match the potential reduction in window size
        windowValues[i] = fold.getSample(0, i) /* *
                          (maxNumDelayProcsPerBand / static_cast<float>(winSize))*/;
    }
}

void FoldWindowAnimation::paint(juce::Graphics &g)
{
    // Fill the background
    g.fillAll(findColour(backgroundColourId));

    // Define the drawing area
    const float width = static_cast<float>(getWidth());
    const float height = static_cast<float>(getHeight());
    const float padding = 10.0f;

    // Drawing area
    const float drawWidth = width - 2.0f * padding;
    const float drawHeight = height;

    // If we have window data, draw it
    if (!windowValues.empty())
    {
        // Set the line thickness and color for the window function
        g.setColour(findColour(windowColourId));

        // Calculate how much horizontal space each sample takes
        const float xStep = drawWidth / static_cast<float>(windowValues.size() - 1);

        // Create a path for the window function
        juce::Path windowPath;

        // Start at the bottom left
        windowPath.startNewSubPath(padding, drawHeight);

        // Add points for each window value
        for (size_t i = 0; i < windowValues.size(); ++i)
        {
            float x = padding + static_cast<float>(i) * xStep;
            // Window values are in the range [0, 1], so we invert and scale to the draw height
            float y = drawHeight - (windowValues[i] * drawHeight);
            windowPath.lineTo(x, y);
        }

        // Close the path back to the bottom
        windowPath.lineTo(padding + drawWidth, drawHeight);
        windowPath.closeSubPath();

        // Fill the path with a gradient
        juce::ColourGradient gradient(
            findColour(windowColourId), padding, 0,
            findColour(windowColourId).withAlpha(0.3f), padding, drawHeight,
            false);
        g.setGradientFill(gradient);
        g.fillPath(windowPath);

        // Draw the outline
        g.setColour(findColour(windowColourId));
        g.strokePath(windowPath, juce::PathStrokeType(2.0f));
    }
}

// FoldWindowViewItem implementation
FoldWindowViewItem::FoldWindowViewItem(foleys::MagicGUIBuilder &builder, const juce::ValueTree &node)
    : foleys::GuiItem(builder, node)
{
    // Create the colour names to have them configurable
    setColourTranslation({
        {"fold-window-background", FoldWindowAnimation::backgroundColourId},
        {"fold-window-line", FoldWindowAnimation::windowColourId}
    });

    // Add the animation component to the view
    addAndMakeVisible(foldAnimation);
}

std::vector<foleys::SettableProperty> FoldWindowViewItem::getSettableProperties() const
{
    std::vector<foleys::SettableProperty> properties;

    // Add a property to set the window size
    properties.push_back({configNode, "windowSize", foleys::SettableProperty::Number, 0.0f, {}});
    // Add a property to set the window shape
    properties.push_back({configNode, "windowShape", foleys::SettableProperty::Number, 0.0f, {}});
    // Add a property to set the window position
    properties.push_back({configNode, "windowPos", foleys::SettableProperty::Number, 0.0f, {}});

    return properties;
}

void FoldWindowViewItem::update()
{
    // Get the window properties
    auto windowSizeValue = getProperty("windowSize");
    auto windowShapeValue = getProperty("windowShape");
    auto windowPosValue = getProperty("windowPos");
    if (windowSizeValue.isVoid() == false)
    {
        foldAnimation.setWindowSize(static_cast<float>(windowSizeValue));
    }
    if (windowShapeValue.isVoid() == false)
    {
        foldAnimation.setWindowShape(static_cast<float>(windowShapeValue));
    }
    if (windowPosValue.isVoid() == false)
    {
        foldAnimation.setWindowPosition(static_cast<float>(windowPosValue));
    }

    // Update the fold window with the new parameters
    foldAnimation.updateFoldWindow();
}

juce::Component* FoldWindowViewItem::getWrappedComponent()
{
    return &foldAnimation;
}