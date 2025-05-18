#include "NetworkGraphAnimation.h"

NetworkGraphAnimation::NetworkGraphAnimation()
{
    // Set default colors
    setColour(backgroundColourId, juce::Colours::transparentWhite);
    setColour(nodeBaseColourId, juce::Colour(0xFF5BA8FF));            // Light blue for young nodes
    setColour(nodeHighAgeColourId, juce::Colour(0xFFFF5733));         // Orange-red for older nodes
    setColour(lineLowWeightColourId, juce::Colour(0x40FFFFFF));       // Transparent white for weak connections
    setColour(lineHighWeightColourId, juce::Colour(0xFFFFFFFF));      // Solid white for strong connections
    setColour(nodeBorderLowLevelColourId, juce::Colour(0xFF2E7D32));  // Green for low buffer levels
    setColour(nodeBorderHighLevelColourId, juce::Colour(0xFFF57F17)); // Amber for high buffer levels
}

void NetworkGraphAnimation::setStretch(float stretch)
{
    // Update stretch level if needed
    this->stretch = stretch;
}

void NetworkGraphAnimation::setNumActiveBands(int numActiveBands)
{
    // Update number of active bands
    this->numActiveBands = numActiveBands;
}

void NetworkGraphAnimation::setBandStates(const std::vector<DelayNodes::BandResources> &states)
{
    // Clear existing snapshots
    bandStateSnapshots.clear();

    // Convert BandResources to our snapshots
    for (const auto &state : states)
    {
        bandStateSnapshots.push_back(BandStateSnapshot::fromBandResources(state));
    }
}

