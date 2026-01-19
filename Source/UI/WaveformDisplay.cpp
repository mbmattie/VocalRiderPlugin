/*
  ==============================================================================

    WaveformDisplay.cpp
    Created: 2026
    Author:  MBM Audio

    FabFilter Pro-C inspired waveform display.

  ==============================================================================
*/

#include "WaveformDisplay.h"
#include "CustomLookAndFeel.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

WaveformDisplay::WaveformDisplay()
{
    displayBuffer.resize(500);
    startTimerHz(60);
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
    
    displayWidth = static_cast<int>(waveformArea.getWidth());
    if (displayWidth > 0)
    {
        juce::SpinLock::ScopedLockType lock(bufferLock);
        displayBuffer.resize(static_cast<size_t>(displayWidth));
        std::fill(displayBuffer.begin(), displayBuffer.end(), SampleData{});
        writeIndex.store(0);
    }
}

//==============================================================================
// Coordinate conversions - LOGARITHMIC scale for waveform

float WaveformDisplay::linearToLogY(float linear) const
{
    // Convert linear (0-1) to logarithmic Y position
    // This compresses the top and expands the bottom for better visibility of quiet parts
    if (linear <= 0.0001f) return waveformArea.getBottom();
    
    // Logarithmic mapping: more space for quiet signals
    float logValue = std::log10(linear * 9.0f + 1.0f);  // log10(1 to 10) = 0 to 1
    float height = waveformArea.getHeight() * 0.9f;
    return waveformArea.getBottom() - logValue * height;
}

float WaveformDisplay::dbToY(float db) const
{
    // For handle positions - linear dB mapping
    float normalized = (db - (-48.0f)) / (6.0f - (-48.0f));
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    return waveformArea.getBottom() - normalized * waveformArea.getHeight();
}

float WaveformDisplay::yToDb(float y) const
{
    float normalized = (waveformArea.getBottom() - y) / waveformArea.getHeight();
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    return -48.0f + normalized * (6.0f - (-48.0f));
}

float WaveformDisplay::gainDbToY(float gainDb) const
{
    float target = targetLevelDb.load();
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    float targetY = dbToY(target);
    
    // Base range height at 12dB (full scale)
    float maxRangeHeight = waveformArea.getHeight() * 0.25f;
    
    // CLAMP the gain to the range bounds so curve never goes outside
    float clampedGain = juce::jlimit(-cut, boost, gainDb);
    
    // Scale the visual range based on actual range setting (same as range lines)
    // This matches how drawTargetAndRangeLines calculates boostY/cutY
    if (clampedGain >= 0)
    {
        // Boost: scale by (boost/12.0f) to match range line position
        float rangeScale = boost / 12.0f;
        float actualRangeHeight = maxRangeHeight * rangeScale;
        float normalizedGain = juce::jlimit(0.0f, 1.0f, clampedGain / boost);
        return targetY - normalizedGain * actualRangeHeight;
    }
    else
    {
        // Cut: scale by (cut/12.0f) to match range line position
        float rangeScale = cut / 12.0f;
        float actualRangeHeight = maxRangeHeight * rangeScale;
        float normalizedGain = juce::jlimit(0.0f, 1.0f, -clampedGain / cut);
        return targetY + normalizedGain * actualRangeHeight;
    }
}

//==============================================================================
// Hit testing

WaveformDisplay::DragTarget WaveformDisplay::hitTestHandle(const juce::Point<float>& pos) const
{
    // Only allow dragging within the waveform area
    if (!waveformArea.contains(pos))
        return DragTarget::None;
    
    float target = targetLevelDb.load();
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    float targetY = dbToY(target);
    float rangeHeight = waveformArea.getHeight() * 0.25f;
    
    // Boost (upper) range handle - check first since it's on top
    float boostY = targetY - rangeHeight * (boost / 12.0f);
    if (std::abs(pos.y - boostY) < handleHitDistance)
        return DragTarget::BoostRangeHandle;
    
    // Cut (lower) range handle
    float cutY = targetY + rangeHeight * (cut / 12.0f);
    if (std::abs(pos.y - cutY) < handleHitDistance)
        return DragTarget::CutRangeHandle;
    
    // Target handle (check last - in the middle, might conflict with range)
    if (std::abs(pos.y - targetY) < handleHitDistance)
        return DragTarget::TargetHandle;
    
    return DragTarget::None;
}

