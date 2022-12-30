/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

constexpr float kMaxDelayTime = 16.0f; // in seconds

namespace ParameterID
{
    #define PARAMETER_ID(str) const juce::ParameterID str(#str, 1);

    PARAMETER_ID(delay)
    PARAMETER_ID(feedback)
    PARAMETER_ID(feedbackTone)
    PARAMETER_ID(lfoDepth)
    PARAMETER_ID(lfoRate)
    PARAMETER_ID(wetMix)
    PARAMETER_ID(output)

    #undef PARAMETER_ID
}

#ifdef DEBUG
inline void checkSample(float& x) {
    if (std::isnan(x)) {
        DBG("!!! WARNING: nan detected in audio buffer, silencing !!!");
        x = 0.0f;
    } else if (std::isinf(x)) {
        DBG("!!! WARNING: inf detected in audio buffer, silencing !!!");
        x = 0.0f;
    } else if (x < -2.0f || x > 2.0f) {  // screaming feedback
        DBG("!!! WARNING: sample out of range, silencing !!!");
        x = 0.0f;
    } else if (x < -1.0f) {
        x = -1.0f;
    } else if (x > 1.0f) {
        x = 1.0f;
    }
}
#endif

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
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    castParameter(apvts, ParameterID::delay, delayParam);
    castParameter(apvts, ParameterID::feedback, feedbackParam);
    castParameter(apvts, ParameterID::feedbackTone, feedbackToneParam);
    castParameter(apvts, ParameterID::lfoDepth, lfoDepthParam);
    castParameter(apvts, ParameterID::lfoRate, lfoRateParam);
    castParameter(apvts, ParameterID::wetMix, wetMixParam);
    castParameter(apvts, ParameterID::output, outputParam);
    apvts.state.addListener(this);
    reset();
}

MdaDubDelayAudioProcessor::~MdaDubDelayAudioProcessor()
{
    apvts.state.removeListener(this);
    if (mybuffer) {
        delete [] mybuffer;
    }
    mybuffer = nullptr;
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
            delete [] mybuffer;
        }
        allocatedBufferSize = newSize;
        mybuffer = new float[allocatedBufferSize];
    }
    parametersChanged.store(true);
    reset();
}

void MdaDubDelayAudioProcessor::reset() {
    if (mybuffer != nullptr) {
        memset(mybuffer, 0, allocatedBufferSize * sizeof(float));
    }
    outputLevelSmoother.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(outputParam->get()));
}

