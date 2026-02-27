/*
  ==============================================================================

    PluginEditor.h
    Created: 2026
    Author:  MBM Audio

    Full-screen waveform with tab-based controls overlay.
    Target knob is larger and centered.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "UI/CustomLookAndFeel.h"
#include "UI/WaveformDisplay.h"
#include "UI/DualRangeKnob.h"

//==============================================================================
// Animated Value Tooltip - appears below knobs with fade animation
class AnimatedTooltip : public juce::Component, private juce::Timer
{
public:
    AnimatedTooltip()
    {
        setAlwaysOnTop(true);
        setInterceptsMouseClicks(false, false);
        // Timer started on-demand when animation is needed (not always-on)
    }
    
    void showValue(const juce::String& label, const juce::String& value, juce::Component* sourceComponent, bool showAbove = false, bool isHelpMode = false)
    {
        bool contentChanged = (label != currentLabel || value != currentValue || sourceComponent != currentSource);
        if (contentChanged)
        {
            currentLabel = label;
            currentValue = value;
            currentSource = sourceComponent;
        }
        positionAbove = showAbove;
        helpMode = isHelpMode;
        targetOpacity = 1.0f;
        fadeSpeed = 0.25f;  // Fast fade-in
        hideDelayCounter = 0;  // Cancel any pending hide
        setVisible(true);
        toFront(false);  // Bring tooltip to front
        updatePosition();  // Update position immediately
        if (!isTimerRunning()) startTimerHz(60);  // Start animation timer on demand
        
        // Force repaint when content changes but tooltip is already fully visible
        // (timer stops repainting once opacity animation completes)
        if (contentChanged && currentOpacity >= 0.99f)
            repaint();
    }
    
    // Helper to count lines in text
    int countLines(const juce::String& text) const
    {
        return text.containsChar('\n') ? text.retainCharacters("\n").length() + 1 : 1;
    }
    
    void hideTooltip()
    {
        // Start fading immediately with smooth animation for all tooltips
        targetOpacity = 0.0f;
        fadeSpeed = helpMode ? 0.06f : 0.08f;  // Slightly slower fade for help mode
        hideDelayCounter = 0;
        if (!isTimerRunning()) startTimerHz(60);  // Ensure timer runs for fade-out
    }
    
    void hideTooltipImmediate()
    {
        targetOpacity = 0.0f;
        fadeSpeed = 0.15f;  // Fast fade
        hideDelayCounter = 0;
    }
    
    void updatePosition()
    {
        if (currentSource == nullptr || getParentComponent() == nullptr) return;
        
        // Calculate tooltip size based on content
        int tooltipWidth = helpMode ? 170 : 68;
        // Widen when showing +/- dB range values
        if (!helpMode && currentValue.contains("/"))
            tooltipWidth = 100;
        int tooltipHeight = helpMode ? 56 : 36;
        
        // Get source bounds relative to parent (works correctly with scaling transforms)
        auto sourceBounds = getParentComponent()->getLocalArea(currentSource, currentSource->getLocalBounds());
        
        int sourceX = sourceBounds.getX();
        int sourceY = sourceBounds.getY();
        int sourceW = sourceBounds.getWidth();
        int sourceH = sourceBounds.getHeight();
        
        int x, y;
        
        if (helpMode)
        {
            // Position ABOVE the component for help mode (better for bottom bar)
            x = sourceX + sourceW / 2 - tooltipWidth / 2;
            y = sourceY - tooltipHeight - 8;
            
            // If tooltip would go off the top, position below instead
            if (y < 4)
            {
                y = sourceY + sourceH + 8;
            }
        }
        else if (positionAbove)
        {
            // Position above the component
            x = sourceX + sourceW / 2 - tooltipWidth / 2;
            y = sourceY - tooltipHeight - 4;
        }
        else
        {
            // Position below the component
            x = sourceX + sourceW / 2 - tooltipWidth / 2;
            y = sourceY + sourceH + 4;
        }
        
        // Make sure tooltip stays within parent bounds
        auto parentBounds = getParentComponent()->getLocalBounds();
        if (x < 4) x = 4;
        if (x + tooltipWidth > parentBounds.getWidth() - 4) x = parentBounds.getWidth() - tooltipWidth - 4;
        
        // Vertical bounds check
        if (y < 4) y = 4;
        if (y + tooltipHeight > parentBounds.getHeight() - 4)
            y = parentBounds.getHeight() - tooltipHeight - 4;
        
        setBounds(x, y, tooltipWidth, tooltipHeight);
    }
    
    void paint(juce::Graphics& g) override
    {
        if (currentOpacity < 0.01f) return;
        
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        
        // Background with rounded corners
        g.setColour(CustomLookAndFeel::getSurfaceColour().withAlpha(0.95f * currentOpacity));
        g.fillRoundedRectangle(bounds, 6.0f);
        
        // Border
        g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(0.5f * currentOpacity));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
        
        if (helpMode)
        {
            // Help mode: show label at top and help text below
            auto labelBounds = bounds.removeFromTop(16.0f);
            g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(currentOpacity));
            g.setFont(CustomLookAndFeel::getPluginFont(11.0f, true));
            g.drawText(currentLabel.toUpperCase(), labelBounds.reduced(4.0f, 0.0f), juce::Justification::centredLeft);
            
            // Help text (slightly larger, wrapping)
            g.setColour(CustomLookAndFeel::getTextColour().withAlpha(0.85f * currentOpacity));
            g.setFont(CustomLookAndFeel::getPluginFont(10.0f, false));
            g.drawFittedText(currentValue, bounds.reduced(4.0f, 2.0f).toNearestInt(), 
                            juce::Justification::topLeft, 3);
        }
        else
        {
            // Normal mode: Label (top, bright grey, ALL CAPS)
            auto labelBounds = bounds.removeFromTop(bounds.getHeight() * 0.42f);
            g.setColour(juce::Colour(0xFFB0B0B0).withAlpha(currentOpacity));  // Bright grey
            g.setFont(CustomLookAndFeel::getPluginFont(10.0f, true));
            g.drawText(currentLabel.toUpperCase(), labelBounds, juce::Justification::centred);
            
            // Value (bottom, white for emphasis) - LARGER for readability
            g.setColour(juce::Colours::white.withAlpha(currentOpacity));
            g.setFont(CustomLookAndFeel::getPluginFont(13.0f, true));
            g.drawText(currentValue, bounds, juce::Justification::centred);
        }
    }
    
    bool isShowing() const { return currentOpacity > 0.01f; }
    bool isHelpMode() const { return helpMode; }
    
private:
    bool helpMode = false;
    int hideDelayCounter = 0;
    
    void timerCallback() override
    {
        // Handle hide delay for help mode tooltips
        if (hideDelayCounter > 0)
        {
            hideDelayCounter--;
            if (hideDelayCounter == 0)
            {
                // Delay expired, now start fading
                targetOpacity = 0.0f;
                fadeSpeed = 0.05f;  // Smooth fade-out after delay
            }
            return;  // Don't process fade while in delay
        }
        
        // Handle fade animation
        if (std::abs(currentOpacity - targetOpacity) > 0.01f)
        {
            currentOpacity += (targetOpacity - currentOpacity) * fadeSpeed;
            updatePosition();
            repaint();
        }
        else if (std::abs(currentOpacity - targetOpacity) > 0.001f)
        {
            currentOpacity = targetOpacity;
            repaint();
            if (currentOpacity < 0.01f)
            {
                setVisible(false);
                stopTimer();  // Stop timer when fully hidden
            }
        }
        else
        {
            // Animation complete - stop polling
            stopTimer();
        }
    }
    
    juce::String currentLabel;
    juce::String currentValue;
    juce::Component::SafePointer<juce::Component> currentSource;  // SafePointer prevents dangling
    float currentOpacity = 0.0f;
    float targetOpacity = 0.0f;
    float fadeSpeed = 0.15f;
    bool positionAbove = false;
};

//==============================================================================
// Advanced Panel Component with opaque background and FADE animation
class AdvancedPanelComponent : public juce::Component, private juce::Timer
{
public:
    AdvancedPanelComponent()
    {
        setOpaque(false);  // Not opaque during fade
        setInterceptsMouseClicks(false, false);  // Don't intercept clicks - let buttons through
        // Timer started on-demand when animation is needed
    }
    
    void setTargetOpacity(float opacity)
    {
        targetOpacity = opacity;
        if (!isTimerRunning()) startTimerHz(60);  // Start timer for animation
    }
    
    float getCurrentOpacity() const { return currentOpacity; }
    bool isAnimating() const { return std::abs(currentOpacity - targetOpacity) > 0.01f; }
    bool isFullyHidden() const { return currentOpacity < 0.01f && targetOpacity < 0.01f; }
    
    std::function<void()> onAnimationUpdate;
    
    void paint(juce::Graphics& g) override
    {
        if (currentOpacity < 0.01f) return;
        
        auto bounds = getLocalBounds().toFloat();
        
        // Shadow effect (drawn first, behind everything)
        g.setColour(juce::Colours::black.withAlpha(0.5f * currentOpacity));
        g.fillRoundedRectangle(bounds.translated(0.0f, 5.0f), 12.0f);
        
        // Background with subtle vertical gradient
        juce::ColourGradient bgGradient(
            juce::Colour(0xFF282C34).withAlpha(0.96f * currentOpacity), bounds.getX(), bounds.getY(),
            juce::Colour(0xFF1A1D22).withAlpha(0.96f * currentOpacity), bounds.getX(), bounds.getBottom(),
            false
        );
        g.setGradientFill(bgGradient);
        g.fillRoundedRectangle(bounds, 12.0f);
        
        // Subtle noise texture (random dots instead of lines)
        juce::Random random(42);  // Fixed seed for consistent noise
        g.setColour(juce::Colours::white.withAlpha(0.015f * currentOpacity));
        for (int i = 0; i < 200; ++i)
        {
            float x = bounds.getX() + random.nextFloat() * bounds.getWidth();
            float y = bounds.getY() + random.nextFloat() * bounds.getHeight();
            g.fillEllipse(x, y, 1.5f, 1.5f);
        }
        
        // Top edge highlight for depth
        juce::ColourGradient topHighlight(
            juce::Colours::white.withAlpha(0.06f * currentOpacity), bounds.getX(), bounds.getY(),
            juce::Colours::transparentWhite, bounds.getX(), bounds.getY() + 30.0f,
            false
        );
        g.setGradientFill(topHighlight);
        g.fillRoundedRectangle(bounds.withHeight(30.0f), 12.0f);
        
        // Border
        g.setColour(CustomLookAndFeel::getBorderColour().withAlpha(0.6f * currentOpacity));
        g.drawRoundedRectangle(bounds, 12.0f, 1.0f);
        
        // Bottom edge accent line
        g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(0.35f * currentOpacity));
        g.fillRoundedRectangle(bounds.getX() + 40.0f, bounds.getBottom() - 3.0f,
                                bounds.getWidth() - 80.0f, 2.0f, 1.0f);
    }
    
private:
    void timerCallback() override
    {
        if (std::abs(currentOpacity - targetOpacity) > 0.01f)
        {
            // Faster fade: 0.25 for fade-in, 0.20 for fade-out
            float speed = (targetOpacity > currentOpacity) ? 0.25f : 0.20f;
            currentOpacity += (targetOpacity - currentOpacity) * speed;
            if (onAnimationUpdate)
                onAnimationUpdate();
        }
        else if (std::abs(currentOpacity - targetOpacity) > 0.001f)
        {
            currentOpacity = targetOpacity;
            if (onAnimationUpdate)
                onAnimationUpdate();
            stopTimer();  // Animation complete
        }
        else
        {
            stopTimer();  // No change needed
        }
    }
    
    float targetOpacity = 0.0f;
    float currentOpacity = 0.0f;
};

//==============================================================================
// Slider with hover callbacks for tooltip support and drag end callback for undo
class TooltipSlider : public juce::Slider
{
public:
    TooltipSlider() = default;
    
    std::function<void()> onMouseEnter;
    std::function<void()> onMouseExit;
    std::function<void()> onDragEnd;  // Called when user releases mouse after dragging
    
    void mouseEnter(const juce::MouseEvent& e) override
    {
        juce::Slider::mouseEnter(e);
        if (onMouseEnter) onMouseEnter();
    }
    
    void mouseExit(const juce::MouseEvent& e) override
    {
        juce::Slider::mouseExit(e);
        if (onMouseExit) onMouseExit();
    }
    
    void mouseUp(const juce::MouseEvent& e) override
    {
        juce::Slider::mouseUp(e);
        if (onDragEnd && wasDragging) onDragEnd();
        wasDragging = false;
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        juce::Slider::mouseDrag(e);
        wasDragging = true;
    }
    
private:
    bool wasDragging = false;
};

//==============================================================================
// Mini Gain Meter Component - shows current gain +/- relative to range
class MiniGainMeter : public juce::Component
{
public:
    MiniGainMeter() = default;
    
    void setGainDb(float gain) { currentGainDb = gain; repaint(); }
    void setRangeDb(float range) { rangeDb = range; repaint(); }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        
        // Calculate colors based on gain - smooth interpolation
        float normalizedGain = (rangeDb > 0.001f) ? juce::jlimit(-1.0f, 1.0f, currentGainDb / rangeDb) : 0.0f;
        
        juce::Colour boostColour(0xFF5BCEFA);   // Bright cyan
        juce::Colour neutralColour = CustomLookAndFeel::getTextColour().withAlpha(0.6f);
        juce::Colour cutColour(0xFFB080E0);     // Bright purple
        
        // Smooth color interpolation
        juce::Colour displayColour;
        if (normalizedGain > 0.02f)
        {
            float t = juce::jmin(normalizedGain * 2.0f, 1.0f);
            displayColour = neutralColour.interpolatedWith(boostColour, t);
        }
        else if (normalizedGain < -0.02f)
        {
            float t = juce::jmin(-normalizedGain * 2.0f, 1.0f);
            displayColour = neutralColour.interpolatedWith(cutColour, t);
        }
        else
        {
            displayColour = neutralColour;
        }
        
        // === CIRCULAR ARC INDICATOR (SPEEDOMETER - GAP AT BOTTOM) ===
        // Using Path::addArc with explicit rectangle bounds for clarity
        float arcRadius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.40f;
        float arcCenterX = bounds.getCentreX();
        float arcCenterY = bounds.getCentreY() + 2.0f;
        float arcThickness = 3.5f;
        
        // Arc bounds (the ellipse that the arc sits on)
        auto arcBounds = juce::Rectangle<float>(
            arcCenterX - arcRadius, arcCenterY - arcRadius,
            arcRadius * 2.0f, arcRadius * 2.0f
        );
        
        // Path::addArc uses radians from 12 o'clock, CLOCKWISE positive
        // For gap at bottom: start at 7:30 (225° from 12), end at 4:30 (-225° or 135° from 12)
        // Actually, addArc: 0 = 12 o'clock (top), positive = clockwise
        // 7:30 position = 225° clockwise from 12 = 225° = 5π/4
        // 4:30 position = 135° clockwise from 12 = 135° = 3π/4
        // We want arc from 7:30 to 4:30 going the LONG way (through top)
        // That means: start at 5π/4 (225°), go COUNTER-clockwise to 3π/4 (135°)
        // In addArc terms: start = 5π/4, end = 3π/4 - 2π = -5π/4
        
        // Actually let's think simpler:
        // 0 rad = 12 o'clock (top)
        // π/2 = 3 o'clock (right)
        // π = 6 o'clock (bottom)
        // 3π/2 = 9 o'clock (left)
        // 
        // Gap at bottom means we skip around π (6 o'clock)
        // Arc goes from about 5π/4 (7:30) counter-clockwise to 3π/4 (4:30)
        // That's: 5π/4 → 3π/2 → 0 → π/2 → 3π/4 (the long way, 270°)
        
        // Gap at bottom (around 180° / π): arc from 7:30 clockwise through top to 4:30
        // 7:30 = 225° = 5π/4, 4:30 = 135° = 3π/4
        // Go clockwise: 225° → 270° → 315° → 0° → 45° → 90° → 135° (through top)
        float arcStart = juce::MathConstants<float>::pi * 1.25f;    // 5π/4 = 225° = 7:30
        float arcEnd = juce::MathConstants<float>::pi * 2.75f;      // 11π/4 = 495° = 4:30 (going clockwise)
        
        // Draw background arc (dim track) - use butt caps for clean straight edges
        juce::Path bgArc;
        bgArc.addArc(arcBounds.getX(), arcBounds.getY(), 
                     arcBounds.getWidth(), arcBounds.getHeight(),
                     arcStart, arcEnd, true);
        g.setColour(CustomLookAndFeel::getSurfaceColour().darker(0.15f));
        g.strokePath(bgArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::butt));
        
        // Draw semi-transparent circle inside the arc for better readability
        float innerCircleRadius = arcRadius - arcThickness - 3.0f;
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(arcCenterX - innerCircleRadius, arcCenterY - innerCircleRadius,
                      innerCircleRadius * 2.0f, innerCircleRadius * 2.0f);
        
        // Draw center tick mark (0 dB = TOP = 0 radians in addArc convention)
        float centerTickAngle = 0.0f;  // 0 = top in addArc
        // Convert to x,y: in addArc, 0 = top means we need to go UP from center
        float tickInnerR = arcRadius - 5.0f;
        float tickOuterR = arcRadius + 5.0f;
        g.setColour(CustomLookAndFeel::getDimTextColour().withAlpha(0.6f));
        // For addArc convention: x = center + r*sin(angle), y = center - r*cos(angle)
        g.drawLine(arcCenterX, arcCenterY - tickInnerR,
                   arcCenterX, arcCenterY - tickOuterR,
                   1.5f);
        
        // Draw filled arc portion based on gain
        if (std::abs(normalizedGain) > 0.01f)
        {
            // Center of arc is at top (0 radians)
            // Total arc is 270° = 3π/2, half is 135° = 3π/4
            float halfArc = juce::MathConstants<float>::pi * 0.75f;
            
            // In addArc: 0 = top, positive = clockwise
            // Boost (positive gain): fill from top toward RIGHT (clockwise = positive)
            // Cut (negative gain): fill from top toward LEFT (counter-clockwise = negative)
            float fillAngle = normalizedGain * halfArc;
            
            juce::Path fillArc;
            if (normalizedGain > 0)
            {
                // Boost: from 0 (top) clockwise toward 3π/4 (right side, 4:30)
                fillArc.addArc(arcBounds.getX(), arcBounds.getY(),
                               arcBounds.getWidth(), arcBounds.getHeight(),
                               0.0f, fillAngle, true);
            }
            else
            {
                // Cut: from negative angle back to 0 (top)
                // fillAngle is negative, so this goes from fillAngle to 0
                fillArc.addArc(arcBounds.getX(), arcBounds.getY(),
                               arcBounds.getWidth(), arcBounds.getHeight(),
                               fillAngle, 0.0f, true);
            }
            
            // Main arc fill - use butt cap at the 0 dB end for clean straight cutoff
            g.setColour(displayColour.withAlpha(0.95f));
            g.strokePath(fillArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::butt));
        }
        
        // === NUMBER DISPLAY (inside the dark circle) ===
        juce::String gainText;
        if (std::abs(currentGainDb) < 0.05f)
        {
            gainText = "0.0";
        }
        else
        {
            gainText = (currentGainDb > 0 ? "+" : "") + juce::String(currentGainDb, 1);
        }
        
        // Draw the number - centered in the inner circle
        g.setFont(CustomLookAndFeel::getPluginFont(12.0f, true));
        g.setColour(displayColour);
        
        auto textBounds = juce::Rectangle<float>(
            bounds.getX(), arcCenterY - 7.0f,
            bounds.getWidth(), 14.0f
        );
        g.drawText(gainText, textBounds.toNearestInt(), juce::Justification::centred);
        
        // Small "dB" label below
        g.setFont(CustomLookAndFeel::getPluginFont(7.0f, false));
        g.setColour(CustomLookAndFeel::getDimTextColour().withAlpha(0.5f));
        auto dbBounds = juce::Rectangle<float>(
            bounds.getX(), arcCenterY + 5.0f,
            bounds.getWidth(), 10.0f
        );
        g.drawText("dB", dbBounds.toNearestInt(), juce::Justification::centred);
    }
    
private:
    float currentGainDb = 0.0f;
    float rangeDb = 12.0f;
};

//==============================================================================
// Animated Toggle Button Component
class AnimatedToggleButton : public juce::ToggleButton, private juce::Timer
{
public:
    AnimatedToggleButton(const juce::String& name) : juce::ToggleButton(name) 
    {
        startTimerHz(60);
    }
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        float toggleWidth = 36.0f;
        float toggleHeight = 18.0f;
        
        // Toggle track on the left
        auto trackBounds = juce::Rectangle<float>(bounds.getX(), bounds.getCentreY() - toggleHeight / 2.0f, 
                                                   toggleWidth, toggleHeight);
        
        // Track background
        g.setColour(getToggleState() ? CustomLookAndFeel::getAccentColour().withAlpha(0.3f) 
                                      : CustomLookAndFeel::getSurfaceDarkColour());
        g.fillRoundedRectangle(trackBounds, toggleHeight / 2.0f);
        
        // Track border
        g.setColour(getToggleState() ? CustomLookAndFeel::getAccentColour() 
                                      : CustomLookAndFeel::getBorderColour());
        g.drawRoundedRectangle(trackBounds, toggleHeight / 2.0f, 1.0f);
        
        // Animated thumb position
        float thumbRadius = toggleHeight * 0.4f;
        float thumbX = trackBounds.getX() + thumbRadius + 2.0f + 
                       animationPosition * (toggleWidth - thumbRadius * 2.0f - 4.0f);
        float thumbY = trackBounds.getCentreY();
        
        // Thumb glow when on
        if (getToggleState() && animationPosition > 0.5f)
        {
            g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(0.3f * animationPosition));
            g.fillEllipse(thumbX - thumbRadius - 3.0f, thumbY - thumbRadius - 3.0f, 
                          (thumbRadius + 3.0f) * 2.0f, (thumbRadius + 3.0f) * 2.0f);
        }
        
        // Thumb
        g.setColour(getToggleState() ? CustomLookAndFeel::getAccentColour() 
                                      : CustomLookAndFeel::getDimTextColour());
        g.fillEllipse(thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);
        
        // Label text - brighter when ON
        juce::Colour textColour = getToggleState() 
            ? CustomLookAndFeel::getTextColour().brighter(0.2f)
            : (shouldDrawButtonAsHighlighted ? CustomLookAndFeel::getTextColour() 
                                              : CustomLookAndFeel::getDimTextColour());
        g.setColour(textColour);
        g.setFont(CustomLookAndFeel::getPluginFont(11.0f, true));
        g.drawText(getButtonText(), static_cast<int>(trackBounds.getRight() + 6.0f), 
                   static_cast<int>(bounds.getY()), 
                   static_cast<int>(bounds.getWidth() - toggleWidth - 8.0f), 
                   static_cast<int>(bounds.getHeight()), 
                   juce::Justification::centredLeft);
    }
    
    void clicked() override
    {
        juce::ToggleButton::clicked();
        targetPosition = getToggleState() ? 1.0f : 0.0f;
    }
    
private:
    void timerCallback() override
    {
        float speed = 0.15f;
        if (std::abs(animationPosition - targetPosition) > 0.01f)
        {
            animationPosition += (targetPosition - animationPosition) * speed;
            repaint();
        }
        else if (animationPosition != targetPosition)
        {
            animationPosition = targetPosition;
            repaint();
        }
    }
    
    float animationPosition = 0.0f;
    float targetPosition = 0.0f;
};

//==============================================================================
// Resize Button Component - shows popup menu with size options
class ResizeButton : public juce::Button
{
public:
    ResizeButton() : juce::Button("Resize") { initIcon(); }
    
    std::function<void(int)> onSizeSelected;
    int currentSize = 1; // 0=Small, 1=Medium, 2=Large
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        
        // Icon color - subtle but visible
        juce::Colour iconColour = shouldDrawButtonAsHighlighted 
            ? CustomLookAndFeel::getAccentColour() 
            : CustomLookAndFeel::getDimTextColour();
        
        if (iconDrawable != nullptr)
        {
            iconDrawable->replaceColour(lastIconColour, iconColour);
            lastIconColour = iconColour;
            iconDrawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
        }
    }
    
    void initIcon()
    {
        // Expand SVG icon - using #000000 for color replacement
        const char* expandSVG = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path fill="#000000" d="M7 8.414V12H5V5h7v2H8.414L17 15.586V12h2v7h-7v-2h3.586z"/></svg>)";
        if (auto xml = juce::XmlDocument::parse(expandSVG))
            iconDrawable = juce::Drawable::createFromSVG(*xml);
    }
    
    std::unique_ptr<juce::Drawable> iconDrawable;
    juce::Colour lastIconColour { juce::Colours::black };
    
    void clicked() override
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel(&getLookAndFeel());
        menu.addItem(1, "Small", true, currentSize == 0);
        menu.addItem(2, "Medium", true, currentSize == 1);
        menu.addItem(3, "Large", true, currentSize == 2);
        
        // Use SafePointer to avoid dangling 'this' if editor closes while menu is open
        juce::Component::SafePointer<ResizeButton> safeThis(this);
        menu.showMenuAsync(juce::PopupMenu::Options()
            .withTargetComponent(this)
            .withMinimumWidth(80),
            [safeThis](int result) {
                if (safeThis == nullptr) return;
                if (result >= 1 && result <= 3 && safeThis->onSizeSelected)
                {
                    safeThis->currentSize = result - 1;
                    safeThis->onSizeSelected(result - 1);
                }
            });
    }
    
    std::function<void(int)> onScaleSelected;
    int currentScale = 100;
};

//==============================================================================
// Gear icon button - SVG settings icon
class GearButton : public juce::Button
{
public:
    GearButton() : juce::Button("Settings") 
    {
        // Settings SVG icon - simplified gear with no opacity issues
        const char* settingsSVG = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path fill="#000000" d="M12 15.5A3.5 3.5 0 0 1 8.5 12A3.5 3.5 0 0 1 12 8.5a3.5 3.5 0 0 1 3.5 3.5a3.5 3.5 0 0 1-3.5 3.5m7.43-2.53c.04-.32.07-.64.07-.97c0-.33-.03-.66-.07-1l2.11-1.63c.19-.15.24-.42.12-.64l-2-3.46c-.12-.22-.39-.31-.61-.22l-2.49 1c-.52-.39-1.06-.73-1.69-.98l-.37-2.65A.506.506 0 0 0 14 2h-4c-.25 0-.46.18-.5.42l-.37 2.65c-.63.25-1.17.59-1.69.98l-2.49-1c-.22-.09-.49 0-.61.22l-2 3.46c-.13.22-.07.49.12.64L4.57 11c-.04.34-.07.67-.07 1c0 .33.03.65.07.97l-2.11 1.66c-.19.15-.25.42-.12.64l2 3.46c.12.22.39.3.61.22l2.49-1.01c.52.4 1.06.74 1.69.99l.37 2.65c.04.24.25.42.5.42h4c.25 0 .46-.18.5-.42l.37-2.65c.63-.26 1.17-.59 1.69-.99l2.49 1.01c.22.08.49 0 .61-.22l2-3.46c.12-.22.07-.49-.12-.64l-2.11-1.66Z"/></svg>)";
        if (auto xml = juce::XmlDocument::parse(settingsSVG))
            iconDrawable = juce::Drawable::createFromSVG(*xml);
    }
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(3.0f);
        
        bool isActive = getToggleState();
        
        // Draw glowing purple circle outline when active
        if (isActive)
        {
            // Calculate perfect circle dimensions
            float size = juce::jmin(bounds.getWidth(), bounds.getHeight()) + 4.0f;
            auto circleBounds = juce::Rectangle<float>(
                bounds.getCentreX() - size / 2.0f,
                bounds.getCentreY() - size / 2.0f,
                size, size
            );
            
            // Outer glow layers for smooth glow effect
            for (int i = 3; i >= 1; --i)
            {
                float expand = static_cast<float>(i) * 2.0f;
                float alpha = 0.12f / static_cast<float>(i);
                g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(alpha));
                g.drawEllipse(circleBounds.expanded(expand), 1.5f);
            }
            
            // Main purple circle outline
            g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(0.7f));
            g.drawEllipse(circleBounds, 1.5f);
        }
        
        // Icon color based on state
        juce::Colour iconColour;
        if (isActive)
            iconColour = CustomLookAndFeel::getAccentColour().brighter(0.6f);  // Bright purple when active
        else if (shouldDrawButtonAsHighlighted)
            iconColour = CustomLookAndFeel::getAccentColour();  // Purple on hover
        else
            iconColour = CustomLookAndFeel::getDimTextColour();  // Dim when inactive
        
        if (iconDrawable != nullptr)
        {
            iconDrawable->replaceColour(lastIconColour, iconColour);
            lastIconColour = iconColour;
            iconDrawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
        }
    }
    
private:
    std::unique_ptr<juce::Drawable> iconDrawable;
    juce::Colour lastIconColour { juce::Colours::black };
};

//==============================================================================
// Learn button with pulsing animation
class LearnButton : public juce::TextButton, private juce::Timer
{
public:
    LearnButton() : juce::TextButton("LEARN")
    {
        startTimerHz(30);
    }
    
    void setLearning(bool learning) 
    { 
        isLearning = learning;
        if (learning)
            pulsePhase = 0.0f;
        repaint();
    }
    
    bool getLearning() const { return isLearning; }
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        
        // Base background - always dark
        g.setColour(juce::Colour(0xFF2A2D35));
        g.fillRoundedRectangle(bounds, 4.0f);
        
        if (isLearning)
        {
            // Pulsing outer glow (drop shadow effect)
            float glowIntensity = 0.3f + 0.5f * (0.5f + 0.5f * std::sin(pulsePhase));
            
            // Multiple layers of glow for smooth effect
            for (int i = 3; i >= 1; --i)
            {
                float expand = static_cast<float>(i) * 2.0f;
                float alpha = glowIntensity * (0.15f / static_cast<float>(i));
                g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(alpha));
                g.drawRoundedRectangle(bounds.expanded(expand), 4.0f + expand * 0.5f, 2.0f);
            }
            
            // Bright pulsing border
            float borderAlpha = 0.7f + 0.3f * std::sin(pulsePhase);
            g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(borderAlpha));
            g.drawRoundedRectangle(bounds, 4.0f, 2.0f);
        }
        else
        {
            // Normal border
            g.setColour(shouldDrawButtonAsHighlighted 
                ? CustomLookAndFeel::getAccentColour().withAlpha(0.5f) 
                : juce::Colour(0xFF454550));
            g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        }
        
        // Text
        g.setFont(CustomLookAndFeel::getPluginFont(10.0f, true));
        g.setColour(isLearning ? CustomLookAndFeel::getAccentColour().brighter(0.3f) : CustomLookAndFeel::getDimTextColour());
        g.drawText(getButtonText(), bounds, juce::Justification::centred);
    }
    
private:
    void timerCallback() override
    {
        if (isLearning)
        {
            pulsePhase += 0.15f;
            if (pulsePhase > juce::MathConstants<float>::twoPi)
                pulsePhase -= juce::MathConstants<float>::twoPi;
            repaint();
        }
    }
    
    bool isLearning = false;
    float pulsePhase = 0.0f;
};

//==============================================================================
// Custom About/Help Dialog with fade animation matching plugin style
class AboutDialogPanel : public juce::Component, private juce::Timer
{
public:
    AboutDialogPanel()
    {
        setOpaque(false);
        setInterceptsMouseClicks(false, false);  // Start non-blocking; enabled when shown
        
        // Add actual close button (more reliable than mouseDown in AU hosts)
        closeButton.setButtonText(juce::CharPointer_UTF8("\xc3\x97"));  // × symbol
        closeButton.onClick = [this] {
            hide();
            // Don't call setVisible(false) here - let the fade animation and timer
            // handle it cleanly. Calling setVisible(false) mid-fade can leave the
            // component in an inconsistent state where isShowing() returns false
            // but the hit-test chain is disrupted.
            if (onClose) onClose();
        };
        closeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        closeButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFA0A8B0));
        addAndMakeVisible(closeButton);
        closeButton.setVisible(false);
        
        // Documentation link button
        docsButton.setButtonText("View Documentation");
        docsButton.onClick = [] {
            juce::URL("https://musicbymattie.com/magic-ride/docs").launchInDefaultBrowser();
        };
        docsButton.setColour(juce::TextButton::buttonColourId, CustomLookAndFeel::getAccentColour().withAlpha(0.3f));
        docsButton.setColour(juce::TextButton::textColourOffId, CustomLookAndFeel::getAccentColour());
        addAndMakeVisible(docsButton);
        docsButton.setVisible(false);
    }
    
    ~AboutDialogPanel() override
    {
        stopTimer();  // Critical: stop timer before destruction
    }
    
    void setVersion(const juce::String& version)
    {
        versionText = "magic.RIDE v" + version;
    }
    
    void show()
    {
        targetOpacity = 0.98f;
        setVisible(true);
        setInterceptsMouseClicks(true, true);  // Block clicks behind dialog
        closeButton.setVisible(true);
        docsButton.setVisible(true);
        toFront(true);
        if (!isTimerRunning())
            startTimerHz(60);
        resized();  // Position buttons
    }
    
    void hide()
    {
        targetOpacity = 0.0f;
        closeButton.setVisible(false);
        docsButton.setVisible(false);
        setInterceptsMouseClicks(false, false);  // Allow clicks through while fading
        
        // Ensure timer is running so the fade-out animation completes
        // and cleanup (setVisible(false), stopTimer) happens properly
        if (!isTimerRunning())
            startTimerHz(60);
    }
    
    bool isShowing() const { return targetOpacity > 0.5f || currentOpacity > 0.01f; }
    bool isAnimating() const { return std::abs(currentOpacity - targetOpacity) > 0.01f; }
    
    void resized() override
    {
        auto fullBounds = getLocalBounds().toFloat();
        float dialogWidth = 280.0f;
        float dialogHeight = 220.0f;  // Slightly taller for docs button
        dialogBounds = juce::Rectangle<float>(
            (fullBounds.getWidth() - dialogWidth) / 2.0f,
            (fullBounds.getHeight() - dialogHeight) / 2.0f,
            dialogWidth, dialogHeight
        );
        
        // Position close button
        int closeSize = 24;
        closeButton.setBounds(
            static_cast<int>(dialogBounds.getRight()) - closeSize - 6,
            static_cast<int>(dialogBounds.getY()) + 6,
            closeSize, closeSize
        );
        
        // Position docs button
        int docsWidth = 140;
        int docsHeight = 26;
        docsButton.setBounds(
            static_cast<int>(dialogBounds.getCentreX()) - docsWidth / 2,
            static_cast<int>(dialogBounds.getBottom()) - docsHeight - 18,
            docsWidth, docsHeight
        );
    }
    
    void paint(juce::Graphics& g) override
    {
        if (currentOpacity < 0.01f) return;
        
        auto fullBounds = getLocalBounds().toFloat();
        
        // Semi-transparent overlay behind the dialog
        g.setColour(juce::Colours::black.withAlpha(0.4f * currentOpacity));
        g.fillRect(fullBounds);
        
        // Shadow
        g.setColour(juce::Colours::black.withAlpha(0.5f * currentOpacity));
        g.fillRoundedRectangle(dialogBounds.translated(0.0f, 6.0f), 12.0f);
        
        // Background with gradient (matches advanced panel)
        juce::ColourGradient bgGradient(
            juce::Colour(0xFF282C34).withAlpha(0.98f * currentOpacity), dialogBounds.getX(), dialogBounds.getY(),
            juce::Colour(0xFF1A1D22).withAlpha(0.98f * currentOpacity), dialogBounds.getX(), dialogBounds.getBottom(),
            false
        );
        g.setGradientFill(bgGradient);
        g.fillRoundedRectangle(dialogBounds, 12.0f);
        
        // Border
        g.setColour(CustomLookAndFeel::getBorderColour().withAlpha(0.6f * currentOpacity));
        g.drawRoundedRectangle(dialogBounds, 12.0f, 1.0f);
        
        // Top accent line
        g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(0.5f * currentOpacity));
        g.fillRoundedRectangle(dialogBounds.getX() + 30, dialogBounds.getY() + 4, dialogBounds.getWidth() - 80, 2.0f, 1.0f);
        
        float y = dialogBounds.getY() + 26.0f;
        
        // Title/Version
        g.setColour(CustomLookAndFeel::getTextColour().withAlpha(currentOpacity));
        g.setFont(CustomLookAndFeel::getPluginFont(16.0f, true));
        g.drawText(versionText, dialogBounds.withY(y).withHeight(24.0f), juce::Justification::centredTop);
        y += 30.0f;
        
        // Subtitle
        g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(currentOpacity));
        g.setFont(CustomLookAndFeel::getPluginFont(11.0f, false));
        g.drawText("Precision Vocal Leveling", dialogBounds.withY(y).withHeight(18.0f), juce::Justification::centredTop);
        y += 28.0f;
        
        // Author info
        g.setColour(CustomLookAndFeel::getDimTextColour().withAlpha(currentOpacity));
        g.setFont(CustomLookAndFeel::getPluginFont(10.0f, false));
        g.drawText("by MBM Audio", dialogBounds.withY(y).withHeight(16.0f), juce::Justification::centredTop);
        y += 16.0f;
        g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(0.8f * currentOpacity));
        g.drawText("musicbymattie.com", dialogBounds.withY(y).withHeight(16.0f), juce::Justification::centredTop);
        y += 32.0f;
        
        // Help hint
        g.setColour(CustomLookAndFeel::getDimTextColour().withAlpha(0.7f * currentOpacity));
        g.setFont(CustomLookAndFeel::getPluginFont(9.0f, false));
        g.drawText("Hover over any control to see", dialogBounds.withY(y).withHeight(14.0f), juce::Justification::centredTop);
        y += 13.0f;
        g.drawText("help info in the status bar.", dialogBounds.withY(y).withHeight(14.0f), juce::Justification::centredTop);
        
        // Note: Documentation button is drawn as a child component (docsButton)
        
        // Bottom accent line
        g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(0.35f * currentOpacity));
        g.fillRoundedRectangle(dialogBounds.getX() + 40.0f, dialogBounds.getBottom() - 5.0f,
                               dialogBounds.getWidth() - 80.0f, 2.0f, 1.0f);
    }
    
    // Only participate in hit-testing when the dialog is actually visible/showing
    bool hitTest(int, int) override
    {
        return targetOpacity > 0.5f && currentOpacity > 0.01f;
    }
    
    // Consume all mouse clicks on the overlay so nothing behind is clickable
    void mouseDown(const juce::MouseEvent&) override {}
    void mouseUp(const juce::MouseEvent&) override {}
    
    std::function<void()> onClose;
    
private:
    juce::TextButton closeButton;
    juce::TextButton docsButton;
    juce::Rectangle<float> dialogBounds;
    
    void timerCallback() override
    {
        if (std::abs(currentOpacity - targetOpacity) > 0.01f)
        {
            float speed = (targetOpacity > currentOpacity) ? 0.25f : 0.20f;
            currentOpacity += (targetOpacity - currentOpacity) * speed;
            closeButton.setAlpha(currentOpacity);
            repaint();
        }
        else if (currentOpacity < 0.01f && targetOpacity < 0.01f)
        {
            setVisible(false);
            setInterceptsMouseClicks(false, false);  // Ensure clicks pass through
            closeButton.setVisible(false);
            stopTimer();  // Stop timer when fully hidden
        }
        else if (std::abs(currentOpacity - targetOpacity) <= 0.01f && targetOpacity > 0.5f)
        {
            // Animation complete at full opacity - can stop timer
            currentOpacity = targetOpacity;
            stopTimer();
            repaint();
        }
    }
    
    juce::String versionText;
    float targetOpacity = 0.0f;
    float currentOpacity = 0.0f;
};

//==============================================================================
// Help button (question mark)
class HelpButton : public juce::Button
{
public:
    HelpButton() : juce::Button("Help") { setClickingTogglesState(true); }
    
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat();
        bool isOn = getToggleState();
        
        juce::Colour iconColour = isOn ? CustomLookAndFeel::getAccentColour() 
                                       : (shouldDrawButtonAsHighlighted ? CustomLookAndFeel::getTextColour() 
                                                                        : CustomLookAndFeel::getDimTextColour());
        
        // Draw perfect circle around question mark when on
        float size = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 2.0f;
        auto circleBounds = juce::Rectangle<float>(
            bounds.getCentreX() - size / 2.0f,
            bounds.getCentreY() - size / 2.0f,
            size, size
        );
        
        if (isOn)
        {
            g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(0.4f));
            g.drawEllipse(circleBounds, 1.5f);
        }
        
        // Draw question mark centered
        g.setColour(iconColour);
        g.setFont(CustomLookAndFeel::getPluginFont(12.0f, true));
        g.drawText("?", circleBounds, juce::Justification::centred);
    }
};

//==============================================================================
// A/B Compare button
class ABCompareButton : public juce::Button
{
public:
    ABCompareButton() : juce::Button("A/B") { setClickingTogglesState(true); }
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        bool isB = getToggleState();
        
        // Background
        g.setColour(isB ? CustomLookAndFeel::getAccentColour().withAlpha(0.2f) : juce::Colour(0xFF2A2D35));
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Border
        g.setColour(shouldDrawButtonAsHighlighted || isB 
            ? CustomLookAndFeel::getAccentColour().withAlpha(0.6f) 
            : juce::Colour(0xFF454550));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        
        // A | B text
        g.setFont(CustomLookAndFeel::getPluginFont(10.0f, true));
        auto leftHalf = bounds.removeFromLeft(bounds.getWidth() / 2);
        auto rightHalf = bounds;
        
        g.setColour(isB ? CustomLookAndFeel::getDimTextColour() : CustomLookAndFeel::getTextColour());
        g.drawText("A", leftHalf, juce::Justification::centred);
        
        g.setColour(isB ? CustomLookAndFeel::getAccentColour() : CustomLookAndFeel::getDimTextColour());
        g.drawText("B", rightHalf, juce::Justification::centred);
    }
};

//==============================================================================
// Undo button (for parameter undo) - SVG icon
class UndoParamButton : public juce::Button
{
public:
    UndoParamButton() : juce::Button("Undo") 
    {
        // Replaced currentColor with #000000 for color replacement
        const char* undoSVG = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path fill="none" stroke="#000000" stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M7 13L3 9m0 0l4-4M3 9h13a5 5 0 0 1 0 10h-5"/></svg>)";
        if (auto xml = juce::XmlDocument::parse(undoSVG))
            iconDrawable = juce::Drawable::createFromSVG(*xml);
    }
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(3.0f);
        
        juce::Colour col = shouldDrawButtonAsHighlighted ? CustomLookAndFeel::getTextColour() : CustomLookAndFeel::getDimTextColour();
        
        if (iconDrawable != nullptr)
        {
            iconDrawable->replaceColour(lastIconColour, col);
            lastIconColour = col;
            iconDrawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
        }
    }
    
private:
    std::unique_ptr<juce::Drawable> iconDrawable;
    juce::Colour lastIconColour { juce::Colours::black };
};

//==============================================================================
// Redo button (for parameter redo) - SVG icon
class RedoParamButton : public juce::Button
{
public:
    RedoParamButton() : juce::Button("Redo") 
    {
        // Replaced currentColor with #000000 for color replacement
        const char* redoSVG = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path fill="none" stroke="#000000" stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="m17 13l4-4m0 0l-4-4m4 4H8a5 5 0 0 0 0 10h5"/></svg>)";
        if (auto xml = juce::XmlDocument::parse(redoSVG))
            iconDrawable = juce::Drawable::createFromSVG(*xml);
    }
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(3.0f);
        
        juce::Colour iconColour = shouldDrawButtonAsHighlighted 
            ? CustomLookAndFeel::getAccentColour() 
            : CustomLookAndFeel::getDimTextColour();
        
        if (iconDrawable != nullptr)
        {
            iconDrawable->replaceColour(lastIconColour, iconColour);
            lastIconColour = iconColour;
            iconDrawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
        }
    }
    
private:
    std::unique_ptr<juce::Drawable> iconDrawable;
    juce::Colour lastIconColour { juce::Colours::black };
};

//==============================================================================
// Bottom bar icon button with label (for Natural, Silence, Speed, Auto)
class BottomBarIconButton : public juce::Button, private juce::Timer
{
public:
    enum IconType { Natural, Silence, Speed, Auto };
    
    BottomBarIconButton(const juce::String& label, IconType type) 
        : juce::Button(label), buttonLabel(label), iconType(type) 
    {
        setClickingTogglesState(type != Speed);  // Speed doesn't toggle, Auto does toggle
        startTimerHz(60);  // Higher framerate for smooth pulsing
        
        // Cache SVG drawable once (parsing XML every paint is expensive)
        const char* svgData = nullptr;
        switch (type)
        {
            case Natural:  svgData = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><g fill="none" stroke="#000000" stroke-linecap="round" stroke-linejoin="round" stroke-width="2"><path d="M5 21c.5-4.5 2.5-8 7-10"/><path d="M9 18c6.218 0 10.5-3.288 11-12V4h-4.014c-9 0-11.986 4-12 9c0 1 0 3 2 5h3z"/></g></svg>)"; break;
            case Silence:  svgData = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256"><path fill="#000000" fill-rule="evenodd" d="m25.856 160.231l-.105 15.5l21.52.091s10.258.899 17.47-1.033c7.21-1.932 5.846.283 11.266-6.664s2.59-5.662 5.685-15.063s5.482-18.859 5.482-18.859l5.383-19.944l6.906-17.103l4.976-5.127l11.477-.617l16.25.1l18.06-.211l6.094.464l5.18 7.82l4.468 14.117l4.062 14.727l4.04 14.208l4.367 14.726s2.14 7.77 6.398 11.62c4.257 3.851 13.406 6.293 13.406 6.293l20.313.3l13.651-.105l.502-15.884l-16.709.405l-17.022-.72l-2.563-.717l-5.742-17.059l-6.713-23.695l-5.777-19.032s-1.753-7.91-6.973-13.517s-8.141-8.08-15.059-10.146s-10.042-.902-21.245-.803c-11.202.099-17.124.015-22.405.19c-5.281.174-10.457.896-10.457.896l-9.33 3.96l-8.1 11.07l-5.023 12.188l-5.23 18.891l-3.999 14.727l-4.511 13.608l-3.282 9.445l-17.84.793l-18.87.16z"/></svg>)"; break;
            case Speed:    svgData = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path fill="#000000" d="m20.38 8.57l-1.23 1.85a8 8 0 0 1-.22 7.58H5.07A8 8 0 0 1 15.58 6.85l1.85-1.23A10 10 0 0 0 3.35 19a2 2 0 0 0 1.72 1h13.85a2 2 0 0 0 1.74-1a10 10 0 0 0-.27-10.44z"/><path fill="#000000" d="M10.59 15.41a2 2 0 0 0 2.83 0l5.66-8.49l-8.49 5.66a2 2 0 0 0 0 2.83"/></svg>)"; break;
            case Auto:     svgData = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path fill="#000000" d="m19 9l-1.25-2.75L15 5l2.75-1.25L19 1l1.25 2.75L23 5l-2.75 1.25Zm0 14l-1.25-2.75L15 19l2.75-1.25L19 15l1.25 2.75L23 19l-2.75 1.25ZM4.8 16L8 7h2l3.2 9h-1.9l-.7-2H7.4l-.7 2Zm3.05-3.35h2.3L9 9ZM9 18q2.5 0 4.25-1.75T15 12q0-2.5-1.75-4.25T9 6Q6.5 6 4.75 7.75T3 12q0 2.5 1.75 4.25T9 18Zm0 2q-3.35 0-5.675-2.325Q1 15.35 1 12q0-3.35 2.325-5.675Q5.65 4 9 4q3.35 0 5.675 2.325Q17 8.65 17 12q0 3.35-2.325 5.675Q12.35 20 9 20Z"/></svg>)"; break;
        }
        if (svgData != nullptr)
        {
            if (auto xml = juce::XmlDocument::parse(svgData))
                cachedIcon = juce::Drawable::createFromSVG(*xml);
        }
    }
    
    // Update the displayed label text
    void setLabel(const juce::String& newLabel) { buttonLabel = newLabel; repaint(); }
    
    // Enable pulsing animation (for auto-target learning mode)
    void setPulsing(bool shouldPulse) { isPulsing = shouldPulse; }
    bool getPulsing() const { return isPulsing; }
    
    std::function<void()> onMouseEnter;
    std::function<void()> onMouseExit;
    
    void mouseEnter(const juce::MouseEvent& e) override
    {
        juce::Button::mouseEnter(e);
        if (onMouseEnter) onMouseEnter();
    }
    
    void mouseExit(const juce::MouseEvent& e) override
    {
        juce::Button::mouseExit(e);
        if (onMouseExit) onMouseExit();
    }
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        bool isOn = getToggleState();
        
        juce::Colour iconColour = isOn ? CustomLookAndFeel::getAccentColour() 
                                       : (shouldDrawButtonAsHighlighted ? CustomLookAndFeel::getTextColour() 
                                                                        : CustomLookAndFeel::getDimTextColour());
        
        // Use cached SVG icon (parsed once in constructor, not every paint)
        
        // === Always center icon + text together, no movement ===
        auto fullBounds = getLocalBounds().toFloat().reduced(1.0f);
        
        // Pulsing effect when learning/active
        if (isPulsing)
        {
            float pulseAlpha = 0.15f + 0.12f * pulseAmount;
            g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(pulseAlpha));
            g.fillRoundedRectangle(fullBounds, 3.0f);
            
            // Pulsing glow ring
            float glowAlpha = 0.2f + 0.15f * pulseAmount;
            g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(glowAlpha));
            g.drawRoundedRectangle(fullBounds, 3.0f, 1.5f);
            
            // Override icon color to accent for pulsing state
            iconColour = CustomLookAndFeel::getAccentBrightColour().interpolatedWith(
                CustomLookAndFeel::getAccentColour(), pulseAmount);
        }
        // Highlight background when on (draw first, behind everything)
        else if (isOn && iconType != Speed)
        {
            g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(0.1f + 0.05f * glowAmount));
            g.fillRoundedRectangle(fullBounds, 3.0f);
        }
        
        // Calculate centered layout for icon + text
        float iconSize = 12.0f;
        float textWidth = CustomLookAndFeel::getPluginFont(8.0f, true).getStringWidthFloat(buttonLabel);
        float gap = 3.0f;
        float totalWidth = iconSize + gap + textWidth;
        float startX = fullBounds.getCentreX() - totalWidth / 2.0f;
        
        // Icon area (centered with text)
        auto centeredIconArea = juce::Rectangle<float>(startX, fullBounds.getCentreY() - iconSize / 2.0f, iconSize, iconSize);
        
        if (cachedIcon != nullptr)
        {
            cachedIcon->replaceColour(lastIconColour, iconColour);
            lastIconColour = iconColour;
            
            // Flip speaker icon upside down
            if (iconType == Silence)
            {
                g.saveState();
                float cy = centeredIconArea.getCentreY();
                auto transform = juce::AffineTransform::verticalFlip(cy * 2.0f);
                g.addTransform(transform);
                cachedIcon->drawWithin(g, centeredIconArea, juce::RectanglePlacement::centred, 1.0f);
                g.restoreState();
            }
            else
            {
                cachedIcon->drawWithin(g, centeredIconArea, juce::RectanglePlacement::centred, 1.0f);
            }
        }
        
        // Label text (centered with icon, always same position)
        g.setColour(iconColour);
        g.setFont(CustomLookAndFeel::getPluginFont(8.0f, true));
        auto textBounds = juce::Rectangle<float>(startX + iconSize + gap, fullBounds.getY(), textWidth + 4.0f, fullBounds.getHeight());
        g.drawText(buttonLabel, textBounds, juce::Justification::centredLeft);
    }
    
private:
    void timerCallback() override
    {
        bool needsRepaint = false;
        
        // Regular glow animation
        float targetGlow = getToggleState() ? 1.0f : 0.0f;
        if (std::abs(glowAmount - targetGlow) > 0.01f)
        {
            glowAmount += (targetGlow - glowAmount) * 0.2f;
            needsRepaint = true;
        }
        
        // Pulsing animation (sine wave)
        if (isPulsing)
        {
            pulsePhase += 0.1f;  // About 1 second period at 60Hz
            if (pulsePhase > juce::MathConstants<float>::twoPi)
                pulsePhase -= juce::MathConstants<float>::twoPi;
            pulseAmount = (std::sin(pulsePhase) + 1.0f) * 0.5f;  // 0 to 1
            needsRepaint = true;
        }
        else if (pulseAmount > 0.01f)
        {
            pulseAmount *= 0.85f;  // Fade out
            needsRepaint = true;
        }
        
        if (needsRepaint)
            repaint();
    }
    
    juce::String buttonLabel;
    IconType iconType;
    std::unique_ptr<juce::Drawable> cachedIcon;  // Parsed once, not every paint
    juce::Colour lastIconColour { juce::Colours::black };
    float glowAmount = 0.0f;
    float pulseAmount = 0.0f;
    float pulsePhase = 0.0f;
    bool isPulsing = false;
};

//==============================================================================
// Clean arrow button (no background, just arrow)
class ArrowButton : public juce::Button
{
public:
    ArrowButton(bool pointsRight) : juce::Button("Arrow"), rightArrow(pointsRight) {}
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float arrowWidth = 5.0f;
        float arrowHeight = 8.0f;
        
        juce::Path arrow;
        if (rightArrow)
        {
            arrow.addTriangle(cx - arrowWidth/2, cy - arrowHeight/2,
                              cx - arrowWidth/2, cy + arrowHeight/2,
                              cx + arrowWidth/2, cy);
        }
        else
        {
            arrow.addTriangle(cx + arrowWidth/2, cy - arrowHeight/2,
                              cx + arrowWidth/2, cy + arrowHeight/2,
                              cx - arrowWidth/2, cy);
        }
        
        g.setColour(shouldDrawButtonAsHighlighted 
            ? CustomLookAndFeel::getAccentColour() 
            : CustomLookAndFeel::getDimTextColour());
        g.fillPath(arrow);
    }
    
private:
    bool rightArrow;
};

//==============================================================================
// Initialize/Reset button (sliders icon)
class InitButton : public juce::Button
{
public:
    InitButton() : juce::Button("Init") {}
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(5.0f);
        
        juce::Colour iconColour = shouldDrawButtonAsHighlighted 
            ? CustomLookAndFeel::getAccentColour() 
            : CustomLookAndFeel::getDimTextColour();
        
        g.setColour(iconColour);
        
        // Draw three horizontal sliders icon (represents resetting parameters)
        float lineY1 = bounds.getY() + bounds.getHeight() * 0.25f;
        float lineY2 = bounds.getCentreY();
        float lineY3 = bounds.getY() + bounds.getHeight() * 0.75f;
        float lineThickness = 1.5f;
        float dotRadius = 2.0f;
        
        // Slider tracks
        g.drawLine(bounds.getX(), lineY1, bounds.getRight(), lineY1, lineThickness);
        g.drawLine(bounds.getX(), lineY2, bounds.getRight(), lineY2, lineThickness);
        g.drawLine(bounds.getX(), lineY3, bounds.getRight(), lineY3, lineThickness);
        
        // Slider dots (at center positions = default)
        g.fillEllipse(bounds.getCentreX() - dotRadius, lineY1 - dotRadius, dotRadius * 2, dotRadius * 2);
        g.fillEllipse(bounds.getCentreX() - dotRadius, lineY2 - dotRadius, dotRadius * 2, dotRadius * 2);
        g.fillEllipse(bounds.getCentreX() - dotRadius, lineY3 - dotRadius, dotRadius * 2, dotRadius * 2);
    }
};

//==============================================================================
// Natural mode icon button (smooth wave)
class NaturalIconButton : public juce::ToggleButton
{
public:
    NaturalIconButton() : juce::ToggleButton() {}
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto iconBounds = bounds.removeFromLeft(bounds.getHeight()).reduced(4.0f);
        
        bool isOn = getToggleState();
        juce::Colour iconColour = isOn ? CustomLookAndFeel::getAccentColour() 
                                       : (shouldDrawButtonAsHighlighted ? CustomLookAndFeel::getTextColour() 
                                                                        : CustomLookAndFeel::getDimTextColour());
        
        g.setColour(iconColour);
        
        // Draw smooth sine wave (natural = smooth, organic)
        juce::Path wavePath;
        float cy = iconBounds.getCentreY();
        float amp = iconBounds.getHeight() * 0.35f;
        
        wavePath.startNewSubPath(iconBounds.getX(), cy);
        for (float x = 0; x <= 1.0f; x += 0.1f)
        {
            float px = iconBounds.getX() + x * iconBounds.getWidth();
            float py = cy - std::sin(x * juce::MathConstants<float>::twoPi) * amp;
            wavePath.lineTo(px, py);
        }
        g.strokePath(wavePath, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved));
        
        // Label
        g.setFont(CustomLookAndFeel::getPluginFont(10.0f, true));
        g.drawText("NATURAL", bounds.withTrimmedLeft(iconBounds.getWidth() + 4), juce::Justification::centredLeft);
    }
};

//==============================================================================
// Smart Silence icon button (gate/threshold symbol)
class SmartSilenceIconButton : public juce::ToggleButton
{
public:
    SmartSilenceIconButton() : juce::ToggleButton() {}
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto iconBounds = bounds.removeFromLeft(bounds.getHeight()).reduced(4.0f);
        
        bool isOn = getToggleState();
        juce::Colour iconColour = isOn ? CustomLookAndFeel::getAccentColour() 
                                       : (shouldDrawButtonAsHighlighted ? CustomLookAndFeel::getTextColour() 
                                                                        : CustomLookAndFeel::getDimTextColour());
        
        g.setColour(iconColour);
        
        // Draw gate/threshold symbol (noise gate representation)
        float cx = iconBounds.getCentreX();
        float cy = iconBounds.getCentreY();
        float size = juce::jmin(iconBounds.getWidth(), iconBounds.getHeight()) * 0.4f;
        
        // Horizontal line (threshold)
        g.drawLine(iconBounds.getX(), cy, iconBounds.getRight(), cy, 1.5f);
        
        // Arrow pointing down (suppression)
        juce::Path arrow;
        arrow.addTriangle(cx - size * 0.5f, cy + 2.0f,
                          cx + size * 0.5f, cy + 2.0f,
                          cx, cy + size + 2.0f);
        g.fillPath(arrow);
        
        // Small "x" above threshold (muted signal)
        float xSize = size * 0.3f;
        g.drawLine(cx - xSize, cy - size * 0.6f - xSize, cx + xSize, cy - size * 0.6f + xSize, 1.2f);
        g.drawLine(cx - xSize, cy - size * 0.6f + xSize, cx + xSize, cy - size * 0.6f - xSize, 1.2f);
        
        // Label
        g.setFont(CustomLookAndFeel::getPluginFont(10.0f, true));
        g.drawText("SILENCE", bounds.withTrimmedLeft(iconBounds.getWidth() + 4), juce::Justification::centredLeft);
    }
};

//==============================================================================
// Undo button - SVG icon
class UndoButton : public juce::Button
{
public:
    UndoButton() : juce::Button("Undo") 
    {
        // Replaced currentColor with #000000 for color replacement
        const char* undoSVG = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path fill="none" stroke="#000000" stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M7 13L3 9m0 0l4-4M3 9h13a5 5 0 0 1 0 10h-5"/></svg>)";
        if (auto xml = juce::XmlDocument::parse(undoSVG))
            iconDrawable = juce::Drawable::createFromSVG(*xml);
    }
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(3.0f);
        
        juce::Colour iconColour = shouldDrawButtonAsHighlighted 
            ? CustomLookAndFeel::getAccentColour() 
            : CustomLookAndFeel::getDimTextColour();
        
        if (iconDrawable != nullptr)
        {
            iconDrawable->replaceColour(lastIconColour, iconColour);
            lastIconColour = iconColour;
            iconDrawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
        }
    }
    
private:
    std::unique_ptr<juce::Drawable> iconDrawable;
    juce::Colour lastIconColour { juce::Colours::black };
};

//==============================================================================
// Logo Component - draws full branding SVG with RIDE in purple, MBM AUDIO clickable to website
class LogoComponent : public juce::Component
{
public:
    LogoComponent()
    {
        parseSVG();
    }
    
    void paint(juce::Graphics& g) override
    {
        // More horizontal padding (left/right), minimal vertical padding
        auto bounds = getLocalBounds().toFloat().reduced(16.0f, 1.0f);
        
        if (logoDrawable != nullptr)
        {
            logoDrawable->drawWithin(g, bounds, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid, 1.0f);
        }
    }
    
    void mouseMove(const juce::MouseEvent& e) override
    {
        // Show pointing hand only over MBM AUDIO section (left ~40% of logo)
        if (isOverMBMAudio(e.getPosition()))
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    
    void mouseUp(const juce::MouseEvent& e) override
    {
        // Only respond to clicks on MBM AUDIO section
        if (e.mouseWasClicked() && isOverMBMAudio(e.getPosition()))
        {
            juce::URL("https://musicbymattie.com/magic-ride").launchInDefaultBrowser();
        }
    }
    
private:
    bool isOverMBMAudio(juce::Point<int> pos) const
    {
        // MBM AUDIO section is roughly the left 40% of the logo (before "magic.RIDE")
        // SVG viewBox is 1928 wide, MBM AUDIO ends around x=750
        auto bounds = getLocalBounds().toFloat().reduced(16.0f, 1.0f);
        float mbmAudioWidth = bounds.getWidth() * 0.40f;  // Left 40%
        return pos.x < (bounds.getX() + mbmAudioWidth);
    }
    
    void parseSVG()
    {
        // Full SVG logo - split into chunks to avoid MSVC string literal limit (C2026)
        // MBM AUDIO magic.RIDE branding
        const char* svgContent = 
        // Part 1: Header and waveform icon
        R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1928 230">
  <defs>
    <linearGradient id="rideGradient" x1="0%" y1="100%" x2="0%" y2="0%">
      <stop offset="0%" style="stop-color:#7B5CAD"/>
      <stop offset="100%" style="stop-color:#D4B8FF"/>
    </linearGradient>
  </defs>
  <!-- Waveform icon - darker gray -->
  <path d="M0,21.57v165.17c0,5.28,4.28,9.55,9.55,9.55h165.17c5.28,0,9.55-4.28,9.55-9.55V21.57c0-5.28-4.28-9.55-9.55-9.55H9.55C4.28,12.01,0,16.29,0,21.57ZM177.43,105.3c-.17,4.44-3.37,7.69-8.1,7.73-7.03.05-13.97.69-20.93-.06-3.25-.35-5.72-1.3-7.81-3.02-1.11-.91-1.97-2.1-2.55-3.42,0,0,0,0,0,0v.73c-2.21-3.26-3.16-1.97-3.46-1.06-.31.91-3.37,19.05-3.37,19.05-2.33,13.14-3.06,31.72-13.55,39.87-.4.31-5.88,2.83-10.68-3.07-.11-.14-.22-.27-.33-.42-.04-.05-.07-.1-.11-.14h0c-3.89-5.4-6.43-18.71-7.04-23.05l-1.19-8.31c-.49-3.41-.86-6.32-1.31-9.74l-3.11-23.84c-1.42-10.91-2.78-21.44-4.85-32.25-.21-1.1-.94-1.7-1.81-1.66-.43.02-1.26.16-1.47,1.01-5.29,22.09-5.83,42.34-13.22,65.72-1.34,4.23-3.64,8.6-7.56,10.63-4.99,2.57-10.37-.62-12.93-5.17-4.97-8.85-8.08-25.23-9.68-35.61-.21-1.39-.5-2.27-1.8-2.31-1.58-.04-1.38.98-1.62,2.42l-8.34,48.71c-1.23,7.17-3.13,13.89-5.92,20.38-1.6,3.71-3.54,6.47-6.93,8.55-5.63,3.45-10.27-1.66-10.44-2.23-.48-1.61.09-2.43.94-3.75,11.07-17.12,7.22-27.02,11.43-46.93.57-2.69.74-5.47,1.16-8.24,2.18-14.09,3.86-27.85,8.55-41.28,1.63-4.66,5.62-9.53,10.76-9.56,11.57-.07,14.71,21.5,16.58,29.4.89,3.75,1.26,7.33,1.97,11.12.25,1.32,1.37,1.86,2.19,1.81,1.28-.08,1.68-.84,1.91-2.24l3.72-22.73c1.5-9.14,7.64-53.29,23.29-47.23,5.36,2.07,8.56,7.29,10.4,12.78,4.15,12.32,6.03,24.79,7.66,37.73l4.81,38.4c.15,1.22.43,2.36,1.63,2.36,1.16,0,1.48-1.14,1.67-2.37l2.41-16.03c1-6.69,2.73-14.84,6.44-20.49,2.45-3.74,8.91-7.01,14.67-3.86,6.77,3.71,6.47,14.28,12.38,14.2l16.91-.23c4.58-.06,8.83,3.31,8.66,7.67Z" fill="#7a7a7a"/>)SVG"
        // Part 2: MBM text
        R"SVG(  <!-- MBM text - darker gray -->
  <path d="M315.28,90.9h-18.45l67.18-69.39h30.76v78.74h-30.76v-53.64l9.23,3.81-48.6,49.83h-36.91l-48.72-49.7,9.35-3.81v53.52h-30.76V21.51h30.76l66.93,69.39Z" fill="#7a7a7a"/>
  <path d="M411.98,100.25V21.51h77.51c15.91,0,27.64,1.56,35.19,4.68,7.55,3.12,11.32,8.53,11.32,16.24,0,4.92-1.62,8.7-4.86,11.32-3.24,2.63-7.87,4.45-13.9,5.48-6.03,1.03-13.27,1.66-21.71,1.91l.98-.98c6.15,0,11.85.17,17.1.49,5.25.33,9.84,1.07,13.78,2.21,3.94,1.15,7.01,2.91,9.23,5.29,2.21,2.38,3.32,5.66,3.32,9.84,0,5.09-1.81,9.27-5.41,12.55-3.61,3.28-8.74,5.72-15.38,7.32-6.64,1.6-14.48,2.4-23.5,2.4h-83.66ZM442.74,54.97h47.74c4.35,0,7.48-.41,9.41-1.23,1.93-.82,2.89-2.13,2.89-3.94s-.97-3.22-2.89-4c-1.93-.78-5.07-1.17-9.41-1.17h-47.74v10.33ZM442.74,77.12h52.9c4.35,0,7.48-.41,9.41-1.23,1.93-.82,2.89-2.13,2.89-3.94,0-1.23-.43-2.21-1.29-2.95s-2.2-1.29-4-1.66c-1.81-.37-4.14-.55-7.01-.55h-52.9v10.33Z" fill="#7a7a7a"/>
  <path d="M649.93,90.9h-18.45l67.17-69.39h30.76v78.74h-30.76v-53.64l9.23,3.81-48.6,49.83h-36.91l-48.72-49.7,9.35-3.81v53.52h-30.76V21.51h30.76l66.93,69.39Z" fill="#7a7a7a"/>)SVG"
        // Part 3: AUDIO text
        R"SVG(  <!-- AUDIO text - darker gray -->
  <path d="M209.96,184.84l41.74-62.27h27.05l42.13,62.27h-26.76l-36.1-55.75h14.5l-35.81,55.75h-26.76ZM231.75,175.41v-14.6h66.55v14.6h-66.55Z" fill="#7a7a7a"/>
  <path d="M411.27,149.33v-26.76h24.32v31.14c0,5.51-1.02,10.2-3.06,14.06-2.04,3.86-4.88,7.04-8.51,9.54-3.63,2.5-7.82,4.43-12.55,5.79-4.74,1.36-9.81,2.32-15.23,2.87-5.42.55-10.91.83-16.49.83-5.9,0-11.61-.28-17.12-.83-5.51-.55-10.62-1.51-15.32-2.87-4.7-1.36-8.81-3.29-12.31-5.79-3.5-2.5-6.24-5.68-8.22-9.54-1.98-3.86-2.97-8.55-2.97-14.06v-31.14h24.33v26.76c0,5.19,1.27,9.15,3.79,11.87s6.14,4.59,10.85,5.59c4.7,1.01,10.36,1.51,16.98,1.51s12-.5,16.74-1.51c4.73-1,8.38-2.87,10.95-5.59,2.56-2.72,3.84-6.68,3.84-11.87Z" fill="#7a7a7a"/>
  <path d="M519.27,122.57c8.76,0,15.99.84,21.7,2.53,5.71,1.69,10.18,3.99,13.43,6.91s5.53,6.24,6.86,9.97c1.33,3.73,1.99,7.64,1.99,11.72s-.75,8-2.24,11.72c-1.49,3.73-3.94,7.05-7.35,9.97s-7.91,5.22-13.53,6.91c-5.61,1.69-12.57,2.53-20.87,2.53h-70.06v-62.27h70.06ZM473.54,165.38h44.76c3.63,0,6.71-.24,9.24-.73,2.53-.49,4.56-1.22,6.08-2.19,1.52-.97,2.63-2.19,3.31-3.65.68-1.46,1.02-3.16,1.02-5.11s-.34-3.65-1.02-5.11c-.68-1.46-1.78-2.68-3.31-3.65-1.52-.97-3.55-1.7-6.08-2.19-2.53-.49-5.61-.73-9.24-.73h-44.76v23.35Z" fill="#7a7a7a"/>
  <path d="M577.36,122.57h24.32v62.27h-24.32v-62.27Z" fill="#7a7a7a"/>
  <path d="M674.66,186.79c-14.14,0-25.53-1.25-34.15-3.75-8.63-2.5-14.9-6.19-18.83-11.09-3.93-4.9-5.89-10.98-5.89-18.24s1.96-13.35,5.89-18.24c3.92-4.9,10.2-8.59,18.83-11.09,8.63-2.5,20.01-3.75,34.15-3.75s25.53,1.25,34.15,3.75c8.63,2.5,14.9,6.2,18.83,11.09,3.92,4.9,5.89,10.98,5.89,18.24s-1.96,13.35-5.89,18.24c-3.93,4.9-10.2,8.6-18.83,11.09-8.63,2.5-20.01,3.75-34.15,3.75ZM674.66,168.3c6.68,0,12.46-.4,17.32-1.22,4.87-.81,8.63-2.27,11.29-4.38,2.66-2.11,3.99-5.11,3.99-9s-1.33-6.89-3.99-9c-2.66-2.11-6.42-3.57-11.29-4.38-4.86-.81-10.64-1.22-17.32-1.22s-12.54.41-17.56,1.22c-5.03.81-8.95,2.27-11.77,4.38-2.82,2.11-4.23,5.11-4.23,9s1.41,6.89,4.23,9c2.82,2.11,6.75,3.57,11.77,4.38,5.03.81,10.88,1.22,17.56,1.22Z" fill="#7a7a7a"/>
  <!-- Vertical separator line -->
  <line x1="841.79" y1="23.72" x2="841.79" y2="189" fill="none" stroke="#808080" stroke-linecap="round" stroke-miterlimit="10" stroke-width="3" opacity="0.65"/>)SVG"
        // Part 4: magic. text
        R"SVG(  <!-- magic. text -->
  <path d="M952.61,66.99h13.4v83.78h-13.4v-83.78ZM994.83,65.31c4.47,0,8.49.67,12.06,2.01,3.57,1.34,6.59,3.3,9.05,5.86,2.46,2.57,4.36,5.67,5.7,9.3,1.34,3.63,2.01,7.74,2.01,12.31v55.96h-13.4v-52.61c0-6.48-1.51-11.34-4.52-14.58-3.02-3.24-7.54-4.86-13.57-4.86-4.58,0-8.83,1.17-12.73,3.52-3.91,2.35-7.12,5.56-9.63,9.63-2.51,4.08-4.05,8.8-4.61,14.16l-.17-9.72c.56-4.69,1.7-8.94,3.43-12.73,1.73-3.8,3.91-7.06,6.53-9.8,2.62-2.74,5.64-4.83,9.05-6.28,3.41-1.45,7.01-2.18,10.81-2.18ZM1051.13,65.31c4.47,0,8.49.67,12.06,2.01,3.57,1.34,6.59,3.3,9.05,5.86,2.46,2.57,4.36,5.67,5.7,9.3,1.34,3.63,2.01,7.74,2.01,12.31v55.96h-13.4v-52.61c0-6.48-1.51-11.34-4.52-14.58-3.02-3.24-7.54-4.86-13.57-4.86-4.58,0-8.83,1.17-12.73,3.52-3.91,2.35-7.12,5.56-9.63,9.63-2.51,4.08-4.05,8.8-4.61,14.16l-.17-9.72c.56-4.69,1.7-8.94,3.43-12.73,1.73-3.8,3.91-7.06,6.53-9.8,2.62-2.74,5.64-4.83,9.05-6.28,3.41-1.45,7.01-2.18,10.81-2.18Z" fill="#e1e1e0"/>
  <path d="M1133.4,152.44c-6.59,0-12.65-1.87-18.18-5.61-5.53-3.74-9.94-8.88-13.24-15.41-3.3-6.53-4.94-13.99-4.94-22.37s1.68-15.86,5.03-22.45c3.35-6.59,7.9-11.78,13.66-15.58,5.75-3.8,12.2-5.7,19.35-5.7,7.82,0,13.77,1.98,17.84,5.95,4.08,3.97,6.87,9.24,8.38,15.83,1.51,6.59,2.26,13.91,2.26,21.95,0,4.25-.45,8.85-1.34,13.82-.9,4.97-2.43,9.72-4.61,14.24-2.18,4.52-5.22,8.21-9.13,11.06-3.91,2.85-8.94,4.27-15.08,4.27ZM1136.75,139.71c5.7,0,10.41-1.4,14.16-4.19,3.74-2.79,6.5-6.51,8.29-11.14,1.79-4.63,2.68-9.74,2.68-15.33,0-6.14-.92-11.53-2.76-16.17-1.84-4.63-4.64-8.27-8.38-10.89-3.74-2.62-8.41-3.94-13.99-3.94-8.38,0-14.86,2.93-19.44,8.8-4.58,5.86-6.87,13.27-6.87,22.2,0,5.92,1.14,11.2,3.43,15.83,2.29,4.64,5.42,8.27,9.38,10.89,3.96,2.63,8.46,3.94,13.49,3.94ZM1161.88,66.99h13.4v83.78h-12.23c-.34-4.02-.62-7.37-.84-10.05-.23-2.68-.33-4.91-.33-6.7v-67.02Z" fill="#e1e1e0"/>)SVG"
        // Part 5: magic. text continued
        R"SVG(  <path d="M1227.06,137.53c-5.92,0-11.54-1.42-16.84-4.27-5.31-2.85-9.61-6.95-12.9-12.31-3.3-5.36-4.94-11.78-4.94-19.27s1.62-13.93,4.86-19.35c3.24-5.42,7.48-9.61,12.73-12.57,5.25-2.96,10.95-4.44,17.09-4.44,1.9,0,3.71.14,5.45.42,1.73.28,3.38.7,4.94,1.26h34.01v14.41c-4.8,0-9.58-.89-14.33-2.68-4.75-1.79-8.8-3.63-12.15-5.53l-.67-.67c3.91,2.12,7.12,4.64,9.63,7.54,2.51,2.91,4.41,6.17,5.7,9.8,1.28,3.63,1.93,7.57,1.93,11.81,0,7.48-1.59,13.91-4.77,19.27-3.18,5.36-7.4,9.47-12.65,12.31-5.25,2.85-10.95,4.27-17.09,4.27ZM1251.69,184.28v-3.69c0-5.92-1.54-10.22-4.61-12.9-3.07-2.68-7.68-4.02-13.82-4.02h-16.75c-3.69,0-6.9-.31-9.63-.92-2.74-.62-5-1.54-6.79-2.76-1.79-1.23-3.13-2.74-4.02-4.52-.9-1.79-1.34-3.8-1.34-6.03,0-4.24,1.28-7.46,3.85-9.63,2.57-2.18,5.75-3.54,9.55-4.1,3.8-.56,7.43-.56,10.89,0l8.04,1.84c-6.03.33-10.7.95-13.99,1.84-3.3.9-4.94,2.79-4.94,5.7,0,1.68.64,3.04,1.93,4.1,1.28,1.06,3.21,1.59,5.78,1.59h17.43c5.81,0,11.11.81,15.92,2.43,4.8,1.62,8.66,4.36,11.56,8.21,2.9,3.85,4.36,9.13,4.36,15.83v7.04h-13.4ZM1227.06,124.96c3.69,0,7.12-.89,10.3-2.68,3.18-1.79,5.78-4.41,7.79-7.87,2.01-3.46,3.02-7.71,3.02-12.73s-.98-9.3-2.93-12.82c-1.96-3.52-4.55-6.2-7.79-8.04-3.24-1.84-6.7-2.76-10.39-2.76s-7.15.92-10.39,2.76c-3.24,1.84-5.86,4.5-7.88,7.96-2.01,3.46-3.02,7.76-3.02,12.9s1,9.27,3.02,12.73c2.01,3.46,4.63,6.09,7.88,7.87,3.24,1.79,6.7,2.68,10.39,2.68Z" fill="#e1e1e0"/>
  <path d="M1303.79,33.48v16.25h-17.43v-16.25h17.43ZM1288.38,66.99h13.4v83.78h-13.4v-83.78Z" fill="#e1e1e0"/>
  <path d="M1399.13,119.27c-.78,6.93-2.99,12.88-6.62,17.84-3.63,4.97-8.18,8.77-13.66,11.39-5.47,2.62-11.39,3.94-17.76,3.94-7.15,0-13.85-1.73-20.11-5.19-6.26-3.46-11.31-8.46-15.16-15-3.85-6.53-5.78-14.27-5.78-23.21s1.93-16.67,5.78-23.21c3.85-6.53,8.91-11.59,15.16-15.16,6.25-3.57,12.96-5.36,20.11-5.36,6.37,0,12.26,1.34,17.68,4.02,5.42,2.68,9.94,6.48,13.57,11.39,3.63,4.92,5.89,10.78,6.79,17.59h-12.23c-1.9-6.37-5.19-11.36-9.88-15-4.69-3.63-10-5.45-15.92-5.45-4.8,0-9.33,1.23-13.57,3.69-4.25,2.46-7.65,6.01-10.22,10.64-2.57,4.64-3.85,10.25-3.85,16.84s1.28,12.04,3.85,16.67c2.57,4.64,5.95,8.15,10.14,10.56,4.19,2.4,8.74,3.6,13.66,3.6,6.81,0,12.37-1.81,16.67-5.45,4.3-3.63,7.4-8.68,9.3-15.16h12.06Z" fill="#e1e1e0"/>
  <path d="M1424.93,153.11c-2.79,0-5.08-.9-6.87-2.68-1.79-1.79-2.68-4.08-2.68-6.87s.89-5.08,2.68-6.87c1.79-1.79,4.08-2.68,6.87-2.68s5.08.9,6.87,2.68c1.79,1.79,2.68,4.08,2.68,6.87s-.9,5.08-2.68,6.87c-1.79,1.79-4.08,2.68-6.87,2.68Z" fill="#e1e1e0"/>)SVG"
        // Part 6: RIDE text with gradient
        R"SVG(  <!-- RIDE text with gradient matching knob outline -->
  <path d="M1457.43,150.77V42.36h73.55c8.38,0,15.67,1.15,21.87,3.43,6.2,2.29,11.03,5.86,14.49,10.72,3.46,4.86,5.19,11.09,5.19,18.68,0,5.14-.9,9.5-2.68,13.07-1.79,3.58-4.27,6.48-7.46,8.71-3.18,2.24-6.84,3.94-10.97,5.11-4.13,1.17-8.54,1.93-13.24,2.26l-2.35-1.34c7.82.11,14.24.75,19.27,1.93,5.03,1.17,8.8,3.3,11.31,6.37,2.51,3.07,3.77,7.62,3.77,13.66v25.8h-22.28v-24.29c0-4.13-.73-7.34-2.18-9.63-1.45-2.29-4.05-3.88-7.79-4.77-3.74-.89-9.13-1.34-16.17-1.34h-42.06v40.04h-22.28ZM1479.72,93.3h51.27c6.25,0,11.03-1.51,14.33-4.52,3.29-3.02,4.94-7.09,4.94-12.23s-1.65-8.65-4.94-11.23c-3.3-2.57-8.07-3.85-14.33-3.85h-51.27v31.83Z" fill="url(#rideGradient)"/>
  <path d="M1595.66,42.36h22.28v108.41h-22.28V42.36Z" fill="url(#rideGradient)"/>
  <path d="M1705.41,42.36c12.17,0,22.34,1.45,30.49,4.36,8.15,2.91,14.66,6.84,19.52,11.81,4.86,4.97,8.32,10.7,10.39,17.17,2.07,6.48,3.1,13.29,3.1,20.44s-1.15,14.02-3.44,20.61c-2.29,6.59-5.92,12.43-10.89,17.51-4.97,5.08-11.51,9.11-19.6,12.06-8.1,2.96-17.96,4.44-29.57,4.44h-61.99V42.36h61.99ZM1665.7,131.67h39.21c8.04,0,14.72-.98,20.02-2.93,5.3-1.95,9.52-4.61,12.65-7.96,3.13-3.35,5.36-7.15,6.7-11.39,1.34-4.24,2.01-8.65,2.01-13.24s-.67-8.94-2.01-13.07c-1.34-4.13-3.58-7.82-6.7-11.06-3.13-3.24-7.35-5.81-12.65-7.71-5.31-1.9-11.98-2.85-20.02-2.85h-39.21v70.2Z" fill="url(#rideGradient)"/>
  <path d="M1809.62,105.53v26.64h83.61v18.6h-105.89V42.36h105.72v18.6h-83.44v26.98h68.36v17.59h-68.36Z" fill="url(#rideGradient)"/>)SVG"
        // Part 7: Precision vocal leveling subtitle (first half)
        R"SVG(  <!-- precision vocal leveling subtitle -->
  <g opacity="0.7">
    <path d="M1362.79,207.92v-4.36h12.18c1.45,0,2.61-.39,3.46-1.17.85-.78,1.28-1.87,1.28-3.27s-.43-2.56-1.28-3.33c-.85-.77-2-.15-3.46-1.15h-11.52v21.93h-5.47v-26.62h16.5c1.48,0,2.87.18,4.18.54,1.3.36,2.44.91,3.42,1.65s1.74,1.69,2.28,2.84.82,2.54.82,4.16-.27,2.95-.82,4.07c-.55,1.13-1.31,2.04-2.28,2.74-.97.7-2.11,1.21-3.42,1.52-1.3.32-2.7.47-4.18.47h-11.69Z" fill="#6f7173"/>
    <path d="M1389.62,196.11h5.35v20.45h-5.35v-20.45ZM1402.83,200.51c-1.54,0-2.87.3-3.99.88-1.12.59-2.03,1.31-2.72,2.16-.69.85-1.17,1.67-1.44,2.47l-.04-2.26c.03-.33.14-.82.33-1.46.19-.64.48-1.34.86-2.1.38-.75.89-1.48,1.52-2.18.63-.7,1.4-1.27,2.3-1.71.91-.44,1.96-.66,3.17-.66v4.86Z" fill="#6f7173"/>
    <path d="M1424.76,209.77h5.18c-.22,1.4-.8,2.65-1.75,3.75-.95,1.1-2.23,1.96-3.85,2.59-1.62.63-3.59.95-5.93.95-2.61,0-4.91-.42-6.91-1.25s-3.57-2.05-4.69-3.64c-1.13-1.59-1.69-3.51-1.69-5.76s.55-4.18,1.65-5.78c1.1-1.6,2.63-2.83,4.59-3.68,1.96-.85,4.26-1.28,6.89-1.28s4.92.43,6.71,1.28c1.78.85,3.11,2.13,3.97,3.85.86,1.71,1.23,3.9,1.09,6.56h-19.51c.14,1.04.53,1.99,1.17,2.84.64.85,1.52,1.52,2.63,2.02,1.11.49,2.43.74,3.97.74,1.7,0,3.12-.29,4.26-.88,1.14-.59,1.87-1.35,2.2-2.28ZM1418.01,199.73c-1.97,0-3.58.43-4.81,1.3-1.23.86-2.03,1.93-2.39,3.19h13.91c-.14-1.37-.79-2.46-1.96-3.27-1.17-.81-2.75-1.21-4.75-1.21Z" fill="#6f7173"/>
    <path d="M1458.3,208.5c-.14,1.78-.71,3.32-1.71,4.61-1,1.29-2.38,2.27-4.14,2.94-1.76.67-3.84,1.01-6.25,1.01s-4.78-.4-6.77-1.21c-1.99-.81-3.55-2-4.69-3.58-1.14-1.58-1.71-3.53-1.71-5.86s.57-4.29,1.71-5.88c1.14-1.59,2.7-2.8,4.69-3.62,1.99-.82,4.25-1.23,6.77-1.23s4.49.34,6.21,1.01c1.73.67,3.09,1.64,4.09,2.9,1,1.26,1.58,2.78,1.75,4.57h-4.9c-.47-1.37-1.33-2.44-2.59-3.21-1.26-.77-2.79-1.15-4.57-1.15-1.43,0-2.72.25-3.89.74-1.17.49-2.1,1.23-2.8,2.2-.7.97-1.05,2.2-1.05,3.68s.34,2.67,1.03,3.64c.69.97,1.62,1.7,2.8,2.18s2.48.72,3.91.72c1.92,0,3.49-.38,4.71-1.15,1.22-.77,2.05-1.87,2.49-3.29h4.9Z" fill="#6f7173"/>
    <path d="M1468.38,187.14v4.69h-6.21v-4.69h6.21ZM1462.58,196.11h5.35v20.45h-5.35v-20.45Z" fill="#6f7173"/>
    <path d="M1472.45,209.77h4.94c.3.93.93,1.69,1.87,2.28.95.59,2.19.88,3.72.88,1.04,0,1.85-.1,2.43-.29s.97-.47,1.19-.84c.22-.37.33-.79.33-1.25,0-.58-.18-1.02-.54-1.34s-.91-.57-1.65-.76c-.74-.19-1.69-.37-2.84-.54-1.15-.19-2.26-.42-3.33-.7-1.07-.27-2.02-.64-2.84-1.09-.82-.45-1.48-1.03-1.96-1.73-.48-.7-.72-1.56-.72-2.57s.24-1.87.72-2.63c.48-.77,1.15-1.41,2.02-1.93.86-.52,1.88-.92,3.04-1.19,1.17-.27,2.42-.41,3.77-.41,2.03,0,3.72.29,5.06.88,1.34.59,2.35,1.41,3.02,2.47.67,1.06,1.01,2.27,1.01,3.64h-4.73c-.22-1.01-.66-1.75-1.32-2.2s-1.67-.68-3.04-.68-2.36.21-3.05.62c-.69.41-1.03.97-1.03,1.69,0,.58.21,1.02.64,1.34.42.32,1.06.57,1.89.76.84.19,1.89.4,3.15.62,1.07.22,2.09.47,3.07.74.97.27,1.84.62,2.61,1.05.77.43,1.38.99,1.83,1.69.45.7.68,1.58.68,2.65,0,1.32-.38,2.43-1.13,3.33-.75.91-1.83,1.6-3.23,2.08-1.4.48-3.07.72-5.02.72-1.73,0-3.22-.19-4.46-.56-1.25-.37-2.28-.85-3.11-1.44-.82-.59-1.46-1.22-1.91-1.89-.45-.67-.76-1.31-.93-1.91-.16-.6-.22-1.1-.16-1.48Z" fill="#6f7173"/>
    <path d="M1502.62,187.14v4.69h-6.21v-4.69h6.21ZM1496.81,196.11h5.35v20.45h-5.35v-20.45Z" fill="#6f7173"/>
    <path d="M1519.57,217.06c-2.58,0-4.85-.41-6.81-1.23-1.96-.82-3.49-2.02-4.59-3.6-1.1-1.58-1.65-3.52-1.65-5.82s.55-4.26,1.65-5.86c1.1-1.6,2.63-2.82,4.59-3.64,1.96-.82,4.23-1.23,6.81-1.23s4.83.41,6.77,1.23c1.93.82,3.45,2.04,4.55,3.64,1.1,1.6,1.65,3.56,1.65,5.86s-.55,4.25-1.65,5.82c-1.1,1.58-2.61,2.78-4.55,3.6-1.93.82-4.19,1.23-6.77,1.23ZM1519.57,212.94c1.43,0,2.71-.25,3.85-.76,1.14-.51,2.04-1.25,2.7-2.22.66-.97.99-2.16.99-3.56s-.33-2.6-.99-3.6-1.55-1.76-2.67-2.28c-1.12-.52-2.42-.78-3.87-.78s-2.72.26-3.87.78c-1.15.52-2.06,1.28-2.74,2.26-.67.99-1.01,2.2-1.01,3.62s.33,2.59.99,3.56c.66.97,1.56,1.71,2.72,2.22,1.15.51,2.45.76,3.91.76Z" fill="#6f7173"/>
    <path d="M1536.48,196.11h5.35v20.45h-5.35v-20.45ZM1551.34,195.66c1.26,0,2.43.16,3.5.49s2,.84,2.8,1.52c.8.69,1.41,1.56,1.85,2.61.44,1.06.66,2.31.66,3.77v12.51h-5.35v-11.56c0-1.73-.42-3-1.25-3.81-.84-.81-2.2-1.21-4.09-1.21-1.43,0-2.72.27-3.87.82-1.15.55-2.08,1.24-2.8,2.08-.71.84-1.12,1.71-1.23,2.61l-.04-2.1c.14-.96.45-1.89.95-2.8.49-.91,1.16-1.73,2-2.49.84-.75,1.83-1.35,2.98-1.79,1.15-.44,2.46-.66,3.91-.66Z" fill="#6f7173"/>
    <path d="M1585.04,216.56l-11.77-26.62h6.05l10.08,24.03h-1.89l10.04-24.03h5.97l-11.65,26.62h-6.83Z" fill="#6f7173"/>)SVG"
        // Part 8: Precision vocal leveling subtitle (second half) and closing
        R"SVG(    <path d="M1616.15,217.06c-2.58,0-4.85-.41-6.81-1.23-1.96-.82-3.49-2.02-4.59-3.6-1.1-1.58-1.65-3.52-1.65-5.82s.55-4.26,1.65-5.86c1.1-1.6,2.63-2.82,4.59-3.64,1.96-.82,4.23-1.23,6.81-1.23s4.83.41,6.77,1.23c1.93.82,3.45,2.04,4.55,3.64,1.1,1.6,1.65,3.56,1.65,5.86s-.55,4.25-1.65,5.82c-1.1,1.58-2.61,2.78-4.55,3.6-1.93.82-4.19,1.23-6.77,1.23ZM1616.15,212.94c1.43,0,2.71-.25,3.85-.76,1.14-.51,2.04-1.25,2.7-2.22.66-.97.99-2.16.99-3.56s-.33-2.6-.99-3.6-1.55-1.76-2.67-2.28c-1.12-.52-2.42-.78-3.87-.78s-2.72.26-3.87.78c-1.15.52-2.06,1.28-2.74,2.26-.67.99-1.01,2.2-1.01,3.62s.33,2.59.99,3.56c.66.97,1.56,1.71,2.72,2.22,1.15.51,2.45.76,3.91.76Z" fill="#6f7173"/>
    <path d="M1657.34,208.5c-.14,1.78-.71,3.32-1.71,4.61-1,1.29-2.38,2.27-4.14,2.94-1.76.67-3.84,1.01-6.25,1.01s-4.78-.4-6.77-1.21c-1.99-.81-3.55-2-4.69-3.58-1.14-1.58-1.71-3.53-1.71-5.86s.57-4.29,1.71-5.88c1.14-1.59,2.7-2.8,4.69-3.62,1.99-.82,4.25-1.23,6.77-1.23s4.49.34,6.21,1.01c1.73.67,3.09,1.64,4.09,2.9,1,1.26,1.58,2.78,1.75,4.57h-4.9c-.47-1.37-1.33-2.44-2.59-3.21-1.26-.77-2.79-1.15-4.57-1.15-1.43,0-2.72.25-3.89.74-1.17.49-2.1,1.23-2.8,2.2-.7.97-1.05,2.2-1.05,3.68s.34,2.67,1.03,3.64c.69.97,1.62,1.7,2.8,2.18s2.48.72,3.91.72c1.92,0,3.49-.38,4.71-1.15,1.22-.77,2.05-1.87,2.49-3.29h4.9Z" fill="#6f7173"/>
    <path d="M1671.25,217.02c-2.03,0-3.88-.45-5.54-1.34s-2.98-2.14-3.95-3.74c-.97-1.6-1.46-3.46-1.46-5.58s.49-4.05,1.48-5.64c.99-1.59,2.33-2.83,4.01-3.72,1.69-.89,3.6-1.34,5.74-1.34,2.36,0,4.26.47,5.7,1.42s2.49,2.23,3.15,3.85c.66,1.62.99,3.43.99,5.43,0,1.21-.19,2.44-.58,3.68-.38,1.25-.97,2.4-1.77,3.46-.8,1.06-1.84,1.91-3.13,2.55-1.29.64-2.84.97-4.65.97ZM1672.98,212.9c1.62,0,3.02-.27,4.2-.82,1.18-.55,2.08-1.32,2.72-2.3.63-.99.95-2.13.95-3.42,0-1.4-.32-2.58-.97-3.56-.65-.97-1.55-1.71-2.72-2.22-1.17-.51-2.56-.76-4.18-.76-2.28,0-4.05.6-5.31,1.79-1.26,1.19-1.89,2.78-1.89,4.75,0,1.32.3,2.46.91,3.44.6.97,1.45,1.74,2.53,2.28,1.08.55,2.34.82,3.77.82ZM1680.84,196.11h5.35v20.45h-4.98c-.14-1.07-.23-2.02-.29-2.84-.05-.82-.08-1.63-.08-2.43v-15.18Z" fill="#6f7173"/>
    <path d="M1691.46,187.88h5.35v28.68h-5.35v-28.68Z" fill="#6f7173"/>
    <path d="M1718.28,189.94v22.06h17.69v4.57h-23.17v-26.62h5.47Z" fill="#6f7173"/>
    <path d="M1758.2,209.77h5.18c-.22,1.4-.8,2.65-1.75,3.75-.95,1.1-2.23,1.96-3.85,2.59-1.62.63-3.59.95-5.93.95-2.61,0-4.91-.42-6.91-1.25s-3.57-2.05-4.69-3.64c-1.13-1.59-1.69-3.51-1.69-5.76s.55-4.18,1.65-5.78c1.1-1.6,2.63-2.83,4.59-3.68,1.96-.85,4.26-1.28,6.89-1.28s4.92.43,6.71,1.28c1.78.85,3.11,2.13,3.97,3.85.86,1.71,1.23,3.9,1.09,6.56h-19.51c.14,1.04.53,1.99,1.17,2.84.64.85,1.52,1.52,2.63,2.02,1.11.49,2.43.74,3.97.74,1.7,0,3.12-.29,4.26-.88,1.14-.59,1.87-1.35,2.2-2.28ZM1751.45,199.73c-1.97,0-3.58.43-4.81,1.3-1.23.86-2.03,1.93-2.39,3.19h13.91c-.14-1.37-.79-2.46-1.96-3.27-1.17-.81-2.75-1.21-4.75-1.21Z" fill="#6f7173"/>
    <path d="M1777.91,213.06h-1.93l7.49-16.95h5.84l-9.83,20.45h-5.06l-9.63-20.45h5.88l7.24,16.95Z" fill="#6f7173"/>
    <path d="M1810.13,209.77h5.18c-.22,1.4-.8,2.65-1.75,3.75-.95,1.1-2.23,1.96-3.85,2.59-1.62.63-3.59.95-5.93.95-2.61,0-4.91-.42-6.91-1.25s-3.57-2.05-4.69-3.64c-1.13-1.59-1.69-3.51-1.69-5.76s.55-4.18,1.65-5.78c1.1-1.6,2.63-2.83,4.59-3.68,1.96-.85,4.26-1.28,6.89-1.28s4.92.43,6.71,1.28c1.78.85,3.11,2.13,3.97,3.85.86,1.71,1.23,3.9,1.09,6.56h-19.51c.14,1.04.53,1.99,1.17,2.84.64.85,1.52,1.52,2.63,2.02,1.11.49,2.43.74,3.97.74,1.7,0,3.12-.29,4.26-.88,1.14-.59,1.87-1.35,2.2-2.28ZM1803.38,199.73c-1.97,0-3.58.43-4.81,1.3-1.23.86-2.03,1.93-2.39,3.19h13.91c-.14-1.37-.79-2.46-1.96-3.27-1.17-.81-2.75-1.21-4.75-1.21Z" fill="#6f7173"/>
    <path d="M1819.8,187.88h5.35v28.68h-5.35v-28.68Z" fill="#6f7173"/>
    <path d="M1836.67,187.14v4.69h-6.21v-4.69h6.21ZM1830.87,196.11h5.35v20.45h-5.35v-20.45Z" fill="#6f7173"/>
    <path d="M1841.57,196.11h5.35v20.45h-5.35v-20.45ZM1856.43,195.66c1.26,0,2.43.16,3.5.49s2,.84,2.8,1.52c.8.69,1.41,1.56,1.85,2.61.44,1.06.66,2.31.66,3.77v12.51h-5.35v-11.56c0-1.73-.42-3-1.25-3.81-.84-.81-2.2-1.21-4.09-1.21-1.43,0-2.72.27-3.87.82-1.15.55-2.08,1.24-2.8,2.08-.71.84-1.12,1.71-1.23,2.61l-.04-2.1c.14-.96.45-1.89.95-2.8.49-.91,1.16-1.73,2-2.49.84-.75,1.83-1.35,2.98-1.79,1.15-.44,2.46-.66,3.91-.66Z" fill="#6f7173"/>
    <path d="M1880.29,213.44c-2.22,0-4.18-.34-5.86-1.01-1.69-.67-3-1.67-3.93-2.98-.93-1.32-1.4-2.91-1.4-4.77s.45-3.43,1.36-4.77c.91-1.34,2.2-2.39,3.89-3.13,1.69-.74,3.67-1.11,5.95-1.11.63,0,1.24.04,1.83.12.59.08,1.17.19,1.75.33l10.33.04v4.03c-1.4.03-2.82-.14-4.26-.51-1.44-.37-2.71-.77-3.81-1.21l-.12-.29c.93.44,1.81.98,2.63,1.63.82.65,1.49,1.39,2,2.22.51.84.76,1.8.76,2.9,0,1.78-.45,3.31-1.36,4.59-.91,1.28-2.19,2.25-3.85,2.92-1.66.67-3.63,1.01-5.9,1.01ZM1887.17,225v-.99c0-1.26-.41-2.14-1.21-2.63-.81-.49-1.91-.74-3.31-.74h-6.38c-1.23,0-2.27-.1-3.11-.29-.84-.19-1.5-.47-2-.82-.49-.36-.85-.78-1.07-1.26s-.33-.99-.33-1.54c0-1.1.36-1.93,1.07-2.49.71-.56,1.67-.94,2.88-1.13,1.21-.19,2.54-.23,3.99-.12l2.59.45c-1.73.06-3.01.2-3.85.43-.84.23-1.26.69-1.26,1.38,0,.41.17.73.49.97.33.23.8.35,1.4.35h6.71c1.84,0,3.41.2,4.71.6,1.3.4,2.3,1.07,2.98,2.02.69.95,1.03,2.24,1.03,3.89v1.93h-5.35ZM1880.29,209.57c1.18,0,2.21-.19,3.11-.58.89-.38,1.58-.94,2.08-1.67.49-.73.74-1.6.74-2.61s-.25-1.93-.74-2.67-1.18-1.31-2.06-1.71c-.88-.4-1.92-.6-3.13-.6s-2.22.2-3.13.6c-.91.4-1.6.97-2.1,1.71s-.74,1.63-.74,2.67.25,1.89.74,2.61c.49.73,1.19,1.28,2.08,1.67.89.38,1.94.58,3.15.58Z" fill="#6f7173"/>
  </g>
</svg>)SVG";
        
        auto xmlDoc = juce::XmlDocument::parse(svgContent);
        if (xmlDoc != nullptr)
        {
            logoDrawable = juce::Drawable::createFromSVG(*xmlDoc);
        }
    }
    
    std::unique_ptr<juce::Drawable> logoDrawable;
};

//==============================================================================
// Speed icon component - displays turtle (slow) or rabbit (fast) icons
class SpeedIcon : public juce::Component
{
public:
    enum IconType { Turtle, Rabbit };
    
    SpeedIcon(IconType type) : iconType(type)
    {
        setInterceptsMouseClicks(false, false);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
        
        g.setColour(CustomLookAndFeel::getDimTextColour().withAlpha(0.45f));
        
        if (iconType == Turtle)
        {
            drawTurtle(g, bounds.getCentreX(), bounds.getCentreY(), size * 0.45f);
        }
        else
        {
            drawRabbit(g, bounds.getCentreX(), bounds.getCentreY(), size * 0.45f);
        }
    }
    
private:
    IconType iconType;
    
    void drawTurtle(juce::Graphics& g, float cx, float cy, float scale)
    {
        // Turtle - solid filled shapes
        juce::Path turtle;
        
        // Shell (oval)
        turtle.addEllipse(cx - scale * 0.8f, cy - scale * 0.4f, scale * 1.6f, scale * 1.0f);
        
        // Head
        turtle.addEllipse(cx + scale * 0.7f, cy - scale * 0.25f, scale * 0.5f, scale * 0.45f);
        
        // Front leg
        turtle.addEllipse(cx + scale * 0.3f, cy + scale * 0.35f, scale * 0.28f, scale * 0.35f);
        
        // Back leg  
        turtle.addEllipse(cx - scale * 0.5f, cy + scale * 0.35f, scale * 0.28f, scale * 0.35f);
        
        // Tail
        turtle.addEllipse(cx - scale * 0.95f, cy - scale * 0.1f, scale * 0.25f, scale * 0.18f);
        
        g.fillPath(turtle);
    }
    
    void drawRabbit(juce::Graphics& g, float cx, float cy, float scale)
    {
        // Rabbit - solid filled shapes
        juce::Path rabbit;
        
        // Body
        rabbit.addEllipse(cx - scale * 0.5f, cy - scale * 0.2f, scale * 1.2f, scale * 0.9f);
        
        // Head
        rabbit.addEllipse(cx + scale * 0.4f, cy - scale * 0.5f, scale * 0.6f, scale * 0.55f);
        
        // Left ear (tall)
        rabbit.addEllipse(cx + scale * 0.35f, cy - scale * 1.3f, scale * 0.2f, scale * 0.7f);
        
        // Right ear (tall)
        rabbit.addEllipse(cx + scale * 0.6f, cy - scale * 1.2f, scale * 0.2f, scale * 0.65f);
        
        // Tail (fluffy circle)
        rabbit.addEllipse(cx - scale * 0.7f, cy - scale * 0.1f, scale * 0.35f, scale * 0.35f);
        
        // Back leg
        rabbit.addEllipse(cx - scale * 0.2f, cy + scale * 0.3f, scale * 0.45f, scale * 0.35f);
        
        g.fillPath(rabbit);
    }
};

//==============================================================================
class VocalRiderAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        public juce::Timer
{
public:
    explicit VocalRiderAudioProcessorEditor(VocalRiderAudioProcessor&);
    ~VocalRiderAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseUp(const juce::MouseEvent& event) override;
    

private:
    // Window size presets (FabFilter-style)
    enum class WindowSize
    {
        Small,
        Medium,
        Large
    };
    
    void toggleAdvancedPanel();
    void updateAdvancedControls();
    void updateGainStats();
    void updateRangeLabel();
    juce::String getRangeTooltipText() const;
    void setWindowSize(WindowSize size);
    void setScale(int scalePercent);

    VocalRiderAudioProcessor& audioProcessor;
    CustomLookAndFeel customLookAndFeel;

    //==============================================================================
    // Header branding
    LogoComponent logoIcon;       // Waveform logo icon
    juce::Label brandLabel;       // "MBM Audio" in Syne font
    juce::Label titleLabel;       // "Magic Ride" in Space Grotesk
    juce::Label footerLabel;      // Bottom bar text
    ArrowButton prevPresetButton { false };  // Left arrow
    ArrowButton nextPresetButton { true };   // Right arrow
    juce::ComboBox presetComboBox;
    
    // A/B Compare and Undo/Redo
    ABCompareButton abCompareButton;
    UndoParamButton undoButton;
    RedoParamButton redoButton;
    
    GearButton advancedButton;
    
    // Bottom bar with resize button - more padding
    juce::Component bottomBar;
    ResizeButton resizeButton;
#if MAGICRIDE_LITE
    static constexpr int bottomBarHeight = 44;  // Extra room for upgrade strip
#else
    static constexpr int bottomBarHeight = 26;  // More padding
#endif
    
    // Automation mode selector (Off/Read/Touch/Latch/Write)
    juce::ComboBox automationModeComboBox;
    juce::Label automationLabel;  // "AUTO" label next to dropdown
    float automationPulsePhase = 0.0f;  // For pulsing animation when writing

    //==============================================================================
    // Main waveform display (fills most of the window)
    WaveformDisplay waveformDisplay;

    //==============================================================================
    // Bottom control panel (tab overlay)
    juce::Component controlPanel;
    
    // Animated tooltip for knob values
    AnimatedTooltip valueTooltip;
    
    // Main knobs: Target (large, center), Range and Speed (smaller, sides)
    TooltipSlider targetSlider;
    DualRangeKnob dualRangeKnob;
    RangeLockButton rangeLockButton;
    TooltipSlider rangeSlider;  // Hidden, kept for legacy APVTS attachment
    TooltipSlider boostRangeSlider;  // Hidden, APVTS-attached
    TooltipSlider cutRangeSlider;    // Hidden, APVTS-attached
    TooltipSlider speedSlider;
    
    juce::Label targetLabel;
    juce::Label rangeLabel;
    juce::Label speedLabel;
    
    // Speed icons (turtle = slow, rabbit = fast)
    SpeedIcon turtleIcon { SpeedIcon::Turtle };
    SpeedIcon rabbitIcon { SpeedIcon::Rabbit };
    
    // Mini gain meter
    MiniGainMeter miniGainMeter;
    juce::Label gainMeterLabel;
    
    // Toggle buttons in bottom bar (with icons)
    BottomBarIconButton naturalToggle { "NATURAL", BottomBarIconButton::Natural };
    BottomBarIconButton silenceToggle { "SMART SILENCE", BottomBarIconButton::Silence };
    BottomBarIconButton speedButton { "10s", BottomBarIconButton::Speed };
    BottomBarIconButton autoTargetButton { "AUTO-TARGET", BottomBarIconButton::Auto };
    
    // Help button (visual indicator - no longer toggle-based)
    HelpButton helpButton;
    AboutDialogPanel aboutDialog;
    
    // Hover-based status bar help (FabFilter-style bottom bar descriptions)
    juce::Component* currentHoveredComponent = nullptr;
    juce::String versionString;  // Cached version text
    void setStatusBarText(const juce::String& text);
    void clearStatusBarText();
    juce::String getShortHelpText(const juce::String& controlName);
    
    // Phrase indicator silence counter (UI-level timeout for natural mode indicator)
    int phraseIndicatorSilenceCount = 0;
    
    // A/B Compare state storage
    struct ParameterState {
        float target = -18.0f;
        float range = 6.0f;
        float boostRange = 6.0f;
        float cutRange = 6.0f;
        float speed = 50.0f;
        float attack = 10.0f;
        float release = 100.0f;
        float hold = 50.0f;
        float breathReduction = 0.0f;
        float transientPreservation = 50.0f;
        float outputTrim = 0.0f;
        float noiseFloor = -60.0f;
    };
    ParameterState stateA;
    ParameterState stateB;
    bool isStateB = false;
    
    // Undo/Redo history
    std::vector<ParameterState> undoHistory;
    int undoIndex = -1;
    void saveStateForUndo();
    void performUndo();
    void performRedo();
    ParameterState getCurrentState();
    void applyState(const ParameterState& state);
    
    // Help descriptions for each control
    juce::String getHelpText(const juce::String& controlName);
    
    // User preset system
    std::vector<VocalRiderAudioProcessor::Preset> cachedUserPresets;
    void rebuildPresetMenu();
    void showSavePresetDialog();
    std::vector<int> getNavigablePresetIds() const;
    
    // Owned save dialog overlay (prevents leak if editor destroyed while dialog open)
    std::unique_ptr<juce::Component> saveDialogOverlay;
    
    // Learn button for auto-analysis (with pulsing animation)
    LearnButton learnButton;
    int learnCountdown = 0;
    
    // Learning analysis data
    float learnMinDb = 0.0f;
    float learnMaxDb = -100.0f;
    float learnSumDb = 0.0f;
    int learnSampleCount = 0;
    
    // Displayed gain (decays to zero when no audio)
    float displayedGainDb = 0.0f;
    
    // Cached noise texture (regenerated only on resize, not every paint)
    juce::Image cachedNoiseTexture;
    
    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> targetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rangeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> boostRangeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutRangeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttachment;
    
    // Advanced parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> holdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> breathReductionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> transientPreservationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> naturalModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> smartSilenceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputTrimAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseFloorAttachment;

    //==============================================================================
    // Advanced panel (slide-down from top)
    bool advancedPanelVisible = false;
    AdvancedPanelComponent advancedPanel;
    
    juce::ComboBox lookAheadComboBox;
    juce::ComboBox detectionModeComboBox;
    
    // Sidechain controls in advanced panel
    juce::ToggleButton sidechainToggle { "Sidechain" };
    TooltipSlider sidechainOffsetSlider;
    juce::Label sidechainOffsetLabel;
    
    TooltipSlider attackSlider;
    TooltipSlider releaseSlider;
    TooltipSlider holdSlider;
    TooltipSlider breathReductionSlider;
    TooltipSlider transientPreservationSlider;
    TooltipSlider noiseFloorSlider;
    
    // Output Trim - FabFilter-style vertical fader with pill handle and purple glow
    class AdjustableGainMeter : public juce::Component
    {
    public:
        AdjustableGainMeter() = default;
        
        void setValue(float valueDb) 
        { 
            currentValueDb = juce::jlimit(-12.0f, 12.0f, valueDb);
            repaint();
        }
        
        float getValue() const { return currentValueDb; }
        
        std::function<void(float)> onValueChanged;
        std::function<void()> onMouseEnter;
        std::function<void()> onMouseExit;
        
        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            
            // Inset the drawing area so the handle never clips at the edges
            float handleOverhang = 4.0f;  // How much handle extends past bounds
            auto drawArea = bounds.reduced(handleOverhang, 0.0f);
            
            float trackWidth = juce::jmin(drawArea.getWidth(), 7.0f);
            float trackX = drawArea.getCentreX() - trackWidth / 2.0f;
            float trackPadding = 12.0f;  // Room for handle overshoot top/bottom
            float trackY = drawArea.getY() + trackPadding;
            float trackHeight = drawArea.getHeight() - trackPadding * 2.0f;
            auto trackBounds = juce::Rectangle<float>(trackX, trackY, trackWidth, trackHeight);
            
            // === TRACK BACKGROUND (dark recessed groove) ===
            g.setColour(juce::Colour(0xFF0F1114));  // Very dark
            g.fillRoundedRectangle(trackBounds, trackWidth / 2.0f);
            
            // Subtle inset shadow
            g.setColour(juce::Colour(0x30000000));
            g.drawRoundedRectangle(trackBounds, trackWidth / 2.0f, 1.0f);
            
            // === CENTER TICK (0 dB reference) ===
            float centerY = trackBounds.getCentreY();
            g.setColour(juce::Colour(0xFF4A4F58));
            g.fillRect(trackX - 2.0f, centerY - 0.5f, trackWidth + 4.0f, 1.0f);
            
            // === HANDLE POSITION ===
            float normalizedValue = currentValueDb / 12.0f;  // -1 to +1
            float handleY = centerY - normalizedValue * (trackHeight / 2.0f - 2.0f);
            
            // === PURPLE GLOW FILL from center (only when not at 0 dB) ===
            if (std::abs(currentValueDb) > 0.2f)
            {
                float fillTop = juce::jmin(centerY, handleY);
                float fillBottom = juce::jmax(centerY, handleY);
                float fillHeight = fillBottom - fillTop;
                
                auto fillBounds = juce::Rectangle<float>(trackX + 1.0f, fillTop, trackWidth - 2.0f, fillHeight);
                
                // Purple gradient fill (matches the other knob accents)
                juce::ColourGradient fillGrad;
                if (currentValueDb > 0)
                {
                    // Boost: bright purple at top (handle end), dimmer at center
                    fillGrad = juce::ColourGradient(
                        juce::Colour(0xFFB070E0), fillBounds.getCentreX(), fillBounds.getY(),
                        juce::Colour(0xFF7050A0), fillBounds.getCentreX(), fillBounds.getBottom(),
                        false
                    );
                }
                else
                {
                    // Cut: bright purple at bottom (handle end), dimmer at center
                    fillGrad = juce::ColourGradient(
                        juce::Colour(0xFF7050A0), fillBounds.getCentreX(), fillBounds.getY(),
                        juce::Colour(0xFFB070E0), fillBounds.getCentreX(), fillBounds.getBottom(),
                        false
                    );
                }
                g.setGradientFill(fillGrad);
                g.fillRoundedRectangle(fillBounds, (trackWidth - 2.0f) / 2.0f);
                
                // Outer glow on track
                g.setColour(juce::Colour(0xFFB070E0).withAlpha(0.12f));
                float glowExpand = 2.0f;
                g.fillRoundedRectangle(fillBounds.expanded(glowExpand, 0.0f), 
                                        (trackWidth - 2.0f) / 2.0f + glowExpand);
            }
            
            // === WIDE HORIZONTAL PILL HANDLE ===
            // Keep handle within the full bounds (not the reduced drawArea) so it extends nicely
            float handleWidth = bounds.getWidth() - 2.0f;  // Slightly inset from component edges
            float handleHeight = 12.0f;  // Capsule height
            float handleRadius = handleHeight / 2.0f;  // Fully rounded ends
            float handleX = bounds.getCentreX() - handleWidth / 2.0f;
            handleY = juce::jlimit(trackY, trackY + trackHeight - handleHeight, handleY - handleHeight / 2.0f);
            auto handleBounds = juce::Rectangle<float>(handleX, handleY, handleWidth, handleHeight);
            
            // Handle shadow
            g.setColour(juce::Colour(0x40000000));
            g.fillRoundedRectangle(handleBounds.translated(0.0f, 1.5f), handleRadius);
            
            // Handle body (dark gray with subtle glass effect)
            juce::ColourGradient handleGrad(
                juce::Colour(0xFF555B65), handleBounds.getCentreX(), handleBounds.getY(),
                juce::Colour(0xFF2A2D35), handleBounds.getCentreX(), handleBounds.getBottom(),
                false
            );
            g.setGradientFill(handleGrad);
            g.fillRoundedRectangle(handleBounds, handleRadius);
            
            // Glass highlight on top half
            auto topHalf = handleBounds.withHeight(handleHeight / 2.0f);
            g.setColour(juce::Colour(0x18FFFFFF));
            g.fillRoundedRectangle(topHalf, handleRadius);
            
            // Handle border
            g.setColour(juce::Colour(0xFF5A5F68));
            g.drawRoundedRectangle(handleBounds, handleRadius, 0.8f);
            
            // Single white center line
            g.setColour(juce::Colour(0xCCFFFFFF));
            float lineWidth = handleWidth * 0.3f;
            float lineCenterX = handleBounds.getCentreX();
            float lineCenterY = handleBounds.getCentreY();
            g.drawLine(lineCenterX - lineWidth / 2.0f, lineCenterY,
                       lineCenterX + lineWidth / 2.0f, lineCenterY, 1.0f);
        }
        
        void mouseDown(const juce::MouseEvent& e) override
        {
            // Option/Alt-click resets to 0 dB
            if (e.mods.isAltDown())
            {
                currentValueDb = 0.0f;
                repaint();
                if (onValueChanged)
                    onValueChanged(currentValueDb);
                return;
            }
            isDragging = true;
            mouseDrag(e);
        }
        
        void mouseDrag(const juce::MouseEvent& e) override
        {
            float trackPadding = 8.0f;
            float trackHeight = static_cast<float>(getHeight()) - trackPadding * 2.0f;
            float centerY = trackPadding + trackHeight / 2.0f;
            
            float y = static_cast<float>(e.y);
            // Map y position to dB: center = 0, top = +12, bottom = -12
            float normalized = (centerY - y) / (trackHeight / 2.0f);
            float newValue = juce::jlimit(-12.0f, 12.0f, normalized * 12.0f);
            
            // Snap to 0 when close
            if (std::abs(newValue) < 0.5f)
                newValue = 0.0f;
            
            if (std::abs(newValue - currentValueDb) > 0.05f)
            {
                currentValueDb = newValue;
                repaint();
                if (onValueChanged)
                    onValueChanged(currentValueDb);
            }
        }
        
        void mouseUp(const juce::MouseEvent&) override
        {
            isDragging = false;
        }
        
        void mouseDoubleClick(const juce::MouseEvent&) override
        {
            // Double-click resets to 0 dB
            currentValueDb = 0.0f;
            repaint();
            if (onValueChanged)
                onValueChanged(currentValueDb);
        }
        
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
        {
            float delta = wheel.deltaY * 2.0f;
            float newValue = juce::jlimit(-12.0f, 12.0f, currentValueDb + delta);
            if (std::abs(newValue - currentValueDb) > 0.05f)
            {
                currentValueDb = newValue;
                repaint();
                if (onValueChanged)
                    onValueChanged(currentValueDb);
            }
        }
        
        void mouseEnter(const juce::MouseEvent&) override
        {
            if (onMouseEnter) onMouseEnter();
        }
        
        void mouseExit(const juce::MouseEvent&) override
        {
            if (onMouseExit) onMouseExit();
        }
        
    private:
        float currentValueDb = 0.0f;
        bool isDragging = false;
    };
    
    AdjustableGainMeter outputTrimMeter;
    
    juce::Label advancedHeaderLabel;  // "ADVANCED SETTINGS" header
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label holdLabel;
    juce::Label breathLabel;
    juce::Label transientLabel;
    juce::Label noiseFloorLabel;
    juce::Label outputTrimLabel;
    
    // Output trim range labels (around the meter)
    juce::Label trimMinLabel;   // -12 dB at bottom
    juce::Label trimMidLabel;   // 0 dB in middle
    juce::Label trimMaxLabel;   // +12 dB at top

    WindowSize currentWindowSize = WindowSize::Medium;
    float uiScaleFactor = 1.0f;
    
    // Fixed window dimensions for each preset
    static constexpr int smallWidth = 550;
    static constexpr int smallHeight = 380;
    static constexpr int mediumWidth = 700;
    static constexpr int mediumHeight = 480;
    static constexpr int largeWidth = 900;
    static constexpr int largeHeight = 600;
    
    static constexpr int controlPanelHeight = 130;  // Extra room for even larger knobs

#if MAGICRIDE_LITE
    //==============================================================================
    // Lite version — upgrade prompt and locked control helpers
    juce::Label liteBadgeLabel;
    juce::Label upgradeStripLabel;
    juce::Label autoTargetBadgeLabel;
    float autoTargetBadgePhase = 0.0f;
    std::unique_ptr<juce::Component> liteUpgradeOverlay;

    void showLiteUpgradeDialog();
    void setupLiteRestrictions();

    static constexpr float liteLockedAlpha = 0.35f;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalRiderAudioProcessorEditor)
};