//==============================================================================
// Mouse interaction

void WaveformDisplay::mouseDown(const juce::MouseEvent& event)
{
    auto pos = event.position;
    currentDragTarget = hitTestHandle(pos);
    
    if (currentDragTarget != DragTarget::None)
    {
        dragStartY = pos.y;
        
        switch (currentDragTarget)
        {
            case DragTarget::TargetHandle:
                dragStartValue = targetLevelDb.load();
                break;
            case DragTarget::BoostRangeHandle:
                dragStartValue = boostRangeDb.load();
                break;
            case DragTarget::CutRangeHandle:
                dragStartValue = cutRangeDb.load();
                break;
            case DragTarget::None:
                break;
        }
    }
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& event)
{
    if (currentDragTarget == DragTarget::None)
        return;
    
    float deltaY = event.position.y - dragStartY;
    float dbPerPixel = 54.0f / waveformArea.getHeight();  // -48 to +6
    float deltaDb = -deltaY * dbPerPixel;
    
    switch (currentDragTarget)
    {
        case DragTarget::TargetHandle:
        {
            float newTarget = juce::jlimit(-40.0f, 0.0f, dragStartValue + deltaDb);
            targetLevelDb.store(newTarget);
            if (onTargetChanged) onTargetChanged(newTarget);
            break;
        }
        
        case DragTarget::BoostRangeHandle:
        {
            // Locked range: both boost and cut move together symmetrically
            float rangePerPixel = 12.0f / (waveformArea.getHeight() * 0.25f);
            float deltaRange = -deltaY * rangePerPixel;
            float newRange = juce::jlimit(1.0f, 12.0f, dragStartValue + deltaRange);
            boostRangeDb.store(newRange);
            cutRangeDb.store(newRange);  // Keep them equal
            if (onBoostRangeChanged) onBoostRangeChanged(newRange);
            if (onCutRangeChanged) onCutRangeChanged(newRange);
            if (onRangeChanged) onRangeChanged(newRange);
            break;
        }
        
        case DragTarget::CutRangeHandle:
        {
            // Locked range: both boost and cut move together symmetrically
            float rangePerPixel = 12.0f / (waveformArea.getHeight() * 0.25f);
            float deltaRange = deltaY * rangePerPixel;
            float newRange = juce::jlimit(1.0f, 12.0f, dragStartValue + deltaRange);
            boostRangeDb.store(newRange);
            cutRangeDb.store(newRange);  // Keep them equal
            if (onBoostRangeChanged) onBoostRangeChanged(newRange);
            if (onCutRangeChanged) onCutRangeChanged(newRange);
            if (onRangeChanged) onRangeChanged(newRange);
            break;
        }
        
        case DragTarget::None:
            break;
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
        
        switch (hoverTarget)
        {
            case DragTarget::TargetHandle:
            case DragTarget::BoostRangeHandle:
            case DragTarget::CutRangeHandle:
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
                break;
            case DragTarget::None:
                setMouseCursor(juce::MouseCursor::NormalCursor);
                break;
        }
        
        repaint();
    }
}

//==============================================================================
// Data

void WaveformDisplay::setTargetLevel(float target)
{
    targetLevelDb.store(target);
}

void WaveformDisplay::setBoostRange(float db)
{
    // Locked range: keep boost and cut equal
    float clampedDb = juce::jlimit(1.0f, 12.0f, db);
    boostRangeDb.store(clampedDb);
    cutRangeDb.store(clampedDb);
}

void WaveformDisplay::setCutRange(float db)
{
    // Locked range: keep boost and cut equal
    float clampedDb = juce::jlimit(1.0f, 12.0f, db);
    boostRangeDb.store(clampedDb);
    cutRangeDb.store(clampedDb);
}

void WaveformDisplay::setRange(float rangeDb)
{
    boostRangeDb.store(rangeDb);
    cutRangeDb.store(rangeDb);
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
    juce::SpinLock::ScopedLockType lock(bufferLock);
    std::fill(displayBuffer.begin(), displayBuffer.end(), SampleData{});
    writeIndex.store(0);
    sampleCounter = 0;
    currentInputMax = 0.0f;
    currentOutputMax = 0.0f;
    currentGainSum = 0.0f;
    gainSampleCount = 0;
    gainCurveOpacity = 0.0f;
    hasActiveAudio = false;
    isClipping = false;
    resetStats();
}

