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
    
    // Pre-allocate pendingData to avoid heap allocation on the audio thread
    // (push_back into a full vector triggers realloc which is a real-time violation)
    pendingData.reserve(2048);
    
    startTimerHz(30);  // 30Hz is plenty smooth and much lighter on CPU/GPU
}

WaveformDisplay::~WaveformDisplay()
{
    stopTimer();
}

void WaveformDisplay::resized()
{
    auto bounds = getLocalBounds().toFloat();
    
    waveformArea = bounds;
    waveformArea.removeFromRight(static_cast<float>(ioMeterWidth));  // No gap - flush with meters
    
    // Round to nearest integer to ensure perfect alignment between waveform and grid lines
    // This prevents scaling artifacts that cause misalignment at different sizes
    waveformArea = waveformArea.toNearestInt().toFloat();
    
    initializeWaveformImage();
    
    // Mark cached images for regeneration
    backgroundNeedsRedraw = true;
    staticOverlayNeedsRedraw = true;
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
        
        // Raw data history for rebuilding on zoom changes
        columnRawData.assign(static_cast<size_t>(imageWidth), SampleData{});
        
        // Reset scroll state
        scrollAccumulator = 0.0;
        
        // Clear pending data
        juce::SpinLock::ScopedLockType lock(pendingLock);
        pendingData.clear();
        pendingReadIndex = 0;
    }
}

//==============================================================================
// Coordinate conversions

float WaveformDisplay::linearToLogY(float linear, float areaHeight) const
{
    if (linear <= 0.00001f || !std::isfinite(linear)) return areaHeight;
    float db = 20.0f * std::log10(linear);
    if (!std::isfinite(db)) return areaHeight;
    float range = displayCeiling - displayFloor;
    if (range < 1.0f) range = 1.0f;
    float normalized = (db - displayFloor) / range;
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    return areaHeight - normalized * areaHeight;
}

float WaveformDisplay::dbToY(float db) const
{
    float range = displayCeiling - displayFloor;
    if (range < 1.0f) range = 1.0f;
    float normalized = (db - displayFloor) / range;
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    return waveformArea.getBottom() - normalized * waveformArea.getHeight();
}

float WaveformDisplay::yToDb(float y) const
{
    float h = waveformArea.getHeight();
    if (h < 1.0f) return displayFloor;
    float normalized = (waveformArea.getBottom() - y) / h;
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    return displayFloor + normalized * (displayCeiling - displayFloor);
}

float WaveformDisplay::gainDbToY(float gainDb) const
{
    float target = targetLevelDb.load();
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    float clampedGain = juce::jlimit(-cut, boost, gainDb);
    float effectiveDb = juce::jlimit(displayFloor, displayCeiling, target + clampedGain);
    return dbToY(effectiveDb);
}

float WaveformDisplay::gainDbToImageY(float gainDb, float imgHeight) const
{
    float target = targetLevelDb.load();
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    float clampedGain = juce::jlimit(-cut, boost, gainDb);
    float effectiveDb = juce::jlimit(displayFloor, displayCeiling, target + clampedGain);
    float range = displayCeiling - displayFloor;
    if (range < 1.0f) range = 1.0f;
    float normalized = (effectiveDb - displayFloor) / range;
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    return imgHeight - normalized * imgHeight;
}

void WaveformDisplay::updateAdaptiveZoom()
{
    float target = targetLevelDb.load();
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    
    // Show enough range to see target ± margin, plus the boost/cut range lines
    float neededCeiling = target + boost + 4.0f;
    float neededFloor = target - cut - 4.0f;
    
    // Ensure at least adaptiveMargin dB above and below target
    neededCeiling = juce::jmax(neededCeiling, target + adaptiveMargin);
    neededFloor = juce::jmin(neededFloor, target - adaptiveMargin);
    
    // Clamp to absolute limits
    neededCeiling = juce::jmin(neededCeiling, adaptiveCeilingMax);
    neededFloor = juce::jmax(neededFloor, adaptiveFloorMin);
    
    // Ensure minimum 30 dB range for readability
    float neededRange = neededCeiling - neededFloor;
    if (neededRange < 30.0f)
    {
        float expand = (30.0f - neededRange) / 2.0f;
        neededCeiling = juce::jmin(neededCeiling + expand, adaptiveCeilingMax);
        neededFloor = juce::jmax(neededFloor - expand, adaptiveFloorMin);
    }
    
    displayCeilingTarget = neededCeiling;
    displayFloorTarget = neededFloor;
    
    // Smooth transition
    float prevFloor = displayFloor;
    float prevCeiling = displayCeiling;
    displayFloor += (displayFloorTarget - displayFloor) * adaptiveSmoothCoeff;
    displayCeiling += (displayCeilingTarget - displayCeiling) * adaptiveSmoothCoeff;
    
    // If the range changed meaningfully, rebuild waveform positions and redraw overlays
    if (std::abs(displayFloor - prevFloor) > 0.05f || std::abs(displayCeiling - prevCeiling) > 0.05f)
    {
        rebuildWaveformFromRawData();
        staticOverlayNeedsRedraw = true;
    }
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
        float newRange = juce::jlimit(0.0f, 12.0f, dragStartValue + deltaDb);
        boostRangeDb.store(newRange);
        if (onBoostRangeChanged) onBoostRangeChanged(newRange);
        
        if (rangeLocked.load())
        {
            cutRangeDb.store(newRange);
            if (onCutRangeChanged) onCutRangeChanged(newRange);
            if (onRangeChanged) onRangeChanged(newRange);
        }
    }
    else if (currentDragTarget == DragTarget::CutRangeHandle)
    {
        float newRange = juce::jlimit(0.0f, 12.0f, dragStartValue - deltaDb);
        cutRangeDb.store(newRange);
        if (onCutRangeChanged) onCutRangeChanged(newRange);
        
        if (rangeLocked.load())
        {
            boostRangeDb.store(newRange);
            if (onBoostRangeChanged) onBoostRangeChanged(newRange);
            if (onRangeChanged) onRangeChanged(newRange);
        }
    }
    
    // Force cached overlay to redraw with new line positions
    staticOverlayNeedsRedraw = true;
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
    float clamped = juce::jlimit(-40.0f, 0.0f, targetDb);
    float prev = targetLevelDb.exchange(clamped);
    if (std::abs(prev - clamped) > 0.1f) staticElementsChanged = true;
}

