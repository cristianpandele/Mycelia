#pragma once

#include "Mycelia.h"
#include "BinaryData.h"

//==============================================================================
class MyceliaEditor : public juce::AudioProcessorEditor
{
public:
    explicit MyceliaEditor (Mycelia&);
    ~MyceliaEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    Mycelia& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MyceliaEditor)
};
