/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class MdaAmbienceAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    MdaAmbienceAudioProcessorEditor (MdaAmbienceAudioProcessor&, juce::AudioProcessorValueTreeState& vts);
    ~MdaAmbienceAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    MdaAmbienceAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MdaAmbienceAudioProcessorEditor)
    
    juce::AudioProcessorValueTreeState& apvts;
    
    juce::Label sizeLabel;
    juce::Slider sizeSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sizeAttachment;
    
    juce::Label hfLabel;
    juce::Slider hfSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hfAttachment;
    
    juce::Label mixLabel;
    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    
    juce::Label outputLabel;
    juce::Slider outputSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    
};
