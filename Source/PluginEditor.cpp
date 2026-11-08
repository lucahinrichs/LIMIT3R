/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

// namespace => colour variables are only visible within this file
namespace
{
    const juce::Colour accentOrange { 0xffff8a1f };
    const juce::Colour lightTop { 0xff6e6e73 };
    const juce::Colour darkBottom { 0xff2b2b2e };

    // Layout constants shared by paint() and resized()
    constexpr int margin = 20;          // margin left, right and bottom
    constexpr int headerTop = 10;       // distance between window top and title
    constexpr int headerHeight = 30;    // height of the title area
    constexpr int headerGap = 10;       // space between title line and the controls
}

//==============================================================================
Limit3rAudioProcessorEditor::Limit3rAudioProcessorEditor (Limit3rAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Styling the three sliders vertical faders with value box below
    for (auto* slider : { &lowSlider, &midSlider, &highSlider, &makeupSlider })
    {
        slider->setColour(juce::Slider::thumbColourId, accentOrange);                   // fader cap
        slider->setColour(juce::Slider::trackColourId, accentOrange);                   // filled part of the track
        slider->setColour(juce::Slider::backgroundColourId, juce::Colours::black.withAlpha(0.4f));
        slider->setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::white.withAlpha(0.3f));

        slider->setSliderStyle(juce::Slider::LinearVertical);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);

        addAndMakeVisible(*slider);
    }

    lowLabel.setText("Low", juce::dontSendNotification);
    midLabel.setText("Mid", juce::dontSendNotification);
    highLabel.setText("High", juce::dontSendNotification);

    for (auto* label : { &lowLabel, &midLabel, &highLabel, &makeupLabel })
    {
        label->setColour(juce::Label::textColourId, juce::Colours::white);

        label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*label);
    }

    // Connect each slider to its parameter by the ID used in createParameterLayout()
    lowAttachment = std::make_unique<Attachment>(audioProcessor.apvts, "low", lowSlider);
    midAttachment = std::make_unique<Attachment>(audioProcessor.apvts, "mid", midSlider);
    highAttachment = std::make_unique<Attachment>(audioProcessor.apvts, "high", highSlider);
    makeupAttachment = std::make_unique<Attachment>(audioProcessor.apvts, "makeup", makeupSlider);

    // Set AFTER the attachments: they install their own text functions on the sliders
    for (auto* slider : { &lowSlider, &midSlider, &highSlider })
    {
        // Show the value as limiting amount in percent (0 % = top = neutral, 100 % = bottom = max)
        slider->textFromValueFunction = [](double value)         // use of lambda function for quick value to percentage conversion

        {
            return juce::String(juce::roundToInt((1.0 - value) * 100.0)) + " %";
        };

        // And the reverse: typed text -> parameter value
        slider->valueFromTextFunction = [](const juce::String& text)
        {
            return 1.0 - text.getDoubleValue() / 100.0;
        };

        slider->updateText();       // refresh the text box with the new functions
    }
    
    // Makeup fader: white to set it apart, value shown in dB
    makeupSlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    makeupSlider.setColour(juce::Slider::trackColourId, juce::Colours::white);

    makeupSlider.textFromValueFunction = [](double value)
    {
        return juce::String(value, 1) + " dB";
    };

    makeupSlider.valueFromTextFunction = [](const juce::String& text)
    {
        return text.getDoubleValue();
    };

    makeupSlider.updateText();

    startTimerHz(30);
    setSize (600, 360);
}

