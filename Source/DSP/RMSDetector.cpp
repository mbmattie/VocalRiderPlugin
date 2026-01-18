/*
  ==============================================================================

    RMSDetector.cpp
    Created: 2026
    Author:  MBM Audio

  ==============================================================================
*/

#include "RMSDetector.h"

RMSDetector::RMSDetector()
{
    updateBufferSize();
}

void RMSDetector::prepare(double newSampleRate, float newWindowSizeMs)
{
    sampleRate = newSampleRate;
    windowSizeMs = newWindowSizeMs;
    updateBufferSize();
    reset();
}

void RMSDetector::reset()
{
    std::fill(squaredBuffer.begin(), squaredBuffer.end(), 0.0f);
    writeIndex = 0;
    runningSum = 0.0f;
    currentLevelDb.store(minDbLevel);
}

void RMSDetector::updateBufferSize()
{
    // Calculate buffer size from window size in ms
    bufferSize = static_cast<int>((windowSizeMs / 1000.0f) * sampleRate);
    bufferSize = juce::jmax(1, bufferSize);
    
    squaredBuffer.resize(static_cast<size_t>(bufferSize), 0.0f);
    writeIndex = 0;
    runningSum = 0.0f;
}

void RMSDetector::setWindowSize(float newWindowSizeMs)
{
    if (std::abs(windowSizeMs - newWindowSizeMs) > 0.01f)
    {
        windowSizeMs = newWindowSizeMs;
        updateBufferSize();
    }
}

float RMSDetector::processSample(float sample)
{
    // Calculate squared value
    float squared = sample * sample;
    
    // Subtract old value from running sum
    runningSum -= squaredBuffer[static_cast<size_t>(writeIndex)];
    
    // Add new squared value
    squaredBuffer[static_cast<size_t>(writeIndex)] = squared;
    runningSum += squared;
    
    // Ensure running sum doesn't go negative due to floating point errors
    runningSum = juce::jmax(0.0f, runningSum);
    
    // Advance write index (circular buffer)
    writeIndex = (writeIndex + 1) % bufferSize;
    
    // Calculate and store current level
    float levelDb = calculateRmsDb();
    currentLevelDb.store(levelDb);
    
    return levelDb;
}

float RMSDetector::processBlock(const float* samples, int numSamples)
{
    float levelDb = minDbLevel;
    
    for (int i = 0; i < numSamples; ++i)
    {
        levelDb = processSample(samples[i]);
    }
    
    return levelDb;
}

float RMSDetector::calculateRmsDb() const
{
    if (bufferSize == 0)
        return minDbLevel;
    
    float meanSquared = runningSum / static_cast<float>(bufferSize);
    
    if (meanSquared <= 0.0f)
        return minDbLevel;
    
    float rms = std::sqrt(meanSquared);
    float db = juce::Decibels::gainToDecibels(rms, minDbLevel);
    
    return db;
}

float RMSDetector::getCurrentLevelLinear() const
{
    float db = currentLevelDb.load();
    if (db <= minDbLevel)
        return 0.0f;
    
    return juce::Decibels::decibelsToGain(db);
}

