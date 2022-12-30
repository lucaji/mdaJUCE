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
    uiRows = 5 // for calculating label spacing of parameters + comment/copyright label
};

//==============================================================================
MdaAmbienceAudioProcessorEditor::MdaAmbienceAudioProcessorEditor (MdaAmbienceAudioProcessor& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p), audioProcessor (p), apvts (vts)
{
    sizeLabel.setText("Size", juce::dontSendNotification);
    addAndMakeVisible(sizeLabel);
    addAndMakeVisible(sizeSlider);
    sizeAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment (apvts, "size", sizeSlider));
    
    hfLabel.setText("Feedback", juce::dontSendNotification);
    addAndMakeVisible(hfLabel);
    addAndMakeVisible(hfSlider);
    hfAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment (apvts, "hf", hfSlider));
    
    mixLabel.setText("FX Mix", juce::dontSendNotification);
    addAndMakeVisible(mixLabel);
    addAndMakeVisible(mixSlider);
    mixAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment (apvts, "mix", mixSlider));
    
    outputLabel.setText("Output Level", juce::dontSendNotification);
    addAndMakeVisible(outputLabel);
    addAndMakeVisible(outputSlider);
    outputAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment (apvts, "output", outputSlider));
    
    setSize (paramSliderWidth + paramLabelWidth, juce::jmax (100, paramControlHeight * uiRows));
}

MdaAmbienceAudioProcessorEditor::~MdaAmbienceAudioProcessorEditor()
{
}

//==============================================================================
void MdaAmbienceAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    auto r = getLocalBounds();
    auto sliderHeight = r.getHeight() / uiRows;
    auto labelRect = r.removeFromBottom(sliderHeight);
    g.setColour (juce::Colours::white);
    g.setFont (10.0f);
    g.drawFittedText ("mda plugins (c) Paul Kellett - JUCE port by lucaji.github.io", labelRect, juce::Justification::right, 1);
}

void MdaAmbienceAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    auto sliderHeight = r.getHeight() / uiRows;

    auto labelRect = juce::Rectangle<int>(0, 0, paramLabelWidth, sliderHeight);
    auto sliderRect = juce::Rectangle<int>(paramLabelWidth, 0, paramSliderWidth, sliderHeight);

    sizeLabel.setBounds(labelRect);
    sizeSlider.setBounds(sliderRect);
    
    labelRect.translate(0, sliderHeight);
    sliderRect.translate(0, sliderHeight);
    hfLabel.setBounds(labelRect);
    hfSlider.setBounds(sliderRect);
    
    labelRect.translate(0, sliderHeight);
    sliderRect.translate(0, sliderHeight);
    mixLabel.setBounds(labelRect);
    mixSlider.setBounds(sliderRect);
    
    labelRect.translate(0, sliderHeight);
    sliderRect.translate(0, sliderHeight);
    outputLabel.setBounds(labelRect);
    outputSlider.setBounds(sliderRect);
}
