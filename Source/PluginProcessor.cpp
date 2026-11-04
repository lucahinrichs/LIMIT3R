/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Limit3rAudioProcessor::Limit3rAudioProcessor()
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
}

Limit3rAudioProcessor::~Limit3rAudioProcessor()
{
}

//==============================================================================
const juce::String Limit3rAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool Limit3rAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Limit3rAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Limit3rAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double Limit3rAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Limit3rAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int Limit3rAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Limit3rAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String Limit3rAudioProcessor::getProgramName (int index)
{
    return {};
}

void Limit3rAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void Limit3rAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // constexpr: value is a literal, known at compile time
    constexpr double lookaheadSeconds = 0.003;   // 3 ms

    // const: value depends on sampleRate, so only known at runtime and fixed after this line
    // static_cast<int>: explicit double -> int conversion (std::round already rounded, the cast drops the type)
    const int lookaheadSamples = static_cast<int>(std::round(lookaheadSeconds * sampleRate));
    
    for (int ch = 0; ch < 2; ++ch)
    {
        lowLP[ch].setCutoff (267.44f, sampleRate);
        midLP[ch].setCutoff (1500.32f, sampleRate);
        highLP[ch].setCutoff (1500.32f, sampleRate);
        
        lowLP[ch].reset();
        midLP[ch].reset();
        highLP[ch].reset();
        
        delayLine[ch].prepare(lookaheadSamples);
    }
    setLatencySamples(lookaheadSamples);
}

void Limit3rAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Limit3rAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void Limit3rAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    // Clear output channels that have no matching input channel
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    // Temporary test switch: 0 = all bands, 1 = low only, 2 = mid only, 3 = high only
    const int solo = 0;
    
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float input = channelData[i]; // current input sample
            const float d = delayLine[channel].process(input);
            
            // Band split
            const float low = lowLP[channel].process(d);
            const float mid = midLP[channel].process(d - low);
            const float high = d - highLP[channel].process(d);
            
            float out = 0.0f;
            if      (solo == 1) out = low;
            else if (solo == 2) out = mid;
            else if (solo == 3) out = high;
            else                out = low + mid + high;
            
            channelData[i] = out;
        }
    }
}

//==============================================================================
bool Limit3rAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* Limit3rAudioProcessor::createEditor()
{
    return new Limit3rAudioProcessorEditor (*this);
}

//==============================================================================
void Limit3rAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void Limit3rAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Limit3rAudioProcessor();
}
