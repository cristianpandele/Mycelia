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

//=========================================================================

// FoldWindowAnimation implementation
FoldWindowAnimation::FoldWindowAnimation()
{
    // Set default background color
    setColour(backgroundColourId, juce::Colours::transparentWhite);
    setColour(windowColourId, juce::Colours::green);
    // setColour(gridColourId, juce::Colours::grey.withAlpha(0.3f));
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
        {"fold-window-line", FoldWindowAnimation::windowColourId},
        {"fold-window-grid", FoldWindowAnimation::gridColourId}
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
