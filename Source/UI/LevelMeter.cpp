/*
  ==============================================================================

    LevelMeter.cpp
    Created: 2026
    Author:  MBM Audio

    FabFilter-inspired level meters.

  ==============================================================================
*/

#include "LevelMeter.h"
#include "CustomLookAndFeel.h"

LevelMeter::LevelMeter(MeterType type)
    : meterType(type)
{
    backgroundColour = CustomLookAndFeel::getSurfaceColour();
    
    switch (meterType)
    {
        case MeterType::Input:
        case MeterType::Output:
            meterColour = CustomLookAndFeel::getAccentColour();
            peakColour = CustomLookAndFeel::getWarningColour();
            break;
            
        case MeterType::GainReduction:
            meterColour = CustomLookAndFeel::getGainCurveColour();
            peakColour = CustomLookAndFeel::getWarningColour();
            minDb = -12.0f;
            maxDb = 12.0f;
            break;
    }
    
    smoothingCoeff = std::exp(-1.0f / (0.05f * refreshRateHz));
    peakHoldSamples = static_cast<int>((peakHoldTimeMs / 1000.0f) * refreshRateHz);
    
    startTimerHz(refreshRateHz);
}

LevelMeter::~LevelMeter()
{
    stopTimer();
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    
    // Background
    g.setColour(backgroundColour);
    g.fillRoundedRectangle(bounds, 3.0f);
    
    // Border
    g.setColour(CustomLookAndFeel::getBorderColour());
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
    
    auto meterBounds = bounds.reduced(2.0f);
    
    if (meterType == MeterType::GainReduction)
    {
        float centerY = meterBounds.getCentreY();
        float levelY = levelToY(displayLevelDb);
        
        juce::Rectangle<float> meterRect;
        
        if (displayLevelDb >= 0.0f)
        {
            meterRect = juce::Rectangle<float>(
                meterBounds.getX(),
                levelY,
                meterBounds.getWidth(),
                centerY - levelY
            );
        }
        else
        {
            meterRect = juce::Rectangle<float>(
                meterBounds.getX(),
                centerY,
                meterBounds.getWidth(),
                levelY - centerY
            );
        }
        
        g.setColour(meterColour.withAlpha(0.8f));
        g.fillRoundedRectangle(meterRect, 2.0f);
        
        // Center line
        g.setColour(CustomLookAndFeel::getBorderColour().brighter(0.2f));
        g.drawHorizontalLine(static_cast<int>(centerY), meterBounds.getX(), meterBounds.getRight());
    }
    else
    {
        float levelY = levelToY(displayLevelDb);
        
        juce::Rectangle<float> meterRect(
            meterBounds.getX(),
            levelY,
            meterBounds.getWidth(),
            meterBounds.getBottom() - levelY
        );
        
        // Color based on level
        juce::Colour fillColour = (displayLevelDb > -3.0f) 
            ? CustomLookAndFeel::getWarningColour() 
            : meterColour;
        
        g.setColour(fillColour.withAlpha(0.85f));
        g.fillRoundedRectangle(meterRect, 2.0f);
        
        // Peak hold indicator
        if (peakHoldEnabled && peakLevelDb > minDb)
        {
            float peakY = levelToY(peakLevelDb);
            g.setColour(peakColour.withAlpha(0.9f));
            g.fillRect(meterBounds.getX(), peakY - 1.0f, meterBounds.getWidth(), 2.0f);
        }
    }
}

void LevelMeter::resized() {}

void LevelMeter::setLevel(float levelDb)
{
    currentLevelDb = levelDb;
}

void LevelMeter::setRange(float newMinDb, float newMaxDb)
{
    minDb = newMinDb;
    maxDb = newMaxDb;
}

void LevelMeter::setPeakHoldEnabled(bool enabled)
{
    peakHoldEnabled = enabled;
    if (!enabled) peakLevelDb = minDb;
}

void LevelMeter::setPeakHoldTime(float timeMs)
{
    peakHoldTimeMs = timeMs;
    peakHoldSamples = static_cast<int>((peakHoldTimeMs / 1000.0f) * refreshRateHz);
}

void LevelMeter::timerCallback()
{
    displayLevelDb = smoothingCoeff * displayLevelDb + (1.0f - smoothingCoeff) * currentLevelDb;
    
    if (meterType != MeterType::GainReduction)
    {
        if (displayLevelDb > peakLevelDb)
        {
            peakLevelDb = displayLevelDb;
            peakHoldCounter = peakHoldSamples;
        }
        else if (peakHoldCounter > 0)
        {
            peakHoldCounter--;
        }
        else
        {
            peakLevelDb -= 0.5f;
            peakLevelDb = juce::jmax(peakLevelDb, minDb);
        }
    }
    
    repaint();
}

float LevelMeter::levelToY(float levelDb) const
{
    auto bounds = getLocalBounds().toFloat().reduced(3.0f);
    float clampedLevel = juce::jlimit(minDb, maxDb, levelDb);
    float normalizedLevel = (clampedLevel - minDb) / (maxDb - minDb);
    return bounds.getBottom() - (normalizedLevel * bounds.getHeight());
}

juce::Colour LevelMeter::getLevelColour(float levelDb) const
{
    if (meterType == MeterType::GainReduction)
        return meterColour;
    
    if (levelDb < -6.0f)
        return meterColour;
    else
        return CustomLookAndFeel::getWarningColour();
}
