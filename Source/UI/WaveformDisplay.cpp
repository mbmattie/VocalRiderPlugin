/*
  ==============================================================================

    WaveformDisplay.cpp
    Created: 2026
    Author:  MBM Audio

    Image-based waveform display for ultra-smooth scrolling.

  ==============================================================================
*/

#include "WaveformDisplay.h"
#include "CustomLookAndFeel.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

WaveformDisplay::WaveformDisplay()
{
    lastFrameTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    startTimerHz(144);  // 144Hz for smooth scrolling at all sizes
}

WaveformDisplay::~WaveformDisplay()
{
    stopTimer();
}

void WaveformDisplay::resized()
{
    auto bounds = getLocalBounds().toFloat();
    
    waveformArea = bounds;
    waveformArea.removeFromRight(static_cast<float>(ioMeterWidth) + 4.0f);
    
    // Round to nearest integer to ensure perfect alignment between waveform and grid lines
    // This prevents scaling artifacts that cause misalignment at different sizes
    waveformArea = waveformArea.toNearestInt().toFloat();
    
    initializeWaveformImage();
}

void WaveformDisplay::initializeWaveformImage()
{
    // waveformArea is now integer-aligned, so direct cast is safe
    int newWidth = static_cast<int>(waveformArea.getWidth());
    int newHeight = static_cast<int>(waveformArea.getHeight());
    
    if (newWidth > 0 && newHeight > 0 && (newWidth != imageWidth || newHeight != imageHeight))
    {
        imageWidth = newWidth;
        imageHeight = newHeight;
        
        // Create waveform image with transparency - exact match to waveformArea
        waveformImage = juce::Image(juce::Image::ARGB, imageWidth, imageHeight, true);
        waveformImage.clear(waveformImage.getBounds(), juce::Colours::transparentBlack);
        
        // Initialize gain curve buffer (one value per pixel)
        // Use assign to clear ALL values when resizing (not just new elements)
        gainCurveBuffer.assign(static_cast<size_t>(imageWidth), 0.0f);
        gainCurveWriteIndex = 0;
        
        // Initialize position buffers for closed path rendering
        // Use assign to ensure ALL values are reset when window size changes
        float defaultY = static_cast<float>(imageHeight);  // Bottom = no signal
        inputTopBuffer.assign(static_cast<size_t>(imageWidth), defaultY);
        inputBottomBuffer.assign(static_cast<size_t>(imageWidth), defaultY);
        outputTopBuffer.assign(static_cast<size_t>(imageWidth), defaultY);
        outputBottomBuffer.assign(static_cast<size_t>(imageWidth), defaultY);
        
        // Reset scroll state
        scrollAccumulator = 0.0;
        
        // Clear pending data
        juce::SpinLock::ScopedLockType lock(pendingLock);
        pendingData.clear();
        pendingDataIndex = 0;
    }
}

//==============================================================================
// Coordinate conversions

float WaveformDisplay::linearToLogY(float linear, float areaHeight) const
{
    if (linear <= 0.00001f) return areaHeight;
    float db = 20.0f * std::log10(linear);
    // Extended range: -64dB at bottom, +6dB at top (70dB total range)
    float normalized = (db - (-64.0f)) / (6.0f - (-64.0f));
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    return areaHeight - normalized * areaHeight;
}

float WaveformDisplay::dbToY(float db) const
{
    // Extended range: -64dB at bottom, +6dB at top
    float normalized = (db - (-64.0f)) / (6.0f - (-64.0f));
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    return waveformArea.getBottom() - normalized * waveformArea.getHeight();
}

float WaveformDisplay::yToDb(float y) const
{
    float normalized = (waveformArea.getBottom() - y) / waveformArea.getHeight();
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    return -64.0f + normalized * (6.0f - (-64.0f));
}

float WaveformDisplay::gainDbToY(float gainDb) const
{
    float target = targetLevelDb.load();
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    float clampedGain = juce::jlimit(-cut, boost, gainDb);
    float effectiveDb = juce::jlimit(-64.0f, 6.0f, target + clampedGain);
    return dbToY(effectiveDb);
}

float WaveformDisplay::gainDbToImageY(float gainDb, float imgHeight) const
{
    float target = targetLevelDb.load();
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    float clampedGain = juce::jlimit(-cut, boost, gainDb);
    float effectiveDb = juce::jlimit(-64.0f, 6.0f, target + clampedGain);
    // Extended range: -64dB at bottom, +6dB at top
    float normalized = (effectiveDb - (-64.0f)) / (6.0f - (-64.0f));
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    return imgHeight - normalized * imgHeight;
}

//==============================================================================
// Hit testing

WaveformDisplay::DragTarget WaveformDisplay::hitTestHandle(const juce::Point<float>& pos) const
{
    float targetY = dbToY(targetLevelDb.load());
    float boostY = dbToY(targetLevelDb.load() + boostRangeDb.load());
    float cutY = dbToY(targetLevelDb.load() - cutRangeDb.load());
    
    float distToTarget = std::abs(pos.y - targetY);
    float distToBoost = std::abs(pos.y - boostY);
    float distToCut = std::abs(pos.y - cutY);
    
    if (distToBoost < handleHitDistance && distToBoost <= distToTarget && distToBoost <= distToCut)
        return DragTarget::BoostRangeHandle;
    if (distToCut < handleHitDistance && distToCut <= distToTarget)
        return DragTarget::CutRangeHandle;
    if (distToTarget < handleHitDistance)
        return DragTarget::TargetHandle;
    
    return DragTarget::None;
}

//==============================================================================
// Mouse interaction

void WaveformDisplay::mouseDown(const juce::MouseEvent& event)
{
    auto pos = event.position;
    currentDragTarget = hitTestHandle(pos);
    
    if (currentDragTarget == DragTarget::TargetHandle)
    {
        dragStartValue = targetLevelDb.load();
        dragStartY = pos.y;
    }
    else if (currentDragTarget == DragTarget::BoostRangeHandle)
    {
        dragStartValue = boostRangeDb.load();
        dragStartY = pos.y;
    }
    else if (currentDragTarget == DragTarget::CutRangeHandle)
    {
        dragStartValue = cutRangeDb.load();
        dragStartY = pos.y;
    }
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& event)
{
    if (currentDragTarget == DragTarget::None) return;
    
    float deltaY = event.position.y - dragStartY;
    float dbPerPixel = 66.0f / waveformArea.getHeight();
    float deltaDb = -deltaY * dbPerPixel;
    
    if (currentDragTarget == DragTarget::TargetHandle)
    {
        float newTarget = juce::jlimit(-40.0f, 0.0f, dragStartValue + deltaDb);
        targetLevelDb.store(newTarget);
        if (onTargetChanged) onTargetChanged(newTarget);
    }
    else if (currentDragTarget == DragTarget::BoostRangeHandle)
    {
        // Both range handles are connected - move both together
        float newRange = juce::jlimit(0.0f, 12.0f, dragStartValue + deltaDb);
        boostRangeDb.store(newRange);
        cutRangeDb.store(newRange);  // Keep both in sync
        if (onBoostRangeChanged) onBoostRangeChanged(newRange);
        if (onCutRangeChanged) onCutRangeChanged(newRange);
        if (onRangeChanged) onRangeChanged(newRange);
    }
    else if (currentDragTarget == DragTarget::CutRangeHandle)
    {
        // Both range handles are connected - move both together
        float newRange = juce::jlimit(0.0f, 12.0f, dragStartValue - deltaDb);
        boostRangeDb.store(newRange);
        cutRangeDb.store(newRange);  // Keep both in sync
        if (onBoostRangeChanged) onBoostRangeChanged(newRange);
        if (onCutRangeChanged) onCutRangeChanged(newRange);
        if (onRangeChanged) onRangeChanged(newRange);
    }
    
    repaint();
}

