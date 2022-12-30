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
class MdaDubDelayAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    MdaDubDelayAudioProcessorEditor (MdaDubDelayAudioProcessor&, juce::AudioProcessorValueTreeState& vts);
    ~MdaDubDelayAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    MdaDubDelayAudioProcessor& audioProcessor;

    juce::AudioProcessorValueTreeState& apvts;
    
    juce::Label delayLabel;
    juce::Slider delaySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAttachment;
    
    juce::Label feedbackLabel;
    juce::Slider feedbackSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;
    
    juce::Label feedbackToneLabel;
    juce::Slider feedbackToneSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackToneAttachment;
    
    juce::Label lfoDepthLabel;
    juce::Slider lfoDepthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoDepthAttachment;
    
    juce::Label lfoRateLabel;
    juce::Slider lfoRateSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateAttachment;
    
    juce::Label wetMixLabel;
    juce::Slider wetMixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wetMixAttachment;
    
    juce::Label outputLabel;
    juce::Slider outputSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MdaDubDelayAudioProcessorEditor)
};
