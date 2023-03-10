/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class MdaDubDelayAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
                            , private juce::ValueTree::Listener
{
public:
    //==============================================================================
    MdaDubDelayAudioProcessor();
    ~MdaDubDelayAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts { *this, &undoManager, "Parameters", createParameterLayout() };
    
    void reset() override;


private:
    
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override
    {
        parametersChanged.store(true);
    }
    std::atomic<bool> parametersChanged { false };

    juce::UndoManager undoManager;

    
    juce::AudioParameterFloat* delayParam;
    juce::AudioParameterFloat* feedbackParam;
    juce::AudioParameterFloat* feedbackToneParam;
    juce::AudioParameterFloat* lfoDepthParam;
    juce::AudioParameterFloat* lfoRateParam;
    juce::AudioParameterFloat* wetMixParam;
    juce::AudioParameterFloat* outputParam;
    
    juce::LinearSmoothedValue<float> outputLevelSmoother;

    float *mybuffer = nullptr; // delay
    long allocatedBufferSize = 0;
    long ipos = 0; // delay max time, pointer, left time, right time
    
    float wet, dry, fbk; // wet & dry mix
    float lmix, hmix; // low & high mix
    float fil; // crossover filter coeff
    float fil0; // crossover filter buffer
    float env, rel; // limiter (clipper when release is instant)
    float del, mod, phi, dphi; // lfo
    float dlbuf; // smoothed modulated delay

    void update(float fs);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MdaDubDelayAudioProcessor)
    
};
