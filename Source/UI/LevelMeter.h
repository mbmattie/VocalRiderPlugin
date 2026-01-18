/*
  ==============================================================================

    LevelMeter.h
    Created: 2026
    Author:  MBM Audio

    Vertical bar meter with peak hold for displaying levels.
    Can be configured for input, output, or gain reduction display.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class LevelMeter : public juce::Component,
                   public juce::Timer
{
public:
    //==============================================================================
    enum class MeterType
    {
        Input,          // Standard input level meter (green to red)
        Output,         // Standard output level meter (green to red)
        GainReduction   // Gain reduction meter (typically amber/yellow)
    };

    //==============================================================================
    LevelMeter(MeterType type = MeterType::Input);
    ~LevelMeter() override;

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    /** Sets the current level in dB.
        For GainReduction type, positive values show gain boost, negative show reduction.
    */
    void setLevel(float levelDb);

    /** Sets the meter range in dB.
        @param minDb Minimum level (bottom of meter)
        @param maxDb Maximum level (top of meter)
    */
    void setRange(float minDb, float maxDb);

    /** Enables or disables peak hold display. */
    void setPeakHoldEnabled(bool enabled);

    /** Sets the peak hold time in milliseconds. */
    void setPeakHoldTime(float timeMs);

    //==============================================================================
    void timerCallback() override;

private:
    //==============================================================================
    float levelToY(float levelDb) const;
    juce::Colour getLevelColour(float levelDb) const;

    //==============================================================================
    MeterType meterType;

    float currentLevelDb = -100.0f;
    float displayLevelDb = -100.0f;
    float peakLevelDb = -100.0f;

    float minDb = -60.0f;
    float maxDb = 6.0f;

    bool peakHoldEnabled = true;
    float peakHoldTimeMs = 1500.0f;
    int peakHoldSamples = 0;
    int peakHoldCounter = 0;

    // Smoothing
    float smoothingCoeff = 0.0f;

    // Colors
    juce::Colour backgroundColour;
    juce::Colour meterColour;
    juce::Colour peakColour;

    static constexpr int refreshRateHz = 60;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};

