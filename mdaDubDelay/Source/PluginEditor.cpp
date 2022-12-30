/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

enum
{
    paramControlHeight = 40,
    paramLabelWidth    = 120,
    paramSliderWidth   = 300,
    uiRows = 8 // for calculating label spacing of parameters + comment/copyright label
};

//==============================================================================
MdaDubDelayAudioProcessorEditor::MdaDubDelayAudioProcessorEditor (MdaDubDelayAudioProcessor& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p), audioProcessor (p), apvts (vts)
{
    delayLabel.setText("Delay", juce::dontSendNotification);
    addAndMakeVisible(delayLabel);
    addAndMakeVisible(delaySlider);
    delayAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment (apvts, "delay", delaySlider));
    
    feedbackLabel.setText("Feedback", juce::dontSendNotification);
    addAndMakeVisible(feedbackLabel);
    addAndMakeVisible(feedbackSlider);
    feedbackAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment (apvts, "feedback", feedbackSlider));
    
    feedbackToneLabel.setText("Feedback Tone", juce::dontSendNotification);
    addAndMakeVisible(feedbackToneLabel);
    addAndMakeVisible(feedbackToneSlider);
    feedbackToneAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment (apvts, "feedbackTone", feedbackToneSlider));
    
    lfoDepthLabel.setText("LFO Depth", juce::dontSendNotification);
    addAndMakeVisible(lfoDepthLabel);
    addAndMakeVisible(lfoDepthSlider);
    lfoDepthAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment (apvts, "lfoDepth", lfoDepthSlider));
    
    lfoRateLabel.setText("LFO Rate", juce::dontSendNotification);
    addAndMakeVisible(lfoRateLabel);
    addAndMakeVisible(lfoRateSlider);
    lfoRateAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment (apvts, "lfoRate", lfoRateSlider));
    
    wetMixLabel.setText("FX Mix", juce::dontSendNotification);
    addAndMakeVisible(wetMixLabel);
    addAndMakeVisible(wetMixSlider);
    wetMixAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment (apvts, "wetMix", wetMixSlider));
    
    outputLabel.setText("Output Level", juce::dontSendNotification);
    addAndMakeVisible(outputLabel);
    addAndMakeVisible(outputSlider);
    outputAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment (apvts, "output", outputSlider));
    
    setSize (paramSliderWidth + paramLabelWidth, juce::jmax (100, paramControlHeight * uiRows));
}

MdaDubDelayAudioProcessorEditor::~MdaDubDelayAudioProcessorEditor()
{
}

//==============================================================================
void MdaDubDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    auto r = getLocalBounds();
    auto sliderHeight = r.getHeight() / uiRows;
    auto labelRect = r.removeFromBottom(sliderHeight);
    g.setColour (juce::Colours::white);
    g.setFont (10.0f);
    g.drawFittedText ("mda plugins (c) Paul Kellett - JUCE port by lucaji.github.io", labelRect, juce::Justification::right, 1);
}

void MdaDubDelayAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    auto sliderHeight = r.getHeight() / uiRows;

    auto labelRect = juce::Rectangle<int>(0, 0, paramLabelWidth, sliderHeight);
    auto sliderRect = juce::Rectangle<int>(paramLabelWidth, 0, paramSliderWidth, sliderHeight);

    delayLabel.setBounds(labelRect);
    delaySlider.setBounds(sliderRect);
    
    labelRect.translate(0, sliderHeight);
    sliderRect.translate(0, sliderHeight);
    feedbackLabel.setBounds(labelRect);
    feedbackSlider.setBounds(sliderRect);
    
    labelRect.translate(0, sliderHeight);
    sliderRect.translate(0, sliderHeight);
    feedbackToneLabel.setBounds(labelRect);
    feedbackToneSlider.setBounds(sliderRect);
    
    labelRect.translate(0, sliderHeight);
    sliderRect.translate(0, sliderHeight);
    lfoDepthLabel.setBounds(labelRect);
    lfoDepthSlider.setBounds(sliderRect);
    
    labelRect.translate(0, sliderHeight);
    sliderRect.translate(0, sliderHeight);
    lfoRateLabel.setBounds(labelRect);
    lfoRateSlider.setBounds(sliderRect);
    
    labelRect.translate(0, sliderHeight);
    sliderRect.translate(0, sliderHeight);
    wetMixLabel.setBounds(labelRect);
    wetMixSlider.setBounds(sliderRect);
    
    labelRect.translate(0, sliderHeight);
    sliderRect.translate(0, sliderHeight);
    outputLabel.setBounds(labelRect);
    outputSlider.setBounds(sliderRect);
    
    
}