void NetworkGraphAnimation::paint(juce::Graphics &g)
{
    const float width = static_cast<float>(getWidth());
    const float height = static_cast<float>(getHeight());

    // Fill background
    g.fillAll(findColour(backgroundColourId));

    if (bandStateSnapshots.empty())
    {
        return;
    }

    auto numAllocatedBands = std::min(static_cast<int>(bandStateSnapshots.size()), numActiveBands);

    // Calculate vertical spacing based on number of bands
    const float bandHeight = height / static_cast<float>(numAllocatedBands);

    // For each band (row in the matrix)
    for (int bandIdx = 0; bandIdx < numAllocatedBands; ++bandIdx)
    {
        auto &targetBandState = bandStateSnapshots[bandIdx];
        float bandY = bandHeight * bandIdx + bandHeight / 2.0f;

        // Calculate max delay time for x-axis normalization
        float maxDelayTime = 0.0f;
        for (auto time : targetBandState.nodeDelayTimes)
        {
            maxDelayTime = std::max(maxDelayTime, time);
        }

        if (maxDelayTime <= 0.0f)
        {
            maxDelayTime = 1.0f; // Avoid division by zero
        }

        // First pass: Draw the nodes
        const int numAllocatedNodes = static_cast<int>(targetBandState.bufferLevels.size());
        for (int procIdx = 0; procIdx < numAllocatedNodes; ++procIdx)
        {
            // Skip if we're past the allocated processors
            if (procIdx >= numAllocatedNodes)
            {
                continue;
            }

            // Calculate x-position based on processor index for even distribution
            // Use margin of 20px on each side
            const float margin = 20.0f;
            float nodeX;

            if (numAllocatedNodes > 1)
            {
                // Evenly space nodes across the width
                float nodeSpacing = (width - 2.0f * margin) / static_cast<float>(numAllocatedNodes - 1);
                nodeX = margin + (static_cast<float>(procIdx) * nodeSpacing);
            }
            else
            {
                // Center a single node
                nodeX = width / 2.0f;
            }

            // Node position
            juce::Point<float> nodePos(nodeX, bandY);

            // Get buffer level for this node
            float bufferLevel = targetBandState.bufferLevels[procIdx];

            // Get the current age of this node processor
            float nodeAge = 0.0f;
            //targetBandState.delayProcs[procIdx]->getAge();

            // Map age to node fill color
            juce::Colour nodeColor = mapValueToColour(nodeAge,
                                                      findColour(nodeBaseColourId),
                                                      findColour(nodeHighAgeColourId));

            // Map buffer level to border color
            juce::Colour borderColor = mapValueToColour(bufferLevel,
                                                        findColour(nodeBorderLowLevelColourId),
                                                        findColour(nodeBorderHighLevelColourId));

            // Draw node
            g.setColour(nodeColor);
            g.fillEllipse(nodeX - nodeRadius, bandY - nodeRadius,
                          nodeRadius * 2.0f, nodeRadius * 2.0f);

            // Draw border
            g.setColour(borderColor);
            g.drawEllipse(nodeX - nodeRadius, bandY - nodeRadius,
                          nodeRadius * 2.0f, nodeRadius * 2.0f, 2.0f);

            // Draw node index for clarity
            g.setColour(juce::Colours::white);
            g.setFont(nodeRadius * 1.2f);
            g.drawText(juce::String(procIdx),
                       nodeX - nodeRadius, bandY - nodeRadius,
                       nodeRadius * 2.0f, nodeRadius * 2.0f,
                       juce::Justification::centred, false);
        }

        // Second pass: Draw the connections
        if (!targetBandState.interNodeConnections.empty())
        {
            for (int destinationIdx = 0; destinationIdx < targetBandState.interNodeConnections.size(); ++destinationIdx)
            {
                // Skip if we're past the nodes we've already drawn
                if (destinationIdx >= numAllocatedNodes)
                {
                    continue;
                }

                auto &targetNode = targetBandState.interNodeConnections[destinationIdx];

                // For each destination node this destination connects to
                for (int sourceBandIdx = 0; sourceBandIdx < targetNode.size(); ++sourceBandIdx)
                {
                    auto &sourceBand = targetNode[sourceBandIdx];

                    // For each processor in the source band
                    for (int sourceProcIdx = 0; sourceProcIdx < sourceBand.size(); ++sourceProcIdx)
                    {
                        // Get connection weight
                        float connectionWeight = sourceBand[sourceProcIdx];

                        // Skip if there's no connection
                        if (connectionWeight <= 0.001f)
                        {
                            continue;
                        }

                        // Calculate source node position using the same logic as above
                        const int numSourceNodes = (sourceBandIdx < bandStateSnapshots.size()) ?
                                                    static_cast<int>(sourceBand.size())
                                                    : 0;

                        if (sourceProcIdx >= numSourceNodes)
                        {
                            continue;
                        }

                        // Skip self-connections
                        if (sourceBandIdx == bandIdx && sourceProcIdx == destinationIdx)
                        {
                            continue;
                        }

                        // Calculate source node position
                        float sourceNodeX = 0.0f;
                        const float margin = 20.0f;
                        if (numSourceNodes > 1)
                        {
                            // Evenly space nodes across the width
                            float nodeSpacing = (width - 2.0f * margin) / static_cast<float>(numSourceNodes - 1);
                            sourceNodeX = margin + (static_cast<float>(sourceProcIdx) * nodeSpacing);
                        }
                        else
                        {
                            // Center a single node
                            sourceNodeX = width / 2.0f;
                        }

                        // Calculate target node position
                        float targetNodeX = 0.0f;
                        if (numAllocatedNodes > 1)
                        {
                            // Evenly space nodes across the width
                            float nodeSpacing = (width - 2.0f * margin) / static_cast<float>(numAllocatedNodes - 1);
                            targetNodeX = margin + (static_cast<float>(destinationIdx) * nodeSpacing);
                        }
                        else
                        {
                            // Center a single node
                            targetNodeX = width / 2.0f;
                        }
                        float sourceBandY = bandHeight * sourceBandIdx + bandHeight / 2.0f;
                        float targetBandY = bandHeight * bandIdx + bandHeight / 2.0f;
                        juce::Point<float> sourcePos(sourceNodeX, sourceBandY);
                        juce::Point<float> targetPos(targetNodeX, targetBandY);

                        // Get buffer levels for source and target
                        float sourceLevel = (sourceBandIdx < targetBandState.bufferLevels.size()) ?
                                                targetBandState.bufferLevels[sourceProcIdx]
                                                : 0.0f;
                        float targetLevel = (bandIdx < bandStateSnapshots.size() && destinationIdx < targetBandState.bufferLevels.size()) ?
                                                targetBandState.bufferLevels[destinationIdx]
                                                : 0.0f;

                        // Draw the connection
                        drawNodeConnection(g, sourcePos, targetPos,
                                           connectionWeight, sourceLevel, targetLevel);
                    }
                }
            }
        }
    }
}

