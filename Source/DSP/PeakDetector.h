/*
  ==============================================================================

    PeakDetector.h
    Created: 2026
    Author:  MBM Audio

    Fast peak detector for transient detection alongside RMS.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

class PeakDetector
{
public:
    PeakDetector();

    void prepare(double sampleRate);
    void reset();

    /** Process a single sample and return the current peak level in dB */
    float processSample(float sample);
    
    /** Process a block of samples and return the peak level in dB */
    float processBlock(const float* samples, int numSamples);

    /** Get current peak level in dB (thread-safe) */
    float getCurrentLevelDb() const { return currentLevelDb.load(); }
    
    /** Set attack time in ms (very fast for transients) */
    void setAttackTime(float attackMs);
    
    /** Set release time in ms */
    void setReleaseTime(float releaseMs);

private:
    /** Internal sample processing without atomic store (for use in processBlock). */
    float processSampleInternal(float sample);
    
    void updateCoefficients();

    double sampleRate = 44100.0;
    float attackTimeMs = 0.1f;   // Very fast attack for transients
    float releaseTimeMs = 50.0f;
    
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    
    float envelope = 0.0f;
    std::atomic<float> currentLevelDb { -100.0f };
    
    static constexpr float minDbLevel = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PeakDetector)
};