void WaveformDisplay::setBoostRange(float db)
{
    float clamped = juce::jlimit(0.0f, 12.0f, db);
    float prev = boostRangeDb.exchange(clamped);
    if (std::abs(prev - clamped) > 0.1f) staticElementsChanged = true;
}

void WaveformDisplay::setCutRange(float db)
{
    float clamped = juce::jlimit(0.0f, 12.0f, db);
    float prev = cutRangeDb.exchange(clamped);
    if (std::abs(prev - clamped) > 0.1f) staticElementsChanged = true;
}

void WaveformDisplay::setRange(float rangeDb)
{
    float clamped = juce::jlimit(0.0f, 12.0f, rangeDb);
    float prev = boostRangeDb.exchange(clamped);
    cutRangeDb.store(clamped);
    if (std::abs(prev - clamped) > 0.1f) staticElementsChanged = true;
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
    gainAccumulator.store(0.0f);
    gainMinTrack.store(100.0f);
    gainMaxTrack.store(-100.0f);
    statsSampleCount.store(0);
}

void WaveformDisplay::clear()
{
    initializeWaveformImage();
    sampleCounter = 0;
    currentInputSumSq = 0.0f;
    currentInputPeak = 0.0f;
    currentOutputSumSq = 0.0f;
    currentGainSum = 0.0f;
    gainSampleCount = 0;
    gainCurveOpacity = 0.0f;
    hasActiveAudio = false;
    isClipping = false;
    hasLastDrawnData = false;
    lastDrawnData = SampleData();
    {
        juce::SpinLock::ScopedLockType lock(pendingLock);
        pendingData.clear();
        pendingReadIndex = 0;
    }
    resetStats();
}

//==============================================================================
// Audio data input

