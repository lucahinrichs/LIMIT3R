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

// Single-pole low pass: y += k * (x - y)
struct OnePole
{
    float k = 0.0f;     // filter coefficient, set from cutof and sample rate
    float y = 0.0f;     // filter state (the previous output)
    
    void setCutoff(float cutoffHz, double sampleRate)
    {
        k = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoffHz / (float) sampleRate);
    }
    
    void reset() {y = 0.0f; }
    
    float process(float x)
    {
        y += k * (x - y);
        return y;
    }
    
};

struct DelayLine
{
    std::vector<float> buffer;                              // stores the last N samples
    int writePos = 0;                                       // index where the next sample goes
    
    void prepare(int delaySamples)
    {
        buffer.assign(delaySamples, 0.0f);                  // N zeros = silence
        writePos = 0;
    }
    
    float process(float x)
    {
        const float delayed = buffer[writePos];            // oldest sample = the one from N samples ago
        buffer[writePos] = x;                              // overwrite it with the new one
        writePos = (writePos + 1) % (int) buffer.size();   // advance, wrap around at the end
        return delayed;
    }
};

class Limit3rAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    Limit3rAudioProcessor();
    ~Limit3rAudioProcessor() override;

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

private:
    //==============================================================================
    OnePole lowLP[2];       // low pass at 267 Hz on original signal
    OnePole midLP[2];       // low pass at 1500 Hz on (signal - low)
    OnePole highLP[2];      // highs = signal  - low pass at 1500 Hz
    
    DelayLine delayLine[2];
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Limit3rAudioProcessor)

};