void WaveformDisplay::mouseUp(const juce::MouseEvent&)
{
    currentDragTarget = DragTarget::None;
}

void WaveformDisplay::mouseMove(const juce::MouseEvent& event)
{
    auto newHover = hitTestHandle(event.position);
    if (newHover != hoverTarget)
    {
        hoverTarget = newHover;
        if (hoverTarget != DragTarget::None)
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}

//==============================================================================
// Parameter setters

void WaveformDisplay::setTargetLevel(float targetDb)
{
    targetLevelDb.store(juce::jlimit(-40.0f, 0.0f, targetDb));
}

void WaveformDisplay::setBoostRange(float db)
{
    boostRangeDb.store(juce::jlimit(0.0f, 12.0f, db));
}

void WaveformDisplay::setCutRange(float db)
{
    cutRangeDb.store(juce::jlimit(0.0f, 12.0f, db));
}

void WaveformDisplay::setRange(float rangeDb)
{
    float clamped = juce::jlimit(0.0f, 12.0f, rangeDb);
    boostRangeDb.store(clamped);
    cutRangeDb.store(clamped);
}

void WaveformDisplay::setGainStats(float avg, float min, float max)
{
    avgGainDb.store(avg);
    minGainDb.store(min);
    maxGainDb.store(max);
}

void WaveformDisplay::resetStats()
{
    avgGainDb.store(0.0f);
    minGainDb.store(0.0f);
    maxGainDb.store(0.0f);
    gainAccumulator = 0.0f;
    gainMinTrack = 100.0f;
    gainMaxTrack = -100.0f;
    statsSampleCount = 0;
}

void WaveformDisplay::clear()
{
    initializeWaveformImage();
    sampleCounter = 0;
    currentInputMax = 0.0f;
    currentOutputMax = 0.0f;
    currentGainSum = 0.0f;
    gainSampleCount = 0;
    gainCurveOpacity = 0.0f;
    hasActiveAudio = false;
    isClipping = false;
    hasLastDrawnData = false;
    lastDrawnData = SampleData();
    resetStats();
}

//==============================================================================
// Audio data input

void WaveformDisplay::pushSamples(const float* inputSamples, const float* outputSamples,
                                   const float* gainValues, int numSamples)
{
    bool hadClip = false;
    
    for (int i = 0; i < numSamples; ++i)
    {
        float inAbs = std::abs(inputSamples[i]);
        float outAbs = (outputSamples != nullptr) ? std::abs(outputSamples[i]) : inAbs;
        
        // Track both min and max for proper waveform display
        currentInputMin = juce::jmin(currentInputMin, inAbs);
        currentInputMax = juce::jmax(currentInputMax, inAbs);
        currentOutputMin = juce::jmin(currentOutputMin, outAbs);
        currentOutputMax = juce::jmax(currentOutputMax, outAbs);
        currentGainSum += gainValues[i];
        gainSampleCount++;

        if (outAbs > 0.99f) hadClip = true;

        float gain = gainValues[i];
        gainAccumulator += gain;
        gainMinTrack = juce::jmin(gainMinTrack, gain);
        gainMaxTrack = juce::jmax(gainMaxTrack, gain);
        statsSampleCount++;

        if (inAbs > 0.001f)
        {
            hasActiveAudio = true;
            silenceSampleCount = 0;
        }
        else
        {
            silenceSampleCount++;
            // Very fast silence detection (~30ms at 44.1kHz) for immediate stop response
            if (silenceSampleCount > 1323)
            {
                hasActiveAudio = false;
                // Clear pending data queue immediately when silence detected
                juce::SpinLock::ScopedLockType lock(pendingLock);
                pendingData.clear();
            }
        }

        sampleCounter++;
        
        if (sampleCounter >= samplesPerEntry)
        {
            // Create sample data with min/max for proper waveform display
            SampleData data;
            data.inputMin = currentInputMin;
            data.inputMax = currentInputMax;
            data.outputMin = currentOutputMin;
            data.outputMax = currentOutputMax;
            data.gainDb = (gainSampleCount > 0) ? 
                (currentGainSum / static_cast<float>(gainSampleCount)) : 0.0f;
            
            {
                juce::SpinLock::ScopedLockType lock(pendingLock);
                pendingData.push_back(data);
                // Limit queue size to prevent delay buildup
                while (pendingData.size() > 50)
                {
                    pendingData.erase(pendingData.begin());
                }
            }

            // Reset accumulators for next block
            sampleCounter = 0;
            currentInputMin = 1.0f;   // Reset min high
            currentInputMax = 0.0f;   // Reset max low
            currentOutputMin = 1.0f;
            currentOutputMax = 0.0f;
            currentGainSum = 0.0f;
            gainSampleCount = 0;
        }
    }

    if (hadClip)
    {
        isClipping = true;
        clippingSampleCount = 0;
    }
    else
    {
        clippingSampleCount++;
        if (clippingSampleCount > 22050)
            isClipping = false;
    }

    if (statsSampleCount > 0)
    {
        avgGainDb.store(gainAccumulator / static_cast<float>(statsSampleCount));
        minGainDb.store(gainMinTrack);
        maxGainDb.store(gainMaxTrack);
    }
    
    if (numSamples > 0)
    {
        currentGainDb.store(gainValues[numSamples - 1]);
    }
}

//==============================================================================
// Image-based waveform rendering

void WaveformDisplay::shiftWaveformImage(int pixels)
{
    if (pixels <= 0) return;
    
    float defaultY = static_cast<float>(imageHeight);  // Bottom = no signal
    
    // Helper lambda to shift a buffer left
    auto shiftBuffer = [&](std::vector<float>& buffer, float fillValue) {
        if (!buffer.empty() && pixels < static_cast<int>(buffer.size()))
        {
            std::memmove(buffer.data(), 
                         buffer.data() + pixels, 
                         (buffer.size() - static_cast<size_t>(pixels)) * sizeof(float));
            for (size_t i = buffer.size() - static_cast<size_t>(pixels); i < buffer.size(); ++i)
            {
                buffer[i] = fillValue;
            }
        }
    };
    
    // Shift all position buffers left
    shiftBuffer(inputTopBuffer, defaultY);
    shiftBuffer(inputBottomBuffer, defaultY);
    shiftBuffer(outputTopBuffer, defaultY);
    shiftBuffer(outputBottomBuffer, defaultY);
    shiftBuffer(gainCurveBuffer, 0.0f);
}

void WaveformDisplay::storeColumnData(int x, const SampleData& data)
{
    if (x < 0 || x >= imageWidth) return;
    
    float h = static_cast<float>(imageHeight);
    size_t idx = static_cast<size_t>(x);
    
    // Store gain value
    if (idx < gainCurveBuffer.size())
    {
        gainCurveBuffer[idx] = data.gainDb;
    }
    
    // Default: no signal (at bottom)
    float inputTopY = h;
    float inputBottomY = h;
    float outputTopY = h;
    float outputBottomY = h;
    
    // === INPUT WAVEFORM positions ===
    if (data.inputMax > 0.0001f)
    {
        inputTopY = linearToLogY(data.inputMax, h);
        inputBottomY = linearToLogY(data.inputMin + 0.0001f, h);
        
        // Smooth the top edge (1-pole low-pass)
        if (x > 0 && idx < inputTopBuffer.size())
        {
            float prevTopY = inputTopBuffer[idx - 1];
            if (prevTopY < h - 2.0f)
            {
                inputTopY = prevTopY * 0.12f + inputTopY * 0.88f;
            }
        }
        
        // Ensure minimum thickness
        inputBottomY = juce::jmax(inputBottomY, inputTopY + 2.0f);
    }
    
    // === OUTPUT (boost/cut) positions ===
    if (std::abs(data.gainDb) > 0.3f && data.outputMax > 0.0001f)
    {
        outputTopY = linearToLogY(data.outputMax, h);
        outputBottomY = linearToLogY(data.outputMin + 0.0001f, h);
        
        // Smooth the top edge
        if (x > 0 && idx < outputTopBuffer.size())
        {
            float prevOutputY = outputTopBuffer[idx - 1];
            if (prevOutputY < h - 2.0f)
            {
                outputTopY = prevOutputY * 0.12f + outputTopY * 0.88f;
            }
        }
        
        outputBottomY = juce::jmax(outputBottomY, outputTopY + 2.0f);
    }
    
    // Store all positions
    if (idx < inputTopBuffer.size())
    {
        inputTopBuffer[idx] = inputTopY;
        inputBottomBuffer[idx] = inputBottomY;
        outputTopBuffer[idx] = outputTopY;
        outputBottomBuffer[idx] = outputBottomY;
    }
}

void WaveformDisplay::drawGainCurvePath(juce::Graphics& g)
{
    if (gainCurveBuffer.empty() || imageWidth <= 0) return;
    
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    float subPixelOffset = static_cast<float>(scrollAccumulator);
    
    // Colors for vertical gradient (cyan at top, purple at bottom - matches target line style)
    juce::Colour topColour(0xFF40E8FF);           // Saturated bright cyan at top (boost region)
    juce::Colour bottomColour(0xFFC060F0);        // Saturated bright purple at bottom (cut region)
    
    // Glow colors (matching the gradient colors)
    juce::Colour aboveGlowColour(0xFF40E8FF);     // Cyan glow (matches topColour)
    juce::Colour belowGlowColour(0xFFC060F0);     // Purple glow (matches bottomColour)
    juce::Colour neutralGlowColour(0xFF80A0D0);   // Subtle cyan-purple blend glow
    
    // First pass: Build complete path and store data for coloring
    juce::Path gainPath;
    std::vector<float> smoothedGains(static_cast<size_t>(imageWidth));
    std::vector<float> yPositions(static_cast<size_t>(imageWidth));
    std::vector<float> xPositions(static_cast<size_t>(imageWidth));
    
    float prevGainDb = 0.0f;
    
    for (int i = 0; i < imageWidth; ++i)
    {
        float gainDb = gainCurveBuffer[static_cast<size_t>(i)];
        
        // Smooth out the gain values
        if (i > 0)
        {
            gainDb = prevGainDb * 0.3f + gainDb * 0.7f;
        }
        prevGainDb = gainDb;
        smoothedGains[static_cast<size_t>(i)] = gainDb;
        
        float clampedGain = juce::jlimit(-cut, boost, gainDb);
        float y = gainDbToY(clampedGain);
        float xPos = waveformArea.getX() + static_cast<float>(i) - subPixelOffset;
        
        yPositions[static_cast<size_t>(i)] = y;
        xPositions[static_cast<size_t>(i)] = xPos;
        
        if (i == 0)
            gainPath.startNewSubPath(xPos, y);
        else
            gainPath.lineTo(xPos, y);
    }
    
    if (gainPath.isEmpty()) return;
    
    // Calculate dominant color for glow based on average gain
    float avgGain = 0.0f;
    for (size_t i = 0; i < smoothedGains.size(); ++i)
        avgGain += smoothedGains[i];
    avgGain /= static_cast<float>(imageWidth);
    
    juce::Colour glowColour;
    float glowIntensity;
    
    if (avgGain > 0.2f)
    {
        float t = juce::jmin(1.0f, avgGain / boost);
        glowColour = neutralGlowColour.interpolatedWith(aboveGlowColour, t);
        glowIntensity = 0.3f + t * 0.4f;
    }
    else if (avgGain < -0.2f)
    {
        float t = juce::jmin(1.0f, std::abs(avgGain) / cut);
        glowColour = neutralGlowColour.interpolatedWith(belowGlowColour, t);
        glowIntensity = 0.3f + t * 0.4f;
    }
    else
    {
        glowColour = neutralGlowColour;
        glowIntensity = 0.25f;
    }
    
    // Draw subtle drop shadow behind the line
    g.setColour(juce::Colours::black.withAlpha(0.10f * gainCurveOpacity));
    juce::Path shadowPath = gainPath;
    shadowPath.applyTransform(juce::AffineTransform::translation(0.0f, 2.0f));
    g.strokePath(shadowPath, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::butt));
    
    // Draw gradient glow - multiple passes fading from full opacity to 0 over 10px
    // Draw from outermost (most transparent) to innermost (most opaque)
    const int glowPasses = 5;
    const float maxGlowRadius = 10.0f;
    for (int pass = 0; pass < glowPasses; ++pass)
    {
        float t = static_cast<float>(pass) / static_cast<float>(glowPasses - 1);  // 0 = outer, 1 = inner
        float strokeWidth = maxGlowRadius * 2.0f * (1.0f - t) + 2.5f;  // From 22.5px down to 2.5px
        float alpha = t * 0.35f * glowIntensity * gainCurveOpacity;  // 0% at outer, 35% at inner (reduced)
        
        g.setColour(glowColour.withAlpha(alpha));
        g.strokePath(gainPath, juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::butt));
    }
    
    // Neutral color for when at/near target (blend of cyan and purple)
    juce::Colour neutralColour = topColour.interpolatedWith(bottomColour, 0.5f).withSaturation(0.3f);
    
    // Draw colored line segments - color based on gain relative to target
    // Above target (boosting) = cyan, below target (cutting) = purple
    for (int i = 1; i < imageWidth; ++i)
    {
        // Get the gain value for this segment (relative to target line)
        float midGain = (smoothedGains[static_cast<size_t>(i)] + smoothedGains[static_cast<size_t>(i-1)]) / 2.0f;
        
        juce::Colour segmentColour;
        if (midGain > 0.1f)
        {
            // Above target = boosting = cyan
            // More saturated cyan the further above target (up to boost range)
            float t = juce::jlimit(0.0f, 1.0f, midGain / boost);
            segmentColour = neutralColour.interpolatedWith(topColour, t);
        }
        else if (midGain < -0.1f)
        {
            // Below target = cutting = purple
            // More saturated purple the further below target (down to cut range)
            float t = juce::jlimit(0.0f, 1.0f, std::abs(midGain) / cut);
            segmentColour = neutralColour.interpolatedWith(bottomColour, t);
        }
        else
        {
            // At or near target = neutral blend
            segmentColour = neutralColour;
        }
        
        g.setColour(segmentColour.withAlpha(0.95f * gainCurveOpacity));
        g.drawLine(xPositions[static_cast<size_t>(i-1)], yPositions[static_cast<size_t>(i-1)],
                   xPositions[static_cast<size_t>(i)], yPositions[static_cast<size_t>(i)], 2.5f);
    }
    
    // Draw white center line on top for extra definition
    g.setColour(juce::Colours::white.withAlpha(0.7f * gainCurveOpacity));
    g.strokePath(gainPath, juce::PathStrokeType(0.5f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::butt));
}

