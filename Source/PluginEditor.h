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
class Limit3rAudioProcessorEditor  : public juce::AudioProcessorEditor, private juce::Timer{
public:
    Limit3rAudioProcessorEditor (Limit3rAudioProcessor&);
    ~Limit3rAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    Limit3rAudioProcessor& audioProcessor;
    
    juce::Slider lowSlider, midSlider, highSlider;
    juce::Label lowLabel, midLabel, highLabel;
    juce::Slider makeupSlider;
    juce::Label makeupLabel;
    
    // Attachments connect a slider to a parameter of th APVTS
    // The are declared AFTER the sliders: members are destroyed in reverse order
    // => Attachments disappear first and never pint to a dead slider
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> lowAttachment, midAttachment, highAttachment, makeupAttachment;
    
    // Called by the timer about 30 times per second
    void timerCallback() override;

    // Areas of the meters, set in resized() and used in paint(). Index: 0 = low, 1 = mid, 2 = high
    std::array<juce::Rectangle<int>, 3> meterBounds;     // level meters
    std::array<juce::Rectangle<int>, 3> grBounds;        // gain reduction bars

    // Values currently shown (smoothed copies of the processors data)
    std::array<float, 3> shownLevel {};                  // linear level
    std::array<float, 3> shownReductionDb {};            // gain reduction in dB (positive numbers)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Limit3rAudioProcessorEditor)
};
