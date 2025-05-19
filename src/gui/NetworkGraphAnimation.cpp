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

inline float NetworkGraphAnimation::getBandMargin(const float canvasWidth, int numBands, int targetBandIdx)
{
    auto bandUsedWidth = getBandUsedWidth(canvasWidth);
    // 0.94f is to account for the 6% offset of each band (but assume the last node is before the reserved end of the band)
    auto bandLeftMargin = (canvasWidth - bandUsedWidth * 0.94f) / 2.0f;
    // Shift the left margin based on band index
    bandLeftMargin += (((static_cast<float>(numBands) / 2.0f) - static_cast<float>(targetBandIdx)) * 0.06f) * canvasWidth;
    return bandLeftMargin;
}

inline float NetworkGraphAnimation::getBandY(int targetBandIdx, int numAllocatedBands)
{
    const float canvasHeight = static_cast<float>(getHeight());

    const float bandHeight = canvasHeight / static_cast<float>(numAllocatedBands);
    // Use power function to create non-linear distribution
    const float power = 1.4f;

    // Handle special case of only one band
    if (numAllocatedBands <= 1)
    {
        return canvasHeight / 2.0f;
    }

    // Normalize the band index to [0,1] range
    float normalizedIdx = static_cast<float>(targetBandIdx) / static_cast<float>(numAllocatedBands - 1);
    float adjustedNormalizedIdx = std::exp(-normalizedIdx * power);

    // Map back to full height range, and offset to center bands vertically
    return (bandHeight / 3.0f) + adjustedNormalizedIdx * (canvasHeight * 0.62f);
}

inline float NetworkGraphAnimation::getBandUsedWidth(const float canvasWidth)
{
    // Calculate width of the canvas used for nodes based on stretch
    // When stretch is 0, use full width; when stretch is 1, use quarter width
    float stretchFactor = std::abs(stretch - 0.5f) * 2.0f; // Convert 0-1 range from unipolar to bipolar
    stretchFactor = juce::jlimit(0.0f, 1.0f, stretchFactor);

    // Calculate the actual width we'll use (from full width to quarter width)
    return canvasWidth * (0.25f + stretchFactor * 0.5f);
}

inline float NetworkGraphAnimation::getNodeX(float positionProportion, int numBandNodes, float bandLeftMargin, float canvasWidth)
{

    float bandUsedWidth = getBandUsedWidth(canvasWidth);
    float nodeX = bandLeftMargin + (positionProportion * bandUsedWidth);
    return nodeX;
}