void WaveformDisplay::drawWaveformPaths(juce::Graphics& g)
{
    if (inputTopBuffer.empty() || imageWidth <= 0) return;
    
    float subPixelOffset = static_cast<float>(scrollAccumulator);
    float h = static_cast<float>(imageHeight);
    float offsetX = waveformArea.getX() - subPixelOffset;
    float offsetY = waveformArea.getY();
    
    // Natural mode opacity: reduce by 20% when not in a phrase
    float naturalOpacityMult = (naturalModeActive && !phraseActive) ? 0.80f : 1.0f;
    
    // Base opacity reduction (10% less than before)
    float baseOpacity = 0.90f;
    
    // Helper to build a closed waveform path - TOP ONLY with flat bottom
    // This avoids spiky transient artifacts at the bottom
    auto buildTopOnlyPath = [&](const std::vector<float>& topBuf, 
                                int startIdx, int endIdx) -> juce::Path
    {
        juce::Path path;
        if (startIdx >= endIdx) return path;
        
        float firstX = offsetX + static_cast<float>(startIdx);
        float lastX = offsetX + static_cast<float>(endIdx - 1);
        float bottomY = offsetY + h;  // Flat bottom at display bottom
        
        // Start at bottom-left
        path.startNewSubPath(firstX, bottomY);
        
        // Go UP to first top point
        float firstTopY = topBuf[static_cast<size_t>(startIdx)];
        path.lineTo(firstX, offsetY + firstTopY);
        
        // Traverse TOP edge left-to-right
        for (int i = startIdx + 1; i < endIdx; ++i)
        {
            float topY = topBuf[static_cast<size_t>(i)];
            float x = offsetX + static_cast<float>(i);
            path.lineTo(x, offsetY + topY);
        }
        
        // Go DOWN to flat bottom at end
        path.lineTo(lastX, bottomY);
        
        // Close back to start (flat bottom line)
        path.closeSubPath();
        return path;
    };
    
    // === DRAW INPUT WAVEFORM (top-only path with flat bottom) ===
    {
        int segmentStart = -1;
        
        for (int i = 0; i <= imageWidth; ++i)
        {
            bool hasSignal = (i < imageWidth) && (inputTopBuffer[static_cast<size_t>(i)] < h - 2.0f);
            
            if (hasSignal && segmentStart < 0)
            {
                segmentStart = i;
            }
            else if (!hasSignal && segmentStart >= 0)
            {
                juce::Path inputPath = buildTopOnlyPath(inputTopBuffer, segmentStart, i);
                if (!inputPath.isEmpty())
                {
                    // Fill with natural mode and base opacity adjustment
                    float fillAlpha = 0.50f * baseOpacity * naturalOpacityMult;  // -10% for gain curve visibility
                    g.setColour(juce::Colour(0xFF5A6878).withAlpha(fillAlpha));
                    g.fillPath(inputPath);
                    
                    // Stroke the top outline only (build separate path for stroke)
                    juce::Path topOutline;
                    float firstTopY = inputTopBuffer[static_cast<size_t>(segmentStart)];
                    topOutline.startNewSubPath(offsetX + static_cast<float>(segmentStart), 
                                               offsetY + firstTopY);
                    for (int j = segmentStart + 1; j < i; ++j)
                    {
                        float topY = inputTopBuffer[static_cast<size_t>(j)];
                        topOutline.lineTo(offsetX + static_cast<float>(j), offsetY + topY);
                    }
                    
                    float strokeAlpha = (naturalModeActive ? 0.69f : 0.59f) * baseOpacity * naturalOpacityMult;  // -10%
                    g.setColour(juce::Colour(0xFFD0D8E0).withAlpha(strokeAlpha));
                    g.strokePath(topOutline, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                                                   juce::PathStrokeType::rounded));
                }
                segmentStart = -1;
            }
        }
    }
    
    // === DRAW BOOST WAVEFORM (above input) ===
    {
        int segmentStart = -1;
        
        for (int i = 0; i <= imageWidth; ++i)
        {
            bool isBoost = (i < imageWidth) && 
                           (gainCurveBuffer[static_cast<size_t>(i)] > 0.3f) &&
                           (outputTopBuffer[static_cast<size_t>(i)] < inputTopBuffer[static_cast<size_t>(i)] - 1.0f);
            
            if (isBoost && segmentStart < 0)
            {
                segmentStart = i;
            }
            else if (!isBoost && segmentStart >= 0)
            {
                // Build boost path (from output top to input top)
                juce::Path boostPath;
                float firstX = offsetX + static_cast<float>(segmentStart);
                float firstInputY = inputTopBuffer[static_cast<size_t>(segmentStart)];
                float firstOutputY = outputTopBuffer[static_cast<size_t>(segmentStart)];
                
                boostPath.startNewSubPath(firstX, offsetY + firstInputY);
                boostPath.lineTo(firstX, offsetY + firstOutputY);
                
                // Top edge (output tops) left-to-right
                for (int j = segmentStart + 1; j < i; ++j)
                {
                    float x = offsetX + static_cast<float>(j);
                    float outputY = outputTopBuffer[static_cast<size_t>(j)];
                    boostPath.lineTo(x, offsetY + outputY);
                }
                
                // Down to input at end
                float lastX = offsetX + static_cast<float>(i - 1);
                float lastInputY = inputTopBuffer[static_cast<size_t>(i - 1)];
                boostPath.lineTo(lastX, offsetY + lastInputY);
                
                // Bottom edge (input tops) right-to-left
                for (int j = i - 2; j >= segmentStart; --j)
                {
                    float x = offsetX + static_cast<float>(j);
                    float inputY = inputTopBuffer[static_cast<size_t>(j)];
                    boostPath.lineTo(x, offsetY + inputY);
                }
                
                boostPath.closeSubPath();
                
                if (!boostPath.isEmpty())
                {
                    // Teal fill with base and natural mode opacity
                    float fillAlpha = 0.36f * baseOpacity * naturalOpacityMult;  // -10% for gain curve visibility
                    g.setColour(juce::Colour(0xFF4090A0).withAlpha(fillAlpha));
                    g.fillPath(boostPath);
                    
                    // Cyan stroke - just the top outline
                    juce::Path boostOutline;
                    boostOutline.startNewSubPath(firstX, offsetY + firstOutputY);
                    for (int j = segmentStart + 1; j < i; ++j)
                    {
                        float x = offsetX + static_cast<float>(j);
                        float outputY = outputTopBuffer[static_cast<size_t>(j)];
                        boostOutline.lineTo(x, offsetY + outputY);
                    }
                    
                    float strokeAlpha = (naturalModeActive ? 0.65f : 0.55f) * baseOpacity * naturalOpacityMult;  // -10%
                    g.setColour(juce::Colour(0xFF70C8E0).withAlpha(strokeAlpha));
                    g.strokePath(boostOutline, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                                                     juce::PathStrokeType::rounded));
                }
                segmentStart = -1;
            }
        }
    }
    
    // === DRAW CUT WAVEFORM (purple, top-only with flat bottom) ===
    {
        int segmentStart = -1;
        
        for (int i = 0; i <= imageWidth; ++i)
        {
            bool isCut = (i < imageWidth) && 
                         (gainCurveBuffer[static_cast<size_t>(i)] < -0.3f) &&
                         (outputTopBuffer[static_cast<size_t>(i)] < h - 2.0f);
            
            if (isCut && segmentStart < 0)
            {
                segmentStart = i;
            }
            else if (!isCut && segmentStart >= 0)
            {
                // Build cut path with flat bottom
                juce::Path cutPath = buildTopOnlyPath(outputTopBuffer, segmentStart, i);
                
                if (!cutPath.isEmpty())
                {
                    // Purple fill with base and natural mode opacity
                    float fillAlpha = 0.36f * baseOpacity * naturalOpacityMult;  // -10% for gain curve visibility
                    g.setColour(juce::Colour(0xFF806098).withAlpha(fillAlpha));
                    g.fillPath(cutPath);
                    
                    // Purple stroke - top outline only
                    juce::Path cutOutline;
                    float firstOutputY = outputTopBuffer[static_cast<size_t>(segmentStart)];
                    cutOutline.startNewSubPath(offsetX + static_cast<float>(segmentStart), 
                                               offsetY + firstOutputY);
                    for (int j = segmentStart + 1; j < i; ++j)
                    {
                        float x = offsetX + static_cast<float>(j);
                        float outputY = outputTopBuffer[static_cast<size_t>(j)];
                        cutOutline.lineTo(x, offsetY + outputY);
                    }
                    
                    float strokeAlpha = (naturalModeActive ? 0.65f : 0.55f) * baseOpacity * naturalOpacityMult;  // -10%
                    g.setColour(juce::Colour(0xFFA078C0).withAlpha(strokeAlpha));
                    g.strokePath(cutOutline, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                                                   juce::PathStrokeType::rounded));
                }
                segmentStart = -1;
            }
        }
    }
}