void NetworkGraphAnimation::drawNodeConnection(juce::Graphics &g, juce::Point<float> start,
                                               juce::Point<float> end, float weight,
                                               float startLevel, float endLevel)
{
    // Map connection weight to line color
    juce::Colour lineColor = mapValueToColour(weight,
                                              findColour(lineLowWeightColourId),
                                              findColour(lineHighWeightColourId));

    // Map connection weight to line thickness (0.5 to 3.0)
    float lineThickness = 0.5f + weight * 2.5f;

    // Create curved path for connection
    juce::Path path;
    path.startNewSubPath(start);

    // Calculate control points for bezier curve
    // We want to create a nice arc between nodes
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float controlOffset = std::min(150.0f, std::abs(dx) * 0.8f);

    juce::Point<float> control1(start.x + controlOffset, start.y);
    juce::Point<float> control2(end.x - controlOffset, end.y);

    // If nodes are on the same band (horizontal connection), use different control points
    if (std::abs(dy) < 1.0f)
    {
        float vertOffset = 5.0f + std::abs(dx) * 0.2f;
        // Alternate the direction of the curve based on position
        if ((start.x + end.x) / 2.0f > getWidth() / 2.0f)
        {
            vertOffset = -vertOffset;
        }
        control1.setY(start.y + vertOffset);
        control2.setY(end.y - vertOffset);
    }

    path.cubicTo(control1, control2, end);

    // Draw the path
    g.setColour(lineColor);
    g.strokePath(path, juce::PathStrokeType(lineThickness, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
}

juce::Colour NetworkGraphAnimation::mapValueToColour(float value, juce::Colour startColour, juce::Colour endColour)
{
    // Ensure value is in [0, 1] range
    value = juce::jlimit(0.0f, 1.0f, value);

    // Linear interpolation between colors
    return startColour.interpolatedWith(endColour, value);
}


NetworkGraphViewItem::NetworkGraphViewItem(foleys::MagicGUIBuilder &builder, const juce::ValueTree &node)
    : foleys::GuiItem(builder, node)
{
    // Set up color translation for the GUI editor
    setColourTranslation({{"network-background", NetworkGraphAnimation::backgroundColourId},
                          {"node-base-color", NetworkGraphAnimation::nodeBaseColourId},
                          {"node-high-age-color", NetworkGraphAnimation::nodeHighAgeColourId},
                          {"line-low-weight-color", NetworkGraphAnimation::lineLowWeightColourId},
                          {"line-high-weight-color", NetworkGraphAnimation::lineHighWeightColourId},
                          {"node-border-low-level-color", NetworkGraphAnimation::nodeBorderLowLevelColourId},
                          {"node-border-high-level-color", NetworkGraphAnimation::nodeBorderHighLevelColourId}});

    // Add the animation component to this item
    addAndMakeVisible(networkAnimation);
}

std::vector<foleys::SettableProperty> NetworkGraphViewItem::getSettableProperties() const
{
    std::vector<foleys::SettableProperty> properties;

    // Add any properties that need to be configured in the GUI editor

    return properties;
}

void NetworkGraphViewItem::update()
{
    // This will be called when properties change
    // We don't need any special handling here since the component
    // will be updated in the Mycelia class
}

juce::Component *NetworkGraphViewItem::getWrappedComponent()
{
    return &networkAnimation;
}
