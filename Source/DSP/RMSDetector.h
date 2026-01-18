/*
  ==============================================================================

    RMSDetector.h
    Created: 2026
    Author:  MBM Audio

    RMS level detection with circular buffer for smooth vocal level tracking.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>

class RMSDetector
{
public:
    RMSDetector();
    ~RMSDetector() = default;

    //==============================================================================
    /** Prepares the detector for playback.
        @param sampleRate The sample rate of the audio
        @param windowSizeMs The RMS window size in milliseconds
    */
    void prepare(double sampleRate, float windowSizeMs = 50.0f);

    /** Resets the detector state */
    void reset();

    //==============================================================================
    /** Processes a single sample and returns the current RMS level in dB.
        @param sample The input sample to process
        @return The current RMS level in decibels
    */
    float processSample(float sample);

    /** Processes a block of samples and returns the final RMS level in dB.
        @param samples Pointer to the sample buffer
        @param numSamples Number of samples to process
        @return The RMS level in decibels at the end of the block
    */
    float processBlock(const float* samples, int numSamples);

    //==============================================================================
    /** Sets the RMS window size in milliseconds.
        @param windowSizeMs Window size in milliseconds (typically 10-100ms)
    */
    void setWindowSize(float windowSizeMs);

    /** Gets the current RMS level in dB (thread-safe for UI access). */
    float getCurrentLevelDb() const { return currentLevelDb.load(); }

    /** Gets the current RMS level as a linear value (0-1 range). */
    float getCurrentLevelLinear() const;

private:
    //==============================================================================
    void updateBufferSize();
    float calculateRmsDb() const;

    //==============================================================================
    double sampleRate = 44100.0;
    float windowSizeMs = 50.0f;

    std::vector<float> squaredBuffer;
    int bufferSize = 0;
    int writeIndex = 0;
    float runningSum = 0.0f;

    std::atomic<float> currentLevelDb { -100.0f };

    static constexpr float minDbLevel = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RMSDetector)
};