//==============================================================================
// Timer callback - handles continuous scrolling

void WaveformDisplay::timerCallback()
{
    if (waveformImage.isNull() || imageWidth <= 0) 
    {
        repaint();
        return;
    }
    
    // Calculate time delta
    double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;
    
    // Clamp delta to avoid large jumps
    deltaTime = juce::jmin(deltaTime, 0.025);
    
    // Fixed pixels per second - same visual speed at all display sizes
    // Larger displays show more time, smaller displays show less time
    double pixelsPerSecond = static_cast<double>(pixelsPerSecondFixed);
    
    // Accumulate scroll pixels
    scrollAccumulator += pixelsPerSecond * deltaTime;
    
    // Process whole pixels
    int pixelsToScroll = static_cast<int>(scrollAccumulator);
    if (pixelsToScroll > 0)
    {
        scrollAccumulator -= pixelsToScroll;
        
        // Shift image left
        shiftWaveformImage(pixelsToScroll);
        
        // Get pending data
        std::vector<SampleData> frameData;
        {
            juce::SpinLock::ScopedLockType lock(pendingLock);
            // Take only as much data as we need for the pixels we're drawing
            while (!pendingData.empty() && frameData.size() < static_cast<size_t>(pixelsToScroll * 2))
            {
                frameData.push_back(pendingData.front());
                pendingData.erase(pendingData.begin());
            }
        }
        
        // Draw new columns on the right
        for (int p = 0; p < pixelsToScroll; ++p)
        {
            int x = imageWidth - pixelsToScroll + p;
            SampleData dataToUse;
            
            if (!frameData.empty())
            {
                // Map pixel position to data index
                size_t dataIdx = static_cast<size_t>(p * static_cast<int>(frameData.size()) / pixelsToScroll);
                dataIdx = juce::jmin(dataIdx, frameData.size() - 1);
                dataToUse = frameData[dataIdx];
                
                // Interpolate with previous for smoothness
                if (hasLastDrawnData)
                {
                    float t = static_cast<float>(p + 1) / static_cast<float>(pixelsToScroll);
                    dataToUse.inputMin = lastDrawnData.inputMin + t * (dataToUse.inputMin - lastDrawnData.inputMin);
                    dataToUse.inputMax = lastDrawnData.inputMax + t * (dataToUse.inputMax - lastDrawnData.inputMax);
                    dataToUse.outputMin = lastDrawnData.outputMin + t * (dataToUse.outputMin - lastDrawnData.outputMin);
                    dataToUse.outputMax = lastDrawnData.outputMax + t * (dataToUse.outputMax - lastDrawnData.outputMax);
                    dataToUse.gainDb = lastDrawnData.gainDb + t * (dataToUse.gainDb - lastDrawnData.gainDb);
                }
            }
            else if (hasLastDrawnData)
            {
                // No new data - decay smoothly
                float decay = 0.92f;
                dataToUse.inputMin = lastDrawnData.inputMin * decay;
                dataToUse.inputMax = lastDrawnData.inputMax * decay;
                dataToUse.outputMin = lastDrawnData.outputMin * decay;
                dataToUse.outputMax = lastDrawnData.outputMax * decay;
                dataToUse.gainDb = lastDrawnData.gainDb * decay;
                if (dataToUse.inputMax < 0.0001f) { dataToUse.inputMin = 0.0f; dataToUse.inputMax = 0.0f; }
                if (dataToUse.outputMax < 0.0001f) { dataToUse.outputMin = 0.0f; dataToUse.outputMax = 0.0f; }
                if (std::abs(dataToUse.gainDb) < 0.1f) dataToUse.gainDb = 0.0f;
                lastDrawnData = dataToUse;
            }
            
            storeColumnData(x, dataToUse);
        }
        
        // Update last drawn data
        if (!frameData.empty())
        {
            lastDrawnData = frameData.back();
            hasLastDrawnData = true;
        }
    }
    
    // Decay gain display when no audio
    {
        juce::SpinLock::ScopedLockType lock(pendingLock);
        if (pendingData.empty())
        {
            float gain = currentGainDb.load();
            gain *= 0.9f;
            if (std::abs(gain) < 0.1f) gain = 0.0f;
            currentGainDb.store(gain);
        }
    }
    
    // Update gain curve opacity - faster fade when no audio
    float targetOpacity = hasActiveAudio ? 1.0f : 0.0f;
    float fadeSpeed = hasActiveAudio ? 0.1f : 0.05f;  // Faster fade in, slower fade out
    gainCurveOpacity += (targetOpacity - gainCurveOpacity) * fadeSpeed;
    
    repaint();
}

