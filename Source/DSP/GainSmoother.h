/*
  ==============================================================================

    GainSmoother.h
    Created: 2026
    Author:  MBM Audio

    Envelope follower with attack/release/hold smoothing for gain adjustments.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

class GainSmoother
{
public:
    GainSmoother();
    ~GainSmoother() = default;

    //==============================================================================
    void prepare(double sampleRate);
    void reset();

    //==============================================================================
    float calculateTargetGain(float currentLevelDb, float targetLevelDb, float rangeDb);
    float processSample(float targetGainDb);

    float getCurrentGainLinear() const;
    float getCurrentGainDb() const { return currentGainDb.load(); }

    //==============================================================================
    void setAttackTime(float attackMs);
    void setReleaseTime(float releaseMs);
    void setHoldTime(float holdMs);
    void setSpeed(float speedPercent);

private:
    //==============================================================================
    void updateCoefficients();

    //==============================================================================
    double sampleRate = 44100.0;
    
    float attackTimeMs = 50.0f;
    float releaseTimeMs = 200.0f;
    float holdTimeMs = 0.0f;
    
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    
    int holdSamples = 0;
    int holdCounter = 0;
    float lastTargetGainDb = 0.0f;
    
    float smoothedGainDb = 0.0f;
    std::atomic<float> currentGainDb { 0.0f };

    static constexpr float silenceThresholdDb = -60.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GainSmoother)
};