Limit3rAudioProcessorEditor::~Limit3rAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void Limit3rAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Vertical gradient from light (top) to darker grey (bottom)
    juce::ColourGradient gradient(lightTop, 0.0f, 0.0f,
                                  darkBottom, 0.0f, (float) getHeight(), false);
    g.setGradientFill(gradient);
    g.fillAll();

    // Title in the header area (same rectangle as reserved in resized())
    auto header = getLocalBounds().reduced(margin, 0).withTrimmedTop(headerTop).removeFromTop(headerHeight);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f));
    g.drawText("LIMIT3R", header, juce::Justification::centredLeft);
    
    // Level meters and gain reduction bars
    for (int band = 0; band < 3; ++band)
    {
        // Dark slots behind the bars
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillRect(meterBounds[band]);
        g.fillRect(grBounds[band]);

        // Level meter: -60 dBFS .. 0 dBFS, fills from the bottom
        const float db = juce::Decibels::gainToDecibels(shownLevel[band], -60.0f);
        const float levelFraction = juce::jlimit(0.0f, 1.0f, juce::jmap(db, -60.0f, 0.0f, 0.0f, 1.0f));
        auto levelArea = meterBounds[band];
        auto levelBar = levelArea.removeFromBottom(juce::roundToInt(levelArea.getHeight() * levelFraction));
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.fillRect(levelBar);

        // Gain reduction: 0 .. 24 dB, hangs down from the top
        const float reductionFraction = juce::jlimit(0.0f, 1.0f, shownReductionDb[band] / 24.0f);
        auto grArea = grBounds[band];
        auto grBar = grArea.removeFromTop(juce::roundToInt(grArea.getHeight() * reductionFraction));
        g.setColour(accentOrange);
        g.fillRect(grBar);
    }
}

void Limit3rAudioProcessorEditor::timerCallback()
{
    const std::array<float, 3> levels { audioProcessor.levelLow.load(), audioProcessor.levelMid.load(), audioProcessor.levelHigh.load() };
    const std::array<float, 3> gains { audioProcessor.gainMinLow.load(), audioProcessor.gainMinMid.load(), audioProcessor.gainMinHigh.load() };

    for (int band = 0; band < 3; ++band)
    {
        // Level: jumps up instantly, falls back slowly
        shownLevel[band] = std::max(levels[band], shownLevel[band] * 0.85f);

        // Gain reduction in dB (0 = none), with the same fall-back behaviour
        const float reductionDb = -20.0f * std::log10(std::max(gains[band], 0.0001f));
        shownReductionDb[band] = std::max(reductionDb, shownReductionDb[band] * 0.85f);
    }

    repaint();      // triggers paint() with the new values
}

void Limit3rAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(margin, 0);                    // side margins only
    area.removeFromTop(headerTop + headerHeight + headerGap);           // space for the title area and a small gap
    area.removeFromBottom(margin);

    // Reserved space on the right for future controls
    auto reserved = area.removeFromRight(160);                                  // right part of the window
    auto makeupColumn = reserved.removeFromRight(90).reduced(10, 0);            // makeup fader at the far right
    makeupLabel.setBounds(makeupColumn.removeFromTop(24));
    makeupSlider.setBounds(makeupColumn);

    const int columnWidth = area.getWidth() / 3;

    const std::array<juce::Label*, 3> labels { &lowLabel, &midLabel, &highLabel };
    const std::array<juce::Slider*, 3> sliders { &lowSlider, &midSlider, &highSlider };

    for (int band = 0; band < 3; ++band)
    {
        auto column = area.removeFromLeft(columnWidth).reduced(10, 0);     // 10 px gap left and right

        labels[band]->setBounds(column.removeFromTop(24));

        // Meters to the right of the fader: level meter and gain reduction bar side by side
        grBounds[band] = column.removeFromRight(14);
        column.removeFromRight(4);                       // small gap between the two bars
        meterBounds[band] = column.removeFromRight(14);
        column.removeFromRight(8);                       // gap between meters and fader

        // Keep the bars clear of the value box and aligned with the fader track
        meterBounds[band].removeFromBottom(24);
        grBounds[band].removeFromBottom(24);
        meterBounds[band] = meterBounds[band].reduced(0, 9);
        grBounds[band] = grBounds[band].reduced(0, 9);

        sliders[band]->setBounds(column);                // the fader gets whatever is left
    }
}