//==============================================================================
// Paint

void WaveformDisplay::paint(juce::Graphics& g)
{
    drawBackground(g);
    
    if (waveformImage.isNull())
        return;
    
    drawGridLines(g);
    drawTargetAndRangeLines(g);
    
    // Draw Natural Mode phrase indicator
    if (naturalModeActive)
    {
        drawPhraseIndicator(g);
    }
    
    // Clip to waveformArea for rendering
    g.saveState();
    g.reduceClipRegion(waveformArea.toNearestInt());
    
    // Draw waveforms as closed paths (fill + stroke together)
    drawWaveformPaths(g);
    
    // Draw smooth gain curve path (fades during silence)
    if (gainCurveOpacity > 0.02f)
    {
        drawGainCurvePath(g);
    }
    
    g.restoreState();
    
    drawIOMeters(g);
    drawClippingIndicator(g);
}

//==============================================================================
// Static drawing functions (drawn fresh each frame)

void WaveformDisplay::drawBackground(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    g.setColour(juce::Colour(0xFF252830));
    g.fillRect(bounds);
    
    // Subtle noise texture
    juce::Random rng(42);
    int step = 3;
    for (int y = static_cast<int>(bounds.getY()); y < static_cast<int>(bounds.getBottom()); y += step)
    {
        for (int x = static_cast<int>(bounds.getX()); x < static_cast<int>(bounds.getRight()); x += step)
        {
            float noise = rng.nextFloat();
            if (noise > 0.7f)
            {
                float brightness = (noise - 0.7f) * 0.08f;
                g.setColour(juce::Colours::white.withAlpha(brightness));
                g.fillRect(x, y, 1, 1);
            }
            else if (noise < 0.3f)
            {
                float darkness = (0.3f - noise) * 0.05f;
                g.setColour(juce::Colours::black.withAlpha(darkness));
                g.fillRect(x, y, 1, 1);
            }
        }
    }
    
    // Vignette effects
    float vignetteWidth = bounds.getWidth() * 0.2f;
    juce::ColourGradient leftVig(
        juce::Colour(0x70000000), bounds.getX(), bounds.getCentreY(),
        juce::Colours::transparentBlack, bounds.getX() + vignetteWidth, bounds.getCentreY(),
        false
    );
    g.setGradientFill(leftVig);
    g.fillRect(bounds.getX(), bounds.getY(), vignetteWidth, bounds.getHeight());
    
    juce::ColourGradient rightVig(
        juce::Colours::transparentBlack, bounds.getRight() - vignetteWidth, bounds.getCentreY(),
        juce::Colour(0x70000000), bounds.getRight(), bounds.getCentreY(),
        false
    );
    g.setGradientFill(rightVig);
    g.fillRect(bounds.getRight() - vignetteWidth, bounds.getY(), vignetteWidth, bounds.getHeight());
}

