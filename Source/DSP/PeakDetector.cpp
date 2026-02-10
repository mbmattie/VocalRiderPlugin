/*
  ==============================================================================

    PeakDetector.cpp
    Created: 2026
    Author:  MBM Audio

  ==============================================================================
*/

#include "PeakDetector.h"

PeakDetector::PeakDetector()
{
    updateCoefficients();
}

void PeakDetector::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
    updateCoefficients();
    reset();
}

void PeakDetector::reset()
{
    envelope = 0.0f;
    currentLevelDb.store(minDbLevel);
}

void PeakDetector::updateCoefficients()
{
    if (sampleRate <= 0.0)
        return;
    
    // Very fast attack for catching transients
    float attackTimeSec = juce::jmax(0.0001f, attackTimeMs / 1000.0f);
    float releaseTimeSec = juce::jmax(0.001f, releaseTimeMs / 1000.0f);
    
    attackCoeff = std::exp(-1.0f / (attackTimeSec * static_cast<float>(sampleRate)));
    releaseCoeff = std::exp(-1.0f / (releaseTimeSec * static_cast<float>(sampleRate)));
}

void PeakDetector::setAttackTime(float attackMs)
{
    attackTimeMs = juce::jmax(0.01f, attackMs);
    updateCoefficients();
}

void PeakDetector::setReleaseTime(float releaseMs)
{
    releaseTimeMs = juce::jmax(1.0f, releaseMs);
    updateCoefficients();
}

float PeakDetector::processSampleInternal(float sample)
{
    float absSample = std::abs(sample);
    
    // Envelope follower with instant attack option
    if (absSample > envelope)
    {
        // Nearly instant attack for transients
        envelope = attackCoeff * envelope + (1.0f - attackCoeff) * absSample;
    }
    else
    {
        envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * absSample;
    }
    
    // Snap to zero to prevent denormal CPU spikes on older Intel hardware
    if (envelope < 1.0e-15f)
        envelope = 0.0f;
    
    return juce::Decibels::gainToDecibels(envelope, minDbLevel);
}

float PeakDetector::processSample(float sample)
{
    float db = processSampleInternal(sample);
    currentLevelDb.store(db);
    return db;
}

float PeakDetector::processBlock(const float* samples, int numSamples)
{
    float levelDb = minDbLevel;
    
    for (int i = 0; i < numSamples; ++i)
    {
        levelDb = processSampleInternal(samples[i]);
    }
    
    // Single atomic store at end of block (not per-sample)
    currentLevelDb.store(levelDb);
    
    return levelDb;
}