void NetworkGraphAnimation::paint(juce::Graphics &g)
{
    const float canvasWidth = static_cast<float>(getWidth());

    // Fill background
    g.fillAll(findColour(backgroundColourId));

    if (bandStateSnapshots.empty())
    {
        return;
    }

    auto numAllocatedBands = std::min(static_cast<int>(bandStateSnapshots.size()), numActiveBands);

    // For each band (row in the matrix)
    for (int targetBandIdx = 0; targetBandIdx < numAllocatedBands; ++targetBandIdx)
    {
        auto &targetBandState = bandStateSnapshots[targetBandIdx];
        // Get the Y position of the band
        float bandY = getBandY(targetBandIdx, numAllocatedBands);
        // Calculate the left margin to center the nodes
        float bandLeftMargin = getBandMargin(canvasWidth, numAllocatedBands, targetBandIdx);

        // Calculate total and max delay time for x-axis mapping
        float totalDelayTime = 0.0f;
        for (auto time : targetBandState.nodeDelayTimes)
        {
            totalDelayTime += time;
        }

        // First pass: Draw the nodes
        const int numAllocatedNodes = static_cast<int>(targetBandState.bufferLevels.size());

        // Track accumulated delay for positioning
        float bandAccumulatedDelay = 0.0f;

        for (int targetProcIdx = 0; targetProcIdx < numAllocatedNodes; ++targetProcIdx)
        {
            // Get the delay time for this node
            float nodeDelayTime = targetProcIdx < targetBandState.nodeDelayTimes.size() ?
                                  targetBandState.nodeDelayTimes[targetProcIdx]
                                  : 0.0f;

            // Calculate x-position based on proportional delay time
            float nodeX = canvasWidth / 2.0f; // Default to center

            if (totalDelayTime > 0.0f && numAllocatedNodes > 1)
            {
                // Position based on accumulated delay proportion
                float positionProportion;

                if (targetProcIdx == 0)
                {
                    // First node position
                    positionProportion = 0.0f;
                } else if (targetProcIdx == numAllocatedNodes - 1)
                {
                    // Last node position
                    positionProportion = 1.0f;
                } else
                {
                    // Middle nodes positioned proportionally to delay time
                    bandAccumulatedDelay += nodeDelayTime;
                    positionProportion = bandAccumulatedDelay / totalDelayTime;
                }

                // Apply the position within the available width
                nodeX = getNodeX(positionProportion, numAllocatedNodes, bandLeftMargin, canvasWidth);
            }

            // Node position
            juce::Point<float> nodePos(nodeX, bandY);

            // Get buffer level for this node
            float bufferLevel = targetBandState.bufferLevels[targetProcIdx];

            // Get the current age of this node processor
            float nodeAge = 0.0f;
            //targetBandState.delayProcs[targetProcIdx]->getAge();

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
            g.drawText(juce::String(targetProcIdx),
                       nodeX - nodeRadius, bandY - nodeRadius,
                       nodeRadius * 2.0f, nodeRadius * 2.0f,
                       juce::Justification::centred, false);
        }

        // Second pass: Draw the connections
        if (!targetBandState.interNodeConnections.empty())
        {
            float targetBandAccumulatedDelay = 0.0f;
            for (int targetProcIdx = 0; targetProcIdx < targetBandState.interNodeConnections.size(); ++targetProcIdx)
            {
                // Skip if we're past the nodes we've already drawn
                if (targetProcIdx >= numAllocatedNodes)
                {
                    continue;
                }

                // Calculate accumulated delay up to this processor
                targetBandAccumulatedDelay += targetBandState.nodeDelayTimes[targetProcIdx];

                auto &targetNode = targetBandState.interNodeConnections[targetProcIdx];

                // For each source node this destination connects to
                for (int sourceBandIdx = 0; sourceBandIdx < targetNode.size(); ++sourceBandIdx)
                {
                    auto &sourceBand = targetNode[sourceBandIdx];

                    // Skip if source band is out of bounds
                    if (sourceBandIdx >= numAllocatedBands || sourceBandIdx >= bandStateSnapshots.size())
                    {
                        continue;
                    }

                    auto &sourceBandState = bandStateSnapshots[sourceBandIdx];

                    // Calculate total delay time for the source band
                    float sourceTotalDelayTime = 0.0f;
                    for (auto time : sourceBandState.nodeDelayTimes)
                    {
                        sourceTotalDelayTime += time;
                    }

                    // Get the number of nodes in source band
                    int numSourceNodes = static_cast<int>(sourceBandState.bufferLevels.size());

                    float sourceBandAccumulatedDelay = 0.0f;
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

                        if (sourceProcIdx >= numSourceNodes)
                        {
                            continue;
                        }

                        // Skip self-connections
                        if (sourceBandIdx == targetBandIdx && sourceProcIdx == targetProcIdx)
                        {
                            continue;
                        }

                        // Calculate source node position using proportional delay time
                        float sourceNodeX = canvasWidth / 2.0f; // Default to center;

                        sourceBandAccumulatedDelay += sourceBandState.nodeDelayTimes[sourceProcIdx];

                        if (sourceTotalDelayTime > 0.0f && numSourceNodes > 1)
                        {
                            float positionProportion;

                            if (sourceProcIdx == 0)
                            {
                                positionProportion = 0.0f;
                            }
                            else
                            {
                                // Position based on accumulated delay proportion
                                positionProportion = sourceBandAccumulatedDelay / sourceTotalDelayTime;
                            }

                            // Calculate the left margin to center the nodes
                            float sourceBandLeftMargin = getBandMargin(canvasWidth, numAllocatedBands, sourceBandIdx);
                            // Calculate the x-position of the source node
                            sourceNodeX = getNodeX(positionProportion, numSourceNodes, sourceBandLeftMargin, canvasWidth);
                        }

                        // Calculate target node position
                        float targetNodeX = canvasWidth / 2.0f; // Default to center

                        if (totalDelayTime > 0.0f && numAllocatedNodes > 1)
                        {
                            float positionProportion;

                            if (targetProcIdx == 0)
                            {
                                positionProportion = 0.0f;
                            }
                            else
                            {
                                // Position based on accumulated delay proportion
                                positionProportion = targetBandAccumulatedDelay / totalDelayTime;
                            }

                            // Calculate the left margin to center the nodes
                            float targetBandLeftMargin = getBandMargin(canvasWidth, numAllocatedBands, targetBandIdx);
                            // Calculate the x-position of the source node
                            targetNodeX = getNodeX(positionProportion, numAllocatedNodes, targetBandLeftMargin, canvasWidth);
                        }

                        float sourceBandY = getBandY(sourceBandIdx, numAllocatedBands);
                        float targetBandY = getBandY(targetBandIdx, numAllocatedBands);
                        juce::Point<float> sourcePos(sourceNodeX, sourceBandY);
                        juce::Point<float> targetPos(targetNodeX, targetBandY);

                        // Get buffer levels for source and target
                        float sourceLevel = (sourceBandIdx < sourceBandState.bufferLevels.size() && sourceProcIdx < sourceBandState.bufferLevels.size()) ?
                                                sourceBandState.bufferLevels[sourceProcIdx]
                                                : 0.0f;
                        float targetLevel = (targetBandIdx < bandStateSnapshots.size() && targetProcIdx < targetBandState.bufferLevels.size()) ?
                                                targetBandState.bufferLevels[targetProcIdx]
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