void WaveformDisplay::pushSamples(const float* inputSamples, const float* outputSamples,
                                   const float* gainValues, int numSamples)
{
    if (inputSamples == nullptr || gainValues == nullptr || numSamples <= 0)
        return;
    
    bool hadClip = false;
    
    for (int i = 0; i < numSamples; ++i)
    {
        float inAbs = std::abs(inputSamples[i]);
        float outAbs = (outputSamples != nullptr) ? std::abs(outputSamples[i]) : inAbs;
        
        currentInputSumSq += inAbs * inAbs;
        currentInputPeak = juce::jmax(currentInputPeak, inAbs);
        currentOutputSumSq += outAbs * outAbs;
        currentGainSum += gainValues[i];
        gainSampleCount++;

        if (outAbs > 0.99f) hadClip = true;

        float gain = gainValues[i];
        gainAccumulator.store(gainAccumulator.load() + gain);
        gainMinTrack.store(juce::jmin(gainMinTrack.load(), gain));
        gainMaxTrack.store(juce::jmax(gainMaxTrack.load(), gain));
        statsSampleCount++;

        if (inAbs > 0.001f)
        {
            hasActiveAudio = true;
            silenceSampleCount = 0;
        }
        else
        {
            silenceSampleCount++;
            if (silenceSampleCount > 1323)
            {
                hasActiveAudio = false;
            }
        }

        sampleCounter++;
        
        if (sampleCounter >= samplesPerEntry)
        {
            float n = static_cast<float>(sampleCounter);
            SampleData data;
            data.inputRms = std::sqrt(currentInputSumSq / n);
            data.inputPeak = currentInputPeak;
            data.outputRms = std::sqrt(currentOutputSumSq / n);
            data.gainDb = (gainSampleCount > 0) ? 
                (currentGainSum / static_cast<float>(gainSampleCount)) : 0.0f;
            
            {
                juce::SpinLock::ScopedLockType lock(pendingLock);
                pendingData.push_back(data);
                int unread = static_cast<int>(pendingData.size()) - pendingReadIndex;
                if (unread > 50)
                {
                    pendingReadIndex = static_cast<int>(pendingData.size()) - 50;
                }
                if (pendingReadIndex > 0 && pendingReadIndex >= static_cast<int>(pendingData.size()))
                {
                    pendingData.clear();
                    pendingReadIndex = 0;
                }
            }

            sampleCounter = 0;
            currentInputSumSq = 0.0f;
            currentInputPeak = 0.0f;
            currentOutputSumSq = 0.0f;
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

    if (statsSampleCount.load() > 0)
    {
        avgGainDb.store(gainAccumulator.load() / static_cast<float>(statsSampleCount.load()));
        minGainDb.store(gainMinTrack.load());
        maxGainDb.store(gainMaxTrack.load());
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
        if (buffer.empty()) return;
        if (pixels >= static_cast<int>(buffer.size()))
        {
            // Entire buffer is stale - fill with default value
            std::fill(buffer.begin(), buffer.end(), fillValue);
            return;
        }
        std::memmove(buffer.data(), 
                     buffer.data() + pixels, 
                     (buffer.size() - static_cast<size_t>(pixels)) * sizeof(float));
        for (size_t i = buffer.size() - static_cast<size_t>(pixels); i < buffer.size(); ++i)
        {
            buffer[i] = fillValue;
        }
    };
    
    // Shift all position buffers left
    shiftBuffer(inputTopBuffer, defaultY);
    shiftBuffer(inputBottomBuffer, defaultY);
    shiftBuffer(outputTopBuffer, defaultY);
    shiftBuffer(outputBottomBuffer, defaultY);
    shiftBuffer(gainCurveBuffer, 0.0f);
    
    // Shift raw data history
    if (!columnRawData.empty())
    {
        if (pixels >= static_cast<int>(columnRawData.size()))
        {
            std::fill(columnRawData.begin(), columnRawData.end(), SampleData{});
        }
        else
        {
            std::memmove(columnRawData.data(), 
                         columnRawData.data() + pixels,
                         (columnRawData.size() - static_cast<size_t>(pixels)) * sizeof(SampleData));
            for (size_t i = columnRawData.size() - static_cast<size_t>(pixels); i < columnRawData.size(); ++i)
                columnRawData[i] = SampleData{};
        }
    }
}

void WaveformDisplay::storeColumnData(int x, const SampleData& data)
{
    if (x < 0 || x >= imageWidth) return;
    
    float h = static_cast<float>(imageHeight);
    size_t idx = static_cast<size_t>(x);
    
    // Store raw data for rebuilding on zoom changes
    if (idx < columnRawData.size())
        columnRawData[idx] = data;
    
    if (idx < gainCurveBuffer.size())
    {
        gainCurveBuffer[idx] = data.gainDb;
    }
    
    float inputTopY = h;
    float inputBottomY = h;
    float outputTopY = h;
    float outputBottomY = h;
    
    // === INPUT WAVEFORM: RMS as solid body, peak as faint outline ===
    if (data.inputRms > 0.0001f)
    {
        inputTopY = linearToLogY(data.inputRms, h);
        
        // Smooth the top edge
        if (x > 0 && idx < inputTopBuffer.size())
        {
            float prevTopY = inputTopBuffer[idx - 1];
            if (prevTopY < h - 2.0f)
            {
                inputTopY = prevTopY * 0.15f + inputTopY * 0.85f;
            }
        }
        
        // Bottom edge: slightly below RMS for visual thickness
        inputBottomY = juce::jmax(linearToLogY(data.inputRms * 0.5f, h), inputTopY + 2.0f);
    }
    
    // === OUTPUT WAVEFORM: RMS of output ===
    if (std::abs(data.gainDb) > 0.3f && data.outputRms > 0.0001f)
    {
        outputTopY = linearToLogY(data.outputRms, h);
        
        if (x > 0 && idx < outputTopBuffer.size())
        {
            float prevOutputY = outputTopBuffer[idx - 1];
            if (prevOutputY < h - 2.0f)
            {
                outputTopY = prevOutputY * 0.15f + outputTopY * 0.85f;
            }
        }
        
        outputBottomY = juce::jmax(linearToLogY(data.outputRms * 0.5f, h), outputTopY + 2.0f);
    }
    
    if (idx < inputTopBuffer.size())
    {
        inputTopBuffer[idx] = inputTopY;
        inputBottomBuffer[idx] = inputBottomY;
        outputTopBuffer[idx] = outputTopY;
        outputBottomBuffer[idx] = outputBottomY;
    }
}

void WaveformDisplay::rebuildWaveformFromRawData()
{
    if (imageWidth <= 0 || columnRawData.empty()) return;
    
    float h = static_cast<float>(imageHeight);
    float defaultY = h;
    
    for (int x = 0; x < imageWidth; ++x)
    {
        size_t idx = static_cast<size_t>(x);
        if (idx >= columnRawData.size()) break;
        
        const auto& data = columnRawData[idx];
        
        if (idx < gainCurveBuffer.size())
            gainCurveBuffer[idx] = data.gainDb;
        
        float inputTopY = h;
        float inputBottomY = h;
        float outputTopY = h;
        float outputBottomY = h;
        
        if (data.inputRms > 0.0001f)
        {
            inputTopY = linearToLogY(data.inputRms, h);
            if (x > 0 && idx < inputTopBuffer.size())
            {
                float prevTopY = inputTopBuffer[idx - 1];
                if (prevTopY < h - 2.0f)
                    inputTopY = prevTopY * 0.15f + inputTopY * 0.85f;
            }
            inputBottomY = juce::jmax(linearToLogY(data.inputRms * 0.5f, h), inputTopY + 2.0f);
        }
        
        if (std::abs(data.gainDb) > 0.3f && data.outputRms > 0.0001f)
        {
            outputTopY = linearToLogY(data.outputRms, h);
            if (x > 0 && idx < outputTopBuffer.size())
            {
                float prevOutputY = outputTopBuffer[idx - 1];
                if (prevOutputY < h - 2.0f)
                    outputTopY = prevOutputY * 0.15f + outputTopY * 0.85f;
            }
            outputBottomY = juce::jmax(linearToLogY(data.outputRms * 0.5f, h), outputTopY + 2.0f);
        }
        
        if (idx < inputTopBuffer.size())
        {
            inputTopBuffer[idx] = inputTopY;
            inputBottomBuffer[idx] = inputBottomY;
            outputTopBuffer[idx] = outputTopY;
            outputBottomBuffer[idx] = outputBottomY;
        }
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
        float t = (boost > 0.001f) ? juce::jmin(1.0f, avgGain / boost) : 1.0f;
        glowColour = neutralGlowColour.interpolatedWith(aboveGlowColour, t);
        glowIntensity = 0.3f + t * 0.4f;
    }
    else if (avgGain < -0.2f)
    {
        float t = (cut > 0.001f) ? juce::jmin(1.0f, std::abs(avgGain) / cut) : 1.0f;
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
            float t = (boost > 0.001f) ? juce::jlimit(0.0f, 1.0f, midGain / boost) : 1.0f;
            segmentColour = neutralColour.interpolatedWith(topColour, t);
        }
        else if (midGain < -0.1f)
        {
            float t = (cut > 0.001f) ? juce::jlimit(0.0f, 1.0f, std::abs(midGain) / cut) : 1.0f;
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
                    // Fill with natural mode and base opacity adjustment - darker gray for better contrast
                    float fillAlpha = 0.55f * baseOpacity * naturalOpacityMult;
                    g.setColour(juce::Colour(0xFF3A4248).withAlpha(fillAlpha));  // Darker charcoal gray
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
                    
                    // Whiter outline for better visibility
                    float strokeAlpha = (naturalModeActive ? 0.75f : 0.65f) * baseOpacity * naturalOpacityMult;
                    g.setColour(juce::Colour(0xFFE0E4E8).withAlpha(strokeAlpha));  // Brighter white outline
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
                    // Darker blue/teal fill for better contrast
                    float fillAlpha = 0.40f * baseOpacity * naturalOpacityMult;
                    g.setColour(juce::Colour(0xFF306878).withAlpha(fillAlpha));  // Darker blue-teal
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
                    
                    float strokeAlpha = (naturalModeActive ? 0.70f : 0.60f) * baseOpacity * naturalOpacityMult;
                    g.setColour(juce::Colour(0xFF50A8C8).withAlpha(strokeAlpha));  // Slightly darker cyan outline
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
                    // Darker purple fill for better contrast
                    float fillAlpha = 0.40f * baseOpacity * naturalOpacityMult;
                    g.setColour(juce::Colour(0xFF584068).withAlpha(fillAlpha));  // Darker purple
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
                    
                    float strokeAlpha = (naturalModeActive ? 0.70f : 0.60f) * baseOpacity * naturalOpacityMult;
                    g.setColour(juce::Colour(0xFF8860A0).withAlpha(strokeAlpha));  // Slightly darker purple outline
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
        return;  // Don't repaint if nothing to draw
    }
    
    // Always update lastFrameTime to prevent time jumps after idle periods
    double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;
    
    // Clamp delta to avoid large jumps (e.g., after returning from idle)
    deltaTime = juce::jmin(deltaTime, 0.04);
    
    // Update adaptive display zoom (smoothly track target level)
    updateAdaptiveZoom();
    
    // Check if we have any pending audio data
    bool hasPendingData = false;
    {
        juce::SpinLock::ScopedLockType lock(pendingLock);
        hasPendingData = (pendingReadIndex < static_cast<int>(pendingData.size()));
    }
    
    // Manage tail scrolling: when audio stops, keep scrolling to clear display
    if (hasActiveAudio || hasPendingData)
    {
        tailScrollFrames = maxTailScrollFrames;  // Reset tail timer when audio is active
    }
    else if (tailScrollFrames > 0)
    {
        tailScrollFrames--;
    }
    
    // If static elements (noise floor, target, range) changed, regenerate cached overlay
    if (staticElementsChanged)
    {
        staticElementsChanged = false;
        staticOverlayNeedsRedraw = true;
        repaint();
        // Still continue to check if we should do full scroll update
    }
    
    // Skip scroll/animation update if truly idle (no audio, no tail scroll, fully faded)
    if (!hasPendingData && !hasActiveAudio && tailScrollFrames <= 0 && gainCurveOpacity < 0.01f)
    {
        return;  // Nothing to animate - save CPU/GPU
    }
    
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
        
        // Get pending data (O(1) read via index - no erase/shift)
        frameDataBuffer.clear();
        {
            juce::SpinLock::ScopedLockType lock(pendingLock);
            size_t available = pendingData.size() - static_cast<size_t>(pendingReadIndex);
            size_t needed = static_cast<size_t>(pixelsToScroll * 2);
            size_t toTake = juce::jmin(available, needed);
            
            if (toTake > 0)
            {
                frameDataBuffer.assign(
                    pendingData.begin() + pendingReadIndex,
                    pendingData.begin() + pendingReadIndex + static_cast<int>(toTake));
                pendingReadIndex += static_cast<int>(toTake);
            }
            
            // Compact the vector when fully consumed (amortized O(1))
            if (pendingReadIndex >= static_cast<int>(pendingData.size()))
            {
                pendingData.clear();
                pendingReadIndex = 0;
            }
            // Also compact when read index is large (prevent unbounded growth)
            else if (pendingReadIndex > 1024)
            {
                pendingData.erase(pendingData.begin(), pendingData.begin() + pendingReadIndex);
                pendingReadIndex = 0;
            }
        }
        auto& frameData = frameDataBuffer;
        
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
                    dataToUse.inputRms = lastDrawnData.inputRms + t * (dataToUse.inputRms - lastDrawnData.inputRms);
                    dataToUse.inputPeak = lastDrawnData.inputPeak + t * (dataToUse.inputPeak - lastDrawnData.inputPeak);
                    dataToUse.outputRms = lastDrawnData.outputRms + t * (dataToUse.outputRms - lastDrawnData.outputRms);
                    dataToUse.gainDb = lastDrawnData.gainDb + t * (dataToUse.gainDb - lastDrawnData.gainDb);
                }
            }
            else if (hasLastDrawnData)
            {
                float decay = 0.92f;
                dataToUse.inputRms = lastDrawnData.inputRms * decay;
                dataToUse.inputPeak = lastDrawnData.inputPeak * decay;
                dataToUse.outputRms = lastDrawnData.outputRms * decay;
                dataToUse.gainDb = lastDrawnData.gainDb * decay;
                if (dataToUse.inputRms < 0.0001f) { dataToUse.inputRms = 0.0f; dataToUse.inputPeak = 0.0f; }
                if (dataToUse.outputRms < 0.0001f) dataToUse.outputRms = 0.0f;
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
        if (pendingReadIndex >= static_cast<int>(pendingData.size()))
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
    
    // Use cached static overlay (grid + target/range lines - regenerated when params change)
    if (staticOverlayNeedsRedraw || cachedStaticOverlay.isNull())
        renderCachedStaticOverlay();
    
    if (!cachedStaticOverlay.isNull())
        g.drawImageAt(cachedStaticOverlay, 0, 0);
    
    // Draw Natural Mode phrase indicator (dynamic - changes per frame)
    if (naturalModeActive)
    {
        drawPhraseIndicator(g);
    }
    
    // Clip to waveformArea for rendering
    g.saveState();
    g.reduceClipRegion(waveformArea.toNearestInt());
    
    // Draw waveforms as closed paths (fill + stroke together)
    drawWaveformPaths(g);
    
    // Draw smooth gain curve path (fades during silence) - on top of waveforms
    if (gainCurveOpacity > 0.02f)
    {
        drawGainCurvePath(g);
    }
    
    g.restoreState();
    
    drawIOMeters(g);
    drawClippingIndicator(g);
}

//==============================================================================
// Cached rendering

void WaveformDisplay::renderCachedBackground()
{
    auto bounds = getLocalBounds();
    if (bounds.isEmpty()) return;
    
    cachedBackgroundImage = juce::Image(juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    juce::Graphics g(cachedBackgroundImage);
    
    auto boundsF = bounds.toFloat();
    
    g.setColour(juce::Colour(0xFF252830));
    g.fillRect(boundsF);
    
    // Subtle noise texture (rendered once, not per frame)
    juce::Random rng(42);
    int step = 3;
    for (int y = static_cast<int>(boundsF.getY()); y < static_cast<int>(boundsF.getBottom()); y += step)
    {
        for (int x = static_cast<int>(boundsF.getX()); x < static_cast<int>(boundsF.getRight()); x += step)
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
    float vignetteWidth = boundsF.getWidth() * 0.2f;
    juce::ColourGradient leftVig(
        juce::Colour(0x70000000), boundsF.getX(), boundsF.getCentreY(),
        juce::Colours::transparentBlack, boundsF.getX() + vignetteWidth, boundsF.getCentreY(),
        false
    );
    g.setGradientFill(leftVig);
    g.fillRect(boundsF.getX(), boundsF.getY(), vignetteWidth, boundsF.getHeight());
    
    juce::ColourGradient rightVig(
        juce::Colours::transparentBlack, boundsF.getRight() - vignetteWidth, boundsF.getCentreY(),
        juce::Colour(0x70000000), boundsF.getRight(), boundsF.getCentreY(),
        false
    );
    g.setGradientFill(rightVig);
    g.fillRect(boundsF.getRight() - vignetteWidth, boundsF.getY(), vignetteWidth, boundsF.getHeight());
    
    backgroundNeedsRedraw = false;
}

void WaveformDisplay::renderCachedStaticOverlay()
{
    auto bounds = getLocalBounds();
    if (bounds.isEmpty()) return;
    
    cachedStaticOverlay = juce::Image(juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    juce::Graphics g(cachedStaticOverlay);
    
    drawGridLines(g);
    drawTargetAndRangeLines(g);
    
    staticOverlayNeedsRedraw = false;
}

//==============================================================================
// Static drawing functions

void WaveformDisplay::drawBackground(juce::Graphics& g)
{
    // Use cached background image (regenerated only on resize)
    if (backgroundNeedsRedraw || cachedBackgroundImage.isNull())
        renderCachedBackground();
    
    if (!cachedBackgroundImage.isNull())
        g.drawImageAt(cachedBackgroundImage, 0, 0);
}

void WaveformDisplay::drawGridLines(juce::Graphics& g)
{
    if (waveformArea.isEmpty()) return;
    
    // Dynamic grid lines based on current adaptive display range
    float step = 6.0f;
    float range = displayCeiling - displayFloor;
    if (range > 50.0f) step = 12.0f;
    else if (range > 30.0f) step = 6.0f;
    else step = 3.0f;
    
    float firstLine = std::ceil(displayFloor / step) * step;
    
    float labelX = waveformArea.getRight() - 32.0f;
    g.setFont(CustomLookAndFeel::getPluginFont(11.0f, false));
    
    for (float db = firstLine; db <= displayCeiling; db += step)
    {
        float y = dbToY(db);
        
        // Major lines at 0, -6, -12, -18 (or multiples of 12 for large ranges)
        bool isMajor = (std::fmod(std::abs(db), 6.0f) < 0.1f);
        
        g.setColour(juce::Colour(0xFF3A3F4B).withAlpha(isMajor ? 0.6f : 0.3f));
        g.drawHorizontalLine(static_cast<int>(y), waveformArea.getX(), waveformArea.getRight());
        
        // Labels
        if (isMajor)
        {
            g.setColour(juce::Colour(0xFFCCCCCC).withAlpha(0.9f));
            int dbInt = static_cast<int>(std::round(db));
            juce::String label = (dbInt == 0) ? "0" : juce::String(dbInt);
            g.drawText(label, static_cast<int>(labelX), static_cast<int>(y - 7), 28, 14, juce::Justification::centredRight);
        }
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
    
    juce::Colour rangeColour(0xFF888899);
    
    juce::Colour targetPurpleDark(0xFF9060D0);
    juce::Colour targetPurpleLight(0xFFD0A0FF);
    
    float lineRightEdge = waveformArea.getRight() - 2.0f;
    float lineWidth = lineRightEdge - waveformArea.getX();
    
    // Range fill (subtle gray)
    g.setColour(rangeColour.withAlpha(0.04f));
    g.fillRect(waveformArea.getX(), boostY, lineWidth, cutY - boostY);
    
    float dashLengths[] = { 6.0f, 4.0f };
    
    // Boost range line - DASHED GRAY
    g.setColour(rangeColour.withAlpha(0.6f));
    g.drawDashedLine(juce::Line<float>(waveformArea.getX(), boostY, lineRightEdge, boostY),
                     dashLengths, 2, 1.0f);
    
    // Cut range line - DASHED GRAY
    g.setColour(rangeColour.withAlpha(0.6f));
    g.drawDashedLine(juce::Line<float>(waveformArea.getX(), cutY, lineRightEdge, cutY),
                     dashLengths, 2, 1.0f);
    
    // Target line - GRADIENT (purple to light purple)
    {
        juce::ColourGradient targetGrad(
            targetPurpleDark, waveformArea.getX(), targetY,
            targetPurpleLight, lineRightEdge, targetY,
            false
        );
        g.setGradientFill(targetGrad);
        g.fillRect(waveformArea.getX(), targetY - 1.0f, lineWidth, 2.0f);
    }
    
    // === LEFT SIDE LABELS - ABOVE their lines ===
    g.setFont(CustomLookAndFeel::getPluginFont(14.0f, true));
    float labelX = waveformArea.getX() + 6.0f;
    
    // Target label - ABOVE the target line
    g.setColour(targetPurpleLight.withAlpha(0.95f));
    juce::String targetLabel = juce::String(static_cast<int>(target)) + " dB";
    g.drawText(targetLabel, static_cast<int>(labelX), static_cast<int>(targetY - 22), 70, 18, juce::Justification::left);
    
    // Range label (at boost line) - ABOVE the line
    g.setColour(rangeColour.withAlpha(0.85f));
    juce::String rangeLabel = "+" + juce::String(static_cast<int>(boost)) + " dB";
    g.drawText(rangeLabel, static_cast<int>(labelX), static_cast<int>(boostY - 22), 70, 18, juce::Justification::left);
    
    // Range label (at cut line) - ABOVE the line
    g.setColour(rangeColour.withAlpha(0.85f));
    juce::String cutLabel = "-" + juce::String(static_cast<int>(cut)) + " dB";
    g.drawText(cutLabel, static_cast<int>(labelX), static_cast<int>(cutY - 22), 70, 18, juce::Justification::left);
    
    // === NOISE FLOOR LINE ===
    if (noiseFloorActive)
    {
        float nfDb = noiseFloorDb.load();
        if (nfDb > -59.9f)  // Only draw when active (above minimum)
        {
            float nfY = dbToY(nfDb);
            
            // Red color for noise floor (represents rejection)
            juce::Colour nfColour(0xFFC04040);
            
            // Semi-transparent fill below the noise floor line (to show "dead zone")
            g.setColour(nfColour.withAlpha(0.06f));
            g.fillRect(waveformArea.getX(), nfY, lineWidth, waveformArea.getBottom() - nfY);
            
            // Dashed line at noise floor level
            float nfDashLengths[] = { 4.0f, 3.0f };
            g.setColour(nfColour.withAlpha(0.7f));
            g.drawDashedLine(juce::Line<float>(waveformArea.getX(), nfY, lineRightEdge, nfY),
                             nfDashLengths, 2, 1.0f);
            
            // Label - just "NF", positioned below the line
            g.setFont(CustomLookAndFeel::getPluginFont(9.0f, false));
            g.setColour(nfColour.withAlpha(0.85f));
            g.drawText("NF", static_cast<int>(labelX), static_cast<int>(nfY + 4), 20, 14, juce::Justification::left);
        }
    }
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
    float readoutHeight = 16.0f;  // Slightly taller for larger font
    float topPadding = 3.0f;
    float bottomPadding = 6.0f;
    
    auto meterArea = fullMeterArea;
    auto readoutArea = meterArea.removeFromTop(readoutHeight);
    meterArea.removeFromTop(topPadding);
    meterArea.removeFromBottom(bottomPadding);
    
    // Center the meters: total meter content = 10 + 6 + 10 = 26px
    float meterWidth = 10.0f;
    float meterGap = 6.0f;
    float totalMeterWidth = meterWidth * 2 + meterGap;  // 26px
    float sidePadding = (meterArea.getWidth() - totalMeterWidth) / 2.0f;
    meterArea.removeFromLeft(sidePadding);
    meterArea.removeFromRight(sidePadding);
    
    float meterHeight = meterArea.getHeight();
    
    // === LEFT METER: Classic peak meter with green-to-yellow-to-red gradient ===
    auto peakMeterBounds = meterArea.removeFromLeft(meterWidth);
    meterArea.removeFromLeft(meterGap);
    
    // Use the higher of input/output for the peak meter
    float inputDb = inputLevelDb.load();
    float outputDb = outputLevelDb.load();
    float peakDb = juce::jmax(inputDb, outputDb);
    
    // Smooth the peak meter for less jittery display (faster response)
    float peakSmoothCoeff = 0.5f;
    smoothedPeakMeterDb = peakSmoothCoeff * smoothedPeakMeterDb + (1.0f - peakSmoothCoeff) * peakDb;
    
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
        
        // === RMS AVERAGE BAR (slower, dimmer - drawn BEHIND peak bar) ===
        // Smooth the RMS bar with slow decay (FabFilter-style trailing bar)
        float peakDbForRms = juce::jmax(inputDb, outputDb);
        if (peakDbForRms > rmsPeakBarDb)
        {
            // Fast attack for RMS bar
            rmsPeakBarDb = rmsPeakBarDb + (peakDbForRms - rmsPeakBarDb) * 0.3f;
        }
        else
        {
            // Moderate decay for RMS bar - shows average/sustained level but
            // fades to zero within ~3 seconds when audio stops (0.5 dB/frame at 30fps = 15 dB/s)
            rmsPeakBarDb = rmsPeakBarDb - 0.5f;
            if (rmsPeakBarDb < -100.0f) rmsPeakBarDb = -100.0f;
        }
        
        // Draw RMS bar (dimmer, wider-looking via lower opacity)
        float rmsNormalized = juce::jmap(rmsPeakBarDb, -64.0f, 0.0f, 0.0f, 1.0f);
        rmsNormalized = juce::jlimit(0.0f, 1.0f, rmsNormalized);
        float rmsFillHeight = rmsNormalized * meterHeight;
        
        if (rmsFillHeight > 0 && rmsPeakBarDb > -60.0f)
        {
            auto rmsFillBounds = peakMeterBounds.withTop(peakMeterBounds.getBottom() - rmsFillHeight);
            
            // Same gradient but much dimmer (trailing average bar)
            juce::ColourGradient rmsGrad;
            rmsGrad.point1 = { rmsFillBounds.getCentreX(), peakMeterBounds.getBottom() };
            rmsGrad.point2 = { rmsFillBounds.getCentreX(), peakMeterBounds.getY() };
            rmsGrad.isRadial = false;
            
            rmsGrad.addColour(0.0, juce::Colour(0xFF3AA060).withAlpha(0.25f));
            rmsGrad.addColour(0.6, juce::Colour(0xFF6AC060).withAlpha(0.25f));
            rmsGrad.addColour(0.75, juce::Colour(0xFFE0C040).withAlpha(0.25f));
            rmsGrad.addColour(0.88, juce::Colour(0xFFF08030).withAlpha(0.25f));
            rmsGrad.addColour(0.95, juce::Colour(0xFFE04040).withAlpha(0.25f));
            rmsGrad.addColour(1.0, juce::Colour(0xFFFF3030).withAlpha(0.25f));
            
            g.setGradientFill(rmsGrad);
            g.fillRoundedRectangle(rmsFillBounds, 2.0f);
        }
        
        // === PEAK BAR (fast, bright - drawn ON TOP of RMS bar) ===
        // Range: -64dB to 0dB — 0 dB fills the meter completely to the top
        // Use smoothed value for less jittery display
        float normalized = juce::jmap(smoothedPeakMeterDb, -64.0f, 0.0f, 0.0f, 1.0f);
        normalized = juce::jlimit(0.0f, 1.0f, normalized);
        float fillHeight = normalized * meterHeight;
        
        // Don't draw if signal is essentially silent (below -60dB)
        if (fillHeight > 0 && smoothedPeakMeterDb > -60.0f)
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
        float peakHoldNorm = juce::jmap(inputPeakHoldDb, -64.0f, 0.0f, 0.0f, 1.0f);
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
    
    // Smooth the meter bar value (faster response, less flicker)
    float meterSmoothCoeff = 0.7f;  // Faster than text, but still smooth
    smoothedMeterGainDb = meterSmoothCoeff * smoothedMeterGainDb + (1.0f - meterSmoothCoeff) * gainDb;
    
    // Smooth the readout value for stability (slower response for text display)
    float textSmoothCoeff = 0.85f;  // Slower/smoother for readable text
    smoothedReadoutGainDb = textSmoothCoeff * smoothedReadoutGainDb + (1.0f - textSmoothCoeff) * gainDb;
    
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
        
        // Use smoothed value for meter bar display
        if (smoothedMeterGainDb > 0.1f)
        {
            float normalizedGain = (boost > 0.001f) ? juce::jmin(smoothedMeterGainDb / boost, 1.0f) : 1.0f;
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
        else if (smoothedMeterGainDb < -0.1f)
        {
            float normalizedGain = (cut > 0.001f) ? juce::jmin(-smoothedMeterGainDb / cut, 1.0f) : 1.0f;
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
    
    // === READOUTS AT TOP ===
    g.setFont(CustomLookAndFeel::getPluginFont(11.0f, true));
    
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
    g.drawText(peakText, static_cast<int>(peakReadoutCenterX - 14), static_cast<int>(readoutArea.getY() + 1), 
               28, 14, juce::Justification::centred);
    
    // Gain meter readout - centered over gain meter (with decimal for precision)
    // Use smoothed value for more readable/stable display
    float gainReadoutCenterX = gainMeterBounds.getCentreX();
    juce::String gainText;
    if (std::abs(smoothedReadoutGainDb) < 0.1f)
    {
        gainText = "0";
        g.setColour(juce::Colour(0xFF808080));
    }
    else if (smoothedReadoutGainDb > 0)
    {
        gainText = "+" + juce::String(smoothedReadoutGainDb, 1);
        g.setColour(juce::Colour(0xFF00D4FF));
    }
    else
    {
        gainText = juce::String(smoothedReadoutGainDb, 1);
        g.setColour(juce::Colour(0xFFB060FF));
    }
    g.drawText(gainText, static_cast<int>(gainReadoutCenterX - 16), static_cast<int>(readoutArea.getY() + 1),
               32, 14, juce::Justification::centred);
    
    g.restoreState();  // Restore from clip region
}

void WaveformDisplay::drawClippingIndicator(juce::Graphics& /*g*/)
{
    // Can add clipping indicator if needed
}

void WaveformDisplay::drawPhraseIndicator(juce::Graphics& g)
{
    // Draw Natural Mode phrase indicator in top-right of waveform area
    // Larger, more visible design with clear active/inactive states
    float indicatorSize = 10.0f;
    float padding = 10.0f;
    float x = waveformArea.getRight() - ioMeterWidth - indicatorSize - padding;
    float y = waveformArea.getY() + padding;
    
    // Background pill for "NATURAL" label - more visible
    juce::Rectangle<float> pillArea(x - 60, y - 2, 70 + indicatorSize, indicatorSize + 4);
    
    // More visible background
    g.setColour(juce::Colour(0x50000000));
    g.fillRoundedRectangle(pillArea, 4.0f);
    g.setColour(juce::Colour(0x30FFFFFF));
    g.drawRoundedRectangle(pillArea, 4.0f, 0.5f);
    
    // "NATURAL" text - brighter
    g.setFont(CustomLookAndFeel::getPluginFont(9.0f, true));
    g.setColour(juce::Colour(0xC0FFFFFF));
    g.drawText("NATURAL", pillArea.withTrimmedRight(indicatorSize + 4), juce::Justification::centred);
    
    // Phrase active indicator dot - clear green vs gray
    juce::Colour dotColour;
    if (phraseActive)
    {
        // Bright green when phrase is active
        dotColour = juce::Colour(0xFF50E880);
        
        // Subtle glow around the dot (reduced from before)
        g.setColour(dotColour.withAlpha(0.10f));
        g.fillEllipse(x - 3, y - 2, indicatorSize + 6, indicatorSize + 4);
        g.setColour(dotColour.withAlpha(0.20f));
        g.fillEllipse(x - 1, y - 1, indicatorSize + 2, indicatorSize + 2);
    }
    else
    {
        // Dim gray when not in phrase
        dotColour = juce::Colour(0xFF5A6068);
    }
    
    // Main dot
    g.setColour(dotColour);
    g.fillEllipse(x, y, indicatorSize, indicatorSize);
    
    // Highlight on dot (specular) - reduced
    g.setColour(juce::Colours::white.withAlpha(phraseActive ? 0.25f : 0.10f));
    g.fillEllipse(x + 2.0f, y + 1.5f, indicatorSize * 0.3f, indicatorSize * 0.3f);
}
