/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

const float kMaxDelayTime = 16.0f;    // in seconds


// Returns a typed pointer to a juce::AudioParameterXXX object from the APVTS.
template<typename T>
inline static void castParameter(juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id, T& destination)
{
    destination = dynamic_cast<T>(apvts.getParameter(id.getParamID()));
    jassert(destination);
}

//==============================================================================
MdaDubDelayAudioProcessor::MdaDubDelayAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::mono(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::mono(), true)
                     #endif
                       )
#endif
{
    castParameter(apvts, ParameterID::delay, delayParam);
    castParameter(apvts, ParameterID::feedback, feedbackParam);
    castParameter(apvts, ParameterID::feedbackMode, feedbackModeParam);
    castParameter(apvts, ParameterID::feedbackTone, feedbackToneParam);
    castParameter(apvts, ParameterID::lfoDepth, lfoDepthParam);
    castParameter(apvts, ParameterID::lfoRate, lfoRateParam);
    castParameter(apvts, ParameterID::wetMix, wetMixParam);
    castParameter(apvts, ParameterID::output, outputParam);
    apvts.state.addListener(this);

    mybuffer = NULL;
    allocatedBufferSize = 0;

}

MdaDubDelayAudioProcessor::~MdaDubDelayAudioProcessor()
{
    apvts.state.removeListener(this);
    
    if (mybuffer != NULL) {
        free(mybuffer);
    }
    mybuffer = NULL;
    allocatedBufferSize = 0;
}

//==============================================================================
const juce::String MdaDubDelayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MdaDubDelayAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool MdaDubDelayAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool MdaDubDelayAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double MdaDubDelayAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MdaDubDelayAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int MdaDubDelayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MdaDubDelayAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String MdaDubDelayAudioProcessor::getProgramName (int index)
{
    return {};
}

void MdaDubDelayAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void MdaDubDelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    long newSize = (long) (kMaxDelayTime * sampleRate);
    if (newSize != allocatedBufferSize)
    {
        if (mybuffer != NULL) {
            free(mybuffer);
        }
        allocatedBufferSize = newSize;
        mybuffer = (float*) malloc((allocatedBufferSize + 2) * sizeof(float));    // spare just in case!
    }
    reset();
}

void MdaDubDelayAudioProcessor::reset() {
    if (mybuffer != NULL) {
        memset(mybuffer, 0, allocatedBufferSize * sizeof(float));
    }
    
    ipos = 0;
    fil0 = 0.0f;
    env = 0.0f;
    phi = 0.0f;
    dlbuf = 0.0f;
}

void MdaDubDelayAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MdaDubDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void MdaDubDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    auto mainInputOutput = getBusBuffer (buffer, true, 0);
    
    auto fs = getSampleRate();
    auto delayValue = delayParam->get();
    auto feedbackModeValue = feedbackModeParam->getCurrentChoiceName();
    auto feedbackValue = feedbackParam->get();
    auto feedbackToneValue = feedbackToneParam->get();
    auto lfoDepthValue = lfoDepthParam->get();
    auto lfoRateValue = lfoRateParam->get();
    auto wetMixValue = wetMixParam->get();
    auto outputValue = outputParam->get();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    
    float del = delayValue * 0.001f * fs;
    if (del > (float)allocatedBufferSize) {
        del = (float)allocatedBufferSize;
    }
    float mod = 0.049f * lfoDepthValue * 0.01f * del;
    float lmix, hmix;
    float fil = feedbackToneValue * 0.01f;
    if (fil > 0.0f) {
        //simultaneously change crossover frequency & high/low mix
        fil = 0.25f * fil;
        lmix = -2.0f * fil;
        hmix = 1.0f;
    } else {
        hmix = fil + 1.0f;
        lmix = 1.0f - hmix;
    }
    fil = expf(-6.2831853f * powf(10.0f, 2.2f + 4.5f * fil) / fs);

    float fbk = feedbackValue * 0.01f;
    float rel = feedbackModeValue == "Limit" ? 0.8f : 0.9997f;
    float wet = 1.0f - wetMixValue * 0.01f;
    wet = outputValue *0.5f * (1.0f - wet * wet); //-3dB at 50% mix
    float dry = outputValue * (1.0f - (wetMixValue * 0.01f) * (wetMixValue * 0.01f));

    float dphi = 628.31853f * lfoRateValue / fs; //100-sample steps



    float ol, w=wet, y=dry, fb=fbk, dl=dlbuf, db=dlbuf, ddl = 0.0f;
    float lx=lmix, hx=hmix, f=fil, f0=fil0, tmp;
    float e=env, g, r=rel; //limiter envelope, gain, release
    float twopi=6.2831853f;
    long    i=ipos, l, s=allocatedBufferSize, k=0;
    
    for (auto samp=0; samp < buffer.getNumSamples(); samp++)
    {
        if (k==0) //update delay length at slower rate (could be improved!)
        {
            db += 0.01f * (del - db - mod - mod * sinf(phi)); //smoothed delay+lfo
            ddl = 0.01f * (db - dl); //linear step
            phi+=dphi; if(phi>twopi) phi-=twopi;
            k=100;
        }
        k--;
        dl += ddl; //lin interp between points
 
        i--; if(i<0) i=s; //delay positions
        
        l = (long)dl;
        tmp = dl - (float)l; //remainder
        l += i; if(l>s) l-=(s+1);
        
        ol = *(mybuffer + l); //delay output
        
        l++; if(l>s) l=0;
        ol += tmp * (*(mybuffer + l) - ol); //lin interp

        //tmp = inSourceP[samp] + fb * ol; //mix input (left only!) & feedback
        tmp = *mainInputOutput.getReadPointer (0, samp) + fb * ol;
        
        
        f0 = f * (f0 - tmp) + tmp; //low-pass filter
        tmp = lx * f0 + hx * tmp;
        
        g =(tmp<0.0f)? -tmp : tmp; //simple limiter
        e *= r; if(g>e) e = g;
        if(e>1.0f) tmp /= e;

        *(mybuffer + i) = tmp; //delay input
        
        ol *= w; //wet

        //inDestP[samp] = y * inSourceP[samp] + ol; //dry
        *mainInputOutput.getWritePointer (0, samp) = y * *mainInputOutput.getReadPointer (0, samp) + ol;
    }
    
    ipos = i;
    dlbuf=dl;
    
    /*
      //trap denormals
    if (fabsf(f0)<1.0e-10f) {
        fil0=0.0f;
        env=0.0f;
    } else {
        fil0=f0;
        env = e;
    }
    */
    
    
}

//==============================================================================
bool MdaDubDelayAudioProcessor::hasEditor() const {
    return true;
}

juce::AudioProcessorEditor* MdaDubDelayAudioProcessor::createEditor()
{
    return new MdaDubDelayAudioProcessorEditor (*this, apvts);
}

//==============================================================================
void MdaDubDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MdaDubDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr) {
        if (xmlState->hasTagName (apvts.state.getType())) {
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MdaDubDelayAudioProcessor();
}

juce::AudioProcessorValueTreeState::ParameterLayout MdaDubDelayAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::delay,
        "Delay",
        juce::NormalisableRange<float>(0.0f, kMaxDelayTime * 1000.0f, 1.0f),
        500.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::feedback,
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        44.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        ParameterID::feedbackMode,
        "Feedback Mode",
        juce::StringArray { "Limit", "Saturate" },
        1));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::feedbackTone,
        "Feedback Tone",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f),
        -20.0f,
        juce::AudioParameterFloatAttributes().withLabel("low <-> high")));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::lfoDepth,
        "LFO Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::lfoRate,
        "LFO Rate",
        juce::NormalisableRange<float>(0.01f, 10.0f, 0.01f),
        3.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::wetMix,
        "FX Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        33.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::output,
        "Out Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.1f),
        0.5f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    
    return layout;
}