void WaveformDisplay::drawGridLines(juce::Graphics& g)
{
    if (waveformArea.isEmpty()) return;
    
    // Grid lines at key dB levels (range extends to -64 but we don't label it)
    float dbLevels[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f };
    
    for (float db : dbLevels)
    {
        float y = dbToY(db);
        bool isMajor = (db == 0.0f || db == -6.0f || db == -12.0f || db == -18.0f);
        
        g.setColour(juce::Colour(0xFF3A3F4B).withAlpha(isMajor ? 0.6f : 0.3f));
        g.drawHorizontalLine(static_cast<int>(y), waveformArea.getX(), waveformArea.getRight());
    }
    
    // dB labels on right side
    float labelX = waveformArea.getRight() - 28.0f;
    
    g.setFont(CustomLookAndFeel::getPluginFont(9.0f, false));
    g.setColour(juce::Colour(0xFFCCCCCC).withAlpha(0.9f));
    
    for (float db : dbLevels)
    {
        float y = dbToY(db);
        juce::String label = (db == 0.0f) ? "0" : juce::String(static_cast<int>(db));
        g.drawText(label, static_cast<int>(labelX), static_cast<int>(y - 6), 24, 12, juce::Justification::centredRight);
    }
}

void WaveformDisplay::drawTargetAndRangeLines(juce::Graphics& g)
{
    float target = targetLevelDb.load();
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    
    float targetY = dbToY(target);
    float boostY = dbToY(target + boost);
    float cutY = dbToY(target - cut);
    
    juce::Colour rangeColour(0xFF888899);  // Gray for range lines
    
    // Purple gradient colors (like the knobs)
    juce::Colour targetPurpleDark(0xFF9060D0);   // Purple
    juce::Colour targetPurpleLight(0xFFD0A0FF);  // Light purple
    
    // Range fill (subtle)
    g.setColour(rangeColour.withAlpha(0.04f));
    g.fillRect(waveformArea.getX(), boostY, waveformArea.getWidth(), cutY - boostY);
    
    // Boost range line - DASHED GRAY
    float dashLengths[] = { 6.0f, 4.0f };
    g.setColour(rangeColour.withAlpha(0.6f));
    g.drawDashedLine(juce::Line<float>(waveformArea.getX(), boostY, waveformArea.getRight(), boostY),
                     dashLengths, 2, 1.0f);
    
    // Cut range line - DASHED GRAY
    g.setColour(rangeColour.withAlpha(0.6f));
    g.drawDashedLine(juce::Line<float>(waveformArea.getX(), cutY, waveformArea.getRight(), cutY),
                     dashLengths, 2, 1.0f);
    
    // Target line - GRADIENT (purple to light purple)
    {
        juce::ColourGradient targetGrad(
            targetPurpleDark, waveformArea.getX(), targetY,
            targetPurpleLight, waveformArea.getRight(), targetY,
            false
        );
        g.setGradientFill(targetGrad);
        g.fillRect(waveformArea.getX(), targetY - 1.0f, waveformArea.getWidth(), 2.0f);
    }
    
    // === LEFT SIDE LABELS - ABOVE their lines ===
    g.setFont(CustomLookAndFeel::getPluginFont(11.0f, false));
    float labelX = waveformArea.getX() + 6.0f;
    
    // Target label - ABOVE the target line
    g.setColour(targetPurpleLight.withAlpha(0.95f));
    juce::String targetLabel = juce::String(static_cast<int>(target)) + " dB";
    g.drawText(targetLabel, static_cast<int>(labelX), static_cast<int>(targetY - 18), 50, 14, juce::Justification::left);
    
    // Range label (at boost line) - ABOVE the line
    g.setColour(rangeColour.withAlpha(0.85f));
    juce::String rangeLabel = "+" + juce::String(static_cast<int>(boost)) + " dB";
    g.drawText(rangeLabel, static_cast<int>(labelX), static_cast<int>(boostY - 18), 50, 14, juce::Justification::left);
    
    // Range label (at cut line) - ABOVE the line
    g.setColour(rangeColour.withAlpha(0.85f));
    juce::String cutLabel = "-" + juce::String(static_cast<int>(cut)) + " dB";
    g.drawText(cutLabel, static_cast<int>(labelX), static_cast<int>(cutY - 18), 50, 14, juce::Justification::left);
}

