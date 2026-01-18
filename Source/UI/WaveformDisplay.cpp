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
    
    handleArea = bounds.removeFromLeft(static_cast<float>(handleAreaWidth));
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
    
    float rangeHeight = waveformArea.getHeight() * 0.25f;
    
    if (gainDb >= 0)
    {
        float normalizedGain = juce::jlimit(0.0f, 1.0f, gainDb / boost);
        return targetY - normalizedGain * rangeHeight;
    }
    else
    {
        float normalizedGain = juce::jlimit(0.0f, 1.0f, -gainDb / cut);
        return targetY + normalizedGain * rangeHeight;
    }
}

//==============================================================================
// Hit testing

WaveformDisplay::DragTarget WaveformDisplay::hitTestHandle(const juce::Point<float>& pos) const
{
    float target = targetLevelDb.load();
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    float targetY = dbToY(target);
    float rangeHeight = waveformArea.getHeight() * 0.25f;
    
    // Target handle
    if (pos.x < handleAreaWidth + 20.0f && std::abs(pos.y - targetY) < handleHitDistance)
        return DragTarget::TargetHandle;
    
    // Boost (upper) range handle
    float boostY = targetY - rangeHeight * (boost / 12.0f);
    if (pos.x < handleAreaWidth + 20.0f && std::abs(pos.y - boostY) < handleHitDistance)
        return DragTarget::BoostRangeHandle;
    
    // Cut (lower) range handle
    float cutY = targetY + rangeHeight * (cut / 12.0f);
    if (pos.x < handleAreaWidth + 20.0f && std::abs(pos.y - cutY) < handleHitDistance)
        return DragTarget::CutRangeHandle;
    
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
    drawHandles(g);
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
    
    CustomLookAndFeel::drawGrainTexture(g, getLocalBounds(), 0.015f);
    
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
    g.setColour(CustomLookAndFeel::getBorderColour().withAlpha(0.25f));
    
    // Horizontal lines at key dB levels
    float dbLevels[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f };
    for (float db : dbLevels)
    {
        float y = dbToY(db);
        if (y > waveformArea.getY() && y < waveformArea.getBottom())
        {
            g.drawHorizontalLine(static_cast<int>(y), waveformArea.getX(), waveformArea.getRight());
            
            // dB labels
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.setColour(CustomLookAndFeel::getVeryDimTextColour());
            g.drawText(juce::String(static_cast<int>(db)), 
                       static_cast<int>(handleArea.getRight() - 25), static_cast<int>(y - 6), 
                       20, 12, juce::Justification::centredRight);
            g.setColour(CustomLookAndFeel::getBorderColour().withAlpha(0.25f));
        }
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
    float target = targetLevelDb.load();
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    float targetY = dbToY(target);
    float rangeHeight = waveformArea.getHeight() * 0.25f;
    
    float handleX = handleArea.getRight() - 8.0f;
    float handleWidth = 55.0f;
    float handleHeight = 18.0f;
    
    // Helper lambda to draw a handle
    auto drawHandle = [&](float y, const juce::String& text, juce::Colour colour, bool isHovered) {
        auto bounds = juce::Rectangle<float>(handleX - handleWidth, y - handleHeight / 2.0f, handleWidth, handleHeight);
        
        g.setColour(CustomLookAndFeel::getSurfaceColour());
        g.fillRoundedRectangle(bounds, 4.0f);
        
        if (isHovered)
        {
            g.setColour(colour.withAlpha(0.3f));
            g.fillRoundedRectangle(bounds.expanded(2.0f), 5.0f);
        }
        
        g.setColour(colour);
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.drawText(text, bounds, juce::Justification::centred);
        
        // Connecting line
        g.setColour(colour.withAlpha(0.4f));
        g.drawLine(handleX, y, waveformArea.getX(), y, 1.0f);
    };
    
    // Target handle
    drawHandle(targetY, juce::String(target, 1) + " dB", 
               CustomLookAndFeel::getTargetLineColour(),
               hoverTarget == DragTarget::TargetHandle);
    
    // Boost handle
    float boostY = targetY - rangeHeight * (boost / 12.0f);
    drawHandle(boostY, "+" + juce::String(boost, 1),
               CustomLookAndFeel::getGainBoostColour(),
               hoverTarget == DragTarget::BoostRangeHandle);
    
    // Cut handle
    float cutY = targetY + rangeHeight * (cut / 12.0f);
    drawHandle(cutY, "-" + juce::String(cut, 1),
               CustomLookAndFeel::getGainCutColour(),
               hoverTarget == DragTarget::CutRangeHandle);
}

void WaveformDisplay::drawGhostWaveform(juce::Graphics& g)
{
    // Ghost waveform shows the OUTPUT (after gain)
    // Only visible when there's a difference from input
    
    juce::SpinLock::ScopedLockType lock(bufferLock);
    
    int currentWrite = writeIndex.load();
    int bufferSize = static_cast<int>(displayBuffer.size());
    
    juce::Path ghostPath;
    bool pathStarted = false;
    float baseY = waveformArea.getBottom();
    
    for (int i = 0; i < bufferSize; ++i)
    {
        int bufIdx = (currentWrite + i) % bufferSize;
        const auto& sample = displayBuffer[static_cast<size_t>(bufIdx)];
        
        float x = waveformArea.getX() + (static_cast<float>(i) / static_cast<float>(bufferSize)) * waveformArea.getWidth();
        float y = linearToLogY(sample.outputPeak);
        
        if (!pathStarted)
        {
            ghostPath.startNewSubPath(x, baseY);
            pathStarted = true;
        }
        ghostPath.lineTo(x, y);
    }
    
    ghostPath.lineTo(waveformArea.getRight(), baseY);
    ghostPath.closeSubPath();
    
    // Modern gradient fill for ghost waveform
    juce::ColourGradient ghostGradient(
        CustomLookAndFeel::getWaveformColour().withAlpha(0.35f),
        waveformArea.getX(), waveformArea.getY() + waveformArea.getHeight() * 0.3f,
        CustomLookAndFeel::getWaveformColour().withAlpha(0.15f),
        waveformArea.getX(), waveformArea.getBottom(),
        false
    );
    g.setGradientFill(ghostGradient);
    g.fillPath(ghostPath);
}

void WaveformDisplay::drawWaveform(juce::Graphics& g)
{
    // Main waveform shows INPUT (original)
    
    juce::SpinLock::ScopedLockType lock(bufferLock);
    
    int currentWrite = writeIndex.load();
    int bufferSize = static_cast<int>(displayBuffer.size());
    
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
    
    // Modern multi-stop gradient fill for waveform
    juce::ColourGradient waveGradient(
        CustomLookAndFeel::getWaveformColour().brighter(0.2f).withAlpha(0.85f),
        waveformArea.getX(), waveformArea.getY() + waveformArea.getHeight() * 0.2f,
        CustomLookAndFeel::getWaveformColour().withAlpha(0.6f),
        waveformArea.getX(), waveformArea.getY() + waveformArea.getHeight() * 0.5f,
        false
    );
    waveGradient.addColour(0.7f, CustomLookAndFeel::getWaveformColour().withAlpha(0.4f));
    waveGradient.addColour(1.0f, CustomLookAndFeel::getWaveformColour().withAlpha(0.2f));
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
    
    // Outline with subtle glow
    g.setColour(CustomLookAndFeel::getWaveformColour().withAlpha(0.3f));
    g.strokePath(outlinePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    
    // Main outline
    g.setColour(CustomLookAndFeel::getWaveformColour().brighter(0.1f));
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
    
    juce::Path gainPath;
    bool pathStarted = false;
    
    for (int i = 0; i < bufferSize; ++i)
    {
        int bufIdx = (currentWrite + i) % bufferSize;
        const auto& sample = displayBuffer[static_cast<size_t>(bufIdx)];
        
        float x = waveformArea.getX() + (static_cast<float>(i) / static_cast<float>(bufferSize)) * waveformArea.getWidth();
        float y = gainDbToY(sample.gainDb);
        
        if (!pathStarted)
        {
            gainPath.startNewSubPath(x, y);
            pathStarted = true;
        }
        else
        {
            gainPath.lineTo(x, y);
        }
    }
    
    if (pathStarted)
    {
        float alpha = gainCurveOpacity;
        
        // Glow
        g.setColour(CustomLookAndFeel::getGainCurveColour().withAlpha(0.15f * alpha));
        g.strokePath(gainPath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        
        // Main line
        g.setColour(CustomLookAndFeel::getGainCurveColour().withAlpha(alpha));
        g.strokePath(gainPath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
    }
    
    juce::ignoreUnused(targetY);
}

void WaveformDisplay::drawIOMeters(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto meterArea = bounds.removeFromRight(static_cast<float>(ioMeterWidth));
    meterArea.reduce(2.0f, 8.0f);
    
    float meterWidth = meterArea.getWidth() / 2.0f - 2.0f;
    
    auto inputMeterBounds = juce::Rectangle<float>(meterArea.getX(), meterArea.getY(), meterWidth, meterArea.getHeight());
    auto outputMeterBounds = juce::Rectangle<float>(meterArea.getX() + meterWidth + 4.0f, meterArea.getY(), meterWidth, meterArea.getHeight());
    
    // Backgrounds
    g.setColour(CustomLookAndFeel::getSurfaceDarkColour());
    g.fillRoundedRectangle(inputMeterBounds, 2.0f);
    g.fillRoundedRectangle(outputMeterBounds, 2.0f);
    
    // Input meter
    float inputDb = inputLevelDb.load();
    float inputNorm = juce::jmap(inputDb, -48.0f, 0.0f, 0.0f, 1.0f);
    inputNorm = juce::jlimit(0.0f, 1.0f, inputNorm);
    
    auto inputFill = inputMeterBounds.reduced(1.0f);
    auto inputBar = inputFill.removeFromBottom(inputNorm * inputFill.getHeight());
    g.setColour(CustomLookAndFeel::getWaveformColour());
    g.fillRoundedRectangle(inputBar, 1.0f);
    
    // Output meter
    float outputDb = outputLevelDb.load();
    float outputNorm = juce::jmap(outputDb, -48.0f, 0.0f, 0.0f, 1.0f);
    outputNorm = juce::jlimit(0.0f, 1.0f, outputNorm);
    
    auto outputFill = outputMeterBounds.reduced(1.0f);
    auto outputBar = outputFill.removeFromBottom(outputNorm * outputFill.getHeight());
    
    auto outputColour = (outputDb > -3.0f) ? CustomLookAndFeel::getWarningColour() : CustomLookAndFeel::getAccentColour();
    g.setColour(outputColour);
    g.fillRoundedRectangle(outputBar, 1.0f);
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
