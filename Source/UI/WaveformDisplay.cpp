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
    // For handle positions - linear dB mapping (0 to -60 dB range for more fidelity)
    float normalized = (db - (-60.0f)) / (6.0f - (-60.0f));
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    return waveformArea.getBottom() - normalized * waveformArea.getHeight();
}

float WaveformDisplay::yToDb(float y) const
{
    float normalized = (waveformArea.getBottom() - y) / waveformArea.getHeight();
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    return -60.0f + normalized * (6.0f - (-60.0f));
}

float WaveformDisplay::gainDbToY(float gainDb) const
{
    float target = targetLevelDb.load();
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    
    // CLAMP the gain to the range bounds so curve never goes outside
    float clampedGain = juce::jlimit(-cut, boost, gainDb);
    
    // Calculate the effective dB level after applying gain
    // The gain curve shows where the target+gain would be on the dB scale
    float effectiveDb = target + clampedGain;
    
    // Clamp to the display range
    effectiveDb = juce::jlimit(-60.0f, 6.0f, effectiveDb);
    
    return dbToY(effectiveDb);
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
    
    // Calculate positions using actual dB values (matching drawTargetAndRangeLines)
    float boostY = dbToY(juce::jmin(6.0f, target + boost));
    float cutY = dbToY(juce::jmax(-60.0f, target - cut));
    
    // Boost (upper) range handle - check first since it's on top
    if (std::abs(pos.y - boostY) < handleHitDistance)
        return DragTarget::BoostRangeHandle;
    
    // Cut (lower) range handle
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
        
        // Apply scroll speed (lower speed = more samples per pixel = slower scrolling)
        int effectiveSamplesPerPixel = static_cast<int>(samplesPerPixel / scrollSpeed);

        if (sampleCounter >= effectiveSamplesPerPixel)
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
    
    // Update current gain immediately using last gain value for responsive meter
    if (numSamples > 0)
    {
        currentGainDb.store(gainValues[numSamples - 1]);
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
    
    // Visual layering for gain changes:
    // 1. Boost fills (BACK - faded teal behind everything)
    drawGhostWaveform(g);  // This now only draws boost zones
    
    // 2. Input waveform (MIDDLE - the original signal)
    drawWaveform(g);
    
    // 3. Cut overlay (FRONT - darker output waveform when cutting)
    drawCutOverlay(g);
    
    drawGainCurve(g);
    drawIOMeters(g);
    drawClippingIndicator(g);
}

void WaveformDisplay::drawBackground(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Background with good contrast for waveform visibility
    g.setColour(juce::Colour(0xFF252830));  // Balanced - not too dark, not too light
    g.fillRect(bounds);
    
    // Subtle noise/grain texture for brushed metal feel
    juce::Random rng(42);  // Seed for consistent pattern
    int step = 3;
    for (int y = static_cast<int>(bounds.getY()); y < static_cast<int>(bounds.getBottom()); y += step)
    {
        for (int x = static_cast<int>(bounds.getX()); x < static_cast<int>(bounds.getRight()); x += step)
        {
            float noise = rng.nextFloat();
            if (noise > 0.7f)
            {
                float brightness = (noise - 0.7f) * 0.08f;  // Very subtle
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
    
    // Strong vignette effect - darker on edges
    // Left edge vignette
    float vignetteWidth = bounds.getWidth() * 0.2f;
    juce::ColourGradient leftVig(
        juce::Colour(0x70000000),
        bounds.getX(), bounds.getCentreY(),
        juce::Colours::transparentBlack,
        bounds.getX() + vignetteWidth, bounds.getCentreY(),
        false
    );
    g.setGradientFill(leftVig);
    g.fillRect(bounds.getX(), bounds.getY(), vignetteWidth, bounds.getHeight());
    
    // Right edge vignette
    juce::ColourGradient rightVig(
        juce::Colours::transparentBlack,
        bounds.getRight() - vignetteWidth, bounds.getCentreY(),
        juce::Colour(0x70000000),
        bounds.getRight(), bounds.getCentreY(),
        false
    );
    g.setGradientFill(rightVig);
    g.fillRect(bounds.getRight() - vignetteWidth, bounds.getY(), vignetteWidth, bounds.getHeight());
    
    // Top vignette
    float topVigHeight = bounds.getHeight() * 0.15f;
    juce::ColourGradient topVig(
        juce::Colour(0x50000000),
        bounds.getCentreX(), bounds.getY(),
        juce::Colours::transparentBlack,
        bounds.getCentreX(), bounds.getY() + topVigHeight,
        false
    );
    g.setGradientFill(topVig);
    g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), topVigHeight);
    
    // Bottom vignette  
    float bottomVigHeight = bounds.getHeight() * 0.15f;
    juce::ColourGradient bottomVig(
        juce::Colours::transparentBlack,
        bounds.getCentreX(), bounds.getBottom() - bottomVigHeight,
        juce::Colour(0x50000000),
        bounds.getCentreX(), bounds.getBottom(),
        false
    );
    g.setGradientFill(bottomVig);
    g.fillRect(bounds.getX(), bounds.getBottom() - bottomVigHeight, bounds.getWidth(), bottomVigHeight);
}

void WaveformDisplay::drawGridLines(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Horizontal lines at every 6dB from 0 to -60 for wider range (more fidelity for soft vocals)
    float dbLevels[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -36.0f, -42.0f, -48.0f, -54.0f, -60.0f };
    
    // Draw dB labels on right side - BIGGER and more legible
    g.setFont(CustomLookAndFeel::getPluginFont(10.0f, true));
    
    for (float db : dbLevels)
    {
        float y = dbToY(db);
        if (y > waveformArea.getY() + 6 && y < waveformArea.getBottom() - 6)
        {
            // Major lines at 0, -12, -24, -36, -48, -60 are brighter
            bool isMajor = (static_cast<int>(db) % 12 == 0);
            float alpha = isMajor ? 0.30f : 0.12f;
            g.setColour(juce::Colour(0xFF555566).withAlpha(alpha));
            g.drawHorizontalLine(static_cast<int>(y), 0.0f, bounds.getRight() - 30.0f);
            
            // dB label to the LEFT of meters
            float meterLeftEdge = bounds.getRight() - static_cast<float>(ioMeterWidth);
            g.setColour(CustomLookAndFeel::getDimTextColour().withAlpha(isMajor ? 0.9f : 0.6f));
            g.drawText(juce::String(static_cast<int>(db)),
                       static_cast<int>(meterLeftEdge - 28),
                       static_cast<int>(y - 7),
                       26, 14, juce::Justification::centredRight);
        }
    }
    
    // Minor 3dB lines (fainter) - extended to -57
    float minorDbLevels[] = { -3.0f, -9.0f, -15.0f, -21.0f, -27.0f, -33.0f, -39.0f, -45.0f, -51.0f, -57.0f };
    for (float db : minorDbLevels)
    {
        float y = dbToY(db);
        if (y > waveformArea.getY() && y < waveformArea.getBottom())
        {
            g.setColour(juce::Colour(0xFF555566).withAlpha(0.08f));
            g.drawHorizontalLine(static_cast<int>(y), waveformArea.getX(), bounds.getRight() - static_cast<float>(ioMeterWidth) - 30.0f);
        }
    }
    
    // Vertical grid lines for time reference
    g.setColour(juce::Colour(0xFF555566).withAlpha(0.06f));
    int numVerticalLines = 10;
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
    
    // Calculate boost/cut Y positions using actual dB values for correct proportions
    // e.g., if target=-18dB and boost=12dB, boostY is at -6dB on the scale
    float boostY = dbToY(juce::jmin(6.0f, target + boost));   // Clamp to max +6dB
    float cutY = dbToY(juce::jmax(-60.0f, target - cut));     // Clamp to min -60dB
    
    // Calculate fade zone on right side (starts fading at 80% of width)
    float fadeStartX = waveformArea.getRight() - 60.0f;
    auto getAlphaAtX = [&](float x) -> float {
        if (x < fadeStartX) return 1.0f;
        return 1.0f - ((x - fadeStartX) / 60.0f);
    };
    
    // Range zone fill with fade at right edge
    // Draw in slices to create gradient fade
    for (float x = waveformArea.getX(); x < waveformArea.getRight(); x += 4.0f)
    {
        float alpha = getAlphaAtX(x) * 0.06f;
        g.setColour(CustomLookAndFeel::getRangeLineColour().withAlpha(alpha));
        g.fillRect(x, boostY, 4.0f, cutY - boostY);
    }
    
    // Boost range line (dashed, green tint) with fade
    for (float x = waveformArea.getX(); x < waveformArea.getRight(); x += 10.0f)
    {
        float alpha = getAlphaAtX(x) * 0.5f;
        g.setColour(CustomLookAndFeel::getGainBoostColour().withAlpha(alpha));
        g.drawLine(x, boostY, juce::jmin(x + 6.0f, waveformArea.getRight()), boostY, 1.0f);
    }
    
    // Boost range label - BIGGER and more legible
    g.setFont(CustomLookAndFeel::getPluginFont(11.0f, true));
    g.setColour(CustomLookAndFeel::getGainBoostColour().withAlpha(0.9f));
    g.drawText("+" + juce::String(static_cast<int>(boost)) + " dB", 
               static_cast<int>(waveformArea.getX() + 8), 
               static_cast<int>(boostY - 16), 
               60, 14, juce::Justification::centredLeft);
    
    // Cut range line (dashed, muted gray to match boost line) with fade
    for (float x = waveformArea.getX(); x < waveformArea.getRight(); x += 10.0f)
    {
        float alpha = getAlphaAtX(x) * 0.5f;
        g.setColour(CustomLookAndFeel::getGainBoostColour().withAlpha(alpha));  // Same muted gray as boost
        g.drawLine(x, cutY, juce::jmin(x + 6.0f, waveformArea.getRight()), cutY, 1.0f);
    }
    
    // Cut range label - BIGGER and more legible
    g.setColour(CustomLookAndFeel::getGainBoostColour().withAlpha(0.9f));  // Same muted gray as boost
    g.drawText("-" + juce::String(static_cast<int>(cut)) + " dB", 
               static_cast<int>(waveformArea.getX() + 8), 
               static_cast<int>(cutY + 3), 
               60, 14, juce::Justification::centredLeft);
    
    // Target line with gradient matching knob outline (light purple to dark purple) - 2px
    juce::Colour targetLightPurple(0xFFD4B8FF);  // Light purple (matches knob gradient)
    juce::Colour targetDarkPurple(0xFF7B5CAD);    // Dark purple (matches knob gradient)
    float lineWidth = waveformArea.getWidth();
    for (float x = waveformArea.getX(); x < waveformArea.getRight(); x += 2.0f)
    {
        float alpha = getAlphaAtX(x);
        float gradientPos = (x - waveformArea.getX()) / lineWidth;
        juce::Colour gradientColour = targetLightPurple.interpolatedWith(targetDarkPurple, gradientPos);
        g.setColour(gradientColour.withAlpha(alpha));
        g.drawLine(x, targetY, juce::jmin(x + 2.0f, waveformArea.getRight()), targetY, 2.0f);
    }
    
    // Target label with gradient effect - use middle color
    g.setFont(CustomLookAndFeel::getPluginFont(11.0f, true));
    juce::Colour labelColour = targetLightPurple.interpolatedWith(targetDarkPurple, 0.3f);
    g.setColour(labelColour.withAlpha(0.95f));
    g.drawText(juce::String(static_cast<int>(target)) + " dB", 
               static_cast<int>(waveformArea.getX() + 8), 
               static_cast<int>(targetY - 16), 
               60, 14, juce::Justification::centredLeft);
}

void WaveformDisplay::drawHandles(juce::Graphics& g)
{
    // Handles are no longer drawn - target and range are now draggable by clicking
    // directly on their lines in the waveform area
    juce::ignoreUnused(g);
}

void WaveformDisplay::drawGhostWaveform(juce::Graphics& g)
{
    // BOOST fills only - drawn BEFORE input waveform so they appear BEHIND
    // The visual hierarchy is: boost (back) -> input (middle) -> cut (front)
    
    juce::SpinLock::ScopedLockType lock(bufferLock);
    
    int currentWrite = writeIndex.load();
    int bufferSize = static_cast<int>(displayBuffer.size());
    
    // Draw BOOST zones (teal) - these go BEHIND the input waveform
    for (int i = 0; i < bufferSize; ++i)
    {
        int bufIdx = (currentWrite + i) % bufferSize;
        const auto& sample = displayBuffer[static_cast<size_t>(bufIdx)];
        
        float x = waveformArea.getX() + (static_cast<float>(i) / static_cast<float>(bufferSize)) * waveformArea.getWidth();
        float nextX = waveformArea.getX() + (static_cast<float>(i + 1) / static_cast<float>(bufferSize)) * waveformArea.getWidth();
        float sliceWidth = juce::jmax(1.0f, nextX - x);
        
        float inputY = linearToLogY(sample.inputPeak);
        float outputY = linearToLogY(sample.outputPeak);
        
        float gainDb = sample.gainDb;
        
        if (gainDb > 0.3f && sample.outputPeak > 0.001f)  // Boosting
        {
            // Output is louder than input - fill the boost area with teal
            float topY = outputY;  // Output is higher (smaller Y value)
            float bottomY = inputY;  // Input is lower (larger Y value)
            float height = bottomY - topY;
            
            if (height > 0.5f)
            {
                float intensity = juce::jmin(gainDb / 10.0f, 1.0f);
                // Faded teal gradient for boost - subtle background fill
                juce::ColourGradient boostGrad(
                    juce::Colour(0xFF5BCEFA).withAlpha(0.20f + intensity * 0.12f), x, topY,
                    juce::Colour(0xFF3A8A9F).withAlpha(0.05f), x, bottomY, false
                );
                g.setGradientFill(boostGrad);
                g.fillRect(x, topY, sliceWidth, height);
            }
        }
    }
}

void WaveformDisplay::drawCutOverlay(juce::Graphics& g)
{
    // CUT visualization - drawn AFTER input waveform so it appears IN FRONT
    // Shows the output (reduced) waveform as a distinct overlay on top of the input
    
    juce::SpinLock::ScopedLockType lock(bufferLock);
    
    int currentWrite = writeIndex.load();
    int bufferSize = static_cast<int>(displayBuffer.size());
    
    // Draw the cut/output waveform as a FILLED area (darker, in front)
    // This shows where the output ends up when gain is reduced
    juce::Path cutOutputPath;
    bool pathStarted = false;
    float baseY = waveformArea.getBottom();
    bool hasCutSamples = false;
    float lastY = baseY;
    float lastX = 0.0f;
    
    float prevOutputPeak = 0.0f;
    
    for (int i = 0; i < bufferSize; ++i)
    {
        int bufIdx = (currentWrite + i) % bufferSize;
        const auto& sample = displayBuffer[static_cast<size_t>(bufIdx)];
        
        float gainDb = sample.gainDb;
        float x = waveformArea.getX() + (static_cast<float>(i) / static_cast<float>(bufferSize)) * waveformArea.getWidth();
        
        // Only draw output waveform for cut regions
        if (gainDb < -0.3f && sample.outputPeak > 0.0001f)
        {
            hasCutSamples = true;
            float y = linearToLogY(sample.outputPeak);
            
            // Check if coming back from silence - this can cause diagonal lines
            bool comingFromSilence = prevOutputPeak < 0.001f && pathStarted;
            
            if (!pathStarted)
            {
                cutOutputPath.startNewSubPath(x, baseY);
                cutOutputPath.lineTo(x, y);
                pathStarted = true;
            }
            else if (comingFromSilence)
            {
                // Close previous section and start new one to avoid diagonal line
                cutOutputPath.lineTo(lastX, baseY);
                cutOutputPath.closeSubPath();
                cutOutputPath.startNewSubPath(x, baseY);
                cutOutputPath.lineTo(x, y);
            }
            else
            {
                // Check for large vertical jumps - add vertical line first to avoid diagonal
                float yDiff = std::abs(y - lastY);
                if (yDiff > waveformArea.getHeight() * 0.3f)
                {
                    // Large jump - draw vertical line first
                    cutOutputPath.lineTo(x, lastY);
                    cutOutputPath.lineTo(x, y);
                }
                else
                {
                    cutOutputPath.lineTo(x, y);
                }
            }
            lastY = y;
            lastX = x;
            prevOutputPeak = sample.outputPeak;
        }
        else if (pathStarted)
        {
            // Close this cut section - go down to baseline first
            cutOutputPath.lineTo(lastX, baseY);
            cutOutputPath.closeSubPath();
            pathStarted = false;
            lastY = baseY;
            prevOutputPeak = 0.0f;
        }
        else
        {
            prevOutputPeak = sample.outputPeak;  // Track even when not drawing
        }
    }
    
    // Close if still open
    if (pathStarted)
    {
        cutOutputPath.lineTo(lastX, baseY);
        cutOutputPath.closeSubPath();
    }
    
    if (hasCutSamples)
    {
        // Draw the cut output waveform with warm coral-red tint for visibility
        // Shows clearly where gain reduction is happening
        juce::ColourGradient cutGrad(
            CustomLookAndFeel::getGainCutColour().withAlpha(0.55f),  // Warm coral at top
            waveformArea.getX(), waveformArea.getY() + waveformArea.getHeight() * 0.3f,
            CustomLookAndFeel::getGainCutColour().darker(0.4f).withAlpha(0.35f),  // Darker at bottom
            waveformArea.getX(), waveformArea.getBottom(),
            false
        );
        g.setGradientFill(cutGrad);
        g.fillPath(cutOutputPath);
        
        // Distinct outline on the cut output waveform
        g.setColour(CustomLookAndFeel::getGainCutColour().withAlpha(0.7f));
        
        // Create outline path (just the top edge) - handle discontinuities
        juce::Path outlinePath;
        bool outlineStarted = false;
        float lastOutlineY = 0.0f;
        float lastOutlinePeak = 0.0f;
        
        for (int i = 0; i < bufferSize; ++i)
        {
            int bufIdx = (currentWrite + i) % bufferSize;
            const auto& sample = displayBuffer[static_cast<size_t>(bufIdx)];
            
            if (sample.gainDb < -0.3f && sample.outputPeak > 0.0001f)
            {
                float x = waveformArea.getX() + (static_cast<float>(i) / static_cast<float>(bufferSize)) * waveformArea.getWidth();
                float y = linearToLogY(sample.outputPeak);
                
                // Check for discontinuity (large jump after silence)
                bool isDiscontinuity = outlineStarted && 
                    (lastOutlinePeak < 0.001f || std::abs(y - lastOutlineY) > waveformArea.getHeight() * 0.4f);
                
                if (!outlineStarted)
                {
                    outlinePath.startNewSubPath(x, y);
                    outlineStarted = true;
                }
                else if (isDiscontinuity)
                {
                    // Start a new subpath to avoid diagonal lines across discontinuities
                    outlinePath.startNewSubPath(x, y);
                }
                else
                {
                    outlinePath.lineTo(x, y);
                }
                lastOutlineY = y;
                lastOutlinePeak = sample.outputPeak;
            }
            else
            {
                // Mark that we went through silence
                lastOutlinePeak = 0.0f;
            }
        }
        
        g.strokePath(outlinePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
    }
}

void WaveformDisplay::drawWaveform(juce::Graphics& g)
{
    // Main waveform shows INPUT (original)
    
    juce::SpinLock::ScopedLockType lock(bufferLock);
    
    int currentWrite = writeIndex.load();
    int bufferSize = static_cast<int>(displayBuffer.size());
    
    // White/light grey waveform - always visible, scrolls continuously like Pro-C
    juce::Colour waveformGrey(0xFFB8BCC5);  // Light grey - consistent regardless of audio activity
    
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
    // Very subtle tints - always visible
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
    
    // Waveform fill - consistent opacity (scrolls continuously like Pro-C/FL Studio)
    float waveformAlpha = 0.7f;
    juce::ColourGradient waveGradient(
        waveformGrey.withAlpha(waveformAlpha),  // Full alpha at top
        waveformArea.getX(), waveformArea.getY(),  // Start at TOP
        waveformGrey.withAlpha(waveformAlpha * 0.08f),  // Very faded at bottom
        waveformArea.getX(), waveformArea.getBottom(),  // End at BOTTOM
        false
    );
    // Smooth gradient stops across entire range
    waveGradient.addColour(0.15f, waveformGrey.withAlpha(waveformAlpha * 0.85f));
    waveGradient.addColour(0.35f, waveformGrey.withAlpha(waveformAlpha * 0.6f));
    waveGradient.addColour(0.55f, waveformGrey.withAlpha(waveformAlpha * 0.35f));
    waveGradient.addColour(0.75f, waveformGrey.withAlpha(waveformAlpha * 0.18f));
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
    
    // Outline with subtle glow - consistent
    g.setColour(waveformGrey.withAlpha(0.25f));
    g.strokePath(outlinePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    
    // Main outline
    g.setColour(waveformGrey.withAlpha(0.6f));
    g.strokePath(outlinePath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
}

void WaveformDisplay::drawGainCurve(juce::Graphics& g)
{
    // Gain curve fades away when no audio is playing
    // Only visible when waveform is scrolling with audio
    float targetOpacity = hasActiveAudio ? 1.0f : 0.0f;
    gainCurveOpacity += (targetOpacity - gainCurveOpacity) * 0.08f;
    
    // Skip drawing if fully faded out
    if (gainCurveOpacity < 0.02f)
        return;
    
    juce::SpinLock::ScopedLockType lock(bufferLock);
    
    int currentWrite = writeIndex.load();
    int bufferSize = static_cast<int>(displayBuffer.size());
    float target = targetLevelDb.load();
    float targetY = dbToY(target);
    float boost = boostRangeDb.load();
    float cut = cutRangeDb.load();
    
    float alpha = gainCurveOpacity;
    
    // Colors for boost (electric cyan) and cut (brighter purple)
    juce::Colour boostColour(0xFF00D4FF);  // Bright electric cyan - key selling feature
    juce::Colour cutColour(0xFFB060FF);     // Bright vibrant purple - stands out
    
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
        
        // Enhanced outer glow - more energy
        g.setColour(segmentColour.withAlpha(0.06f * alpha));
        g.drawLine(x1, y1, x2, y2, 10.0f);  // Wider outer glow
        
        // Medium glow
        g.setColour(segmentColour.withAlpha(0.15f * alpha));
        g.drawLine(x1, y1, x2, y2, 5.0f);
        
        // Inner glow - brighter
        g.setColour(segmentColour.withAlpha(0.25f * alpha));
        g.drawLine(x1, y1, x2, y2, 3.0f);
        
        // Draw main line segment - bright core
        g.setColour(segmentColour.withAlpha(alpha));
        g.drawLine(x1, y1, x2, y2, 1.5f);
    }
    
    juce::ignoreUnused(targetY);
}

void WaveformDisplay::drawIOMeters(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto fullMeterArea = bounds.removeFromRight(static_cast<float>(ioMeterWidth));
    
    // Draw vignette/fade effect extending from meters into waveform area
    // This creates the illusion of meters extending the full height
    juce::ColourGradient meterVignette(
        juce::Colour(0x00000000), fullMeterArea.getX() - 40.0f, fullMeterArea.getCentreY(),
        juce::Colour(0x60000000), fullMeterArea.getX(), fullMeterArea.getCentreY(),
        false
    );
    g.setGradientFill(meterVignette);
    g.fillRect(fullMeterArea.getX() - 40.0f, fullMeterArea.getY(), 40.0f, fullMeterArea.getHeight());
    
    // Subtle dark background for meter area (extends full height)
    g.setColour(juce::Colour(0x40000000));
    g.fillRect(fullMeterArea);
    
    auto meterArea = fullMeterArea;
    meterArea.reduce(2.0f, 8.0f);
    
    // Reserve space for labels at top
    float labelHeight = 14.0f;
    auto labelArea = meterArea.removeFromTop(labelHeight);
    meterArea.removeFromTop(2.0f);
    
    // Reserve space for readouts below labels
    float readoutHeight = 14.0f;
    auto readoutArea = meterArea.removeFromTop(readoutHeight);
    meterArea.removeFromTop(2.0f);
    
    // Two narrow meters: Output level (left) and Gain change (right)
    float meterGap = 12.0f;  // More spacing between meters
    float singleMeterWidth = 10.0f;
    
    // Position meters on the right side with more gap
    float metersRight = meterArea.getRight() - 4.0f;
    auto gainMeterBounds = juce::Rectangle<float>(metersRight - singleMeterWidth, meterArea.getY(), 
                                                    singleMeterWidth, meterArea.getHeight());
    auto outputMeterBounds = juce::Rectangle<float>(metersRight - singleMeterWidth * 2 - meterGap, meterArea.getY(), 
                                                      singleMeterWidth, meterArea.getHeight());
    
    // Draw darker opaque backgrounds for meters
    g.setColour(juce::Colour(0xFF0D0F12));  // Very dark background
    g.fillRoundedRectangle(outputMeterBounds.expanded(3.0f), 4.0f);
    g.fillRoundedRectangle(gainMeterBounds.expanded(3.0f), 4.0f);
    
    // Subtle border on meter backgrounds
    g.setColour(juce::Colour(0xFF1E2128));
    g.drawRoundedRectangle(outputMeterBounds.expanded(3.0f), 4.0f, 1.0f);
    g.drawRoundedRectangle(gainMeterBounds.expanded(3.0f), 4.0f, 1.0f);
    
    // Draw labels "O" and "G" above meters
    g.setFont(CustomLookAndFeel::getPluginFont(10.0f, true));
    g.setColour(CustomLookAndFeel::getDimTextColour());
    g.drawText("O", outputMeterBounds.toNearestInt().withY(static_cast<int>(labelArea.getY())).withHeight(static_cast<int>(labelHeight)),
               juce::Justification::centred);
    g.drawText("G", gainMeterBounds.toNearestInt().withY(static_cast<int>(labelArea.getY())).withHeight(static_cast<int>(labelHeight)),
               juce::Justification::centred);
    
    float meterHeight = outputMeterBounds.getHeight();
    
    // ========== OUTPUT METER ==========
    float outputDb = outputLevelDb.load();
    float outputNorm = juce::jmap(outputDb, -48.0f, 0.0f, 0.0f, 1.0f);
    outputNorm = juce::jlimit(0.0f, 1.0f, outputNorm);
    
    // Update RMS average (slower decay)
    // Longer RMS averaging (slower decay for more stable display)
    if (outputDb > -60.0f)
    {
        // Exponential moving average with longer time constant
        float alpha = 0.02f;  // Slower response = longer averaging
        outputRmsDb = outputRmsDb * (1.0f - alpha) + outputDb * alpha;
    }
    else
    {
        // Slow decay when no signal
        outputRmsDb = outputRmsDb * 0.995f - 0.1f;
        if (outputRmsDb < -60.0f) outputRmsDb = -100.0f;
    }
    
    // Draw RMS average line (faint, behind main meter) - only when signal is active
    float rmsNorm = juce::jmap(outputRmsDb, -48.0f, 0.0f, 0.0f, 1.0f);
    rmsNorm = juce::jlimit(0.0f, 1.0f, rmsNorm);
    // Only show RMS if there's actual signal (outputDb > -55dB) and RMS is significant
    if (rmsNorm > 0.05f && outputDb > -55.0f)
    {
        float rmsBarHeight = rmsNorm * meterHeight;
        auto rmsBounds = juce::Rectangle<float>(
            outputMeterBounds.getX(), 
            outputMeterBounds.getBottom() - rmsBarHeight,
            singleMeterWidth, 
            rmsBarHeight
        );
        g.setColour(juce::Colour(0xFF4CAF50).withAlpha(0.15f));  // Subtle faint green
        g.fillRoundedRectangle(rmsBounds, 2.0f);
    }
    
    // Draw main output meter
    float barHeight = outputNorm * meterHeight;
    if (barHeight > 0.5f)
    {
        auto barBounds = juce::Rectangle<float>(
            outputMeterBounds.getX(), 
            outputMeterBounds.getBottom() - barHeight,
            singleMeterWidth, 
            barHeight
        );
        
        juce::ColourGradient meterGradient(
            juce::Colour(0xFF4CAF50), barBounds.getX(), outputMeterBounds.getBottom(),
            juce::Colour(0xFFFF5252), barBounds.getX(), outputMeterBounds.getY(),
            false
        );
        meterGradient.addColour(0.6, juce::Colour(0xFF8BC34A));
        meterGradient.addColour(0.8, juce::Colour(0xFFFFEB3B));
        meterGradient.addColour(0.9, juce::Colour(0xFFFF9800));
        
        g.setGradientFill(meterGradient);
        g.fillRoundedRectangle(barBounds, 2.0f);
    }
    
    // Peak hold logic (output)
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
        outputPeakHoldDb -= 0.25f;
        if (outputPeakHoldDb < -60.0f) outputPeakHoldDb = -100.0f;
    }
    
    // Peak hold line
    if (outputPeakHoldDb > -60.0f)
    {
        float peakNorm = juce::jmap(outputPeakHoldDb, -48.0f, 0.0f, 0.0f, 1.0f);
        peakNorm = juce::jlimit(0.0f, 1.0f, peakNorm);
        float peakY = outputMeterBounds.getBottom() - peakNorm * meterHeight;
        
        juce::Colour peakColour = (outputPeakHoldDb > -3.0f) ? juce::Colour(0xFFFF5252) :
                                   (outputPeakHoldDb > -12.0f) ? juce::Colour(0xFFFFEB3B) :
                                   juce::Colour(0xFF8BC34A);
        g.setColour(peakColour);
        g.fillRect(outputMeterBounds.getX(), peakY - 1.0f, singleMeterWidth, 2.0f);
    }
    
    // Clip indicator (stays longer)
    if (outputDb > -0.5f)
    {
        clipIndicatorActive = true;
        clipHoldCounter = clipHoldFrames;
    }
    else if (clipHoldCounter > 0)
    {
        clipHoldCounter--;
    }
    else
    {
        clipIndicatorActive = false;
    }
    
    if (clipIndicatorActive)
    {
        g.setColour(juce::Colour(0xFFFF5252));
        g.fillRect(outputMeterBounds.getX(), outputMeterBounds.getY(), singleMeterWidth, 4.0f);
    }
    
    // ========== GAIN CHANGE METER ==========
    float gainDb = currentGainDb.load();
    
    // Gain peak hold
    if (std::abs(gainDb) > std::abs(gainPeakHoldDb))
    {
        gainPeakHoldDb = gainDb;
        gainPeakHoldCounter = peakHoldFrames;
    }
    else if (gainPeakHoldCounter > 0)
    {
        gainPeakHoldCounter--;
    }
    else
    {
        gainPeakHoldDb *= 0.95f;  // Decay toward zero
        if (std::abs(gainPeakHoldDb) < 0.1f) gainPeakHoldDb = 0.0f;
    }
    
    // Gain meter: center = 0dB, up = boost, down = cut
    // Uses gradient: electric blue at top -> purple at bottom
    float gainMeterCenter = gainMeterBounds.getCentreY();
    float maxGainDisplay = 12.0f;  // Max display ±12dB
    
    // Draw center line (subtle)
    g.setColour(CustomLookAndFeel::getDimTextColour().withAlpha(0.2f));
    g.fillRect(gainMeterBounds.getX(), gainMeterCenter - 0.5f, singleMeterWidth, 1.0f);
    
    // Draw gain bar with gradient (blue top to purple bottom)
    if (std::abs(gainDb) > 0.1f)
    {
        float gainNorm = juce::jlimit(-1.0f, 1.0f, gainDb / maxGainDisplay);
        float gainBarHeight = std::abs(gainNorm) * (meterHeight / 2.0f);
        
        juce::Rectangle<float> gainBar;
        if (gainNorm > 0)
        {
            // Boost: draw upward from center
            gainBar = juce::Rectangle<float>(
                gainMeterBounds.getX(), 
                gainMeterCenter - gainBarHeight,
                singleMeterWidth, 
                gainBarHeight
            );
        }
        else
        {
            // Cut: draw downward from center
            gainBar = juce::Rectangle<float>(
                gainMeterBounds.getX(), 
                gainMeterCenter,
                singleMeterWidth, 
                gainBarHeight
            );
        }
        
        // Gradient: softer cyan-blue at top -> softer purple at bottom
        juce::ColourGradient gainGrad(
            juce::Colour(0xFF60B8D0),  // Desaturated cyan-blue at top
            gainMeterBounds.getX(), gainMeterBounds.getY(),
            juce::Colour(0xFF9070B8),  // Desaturated purple at bottom
            gainMeterBounds.getX(), gainMeterBounds.getBottom(),
            false
        );
        g.setGradientFill(gainGrad);
        g.fillRoundedRectangle(gainBar, 1.5f);
    }
    
    // Gain peak hold line with same gradient color at that position
    if (std::abs(gainPeakHoldDb) > 0.1f)
    {
        float peakNorm = juce::jlimit(-1.0f, 1.0f, gainPeakHoldDb / maxGainDisplay);
        float peakY = gainMeterCenter - peakNorm * (meterHeight / 2.0f);
        
        // Interpolate color based on position
        float colorPos = (peakY - gainMeterBounds.getY()) / gainMeterBounds.getHeight();
        juce::Colour peakColour = juce::Colour(0xFF60B8D0).interpolatedWith(
            juce::Colour(0xFF9070B8), colorPos);
        g.setColour(peakColour);
        g.fillRect(gainMeterBounds.getX(), peakY - 1.0f, singleMeterWidth, 2.0f);
    }
    
    // dB scale labels removed for cleaner transparent look
    
    // ========== READOUTS ==========
    auto formatDbShort = [](float db) -> juce::String {
        if (db < -60.0f) return "---";  // Show hyphens instead of Unicode infinity
        if (db >= -9.9f && db <= 0.0f)
            return juce::String(db, 1);
        else
            return juce::String(static_cast<int>(std::round(db)));
    };
    
    g.setFont(CustomLookAndFeel::getPluginFont(10.0f, true));
    
    // Output peak readout - centered ABOVE output meter (greyish, not colored)
    g.setColour(CustomLookAndFeel::getDimTextColour().withAlpha(0.8f));
    g.drawText(formatDbShort(outputPeakHoldDb), 
               static_cast<int>(outputMeterBounds.getCentreX() - 16),
               static_cast<int>(readoutArea.getY()),
               32, 
               static_cast<int>(readoutHeight),
               juce::Justification::centred);
    
    // Gain readout - centered ABOVE gain meter (greyish, not colored)
    g.setColour(CustomLookAndFeel::getDimTextColour().withAlpha(0.8f));
    juce::String gainText = (gainPeakHoldDb >= 0 ? "+" : "") + juce::String(gainPeakHoldDb, 1);
    g.drawText(gainText, 
               static_cast<int>(gainMeterBounds.getCentreX() - 18),
               static_cast<int>(readoutArea.getY()),
               36, 
               static_cast<int>(readoutHeight),
               juce::Justification::centred);
}

void WaveformDisplay::drawClippingIndicator(juce::Graphics& /*g*/)
{
    // Removed - no clipping indicator bar
}

void WaveformDisplay::timerCallback()
{
    // Simple repaint - don't push empty samples from timer
    // The pushSamples() function handles all sample pushing including during silence
    // This avoids the oscillation issue caused by timer interference
    repaint();
}