void WaveformDisplay::drawIOMeters(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto fullMeterArea = bounds.removeFromRight(static_cast<float>(ioMeterWidth));
    
    // Clip all meter drawing to fullMeterArea to prevent any overflow
    g.saveState();
    g.reduceClipRegion(fullMeterArea.toNearestInt());
    
    // Vignette
    juce::ColourGradient meterVignette(
        juce::Colour(0x00000000), fullMeterArea.getX() - 40.0f, fullMeterArea.getCentreY(),
        juce::Colour(0x60000000), fullMeterArea.getX(), fullMeterArea.getCentreY(),
        false
    );
    g.setGradientFill(meterVignette);
    g.fillRect(fullMeterArea.getX() - 40.0f, fullMeterArea.getY(), 40.0f, fullMeterArea.getHeight());
    
    g.setColour(juce::Colour(0x40000000));
    g.fillRect(fullMeterArea);
    
    // Reserve space at top for readouts
    float readoutHeight = 18.0f;
    float topPadding = 4.0f;
    float bottomPadding = 8.0f;
    
    auto meterArea = fullMeterArea;
    auto readoutArea = meterArea.removeFromTop(readoutHeight);
    meterArea.removeFromTop(topPadding);
    meterArea.removeFromBottom(bottomPadding);
    meterArea.removeFromLeft(6.0f);
    meterArea.removeFromRight(6.0f);
    
    float meterWidth = 10.0f;
    float meterGap = 6.0f;
    float meterHeight = meterArea.getHeight();
    
    // === LEFT METER: Classic peak meter with green-to-yellow-to-red gradient ===
    auto peakMeterBounds = meterArea.removeFromLeft(meterWidth);
    meterArea.removeFromLeft(meterGap);
    
    // Use the higher of input/output for the peak meter
    float inputDb = inputLevelDb.load();
    float outputDb = outputLevelDb.load();
    float peakDb = juce::jmax(inputDb, outputDb);
    
    // Update peak hold
    if (peakDb > inputPeakHoldDb)
    {
        inputPeakHoldDb = peakDb;
        inputPeakHoldCounter = 0;
    }
    else
    {
        inputPeakHoldCounter++;
        if (inputPeakHoldCounter > peakHoldFrames)
        {
            inputPeakHoldDb -= 0.3f;  // Slower decay
            if (inputPeakHoldDb < -100.0f) inputPeakHoldDb = -100.0f;  // Decay fully to hide
        }
    }
    
    {
        // Background
        g.setColour(juce::Colour(0xFF1A1D22));
        g.fillRoundedRectangle(peakMeterBounds, 2.0f);
        
        // Extended range: -64dB to +6dB to match waveform display
        // Only show meter if there's actual signal (not -infinity)
        float normalized = juce::jmap(peakDb, -64.0f, 6.0f, 0.0f, 1.0f);
        normalized = juce::jlimit(0.0f, 1.0f, normalized);
        float fillHeight = normalized * meterHeight;
        
        // Don't draw if signal is essentially silent (below -60dB)
        if (fillHeight > 0 && peakDb > -60.0f)
        {
            auto fillBounds = peakMeterBounds.withTop(peakMeterBounds.getBottom() - fillHeight);
            
            // Green to yellow to red gradient (bottom to top)
            juce::ColourGradient peakGrad;
            peakGrad.point1 = { fillBounds.getCentreX(), peakMeterBounds.getBottom() };
            peakGrad.point2 = { fillBounds.getCentreX(), peakMeterBounds.getY() };
            peakGrad.isRadial = false;
            
            peakGrad.addColour(0.0, juce::Colour(0xFF3AA060));   // Green
            peakGrad.addColour(0.6, juce::Colour(0xFF6AC060));   // Light green
            peakGrad.addColour(0.75, juce::Colour(0xFFE0C040)); // Yellow
            peakGrad.addColour(0.88, juce::Colour(0xFFF08030)); // Orange
            peakGrad.addColour(0.95, juce::Colour(0xFFE04040)); // Red
            peakGrad.addColour(1.0, juce::Colour(0xFFFF3030));  // Bright red at top
            
            g.setGradientFill(peakGrad);
            g.fillRoundedRectangle(fillBounds, 2.0f);
        }
        
        // === PEAK HOLD INDICATOR LINE ===
        float peakHoldNorm = juce::jmap(inputPeakHoldDb, -64.0f, 6.0f, 0.0f, 1.0f);
        peakHoldNorm = juce::jlimit(0.0f, 1.0f, peakHoldNorm);
        float peakHoldY = peakMeterBounds.getBottom() - peakHoldNorm * meterHeight;
        
        if (inputPeakHoldDb > -63.0f)
        {
            // Color based on level
            juce::Colour holdColour;
            if (inputPeakHoldDb > -3.0f)
                holdColour = juce::Colour(0xFFFF5050);  // Red
            else if (inputPeakHoldDb > -12.0f)
                holdColour = juce::Colour(0xFFE0C040);  // Yellow
            else
                holdColour = juce::Colour(0xFF6AC060);  // Green
            
            g.setColour(holdColour);
            g.fillRect(peakMeterBounds.getX() - 1.0f, peakHoldY - 1.0f, 
                       peakMeterBounds.getWidth() + 2.0f, 2.0f);
        }
    }
    
    // === RIGHT METER: Gain +/- meter (bipolar, center = 0 dB) ===
    auto gainMeterBounds = meterArea.removeFromLeft(meterWidth);
    
    float gainDb = currentGainDb.load();
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    
    {
        // Background
        g.setColour(juce::Colour(0xFF1A1D22));
        g.fillRoundedRectangle(gainMeterBounds, 2.0f);
        
        float centerY = gainMeterBounds.getCentreY();
        float halfHeight = gainMeterBounds.getHeight() / 2.0f;
        
        // Center line (0 dB reference)
        g.setColour(juce::Colour(0xFF5A5F6A));
        g.fillRect(gainMeterBounds.getX(), centerY - 0.5f, gainMeterBounds.getWidth(), 1.0f);
        
        if (gainDb > 0.1f)
        {
            float normalizedGain = juce::jmin(gainDb / boost, 1.0f);
            float barHeight = normalizedGain * halfHeight;
            auto boostBar = gainMeterBounds.withHeight(barHeight).withBottomY(centerY);
            
            juce::ColourGradient boostGrad(
                juce::Colour(0xFF40E8FF), boostBar.getCentreX(), boostBar.getY(),    // Matches gain curve cyan
                juce::Colour(0xFF2090B0), boostBar.getCentreX(), boostBar.getBottom(),
                false
            );
            g.setGradientFill(boostGrad);
            g.fillRoundedRectangle(boostBar, 1.0f);
        }
        else if (gainDb < -0.1f)
        {
            float normalizedGain = juce::jmin(-gainDb / cut, 1.0f);
            float barHeight = normalizedGain * halfHeight;
            auto cutBar = gainMeterBounds.withHeight(barHeight).withY(centerY);
            
            juce::ColourGradient cutGrad(
                juce::Colour(0xFFC060F0), cutBar.getCentreX(), cutBar.getY(),         // Matches gain curve purple
                juce::Colour(0xFF8040A0), cutBar.getCentreX(), cutBar.getBottom(),
                false
            );
            g.setGradientFill(cutGrad);
            g.fillRoundedRectangle(cutBar, 1.0f);
        }
    }
    
    // === READOUTS AT TOP - Better centered ===
    g.setFont(CustomLookAndFeel::getPluginFont(9.0f, false));
    
    // Peak meter readout - centered over peak meter
    float peakReadoutCenterX = peakMeterBounds.getCentreX();
    juce::String peakText;
    if (inputPeakHoldDb <= -59.0f)
    {
        peakText = "-INF";
        g.setColour(juce::Colour(0xFF808080));
    }
    else
    {
        peakText = juce::String(static_cast<int>(std::round(inputPeakHoldDb)));
        if (inputPeakHoldDb > -3.0f)
            g.setColour(juce::Colour(0xFFFF5050));
        else if (inputPeakHoldDb > -12.0f)
            g.setColour(juce::Colour(0xFFE0C040));
        else
            g.setColour(juce::Colour(0xFF6AC060));
    }
    g.drawText(peakText, static_cast<int>(peakReadoutCenterX - 12), static_cast<int>(readoutArea.getY() + 2), 
               24, 14, juce::Justification::centred);
    
    // Gain meter readout - centered over gain meter
    float gainReadoutCenterX = gainMeterBounds.getCentreX();
    juce::String gainText;
    if (std::abs(gainDb) < 0.1f)
    {
        gainText = "0";
        g.setColour(juce::Colour(0xFF808080));
    }
    else if (gainDb > 0)
    {
        gainText = "+" + juce::String(gainDb, 1);
        g.setColour(juce::Colour(0xFF00D4FF));
    }
    else
    {
        gainText = juce::String(gainDb, 1);
        g.setColour(juce::Colour(0xFFB060FF));
    }
    g.drawText(gainText, static_cast<int>(gainReadoutCenterX - 14), static_cast<int>(readoutArea.getY() + 2),
               28, 14, juce::Justification::centred);
    
    g.restoreState();  // Restore from clip region
}

