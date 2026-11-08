/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==Constructor=================================================================
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
                    , apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    // Look up the parameter pointers once by the IDs defined in createParameterLayout()
    lowParam = apvts.getRawParameterValue("low");
    midParam = apvts.getRawParameterValue("mid");
    highParam = apvts.getRawParameterValue("high");
    makeupParam = apvts.getRawParameterValue("makeup");
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

juce::AudioProcessorValueTreeState::ParameterLayout Limit3rAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    // One knob per band: id, display name, min, max, default
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID {"low", 1}, "Low", 0.0f, 1.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID {"mid", 1}, "Mid", 0.0f, 1.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID {"high", 1}, "High", 0.0f, 1.0f, 1.0f));
    
    // Output gain in dB (0 dB = original makeup)
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID {"makeup", 1}, "Makeup",
               juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));
    
    return layout;
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
        
        lowEnv[ch].setCutoff(15.6f, sampleRate);
        midEnv[ch].setCutoff(52.5f, sampleRate);
        highEnv[ch].setCutoff(105.5f, sampleRate);
        
        lowEnv[ch].reset();
        midEnv[ch].reset();
        highEnv[ch].reset();
        
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
    constexpr float makeupGain = 3.2f;          // applied to the sum of all bands
    constexpr float gateSlope = 10000.0f;      // gate opens fully above ~ -80 dBFS of the mid envelope
    
    // Read the current knob positions once per block and convert them to the band factors
    // -> accesses the pointer value / load() reads the value from the atomic
    tLow = poti2T(lowParam->load());
    tMid = poti2T(midParam->load());
    tHigh = poti2T(highParam->load());
    
    // Output gain: original makeup factor times the extra gain from the fader (dB -> linear)
    const float totalMakeup = makeupGain * juce::Decibels::decibelsToGain(makeupParam->load());
    
    // Meter values of this block, written to the atomics at the end
    float peakLow = 0.0f, peakMid = 0.0f, peakHigh = 0.0f;
    float minGainLow = 1.0f, minGainMid = 1.0f, minGainHigh = 1.0f;
    
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float input = channelData[i]; // current input sample
            const float d = delayLine[channel].process(input);
            
            // Level detectors: rectify the undelayed input, then smooth it
            const float level = std::abs(input);
            const float lowLevel = lowEnv[channel].process(level);
            const float midLevel = midEnv[channel].process(level);
            const float highLevel = highEnv[channel].process(level);
            
            // Gain per band: min(1, 0.1886 / ((level + offset) * T))
            const float gainLow = std::min(1.0f, 0.1886f / ((lowLevel + 0.0596f) * tLow));
            const float gainMid = std::min(1.0f, 0.1886f / ((midLevel + 0.0303f) * tMid));
            const float gainHigh = std::min(1.0f, 0.1886f / ((highLevel + 0.0029f) * tHigh));
            
            // Noise gate driven by the mid detetor: fades the output out for near silence
            const float gate = std::min(1.0f, gateSlope * midLevel);
            
            // Band split
            const float low = lowLP[channel].process(d);
            const float mid = midLP[channel].process(d - low);
            const float high = d - highLP[channel].process(d);
            
            // Apply gain and T to each band
            const float lowOut = low * gainLow * tLow;
            const float midOut = mid * gainMid * tMid;
            const float highOut = high * gainHigh * tHigh;
           
            float out = 0.0f;
            if      (solo == 1) out = lowOut;
            else if (solo == 2) out = midOut;
            else if (solo == 3) out = highOut;
            else                out = lowOut + midOut + highOut;
            
            // Meter data: highest band level and strongest gain reduction in this block
            peakLow = std::max(peakLow, std::abs(lowOut) * totalMakeup * gate);
            peakMid = std::max(peakMid, std::abs(midOut) * totalMakeup * gate);
            peakHigh = std::max(peakHigh, std::abs(highOut) * totalMakeup * gate);

            minGainLow = std::min(minGainLow, gainLow);
            minGainMid = std::min(minGainMid, gainMid);
            minGainHigh = std::min(minGainHigh, gainHigh);
            
            // Hard clip at full scale
            channelData[i] = std::clamp(out * totalMakeup * gate, -1.0f, 1.0f);
        }
    }
    
    // Hand the results over to the editor
    levelLow.store(peakLow);
    levelMid.store(peakMid);
    levelHigh.store(peakHigh);

    gainMinLow.store(minGainLow);
    gainMinMid.store(minGainMid);
    gainMinHigh.store(minGainHigh);
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