void MdaDubDelayAudioProcessor::releaseResources()
{
    
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

// recalculate on changed parameters
void MdaDubDelayAudioProcessor::update(float fs)
{
    // get normalised values
    auto delayValue = apvts.getParameter("delay")->getValue();
    auto lfoDepthValue = apvts.getParameter("lfoDepth")->getValue();
    auto feedbackToneValue = apvts.getParameter("feedbackTone")->getValue();
    auto feedbackValue = apvts.getParameter("feedback")->getValue();
    auto wetMixValue = apvts.getParameter("wetMix")->getValue();
    auto outputValue = juce::Decibels::decibelsToGain(apvts.getParameter("output")->getValue());

    del = delayValue * delayValue * allocatedBufferSize;
    if (del > (float)allocatedBufferSize)
        del = (float)allocatedBufferSize;
    mod = 0.049f * lfoDepthValue * del;
    
    fil = feedbackToneValue;
    if (fil>0.5f)  //simultaneously change crossover frequency & high/low mix
    {
      fil = 0.5f * fil - 0.25f;
      lmix = -2.0f * fil;
      hmix = 1.0f;
    }
    else
    {
      hmix = 2.0f * fil;
      lmix = 1.0f - hmix;
    }
    fil = expf(-juce::MathConstants<float>::twoPi * std::powf(10.0f, 2.2f + 4.5f * fil) / fs);
    
    fbk = std::fabs(2.2f * feedbackValue - 1.1f);
    if (feedbackValue>0.5f) {
        rel=0.9997f;
    }
    else
    {
        rel=0.8f; //limit or clip
    }
    
    wet = 1.0f - wetMixValue;
    wet = outputValue * (1.0f - wet * wet); //-3dB at 50% mix
    dry = outputValue * 2.0f * (1.0f - wetMixValue * wetMixValue);
    
    float lfoRateValue = std::exp(7.0f * lfoRateParam->get() - 4.0f);
    dphi = 628.31853f * std::pow(10.0f, 3.0f * lfoRateValue - 2.0f) / fs; //100-sample steps
    
    outputLevelSmoother.setCurrentAndTargetValue(outputValue);
}

void MdaDubDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    auto mainInputOutput = getBusBuffer (buffer, true, 0);

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    auto fs = (float)getSampleRate();
    bool expected = true;
    if (parametersChanged.compare_exchange_strong(expected, false)) {
        update(fs);
    }
    
    float a, b, c, d;
    float ol, w=wet, y=dry, fb=fbk, dl=dlbuf, db=dlbuf, ddl = 0.0f;
    float lx=lmix, hx=hmix, f=fil, f0=fil0, tmp;
    float e=env, g, r=rel; //limiter envelope, gain, release
    long i=ipos, l, s=allocatedBufferSize, k=0;
    
    for (auto samp=0; samp < buffer.getNumSamples(); samp++)
    {
        a = *mainInputOutput.getReadPointer (0, samp);
        b = *mainInputOutput.getReadPointer (1, samp);
        c = a;
        d = b;
        
        if (k==0) //update delay length at slower rate (could be improved!)
        {
            db += 0.01f * (del - db - mod - mod * std::sinf(phi)); //smoothed delay+lfo
            ddl = 0.01f * (db - dl); //linear step
            phi+=dphi;
            if (phi>juce::MathConstants<float>::twoPi) phi-=juce::MathConstants<float>::twoPi;
            k=100;
        }
        k--;
        dl += ddl; //lin interp between points
 
        i--; if (i<0) i=s; //delay positions
        
        l = (long)dl;
        tmp = dl - (float)l; //remainder
        l += i; if (l>s) l-=(s+1);
        
        ol = *(mybuffer + l); //delay output
        
        l++; if (l>s) l=0;
        ol += tmp * (*(mybuffer + l) - ol); //lin interp

        tmp = a + fb * ol;
        
        f0 = f * (f0 - tmp) + tmp; //low-pass filter
        tmp = lx * f0 + hx * tmp;
        
        g = (tmp<0.0f)? -tmp : tmp; //simple limiter
        e *= r; if (g>e) e = g;
        if (e>1.0f) tmp /= e;

        *(mybuffer + i) = tmp; //delay input
        
        ol *= w; //wet
        
        auto x1 = c + y * a + ol;
        auto x2 = d + y * b + ol;
#if DEBUG
        checkSample(x1);
        checkSample(x2);
        
#else
        *mainInputOutput.getWritePointer (0, samp) = x1;
        *mainInputOutput.getWritePointer (1, samp) = x2;
#endif
    }
    
    ipos = i;
    dlbuf = dl;
    
    //trap denormals
    if (fabsf(f0)<1.0e-10f) {
        fil0=0.0f;
        env=0.0f;
    } else {
        fil0=f0;
        env = e;
    }
    
    
    
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
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MdaDubDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
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
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterID::delay,
                                                           "Delay",
                                                           juce::NormalisableRange<float>(0.0f, kMaxDelayTime, 0.1f),
                                                           5.0f,
                                                           juce::AudioParameterFloatAttributes().withLabel("s")));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterID::feedback,
                                                           "Feedback",
                                                           juce::NormalisableRange<float>(0.0f, 100.0, 0.1f),
                                                           50.0f,
                                                           juce::AudioParameterFloatAttributes().withLabel("%")));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterID::feedbackTone,
                                                           "Feedback Tone",
                                                           0.0f,
                                                           1.0f,
                                                           0.4f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterID::lfoDepth,
                                                           "LFO Depth",
                                                           juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
                                                           0.0f,
                                                           juce::AudioParameterFloatAttributes().withLabel("%")));
    auto lfoRateStringFromValue = [](float value, int)
    {
        float lfoHz = std::exp(7.0f * value - 4.0f);
        return juce::String(lfoHz, 3);
    };
    layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterID::lfoRate,
                                                           "LFO Rate",
                                                           juce::NormalisableRange<float>(),
                                                           2.0f,
                                                           juce::AudioParameterFloatAttributes()
                                                           .withLabel("Hz")
                                                           .withStringFromValueFunction(lfoRateStringFromValue)));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterID::wetMix,
                                                           "FX Mix",
                                                           juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
                                                           50.0f,
                                                           juce::AudioParameterFloatAttributes().withLabel("%")));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterID::output,
                                                           "Output Level",
                                                           juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f),
                                                           0.0f,
                                                           juce::AudioParameterFloatAttributes().withLabel("dB")));
    
    return layout;
}