void WaveformDisplay::pushSamples(const float* inputSamples, const float* outputSamples,
                                   const float* gainValues, int numSamples)
{
    bool hadClip = false;
    
    for (int i = 0; i < numSamples; ++i)
    {
        float inAbs = std::abs(inputSamples[i]);
        float outAbs = (outputSamples != nullptr) ? std::abs(outputSamples[i]) : inAbs;
        
        currentInputMax = juce::jmax(currentInputMax, inAbs);
        currentOutputMax = juce::jmax(currentOutputMax, outAbs);
        currentGainSum += gainValues[i];
        gainSampleCount++;

        // Clipping detection
        if (outAbs > 0.99f) hadClip = true;

        float gain = gainValues[i];
        gainAccumulator += gain;
        gainMinTrack = juce::jmin(gainMinTrack, gain);
        gainMaxTrack = juce::jmax(gainMaxTrack, gain);
        statsSampleCount++;

        // Check for active audio (for fading)
        if (inAbs > 0.001f)
        {
            hasActiveAudio = true;
            silenceSampleCount = 0;
        }
        else
        {
            silenceSampleCount++;
            if (silenceSampleCount > 44100)  // ~1 second of silence
                hasActiveAudio = false;
        }

        sampleCounter++;

        if (sampleCounter >= static_cast<int>(samplesPerPixel))
        {
            juce::SpinLock::ScopedLockType lock(bufferLock);
            
            int idx = writeIndex.load();
            if (idx < static_cast<int>(displayBuffer.size()))
            {
                displayBuffer[static_cast<size_t>(idx)].inputPeak = currentInputMax;
                displayBuffer[static_cast<size_t>(idx)].outputPeak = currentOutputMax;
                displayBuffer[static_cast<size_t>(idx)].gainDb = 
                    (gainSampleCount > 0) ? (currentGainSum / static_cast<float>(gainSampleCount)) : 0.0f;
            }
            
            idx = (idx + 1) % static_cast<int>(displayBuffer.size());
            writeIndex.store(idx);

            sampleCounter = 0;
            currentInputMax = 0.0f;
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
        if (clippingSampleCount > 22050)  // ~0.5 second
            isClipping = false;
    }

    if (statsSampleCount > 0)
    {
        avgGainDb.store(gainAccumulator / static_cast<float>(statsSampleCount));
        minGainDb.store(gainMinTrack);
        maxGainDb.store(gainMaxTrack);
    }
}

//==============================================================================
// Drawing

void WaveformDisplay::paint(juce::Graphics& g)
{
    drawBackground(g);
    
    if (displayBuffer.empty() || displayWidth <= 0)
        return;
    
    drawGridLines(g);
    drawTargetAndRangeLines(g);
    drawGhostWaveform(g);
    drawWaveform(g);
    drawGainCurve(g);
    // drawHandles removed - no left-side handles
    drawIOMeters(g);
    drawClippingIndicator(g);
}

void WaveformDisplay::drawBackground(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Background with subtle gradient
    juce::ColourGradient bgGradient(
        CustomLookAndFeel::getBackgroundColour().brighter(0.02f),
        bounds.getX(), bounds.getY(),
        CustomLookAndFeel::getBackgroundColour().darker(0.03f),
        bounds.getX(), bounds.getBottom(),
        false
    );
    g.setGradientFill(bgGradient);
    g.fillRect(bounds);
    
    // Grain texture removed for cleaner look
    
    // Waveform area with gradient (darker inset)
    juce::ColourGradient areaGradient(
        CustomLookAndFeel::getSurfaceDarkColour().withAlpha(0.5f),
        waveformArea.getX(), waveformArea.getY(),
        CustomLookAndFeel::getSurfaceDarkColour().withAlpha(0.7f),
        waveformArea.getX(), waveformArea.getBottom(),
        false
    );
    g.setGradientFill(areaGradient);
    g.fillRect(waveformArea);
}

void WaveformDisplay::drawGridLines(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Pro-C 3 style: horizontal lines at every 3dB from 0 to -36
    // These lines extend across the ENTIRE display (including meter area)
    float dbLevels[] = { 0.0f, -3.0f, -6.0f, -9.0f, -12.0f, -15.0f, -18.0f, -21.0f, 
                         -24.0f, -27.0f, -30.0f, -33.0f, -36.0f };
    
    for (float db : dbLevels)
    {
        float y = dbToY(db);
        if (y > waveformArea.getY() && y < waveformArea.getBottom())
        {
            // Major lines at 0, -12, -24, -36 are brighter
            bool isMajor = (static_cast<int>(db) % 12 == 0);
            g.setColour(CustomLookAndFeel::getBorderColour().withAlpha(isMajor ? 0.2f : 0.08f));
            // Extend line across the entire width (through meter area)
            g.drawHorizontalLine(static_cast<int>(y), waveformArea.getX(), bounds.getRight() - 4.0f);
        }
    }
    
    // Vertical grid lines for time reference (very faint)
    g.setColour(CustomLookAndFeel::getBorderColour().withAlpha(0.04f));
    int numVerticalLines = 8;
    float xStep = waveformArea.getWidth() / static_cast<float>(numVerticalLines);
    for (int i = 1; i < numVerticalLines; ++i)
    {
        float x = waveformArea.getX() + i * xStep;
        g.drawVerticalLine(static_cast<int>(x), waveformArea.getY(), waveformArea.getBottom());
    }
}

void WaveformDisplay::drawTargetAndRangeLines(juce::Graphics& g)
{
    float target = targetLevelDb.load();
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    float targetY = dbToY(target);
    float rangeHeight = waveformArea.getHeight() * 0.25f;
    
    float boostY = targetY - rangeHeight * (boost / 12.0f);
    float cutY = targetY + rangeHeight * (cut / 12.0f);
    
    // Range zone fill
    g.setColour(CustomLookAndFeel::getRangeLineColour().withAlpha(0.06f));
    g.fillRect(waveformArea.getX(), boostY, waveformArea.getWidth(), cutY - boostY);
    
    // Boost range line (dashed, green tint)
    g.setColour(CustomLookAndFeel::getGainBoostColour().withAlpha(0.5f));
    for (float x = waveformArea.getX(); x < waveformArea.getRight(); x += 10.0f)
    {
        g.drawLine(x, boostY, juce::jmin(x + 6.0f, waveformArea.getRight()), boostY, 1.0f);
    }
    
    // Cut range line (dashed, red tint)
    g.setColour(CustomLookAndFeel::getGainCutColour().withAlpha(0.5f));
    for (float x = waveformArea.getX(); x < waveformArea.getRight(); x += 10.0f)
    {
        g.drawLine(x, cutY, juce::jmin(x + 6.0f, waveformArea.getRight()), cutY, 1.0f);
    }
    
    // Target line (solid, gold)
    g.setColour(CustomLookAndFeel::getTargetLineColour());
    g.drawLine(waveformArea.getX(), targetY, waveformArea.getRight(), targetY, 2.0f);
    
    // Glow
    g.setColour(CustomLookAndFeel::getTargetLineColour().withAlpha(0.15f));
    g.drawLine(waveformArea.getX(), targetY, waveformArea.getRight(), targetY, 6.0f);
}

void WaveformDisplay::drawHandles(juce::Graphics& g)
{
    // Handles are no longer drawn - target and range are now draggable by clicking
    // directly on their lines in the waveform area
    juce::ignoreUnused(g);
}

void WaveformDisplay::drawGhostWaveform(juce::Graphics& g)
{
    // Output waveform - shows the processed signal with color-coded boost/cut
    // When boosted (output > input): draw in GREEN above input
    // When cut (output < input): draw in RED below input (fill gap between)
    
    juce::SpinLock::ScopedLockType lock(bufferLock);
    
    int currentWrite = writeIndex.load();
    int bufferSize = static_cast<int>(displayBuffer.size());
    
    if (!hasActiveAudio) return;  // Don't show output waveform when no audio
    
    // Colors for boost/cut visualization
    juce::Colour boostColour = CustomLookAndFeel::getGainBoostColour();  // Green
    juce::Colour cutColour = CustomLookAndFeel::getGainCutColour();      // Red
    
    // Draw vertical slices showing the difference between input and output
    for (int i = 0; i < bufferSize; ++i)
    {
        int bufIdx = (currentWrite + i) % bufferSize;
        const auto& sample = displayBuffer[static_cast<size_t>(bufIdx)];
        
        float x = waveformArea.getX() + (static_cast<float>(i) / static_cast<float>(bufferSize)) * waveformArea.getWidth();
        float nextX = waveformArea.getX() + (static_cast<float>(i + 1) / static_cast<float>(bufferSize)) * waveformArea.getWidth();
        float sliceWidth = nextX - x + 0.5f;
        
        float inputY = linearToLogY(sample.inputPeak);
        float outputY = linearToLogY(sample.outputPeak);
        
        float gainDb = sample.gainDb;
        
        if (gainDb > 0.3f && sample.outputPeak > 0.001f)  // Boosting
        {
            // Green area above input waveform (output is higher/louder)
            float topY = juce::jmin(inputY, outputY);
            float bottomY = juce::jmax(inputY, outputY);
            float height = bottomY - topY;
            
            if (height > 0.5f)
            {
                float intensity = juce::jmin(gainDb / 8.0f, 1.0f);
                g.setColour(boostColour.withAlpha(0.25f + intensity * 0.2f));
                g.fillRect(x, topY, sliceWidth, height);
            }
        }
        else if (gainDb < -0.3f && sample.inputPeak > 0.001f)  // Cutting
        {
            // Red area showing the "missing" audio (what was cut)
            float topY = juce::jmin(inputY, outputY);
            float bottomY = juce::jmax(inputY, outputY);
            float height = bottomY - topY;
            
            if (height > 0.5f)
            {
                float intensity = juce::jmin(-gainDb / 8.0f, 1.0f);
                g.setColour(cutColour.withAlpha(0.2f + intensity * 0.15f));
                g.fillRect(x, topY, sliceWidth, height);
            }
        }
    }
    
    // Draw the output waveform outline
    juce::Path outputPath;
    bool pathStarted = false;
    
    for (int i = 0; i < bufferSize; ++i)
    {
        int bufIdx = (currentWrite + i) % bufferSize;
        const auto& sample = displayBuffer[static_cast<size_t>(bufIdx)];
        
        float x = waveformArea.getX() + (static_cast<float>(i) / static_cast<float>(bufferSize)) * waveformArea.getWidth();
        float y = linearToLogY(sample.outputPeak);
        
        if (!pathStarted)
        {
            outputPath.startNewSubPath(x, y);
            pathStarted = true;
        }
        else
        {
            outputPath.lineTo(x, y);
        }
    }
    
    // Draw output waveform line with color based on current gain
    // Use a subtle white/grey line for the output
    g.setColour(juce::Colour(0xFFFFFFFF).withAlpha(0.35f));
    g.strokePath(outputPath, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
}

void WaveformDisplay::drawWaveform(juce::Graphics& g)
{
    // Main waveform shows INPUT (original)
    
    juce::SpinLock::ScopedLockType lock(bufferLock);
    
    int currentWrite = writeIndex.load();
    int bufferSize = static_cast<int>(displayBuffer.size());
    
    // Color depends on audio activity
    // When no audio: very dark grey (almost black)
    // When audio playing: light grey for waveform
    juce::Colour waveformGrey = hasActiveAudio 
        ? juce::Colour(0xFFB8BCC5)   // Light grey when active
        : juce::Colour(0xFF252830);  // Very dark when no audio
    
    juce::Path waveformPath;
    bool pathStarted = false;
    float baseY = waveformArea.getBottom();
    
    for (int i = 0; i < bufferSize; ++i)
    {
        int bufIdx = (currentWrite + i) % bufferSize;
        const auto& sample = displayBuffer[static_cast<size_t>(bufIdx)];
        
        float x = waveformArea.getX() + (static_cast<float>(i) / static_cast<float>(bufferSize)) * waveformArea.getWidth();
        float y = linearToLogY(sample.inputPeak);
        
        if (!pathStarted)
        {
            waveformPath.startNewSubPath(x, baseY);
            pathStarted = true;
        }
        waveformPath.lineTo(x, y);
    }
    
    waveformPath.lineTo(waveformArea.getRight(), baseY);
    waveformPath.closeSubPath();
    
    // Draw gain indication background (green for boost, red for cut)
    // Very subtle tints - only show when audio is active
    if (hasActiveAudio)
    {
        for (int i = 0; i < bufferSize; ++i)
        {
            int bufIdx = (currentWrite + i) % bufferSize;
            const auto& sample = displayBuffer[static_cast<size_t>(bufIdx)];
            
            float x = waveformArea.getX() + (static_cast<float>(i) / static_cast<float>(bufferSize)) * waveformArea.getWidth();
            float nextX = waveformArea.getX() + (static_cast<float>(i + 1) / static_cast<float>(bufferSize)) * waveformArea.getWidth();
            float sliceWidth = nextX - x;
            
            // Gain in dB - positive is boost, negative is cut
            float gainDb = sample.gainDb;
            
            if (gainDb > 0.5f)  // Boosting
            {
                float intensity = juce::jmin(gainDb / 12.0f, 1.0f);  // Normalize to 0-1
                // Very subtle - max alpha of 0.08
                g.setColour(CustomLookAndFeel::getGainBoostColour().withAlpha(intensity * 0.08f));
                g.fillRect(x, waveformArea.getY(), sliceWidth + 1.0f, waveformArea.getHeight());
            }
            else if (gainDb < -0.5f)  // Cutting
            {
                float intensity = juce::jmin(-gainDb / 12.0f, 1.0f);  // Normalize to 0-1
                // Very subtle - max alpha of 0.08
                g.setColour(CustomLookAndFeel::getGainCutColour().withAlpha(intensity * 0.08f));
                g.fillRect(x, waveformArea.getY(), sliceWidth + 1.0f, waveformArea.getHeight());
            }
        }
    }
    
    // Waveform fill - opacity depends on activity
    float waveformAlpha = hasActiveAudio ? 0.7f : 0.15f;
    juce::ColourGradient waveGradient(
        waveformGrey.withAlpha(waveformAlpha),
        waveformArea.getX(), waveformArea.getY() + waveformArea.getHeight() * 0.2f,
        waveformGrey.withAlpha(waveformAlpha * 0.6f),
        waveformArea.getX(), waveformArea.getY() + waveformArea.getHeight() * 0.5f,
        false
    );
    waveGradient.addColour(0.7f, waveformGrey.withAlpha(waveformAlpha * 0.4f));
    waveGradient.addColour(1.0f, waveformGrey.withAlpha(waveformAlpha * 0.15f));
    g.setGradientFill(waveGradient);
    g.fillPath(waveformPath);
    
    // Outline with gradient
    juce::Path outlinePath;
    outlinePath.startNewSubPath(waveformArea.getX(), baseY);
    for (int i = 0; i < bufferSize; ++i)
    {
        int bufIdx = (currentWrite + i) % bufferSize;
        const auto& sample = displayBuffer[static_cast<size_t>(bufIdx)];
        
        float x = waveformArea.getX() + (static_cast<float>(i) / static_cast<float>(bufferSize)) * waveformArea.getWidth();
        float y = linearToLogY(sample.inputPeak);
        outlinePath.lineTo(x, y);
    }
    
    // Outline with subtle glow - only prominent when active
    float outlineAlpha = hasActiveAudio ? 0.25f : 0.08f;
    g.setColour(waveformGrey.withAlpha(outlineAlpha));
    g.strokePath(outlinePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    
    // Main outline
    g.setColour(waveformGrey.withAlpha(hasActiveAudio ? 0.6f : 0.1f));
    g.strokePath(outlinePath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
}

void WaveformDisplay::drawGainCurve(juce::Graphics& g)
{
    // Fade in/out based on audio activity
    float targetOpacity = hasActiveAudio ? 1.0f : 0.0f;
    gainCurveOpacity += (targetOpacity - gainCurveOpacity) * 0.1f;
    
    if (gainCurveOpacity < 0.01f)
        return;
    
    juce::SpinLock::ScopedLockType lock(bufferLock);
    
    int currentWrite = writeIndex.load();
    int bufferSize = static_cast<int>(displayBuffer.size());
    float target = targetLevelDb.load();
    float targetY = dbToY(target);
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    
    float alpha = gainCurveOpacity;
    
    // Colors for boost (electric blue) and cut (purple)
    juce::Colour boostColour(0xFF00D4FF);  // Electric blue/cyan
    juce::Colour cutColour(0xFFB48EFF);     // Purple
    
    // Draw the gain curve as individual line segments with color based on gain direction
    for (int i = 0; i < bufferSize - 1; ++i)
    {
        int bufIdx = (currentWrite + i) % bufferSize;
        int nextBufIdx = (currentWrite + i + 1) % bufferSize;
        const auto& sample = displayBuffer[static_cast<size_t>(bufIdx)];
        const auto& nextSample = displayBuffer[static_cast<size_t>(nextBufIdx)];
        
        // Clamp gain to range bounds
        float gainDb = juce::jlimit(-cut, boost, sample.gainDb);
        float nextGainDb = juce::jlimit(-cut, boost, nextSample.gainDb);
        
        float x1 = waveformArea.getX() + (static_cast<float>(i) / static_cast<float>(bufferSize)) * waveformArea.getWidth();
        float x2 = waveformArea.getX() + (static_cast<float>(i + 1) / static_cast<float>(bufferSize)) * waveformArea.getWidth();
        float y1 = gainDbToY(gainDb);
        float y2 = gainDbToY(nextGainDb);
        
        // Average gain of this segment determines color
        float avgGain = (gainDb + nextGainDb) / 2.0f;
        
        // Interpolate color based on gain: boost = electric blue, cut = purple
        // At 0 gain (target line), we blend between the two
        juce::Colour segmentColour;
        if (avgGain >= 0)
        {
            // Boosting: interpolate from middle color to electric blue
            float t = juce::jmin(avgGain / boost, 1.0f);
            segmentColour = cutColour.interpolatedWith(boostColour, 0.5f + t * 0.5f);
        }
        else
        {
            // Cutting: interpolate from middle color to purple
            float t = juce::jmin(-avgGain / cut, 1.0f);
            segmentColour = boostColour.interpolatedWith(cutColour, 0.5f + t * 0.5f);
        }
        
        // Draw glow
        g.setColour(segmentColour.withAlpha(0.12f * alpha));
        g.drawLine(x1, y1, x2, y2, 5.0f);
        
        // Draw main line segment
        g.setColour(segmentColour.withAlpha(alpha));
        g.drawLine(x1, y1, x2, y2, 2.5f);
    }
    
    juce::ignoreUnused(targetY);
}

void WaveformDisplay::drawIOMeters(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto meterArea = bounds.removeFromRight(static_cast<float>(ioMeterWidth));
    meterArea.reduce(2.0f, 8.0f);
    
    // Reserve space for dB readouts at top
    float readoutHeight = 16.0f;
    auto readoutArea = meterArea.removeFromTop(readoutHeight);
    meterArea.removeFromTop(4.0f);  // Gap
    
    // Reserve space for dB scale labels on LEFT (Pro-C 2 style)
    float scaleWidth = 16.0f;
    auto scaleArea = meterArea.removeFromLeft(scaleWidth);
    meterArea.removeFromLeft(2.0f);  // Gap
    
    float meterWidth = (meterArea.getWidth() - 3.0f) / 2.0f;
    
    auto inputMeterBounds = juce::Rectangle<float>(meterArea.getX(), meterArea.getY(), meterWidth, meterArea.getHeight());
    auto outputMeterBounds = juce::Rectangle<float>(meterArea.getX() + meterWidth + 3.0f, meterArea.getY(), meterWidth, meterArea.getHeight());
    
    // Backgrounds
    g.setColour(CustomLookAndFeel::getSurfaceDarkColour());
    g.fillRoundedRectangle(inputMeterBounds, 3.0f);
    g.fillRoundedRectangle(outputMeterBounds, 3.0f);
    
    // Input meter - RED color (range: -36dB to 0dB to match scale)
    float inputDb = inputLevelDb.load();
    float inputNorm = juce::jmap(inputDb, -36.0f, 0.0f, 0.0f, 1.0f);
    inputNorm = juce::jlimit(0.0f, 1.0f, inputNorm);
    
    auto inputFill = inputMeterBounds.reduced(1.5f);
    auto inputBar = inputFill.removeFromBottom(inputNorm * inputFill.getHeight());
    
    // Red color for input meter
    juce::Colour inputColour = (inputDb > -3.0f) 
        ? CustomLookAndFeel::getWarningColour()  // Bright red when hot
        : juce::Colour(0xFFE05555);              // Softer red normally
    g.setColour(inputColour);
    g.fillRoundedRectangle(inputBar, 2.0f);
    
    // Output meter - GREEN color (range: -36dB to 0dB to match scale)
    float outputDb = outputLevelDb.load();
    float outputNorm = juce::jmap(outputDb, -36.0f, 0.0f, 0.0f, 1.0f);
    outputNorm = juce::jlimit(0.0f, 1.0f, outputNorm);
    
    auto outputFill = outputMeterBounds.reduced(1.5f);
    auto outputBar = outputFill.removeFromBottom(outputNorm * outputFill.getHeight());
    
    // Green color for output meter
    juce::Colour outputColour = (outputDb > -3.0f) 
        ? CustomLookAndFeel::getWarningColour()  // Red when clipping
        : CustomLookAndFeel::getGainBoostColour();  // Green normally
    g.setColour(outputColour);
    g.fillRoundedRectangle(outputBar, 2.0f);
    
    // Draw dB scale labels on LEFT side (Pro-C 3 style) - every 6dB
    // Labels are positioned using dbToY to align with grid lines
    g.setFont(CustomLookAndFeel::getPluginFont(6.0f));
    g.setColour(CustomLookAndFeel::getVeryDimTextColour());
    
    float dbLabels[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -36.0f };
    for (float db : dbLabels)
    {
        // Use dbToY to ensure alignment with grid lines
        float y = dbToY(db);
        if (y > waveformArea.getY() + 4.0f && y < waveformArea.getBottom() - 4.0f)
        {
            g.drawText(juce::String(static_cast<int>(db)),
                       static_cast<int>(scaleArea.getX() - 2),
                       static_cast<int>(y - 4),
                       static_cast<int>(scaleWidth),
                       8,
                       juce::Justification::centredRight);
        }
    }
    
    // Update peak hold values
    if (inputDb > inputPeakHoldDb)
    {
        inputPeakHoldDb = inputDb;
        inputPeakHoldCounter = peakHoldFrames;
    }
    else if (inputPeakHoldCounter > 0)
    {
        inputPeakHoldCounter--;
    }
    else
    {
        inputPeakHoldDb -= 0.5f;  // Decay
        if (inputPeakHoldDb < -60.0f) inputPeakHoldDb = -100.0f;
    }
    
    if (outputDb > outputPeakHoldDb)
    {
        outputPeakHoldDb = outputDb;
        outputPeakHoldCounter = peakHoldFrames;
    }
    else if (outputPeakHoldCounter > 0)
    {
        outputPeakHoldCounter--;
    }
    else
    {
        outputPeakHoldDb -= 0.5f;  // Decay
        if (outputPeakHoldDb < -60.0f) outputPeakHoldDb = -100.0f;
    }
    
    // Helper to format dB value to max 3-4 chars (e.g., "-18", "-5.5", "-0.5")
    auto formatDbShort = [](float db) -> juce::String {
        if (db < -60.0f) return "-∞";
        if (db >= -9.9f && db <= 0.0f)
            return juce::String(db, 1);  // e.g., "-5.5"
        else
            return juce::String(static_cast<int>(std::round(db)));  // e.g., "-18"
    };
    
    // Draw peak dB readouts at top (using Space Grotesk font)
    g.setFont(CustomLookAndFeel::getPluginFont(10.0f, true));
    
    // Input peak dB readout (red text)
    juce::String inputText = formatDbShort(inputPeakHoldDb);
    g.setColour(juce::Colour(0xFFE05555).withAlpha(0.95f));
    g.drawText(inputText, 
               static_cast<int>(readoutArea.getX()), 
               static_cast<int>(readoutArea.getY()),
               static_cast<int>(meterWidth), 
               static_cast<int>(readoutHeight),
               juce::Justification::centred);
    
    // Output peak dB readout (green text)
    juce::String outputText = formatDbShort(outputPeakHoldDb);
    g.setColour(CustomLookAndFeel::getGainBoostColour().withAlpha(0.95f));
    g.drawText(outputText, 
               static_cast<int>(readoutArea.getX() + meterWidth + 3.0f), 
               static_cast<int>(readoutArea.getY()),
               static_cast<int>(meterWidth), 
               static_cast<int>(readoutHeight),
               juce::Justification::centred);
}

void WaveformDisplay::drawClippingIndicator(juce::Graphics& g)
{
    if (!isClipping) return;
    
    // Red bar at top when clipping
    auto clipBar = waveformArea.withHeight(4.0f);
    
    g.setColour(CustomLookAndFeel::getWarningColour());
    g.fillRect(clipBar);
    
    // Glow effect
    g.setColour(CustomLookAndFeel::getWarningColour().withAlpha(0.3f));
    g.fillRect(clipBar.withHeight(8.0f));
}

void WaveformDisplay::timerCallback()
{
    repaint();
}