void WaveformDisplay::drawClippingIndicator(juce::Graphics& /*g*/)
{
    // Can add clipping indicator if needed
}

void WaveformDisplay::drawPhraseIndicator(juce::Graphics& g)
{
    // Draw Natural Mode phrase indicator in top-right of waveform area
    // More visible design matching main waveform brightness
    float indicatorSize = 8.0f;
    float padding = 8.0f;
    float x = waveformArea.getRight() - ioMeterWidth - indicatorSize - padding;
    float y = waveformArea.getY() + padding;
    
    // Background pill for "NATURAL" label - more visible
    juce::Rectangle<float> pillArea(x - 52, y - 1, 60 + indicatorSize, indicatorSize + 2);
    
    // More visible background
    g.setColour(juce::Colour(0x40000000));
    g.fillRoundedRectangle(pillArea, 2.0f);
    
    // "NATURAL" text - brighter to match main waveform
    g.setFont(CustomLookAndFeel::getPluginFont(8.0f, true));
    g.setColour(juce::Colour(0xB0FFFFFF));  // Brighter text
    g.drawText("NATURAL", pillArea.withTrimmedRight(indicatorSize + 3), juce::Justification::centred);
    
    // Phrase active indicator dot - brighter to match main waveform
    juce::Colour dotColour;
    if (phraseActive)
    {
        // Brighter green-cyan when phrase is active (matches waveform brightness)
        dotColour = juce::Colour(0xFF70C8A0);  // Brighter
        
        // More visible glow
        g.setColour(dotColour.withAlpha(0.35f));
        g.fillEllipse(x - 2, y - 2, indicatorSize + 4, indicatorSize + 4);
    }
    else
    {
        // Brighter when not in phrase
        dotColour = juce::Colour(0xFF6A7078);  // Brighter gray
    }
    
    // Main dot
    g.setColour(dotColour);
    g.fillEllipse(x, y, indicatorSize, indicatorSize);
    
    // Brighter highlight on dot
    g.setColour(juce::Colours::white.withAlpha(phraseActive ? 0.35f : 0.15f));
    g.fillEllipse(x + 1.5f, y + 1.5f, indicatorSize * 0.35f, indicatorSize * 0.35f);
}
