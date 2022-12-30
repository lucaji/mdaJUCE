/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParameterID
{
    #define PARAMETER_ID(str) const juce::ParameterID str(#str, 1);

    PARAMETER_ID(size)
    PARAMETER_ID(hf)
    PARAMETER_ID(mix)
    PARAMETER_ID(output)

    #undef PARAMETER_ID
}

// Returns a typed pointer to a juce::AudioParameterXXX object from the APVTS.
template<typename T>
inline static void castParameter(juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id, T& destination)
{
    destination = dynamic_cast<T>(apvts.getParameter(id.getParamID()));
    jassert(destination);
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

//==============================================================================
MdaAmbienceAudioProcessor::MdaAmbienceAudioProcessor()
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
    castParameter(apvts, ParameterID::size, sizeParam);
    castParameter(apvts, ParameterID::hf, hfParam);
    castParameter(apvts, ParameterID::mix, mixParam);
    castParameter(apvts, ParameterID::output, outputParam);
    apvts.state.addListener(this);
        
    buf1 = new float[1024];
    buf2 = new float[1024];
    buf3 = new float[1024];
    buf4 = new float[1024];
    
    reset();
}

MdaAmbienceAudioProcessor::~MdaAmbienceAudioProcessor()
{
    apvts.state.removeListener(this);

    if(buf1) delete [] buf1;
    if(buf2) delete [] buf2;
    if(buf3) delete [] buf3;
    if(buf4) delete [] buf4;
}

//==============================================================================
const juce::String MdaAmbienceAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MdaAmbienceAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool MdaAmbienceAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool MdaAmbienceAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double MdaAmbienceAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MdaAmbienceAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int MdaAmbienceAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MdaAmbienceAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String MdaAmbienceAudioProcessor::getProgramName (int index)
{
    return {};
}

void MdaAmbienceAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void MdaAmbienceAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    parametersChanged.store(true);
    reset();
}

void MdaAmbienceAudioProcessor::reset()
{
    fil = 0.0f;
    den = pos = 0;
    memset(buf1, 0, 1024 * sizeof(float));
    memset(buf2, 0, 1024 * sizeof(float));
    memset(buf3, 0, 1024 * sizeof(float));
    memset(buf4, 0, 1024 * sizeof(float));
    rdy = 1;
}

void MdaAmbienceAudioProcessor::releaseResources()
{
    //reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MdaAmbienceAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void MdaAmbienceAudioProcessor::update(float fs) {
    // get normalised values
    auto sizeValue = apvts.getParameter("size")->getValue();
    auto hfValue = apvts.getParameter("hf")->getValue();
    auto mixValue = apvts.getParameter("mix")->getValue();
    auto outputValue = apvts.getParameter("output")->getValue();
    
    fbak = 0.8f;
    damp = 0.05f + 0.9f * hfValue;
    float tmp = (float)pow(10.0f, 2.0f * outputValue - 1.0f);
    dry = tmp - mixValue * mixValue* tmp;
    wet = (0.4f + 0.4f) * mixValue * tmp;

    tmp = 0.025f + 2.665f * sizeValue;
    if(size!=tmp) rdy=0;  //need to flush buffer
    size = tmp;
}

void MdaAmbienceAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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
    
    float a, b, c, d, r;
    float t, f=fil, fb=fbak, dmp=damp, y=dry, w=wet;
    long  p=pos, d1, d2, d3, d4;

    if (rdy==0) reset();

    d1 = (p + (long)(107 * size)) & 1023;
    d2 = (p + (long)(142 * size)) & 1023;
    d3 = (p + (long)(277 * size)) & 1023;
    d4 = (p + (long)(379 * size)) & 1023;
    
    for (auto samp=0; samp < buffer.getNumSamples(); samp++)
    {
        a = *mainInputOutput.getReadPointer (0, samp);
        b = *mainInputOutput.getReadPointer (1, samp);
        c = a;
        d = b;
        
        f += dmp * (w * (a + b) - f); //HF damping
        r = f;

        t = *(buf1 + p);
        r -= fb * t;
        *(buf1 + d1) = r; //allpass
        r += t;

        t = *(buf2 + p);
        r -= fb * t;
        *(buf2 + d2) = r; //allpass
        r += t;
        
        t = *(buf3 + p);
        r -= fb * t;
        *(buf3 + d3) = r; //allpass
        r += t;
        c += y * a + r - f; //left output

        t = *(buf4 + p);
        r -= fb * t;
        *(buf4 + d4) = r; //allpass
        r += t;
        d += y * b + r - f; //right output

        ++p  &= 1023;
        ++d1 &= 1023;
        ++d2 &= 1023;
        ++d3 &= 1023;
        ++d4 &= 1023;
        
#ifdef DEBUG
        checkSample(c);
        checkSample(d);
#endif
        *mainInputOutput.getWritePointer (0, samp) = c;
        *mainInputOutput.getWritePointer (1, samp) = d;
    }
    pos=p;
    //catch denormals
    if (fabs(f)>1.0e-10)
    {
        fil=f;
        den=0;
    }
    else
    {
        fil=0.0f;
        if (den==0) {
            den=1;
            reset();
        }
    }
}

//==============================================================================
bool MdaAmbienceAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* MdaAmbienceAudioProcessor::createEditor()
{
    return new MdaAmbienceAudioProcessorEditor (*this, apvts);
}

//==============================================================================
void MdaAmbienceAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MdaAmbienceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new MdaAmbienceAudioProcessor();
}

juce::AudioProcessorValueTreeState::ParameterLayout MdaAmbienceAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterID::size,
                                                           "Size",
                                                           juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f),
                                                           7.0f,
                                                           juce::AudioParameterFloatAttributes().withLabel("m")));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterID::hf,
                                                           "HF Damping",
                                                           juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
                                                           70.0f,
                                                           juce::AudioParameterFloatAttributes().withLabel("%")));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterID::mix,
                                                           "Mix",
                                                           juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
                                                           70.0f,
                                                           juce::AudioParameterFloatAttributes().withLabel("%")));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterID::output,
                                                           "Output Level",
                                                           juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f),
                                                           0.0f,
                                                           juce::AudioParameterFloatAttributes().withLabel("dB")));
    
    return layout;
}
