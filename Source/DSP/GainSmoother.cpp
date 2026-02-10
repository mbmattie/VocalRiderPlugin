/*
  ==============================================================================

    GainSmoother.cpp
    Created: 2026
    Author:  MBM Audio

  ==============================================================================
*/

#include "GainSmoother.h"

GainSmoother::GainSmoother()
{
    updateCoefficients();
}

void GainSmoother::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
    updateCoefficients();
    reset();
}

void GainSmoother::reset()
{
    smoothedGainDb = 0.0f;
    currentGainDb.store(0.0f);
    holdCounter = 0;
    lastTargetGainDb = 0.0f;
}

void GainSmoother::updateCoefficients()
{
    if (sampleRate <= 0.0)
        return;
    
    float attackTimeSec = juce::jmax(0.001f, attackTimeMs / 1000.0f);
    float releaseTimeSec = juce::jmax(0.001f, releaseTimeMs / 1000.0f);
    
    attackCoeff.store(std::exp(-1.0f / (attackTimeSec * static_cast<float>(sampleRate))), std::memory_order_relaxed);
    releaseCoeff.store(std::exp(-1.0f / (releaseTimeSec * static_cast<float>(sampleRate))), std::memory_order_relaxed);
    holdSamples.store(static_cast<int>((holdTimeMs / 1000.0f) * sampleRate), std::memory_order_relaxed);
}

void GainSmoother::setAttackTime(float attackMs)
{
    attackTimeMs = juce::jmax(1.0f, attackMs);
    updateCoefficients();
}

void GainSmoother::setReleaseTime(float releaseMs)
{
    releaseTimeMs = juce::jmax(1.0f, releaseMs);
    updateCoefficients();
}

void GainSmoother::setHoldTime(float holdMs)
{
    holdTimeMs = juce::jmax(0.0f, holdMs);
    updateCoefficients();
}

void GainSmoother::setSpeed(float speedPercent)
{
    float normalizedSpeed = juce::jlimit(0.0f, 100.0f, speedPercent) / 100.0f;
    float speedFactor = std::pow(normalizedSpeed, 0.5f);
    
    attackTimeMs = juce::jmap(speedFactor, 500.0f, 5.0f);
    releaseTimeMs = juce::jmap(speedFactor, 1000.0f, 20.0f);
    
    updateCoefficients();
}

float GainSmoother::calculateTargetGain(float currentLevelDb, float targetLevelDb, float rangeDb)
{
    if (currentLevelDb < silenceThresholdDb)
        return 0.0f;
    
    float gainNeeded = targetLevelDb - currentLevelDb;
    return juce::jlimit(-rangeDb, rangeDb, gainNeeded);
}

float GainSmoother::processSample(float targetGainDb)
{
    // Load atomic coefficients once per sample for consistent use
    const int currentHoldSamples = holdSamples.load(std::memory_order_relaxed);
    const float currentAttackCoeff = attackCoeff.load(std::memory_order_relaxed);
    const float currentReleaseCoeff = releaseCoeff.load(std::memory_order_relaxed);
    
    // Hold logic: if gain is decreasing (less boost or more reduction), apply hold
    if (currentHoldSamples > 0)
    {
        if (targetGainDb < lastTargetGainDb)
        {
            if (holdCounter < currentHoldSamples)
            {
                holdCounter++;
                targetGainDb = lastTargetGainDb;  // Hold at previous level
            }
            else
            {
                // Hold time expired, allow change
                lastTargetGainDb = targetGainDb;
                holdCounter = 0;
            }
        }
        else
        {
            // Gain increasing, no hold needed
            lastTargetGainDb = targetGainDb;
            holdCounter = 0;
        }
    }
    else
    {
        lastTargetGainDb = targetGainDb;
    }

    // Apply one-pole smoothing with different attack/release coefficients
    float coeff = (targetGainDb > smoothedGainDb) ? currentAttackCoeff : currentReleaseCoeff;
    
    smoothedGainDb = coeff * smoothedGainDb + (1.0f - coeff) * targetGainDb;
    
    // Snap to target when very close - prevents denormal floats that cause
    // massive CPU spikes on pre-Haswell Intel Macs (~2015 hardware)
    if (std::abs(smoothedGainDb - targetGainDb) < 1.0e-6f)
        smoothedGainDb = targetGainDb;
    
    currentGainDb.store(smoothedGainDb);
    
    return smoothedGainDb;
}

float GainSmoother::getCurrentGainLinear() const
{
    return juce::Decibels::decibelsToGain(currentGainDb.load());
}
