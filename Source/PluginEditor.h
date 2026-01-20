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

//==============================================================================
// Animated Value Tooltip - appears below knobs with fade animation
class AnimatedTooltip : public juce::Component, private juce::Timer
{
public:
    AnimatedTooltip()
    {
        setAlwaysOnTop(true);
        setInterceptsMouseClicks(false, false);
        startTimerHz(60);
    }
    
    void showValue(const juce::String& label, const juce::String& value, juce::Component* sourceComponent, bool showAbove = false, bool isHelpMode = false)
    {
        if (label != currentLabel || value != currentValue || sourceComponent != currentSource)
        {
            currentLabel = label;
            currentValue = value;
            currentSource = sourceComponent;
        }
        positionAbove = showAbove;
        helpMode = isHelpMode;
        targetOpacity = 1.0f;
        fadeSpeed = 0.25f;  // Fast fade-in
        setVisible(true);
        toFront(false);  // Bring tooltip to front
        updatePosition();  // Update position immediately
    }
    
    // Helper to count lines in text
    int countLines(const juce::String& text) const
    {
        return text.containsChar('\n') ? text.retainCharacters("\n").length() + 1 : 1;
    }
    
    void hideTooltip()
    {
        targetOpacity = 0.0f;
        fadeSpeed = 0.04f;  // Very slow fade-out
    }
    
    void updatePosition()
    {
        if (currentSource == nullptr || getParentComponent() == nullptr) return;
        
        // Calculate tooltip size based on content
        int tooltipWidth = helpMode ? 180 : 58;  // Narrower for non-help mode
        int tooltipHeight = helpMode ? 56 : 30;  // Taller for 2 lines
        
        // Get source position in screen coordinates, then convert to parent coordinates
        auto sourceScreenBounds = currentSource->getScreenBounds();
        auto parentScreenPos = getParentComponent()->getScreenPosition();
        
        int sourceX = sourceScreenBounds.getX() - parentScreenPos.x;
        int sourceY = sourceScreenBounds.getY() - parentScreenPos.y;
        int sourceW = sourceScreenBounds.getWidth();
        int sourceH = sourceScreenBounds.getHeight();
        
        int x, y;
        
        if (helpMode)
        {
            // Position to the RIGHT of the component for help mode
            x = sourceX + sourceW + 8;
            y = sourceY + sourceH / 2 - tooltipHeight / 2;
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
            auto labelBounds = bounds.removeFromTop(14.0f);
            g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(currentOpacity));
            g.setFont(CustomLookAndFeel::getPluginFont(10.0f, true));
            g.drawText(currentLabel.toUpperCase(), labelBounds.reduced(4.0f, 0.0f), juce::Justification::centredLeft);
            
            // Help text (smaller, wrapping)
            g.setColour(CustomLookAndFeel::getTextColour().withAlpha(0.85f * currentOpacity));
            g.setFont(CustomLookAndFeel::getPluginFont(9.0f, false));
            g.drawFittedText(currentValue, bounds.reduced(4.0f, 2.0f).toNearestInt(), 
                            juce::Justification::topLeft, 3);
        }
        else
        {
            // Normal mode: Label (top, bright grey, ALL CAPS)
            auto labelBounds = bounds.removeFromTop(bounds.getHeight() * 0.45f);
            g.setColour(juce::Colour(0xFFB0B0B0).withAlpha(currentOpacity));  // Bright grey
            g.setFont(CustomLookAndFeel::getPluginFont(9.0f, true));
            g.drawText(currentLabel.toUpperCase(), labelBounds, juce::Justification::centred);
            
            // Value (bottom, white for emphasis)
            g.setColour(juce::Colours::white.withAlpha(currentOpacity));
            g.setFont(CustomLookAndFeel::getPluginFont(11.0f, true));
            g.drawText(currentValue, bounds, juce::Justification::centred);
        }
    }
    
    bool isShowing() const { return currentOpacity > 0.01f; }
    bool isHelpMode() const { return helpMode; }
    
private:
    bool helpMode = false;
    void timerCallback() override
    {
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
        }
    }
    
    juce::String currentLabel;
    juce::String currentValue;
    juce::Component* currentSource = nullptr;
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
        startTimerHz(60);
    }
    
    void setTargetOpacity(float opacity)
    {
        targetOpacity = opacity;
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
            // Slower fade: 0.08 for fade-out, 0.12 for fade-in
            float speed = (targetOpacity > currentOpacity) ? 0.12f : 0.06f;
            currentOpacity += (targetOpacity - currentOpacity) * speed;
            if (onAnimationUpdate)
                onAnimationUpdate();
        }
        else if (std::abs(currentOpacity - targetOpacity) > 0.001f)
        {
            currentOpacity = targetOpacity;
            if (onAnimationUpdate)
                onAnimationUpdate();
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
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        
        // Background
        g.setColour(CustomLookAndFeel::getSurfaceDarkColour());
        g.fillRoundedRectangle(bounds, 3.0f);
        
        // Border
        g.setColour(CustomLookAndFeel::getBorderColour().withAlpha(0.5f));
        g.drawRoundedRectangle(bounds, 3.0f, 0.5f);
        
        // Center line (0 dB)
        float centerY = bounds.getCentreY();
        g.setColour(CustomLookAndFeel::getDimTextColour().withAlpha(0.3f));
        g.fillRect(bounds.getX() + 2.0f, centerY - 0.5f, bounds.getWidth() - 4.0f, 1.0f);
        
        // Gain bar
        if (rangeDb > 0.1f && std::abs(currentGainDb) > 0.1f)
        {
            float normalizedGain = juce::jlimit(-1.0f, 1.0f, currentGainDb / rangeDb);
            float barHeight = std::abs(normalizedGain) * (bounds.getHeight() / 2.0f - 2.0f);
            
            if (currentGainDb > 0)
            {
                // Boost - draw upward from center (cyan)
                g.setColour(CustomLookAndFeel::getGainBoostColour().withAlpha(0.85f));
                g.fillRoundedRectangle(bounds.getX() + 2.0f, centerY - barHeight, 
                                        bounds.getWidth() - 4.0f, barHeight, 2.0f);
            }
            else
            {
                // Cut - draw downward from center (purple)
                g.setColour(CustomLookAndFeel::getGainCutColour().withAlpha(0.85f));
                g.fillRoundedRectangle(bounds.getX() + 2.0f, centerY,
                                        bounds.getWidth() - 4.0f, barHeight, 2.0f);
            }
        }
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
            iconDrawable->replaceColour(juce::Colours::black, iconColour);
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
    
    void clicked() override
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel(&getLookAndFeel());
        menu.addSectionHeader("Size");
        menu.addItem(1, "Small", true, currentSize == 0);
        menu.addItem(2, "Medium", true, currentSize == 1);
        menu.addItem(3, "Large", true, currentSize == 2);
        
        menu.addSeparator();
        menu.addSectionHeader("Scale");
        menu.addItem(10, "100%", true, currentScale == 100);
        menu.addItem(11, "125%", true, currentScale == 125);
        menu.addItem(12, "150%", true, currentScale == 150);
        menu.addItem(13, "175%", true, currentScale == 175);
        menu.addItem(14, "200%", true, currentScale == 200);
        
        menu.showMenuAsync(juce::PopupMenu::Options()
            .withTargetComponent(this)
            .withMinimumWidth(100),
            [this](int result) {
                if (result >= 1 && result <= 3 && onSizeSelected)
                {
                    currentSize = result - 1;
                    onSizeSelected(result - 1);
                }
                else if (result >= 10 && result <= 14 && onScaleSelected)
                {
                    int scales[] = { 100, 125, 150, 175, 200 };
                    currentScale = scales[result - 10];
                    onScaleSelected(currentScale);
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
        
        juce::Colour iconColour = shouldDrawButtonAsHighlighted 
            ? CustomLookAndFeel::getAccentColour().brighter(0.3f)
            : (getToggleState() ? CustomLookAndFeel::getAccentColour() : CustomLookAndFeel::getDimTextColour());
        
        if (iconDrawable != nullptr)
        {
            iconDrawable->replaceColour(juce::Colours::black, iconColour);
            iconDrawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
        }
    }
    
private:
    std::unique_ptr<juce::Drawable> iconDrawable;
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
            iconDrawable->replaceColour(juce::Colours::black, col);
            iconDrawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
        }
    }
    
private:
    std::unique_ptr<juce::Drawable> iconDrawable;
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
        
        juce::Colour col = shouldDrawButtonAsHighlighted ? CustomLookAndFeel::getTextColour() : CustomLookAndFeel::getDimTextColour();
        
        if (iconDrawable != nullptr)
        {
            iconDrawable->replaceColour(juce::Colours::black, col);
            iconDrawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
        }
    }
    
private:
    std::unique_ptr<juce::Drawable> iconDrawable;
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
        startTimerHz(30);
    }
    
    // Update the displayed label text
    void setLabel(const juce::String& newLabel) { buttonLabel = newLabel; repaint(); }
    
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
        
        // Draw SVG icon based on type
        std::unique_ptr<juce::Drawable> iconDrawable;
        
        switch (iconType)
        {
            case Natural:
            {
                // Leaf icon SVG - replaced currentColor with #000000 for color replacement
                const char* leafSVG = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><g fill="none" stroke="#000000" stroke-linecap="round" stroke-linejoin="round" stroke-width="2"><path d="M5 21c.5-4.5 2.5-8 7-10"/><path d="M9 18c6.218 0 10.5-3.288 11-12V4h-4.014c-9 0-11.986 4-12 9c0 1 0 3 2 5h3z"/></g></svg>)";
                if (auto xml = juce::XmlDocument::parse(leafSVG))
                    iconDrawable = juce::Drawable::createFromSVG(*xml);
                break;
            }
            case Silence:
            {
                // Speaker icon SVG (will be flipped upside down when drawing) - replaced currentColor with #000000
                const char* speakerSVG = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256"><path fill="#000000" fill-rule="evenodd" d="m25.856 160.231l-.105 15.5l21.52.091s10.258.899 17.47-1.033c7.21-1.932 5.846.283 11.266-6.664s2.59-5.662 5.685-15.063s5.482-18.859 5.482-18.859l5.383-19.944l6.906-17.103l4.976-5.127l11.477-.617l16.25.1l18.06-.211l6.094.464l5.18 7.82l4.468 14.117l4.062 14.727l4.04 14.208l4.367 14.726s2.14 7.77 6.398 11.62c4.257 3.851 13.406 6.293 13.406 6.293l20.313.3l13.651-.105l.502-15.884l-16.709.405l-17.022-.72l-2.563-.717l-5.742-17.059l-6.713-23.695l-5.777-19.032s-1.753-7.91-6.973-13.517s-8.141-8.08-15.059-10.146s-10.042-.902-21.245-.803c-11.202.099-17.124.015-22.405.19c-5.281.174-10.457.896-10.457.896l-9.33 3.96l-8.1 11.07l-5.023 12.188l-5.23 18.891l-3.999 14.727l-4.511 13.608l-3.282 9.445l-17.84.793l-18.87.16z"/></svg>)";
                if (auto xml = juce::XmlDocument::parse(speakerSVG))
                    iconDrawable = juce::Drawable::createFromSVG(*xml);
                break;
            }
            case Speed:
            {
                // Speed/refresh icon SVG - replaced currentColor with #000000
                const char* speedSVG = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path fill="#000000" d="m20.38 8.57l-1.23 1.85a8 8 0 0 1-.22 7.58H5.07A8 8 0 0 1 15.58 6.85l1.85-1.23A10 10 0 0 0 3.35 19a2 2 0 0 0 1.72 1h13.85a2 2 0 0 0 1.74-1a10 10 0 0 0-.27-10.44z"/><path fill="#000000" d="M10.59 15.41a2 2 0 0 0 2.83 0l5.66-8.49l-8.49 5.66a2 2 0 0 0 0 2.83"/></svg>)";
                if (auto xml = juce::XmlDocument::parse(speedSVG))
                    iconDrawable = juce::Drawable::createFromSVG(*xml);
                break;
            }
            case Auto:
            {
                // Auto icon SVG - replaced currentColor with #000000 for color replacement
                const char* autoSVG = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path fill="#000000" d="m19 9l-1.25-2.75L15 5l2.75-1.25L19 1l1.25 2.75L23 5l-2.75 1.25Zm0 14l-1.25-2.75L15 19l2.75-1.25L19 15l1.25 2.75L23 19l-2.75 1.25ZM4.8 16L8 7h2l3.2 9h-1.9l-.7-2H7.4l-.7 2Zm3.05-3.35h2.3L9 9ZM9 18q2.5 0 4.25-1.75T15 12q0-2.5-1.75-4.25T9 6Q6.5 6 4.75 7.75T3 12q0 2.5 1.75 4.25T9 18Zm0 2q-3.35 0-5.675-2.325Q1 15.35 1 12q0-3.35 2.325-5.675Q5.65 4 9 4q3.35 0 5.675 2.325Q17 8.65 17 12q0 3.35-2.325 5.675Q12.35 20 9 20Z"/></svg>)";
                if (auto xml = juce::XmlDocument::parse(autoSVG))
                    iconDrawable = juce::Drawable::createFromSVG(*xml);
                break;
            }
        }
        
        // === Always center icon + text together, no movement ===
        auto fullBounds = getLocalBounds().toFloat().reduced(1.0f);
        
        // Highlight background when on (draw first, behind everything)
        if (isOn && iconType != Speed)
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
        
        if (iconDrawable != nullptr)
        {
            iconDrawable->replaceColour(juce::Colours::black, iconColour);
            
            // Flip speaker icon upside down
            if (iconType == Silence)
            {
                g.saveState();
                float cy = centeredIconArea.getCentreY();
                auto transform = juce::AffineTransform::verticalFlip(cy * 2.0f);
                g.addTransform(transform);
                iconDrawable->drawWithin(g, centeredIconArea, juce::RectanglePlacement::centred, 1.0f);
                g.restoreState();
            }
            else
            {
                iconDrawable->drawWithin(g, centeredIconArea, juce::RectanglePlacement::centred, 1.0f);
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
        float targetGlow = getToggleState() ? 1.0f : 0.0f;
        if (std::abs(glowAmount - targetGlow) > 0.01f)
        {
            glowAmount += (targetGlow - glowAmount) * 0.2f;
            repaint();
        }
    }
    
    juce::String buttonLabel;
    IconType iconType;
    float glowAmount = 0.0f;
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
            iconDrawable->replaceColour(juce::Colours::black, iconColour);
            iconDrawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
        }
    }
    
private:
    std::unique_ptr<juce::Drawable> iconDrawable;
};

//==============================================================================
// Logo Component - draws full branding using exact SVG paths from user's exported file
class LogoComponent : public juce::Component
{
public:
    LogoComponent()
    {
        // Parse the full SVG logo with all paths
        parseSVG();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Draw the full SVG logo scaled to fit
        if (logoDrawable != nullptr)
        {
            logoDrawable->drawWithin(g, bounds.reduced(2.0f), juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid, 1.0f);
        }
    }
    
private:
    void parseSVG()
    {
        // Full SVG logo - cropped to just show the content we need (icon + MBM AUDIO + magic.RIDE + subtitle)
        // The original SVG is 1553.89 x 721.91 but main content is in top ~190px
        const char* svgContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1450 170">
  <path d="M0,11.47v165.17c0,5.28,4.28,9.55,9.55,9.55h165.17c5.28,0,9.55-4.28,9.55-9.55V11.47c0-5.28-4.28-9.55-9.55-9.55H9.55C4.28,1.92,0,6.2,0,11.47ZM177.43,95.21c-.17,4.44-3.37,7.69-8.1,7.73-7.03.05-13.97.69-20.93-.06-3.25-.35-5.72-1.3-7.81-3.02-1.11-.91-1.97-2.1-2.55-3.42,0,0,0,0,0,0v.73c-2.21-3.26-3.16-1.97-3.46-1.06-.31.91-3.37,19.05-3.37,19.05-2.33,13.14-3.06,31.72-13.55,39.87-.4.31-5.88,2.83-10.68-3.07-.11-.14-.22-.27-.33-.42-.04-.05-.07-.1-.11-.14h0c-3.89-5.4-6.43-18.71-7.04-23.05l-1.19-8.31c-.49-3.41-.86-6.32-1.31-9.74l-3.11-23.84c-1.42-10.91-2.78-21.44-4.85-32.25-.21-1.1-.94-1.7-1.81-1.66-.43.02-1.26.16-1.47,1.01-5.29,22.09-5.83,42.34-13.22,65.72-1.34,4.23-3.64,8.6-7.56,10.63-4.99,2.57-10.37-.62-12.93-5.17-4.97-8.85-8.08-25.23-9.68-35.61-.21-1.39-.5-2.27-1.8-2.31-1.58-.04-1.38.98-1.62,2.42l-8.34,48.71c-1.23,7.17-3.13,13.89-5.92,20.38-1.6,3.71-3.54,6.47-6.93,8.55-5.63,3.45-10.27-1.66-10.44-2.23-.48-1.61.09-2.43.94-3.75,11.07-17.12,7.22-27.02,11.43-46.93.57-2.69.74-5.47,1.16-8.24,2.18-14.09,3.86-27.85,8.55-41.28,1.63-4.66,5.62-9.53,10.76-9.56,11.57-.07,14.71,21.5,16.58,29.4.89,3.75,1.26,7.33,1.97,11.12.25,1.32,1.37,1.86,2.19,1.81,1.28-.08,1.68-.84,1.91-2.24l3.72-22.73c1.5-9.14,7.64-53.29,23.29-47.23,5.36,2.07,8.56,7.29,10.4,12.78,4.15,12.32,6.03,24.79,7.66,37.73l4.81,38.4c.15,1.22.43,2.36,1.63,2.36,1.16,0,1.48-1.14,1.67-2.37l2.41-16.03c1-6.69,2.73-14.84,6.44-20.49,2.45-3.74,8.91-7.01,14.67-3.86,6.77,3.71,6.47,14.28,12.38,14.2l16.91-.23c4.58-.06,8.83,3.31,8.66,7.67Z" fill="#fff"/>
  <path d="M324.51,79.56h-20.42L378.42,2.78h34.04v87.13h-34.04V30.55l10.21,4.22-53.78,55.14h-40.84l-53.91-55,10.35-4.22v59.22h-34.04V2.78h34.04l74.06,76.78Z" fill="#fff"/>
  <path d="M431.51,89.91V2.78h85.77c17.61,0,30.58,1.73,38.94,5.17,8.35,3.45,12.53,9.44,12.53,17.97,0,5.45-1.79,9.62-5.38,12.52-3.59,2.91-8.71,4.92-15.38,6.06-6.67,1.14-14.68,1.84-24.03,2.11l1.09-1.09c6.81,0,13.11.18,18.92.54,5.81.36,10.89,1.18,15.25,2.45,4.36,1.27,7.76,3.22,10.21,5.85,2.45,2.63,3.68,6.26,3.68,10.89,0,5.63-2,10.26-5.99,13.89-4,3.63-9.67,6.33-17.02,8.1-7.35,1.77-16.02,2.65-26,2.65h-92.58ZM465.55,39.81h52.82c4.81,0,8.28-.45,10.42-1.36,2.13-.91,3.2-2.36,3.2-4.36s-1.07-3.56-3.2-4.42c-2.13-.86-5.61-1.29-10.42-1.29h-52.82v11.44ZM465.55,64.31h58.54c4.81,0,8.28-.45,10.41-1.36,2.13-.91,3.2-2.36,3.2-4.36,0-1.36-.48-2.45-1.43-3.27s-2.43-1.43-4.42-1.84c-2-.41-4.58-.61-7.76-.61h-58.54v11.44Z" fill="#fff"/>
  <path d="M694.81,79.56h-20.42L748.72,2.78h34.04v87.13h-34.04V30.55l10.21,4.22-53.78,55.14h-40.84l-53.91-55,10.35-4.22v59.22h-34.04V2.78h34.04l74.06,76.78Z" fill="#fff"/>
  <path d="M207.96,183.52l46.19-68.91h29.93l46.62,68.91h-29.61l-39.95-61.69h16.04l-39.62,61.69h-29.61ZM232.07,173.08v-16.15h73.65v16.15h-73.65Z" fill="#fff"/>
  <path d="M430.72,144.22v-29.61h26.92v34.45c0,6.1-1.13,11.29-3.39,15.56-2.26,4.27-5.4,7.79-9.42,10.55-4.02,2.76-8.65,4.9-13.89,6.41-5.24,1.51-10.86,2.57-16.85,3.18-5.99.61-12.08.92-18.25.92-6.53,0-12.85-.31-18.95-.92-6.1-.61-11.75-1.67-16.96-3.18-5.21-1.51-9.74-3.64-13.62-6.41-3.88-2.76-6.91-6.28-9.1-10.55-2.19-4.27-3.28-9.46-3.28-15.56v-34.45h26.92v29.61c0,5.74,1.4,10.12,4.2,13.14s6.8,5.08,12,6.19c5.2,1.11,11.47,1.67,18.79,1.67s13.28-.56,18.52-1.67c5.24-1.11,9.28-3.18,12.11-6.19,2.83-3.01,4.25-7.39,4.25-13.14Z" fill="#fff"/>
  <path d="M550.24,114.61c9.69,0,17.69.93,24.01,2.8,6.32,1.87,11.27,4.41,14.86,7.64s6.12,6.91,7.59,11.04c1.47,4.13,2.21,8.45,2.21,12.97s-.83,8.85-2.48,12.97c-1.65,4.13-4.36,7.81-8.13,11.04s-8.76,5.78-14.97,7.64c-6.21,1.87-13.91,2.8-23.09,2.8h-77.52v-68.91h77.52ZM499.63,161.99h49.53c4.02,0,7.43-.27,10.23-.81,2.8-.54,5.04-1.35,6.73-2.42,1.69-1.08,2.91-2.42,3.66-4.04.75-1.62,1.13-3.5,1.13-5.65s-.38-4.04-1.13-5.65c-.75-1.61-1.97-2.96-3.66-4.04-1.69-1.08-3.93-1.88-6.73-2.42-2.8-.54-6.21-.81-10.23-.81h-49.53v25.84Z" fill="#fff"/>
  <path d="M614.51,114.61h26.92v68.91h-26.92v-68.91Z" fill="#fff"/>
  <path d="M722.18,185.67c-15.65,0-28.25-1.38-37.79-4.15-9.55-2.76-16.49-6.85-20.83-12.27-4.34-5.42-6.51-12.15-6.51-20.19s2.17-14.77,6.51-20.19c4.34-5.42,11.29-9.51,20.83-12.27,9.54-2.76,22.14-4.15,37.79-4.15s28.25,1.38,37.79,4.15c9.55,2.76,16.49,6.86,20.83,12.27,4.34,5.42,6.51,12.15,6.51,20.19s-2.17,14.77-6.51,20.19c-4.34,5.42-11.29,9.51-20.83,12.27-9.55,2.76-22.14,4.15-37.79,4.15ZM722.18,165.22c7.39,0,13.78-.45,19.17-1.35,5.38-.9,9.55-2.51,12.49-4.85,2.94-2.33,4.41-5.65,4.41-9.96s-1.47-7.63-4.41-9.96c-2.94-2.33-7.11-3.95-12.49-4.85-5.38-.9-11.77-1.35-19.17-1.35s-13.87.45-19.43,1.35c-5.56.9-9.91,2.51-13.03,4.85-3.12,2.33-4.68,5.65-4.68,9.96s1.56,7.63,4.68,9.96c3.12,2.33,7.46,3.95,13.03,4.85,5.56.9,12.04,1.35,19.43,1.35Z" fill="#fff"/>
  <path d="M881.99,88.27V23.13h7.69l.06,14.86h-.87c.89-3.65,2.33-6.64,4.31-8.97s4.3-4.08,6.96-5.24c2.66-1.17,5.39-1.75,8.19-1.75,4.78,0,8.71,1.48,11.8,4.43,3.09,2.95,5.06,6.91,5.91,11.89h-1.28c.78-3.38,2.17-6.28,4.2-8.71,2.02-2.43,4.49-4.3,7.4-5.62,2.91-1.32,6.1-1.98,9.56-1.98,3.88,0,7.35.85,10.4,2.53,3.05,1.69,5.46,4.2,7.22,7.52,1.77,3.32,2.65,7.43,2.65,12.32v43.87h-7.87v-43.64c0-5.24-1.4-9.11-4.2-11.6-2.8-2.49-6.23-3.73-10.31-3.73-3.11,0-5.85.66-8.22,1.98-2.37,1.32-4.22,3.19-5.56,5.59-1.34,2.41-2.01,5.22-2.01,8.45v42.94h-7.87v-44.34c0-4.51-1.31-8.07-3.93-10.69s-6.01-3.93-10.17-3.93c-2.91,0-5.59.68-8.04,2.04-2.45,1.36-4.41,3.3-5.88,5.83-1.48,2.53-2.21,5.59-2.21,9.21v41.89h-7.92Z" fill="#e6e7e8"/>
  <path d="M1003.24,89.73c-3.96,0-7.57-.76-10.81-2.27-3.24-1.51-5.83-3.75-7.75-6.7-1.92-2.95-2.88-6.53-2.88-10.72,0-3.22.6-5.93,1.81-8.13,1.2-2.19,2.93-4,5.19-5.42,2.25-1.42,4.9-2.53,7.95-3.35,3.05-.82,6.42-1.46,10.11-1.92,3.61-.47,6.68-.86,9.21-1.19,2.52-.33,4.46-.86,5.8-1.6,1.34-.74,2.01-1.92,2.01-3.55v-1.52c0-2.91-.59-5.42-1.78-7.52-1.19-2.1-2.91-3.71-5.19-4.84-2.27-1.13-5.04-1.69-8.3-1.69-3.07,0-5.77.48-8.1,1.43-2.33.95-4.26,2.19-5.8,3.73-1.54,1.53-2.69,3.21-3.47,5.04l-7.57-2.56c1.44-3.53,3.46-6.37,6.06-8.51,2.6-2.14,5.54-3.7,8.8-4.69,3.26-.99,6.56-1.49,9.91-1.49,2.56,0,5.2.34,7.92,1.02,2.72.68,5.24,1.83,7.57,3.44,2.33,1.61,4.21,3.83,5.65,6.64,1.44,2.82,2.16,6.38,2.16,10.69v44.22h-7.75v-10.31h-.58c-.93,1.98-2.31,3.87-4.14,5.68-1.83,1.81-4.07,3.27-6.73,4.4-2.66,1.13-5.76,1.69-9.29,1.69ZM1004.41,82.68c3.96,0,7.4-.87,10.31-2.62,2.91-1.75,5.17-4.1,6.76-7.05,1.59-2.95,2.39-6.2,2.39-9.73v-9.26c-.58.54-1.54,1.03-2.86,1.46-1.32.43-2.83.8-4.52,1.11-1.69.31-3.39.58-5.1.82-1.71.23-3.21.43-4.49.58-3.61.43-6.71,1.13-9.29,2.1-2.58.97-4.54,2.29-5.88,3.96-1.34,1.67-2.01,3.79-2.01,6.35s.64,4.82,1.92,6.64c1.28,1.83,3.03,3.22,5.24,4.19,2.21.97,4.72,1.46,7.52,1.46Z" fill="#e6e7e8"/>
  <path d="M1076.6,114.03c-4.43,0-8.32-.57-11.68-1.72-3.36-1.15-6.19-2.71-8.48-4.69-2.29-1.98-4.08-4.25-5.36-6.82l6.53-3.96c.93,1.67,2.19,3.29,3.76,4.87,1.57,1.57,3.6,2.85,6.09,3.85,2.49.99,5.54,1.49,9.15,1.49,5.75,0,10.37-1.43,13.87-4.28,3.5-2.85,5.24-7.27,5.24-13.26v-14.62h-.76c-.86,2.02-2.11,4.01-3.76,5.97-1.65,1.96-3.8,3.57-6.44,4.84-2.64,1.26-5.91,1.89-9.79,1.89-5.17,0-9.76-1.28-13.78-3.85-4.02-2.56-7.18-6.23-9.47-11.01-2.29-4.78-3.44-10.49-3.44-17.13s1.13-12.46,3.38-17.45c2.25-4.99,5.4-8.89,9.44-11.68,4.04-2.8,8.72-4.19,14.04-4.19,3.96,0,7.26.7,9.91,2.1,2.64,1.4,4.77,3.14,6.38,5.21,1.61,2.08,2.84,4.11,3.7,6.09h.82v-12.53h7.69v66.71c0,5.52-1.17,10.06-3.5,13.63-2.33,3.57-5.54,6.22-9.61,7.95-4.08,1.73-8.72,2.59-13.92,2.59ZM1076.13,80.46c4.16,0,7.7-1,10.63-3,2.93-2,5.18-4.87,6.73-8.62,1.55-3.75,2.33-8.22,2.33-13.43s-.77-9.53-2.3-13.43c-1.54-3.9-3.76-6.97-6.67-9.21-2.91-2.23-6.49-3.35-10.72-3.35s-7.89,1.16-10.84,3.47c-2.95,2.31-5.2,5.43-6.73,9.35-1.54,3.92-2.3,8.31-2.3,13.17s.78,9.28,2.33,13.05c1.55,3.77,3.81,6.71,6.76,8.83,2.95,2.12,6.54,3.18,10.78,3.18Z" fill="#e6e7e8"/>
  <path d="M1127.58,11.07c-1.55,0-2.9-.54-4.05-1.63-1.15-1.09-1.72-2.39-1.72-3.9s.57-2.86,1.72-3.93c1.14-1.07,2.5-1.6,4.05-1.6s2.95.53,4.08,1.6c1.13,1.07,1.69,2.38,1.69,3.93s-.56,2.82-1.69,3.9c-1.13,1.09-2.49,1.63-4.08,1.63ZM1123.56,88.27V23.13h7.92v65.14h-7.92Z" fill="#e6e7e8"/>
  <path d="M1176.82,89.5c-5.67,0-10.66-1.42-14.97-4.25-4.31-2.83-7.69-6.78-10.14-11.83-2.45-5.05-3.67-10.86-3.67-17.42s1.22-12.56,3.67-17.62c2.45-5.07,5.83-9.02,10.14-11.86,4.31-2.83,9.3-4.25,14.97-4.25,3.3,0,6.33.49,9.09,1.46,2.76.97,5.2,2.26,7.34,3.87,2.14,1.61,3.91,3.4,5.33,5.36,1.42,1.96,2.46,3.93,3.12,5.91l-7.52,2.33c-.35-1.2-.99-2.49-1.92-3.85-.93-1.36-2.13-2.64-3.58-3.85-1.46-1.2-3.18-2.19-5.16-2.94-1.98-.76-4.21-1.14-6.7-1.14-4.43,0-8.19,1.2-11.27,3.61-3.09,2.41-5.45,5.61-7.08,9.61-1.63,4-2.45,8.45-2.45,13.34s.82,9.23,2.45,13.23c1.63,4,3.99,7.2,7.08,9.58,3.09,2.39,6.85,3.58,11.27,3.58,2.52,0,4.8-.39,6.82-1.17,2.02-.78,3.78-1.78,5.27-3s2.71-2.54,3.64-3.96c.93-1.42,1.57-2.75,1.92-3.99l7.46,2.33c-.62,2.02-1.65,4.02-3.09,6-1.44,1.98-3.24,3.8-5.42,5.45-2.18,1.65-4.66,2.97-7.46,3.96s-5.85,1.49-9.15,1.49Z" fill="#e6e7e8"/>
  <path d="M1226.28,89.15c-2.41,0-4.45-.84-6.12-2.51-1.67-1.67-2.51-3.71-2.51-6.12s.83-4.38,2.51-6.03c1.67-1.65,3.71-2.48,6.12-2.48s4.39.83,6.06,2.48c1.67,1.65,2.51,3.66,2.51,6.03s-.84,4.45-2.51,6.12c-1.67,1.67-3.69,2.51-6.06,2.51Z" fill="#fff"/>
  <path d="M1249.94,88.27V1.46h37.41c6.45,0,12.09,1.18,16.93,3.52,4.84,2.35,8.6,5.73,11.3,10.14,2.7,4.41,4.05,9.68,4.05,15.82s-1.38,11.46-4.14,15.73c-2.76,4.27-6.62,7.5-11.6,9.67-4.97,2.18-10.76,3.26-17.36,3.26h-22.37v-18.3h17.6c2.8,0,5.19-.35,7.17-1.05,1.98-.7,3.5-1.81,4.54-3.32,1.05-1.51,1.57-3.52,1.57-6s-.52-4.51-1.57-6.06c-1.05-1.55-2.56-2.7-4.54-3.44-1.98-.74-4.37-1.11-7.17-1.11h-8.27v67.94h-23.54ZM1296.9,88.27l-21.21-39.85h25.05l21.79,39.85h-25.64Z" fill="#00aeef"/>
  <path d="M1353.19,1.46v86.82h-23.54V1.46h23.54Z" fill="#00aeef"/>
  <path d="M1387.91,1.46v86.82h-23.54V1.46h23.54ZM1397.82,88.27h-23.77v-20.04h22.84c4.31,0,7.99-.68,11.04-2.04,3.05-1.36,5.37-3.74,6.96-7.14,1.59-3.4,2.39-8.11,2.39-14.13s-.82-10.79-2.45-14.19c-1.63-3.4-4.01-5.79-7.14-7.17-3.13-1.38-6.96-2.07-11.51-2.07h-22.49V1.46h23.77c8.93,0,16.64,1.74,23.13,5.21,6.49,3.48,11.5,8.46,15.03,14.94,3.53,6.49,5.3,14.26,5.3,23.31s-1.76,16.76-5.27,23.25c-3.52,6.49-8.5,11.46-14.94,14.92-6.45,3.46-14.08,5.19-22.9,5.19Z" fill="#00aeef"/>
  <path d="M1450.61,88.27V1.46h62.58v18.99h-39.04v14.68h35.78v18.53h-35.78v15.62h38.8v18.99h-62.34Z" fill="#00aeef"/>
  <path d="M885.98,166.8v-33.07h3.7v3.89h.67c.61-1.18,1.59-2.24,2.95-3.17,1.36-.93,3.26-1.39,5.69-1.39,2.02,0,3.86.49,5.54,1.46,1.68.98,3.02,2.36,4.03,4.15,1.01,1.79,1.51,3.95,1.51,6.48v.62c0,2.5-.5,4.66-1.49,6.48-.99,1.82-2.33,3.22-4.01,4.18s-3.54,1.44-5.59,1.44c-1.63,0-3.02-.22-4.15-.65s-2.06-.98-2.76-1.66c-.7-.67-1.25-1.36-1.63-2.06h-.67v13.3h-3.79ZM897.98,154.51c2.43,0,4.41-.78,5.93-2.33,1.52-1.55,2.28-3.72,2.28-6.5v-.43c0-2.78-.76-4.95-2.28-6.5-1.52-1.55-3.5-2.33-5.93-2.33s-4.38.78-5.93,2.33c-1.55,1.55-2.33,3.72-2.33,6.5v.43c0,2.78.78,4.95,2.33,6.5,1.55,1.55,3.53,2.33,5.93,2.33Z" fill="#6f7173"/>
  <path d="M916.89,157.2v-23.47h3.7v2.88h.67c.42-1.02,1.06-1.78,1.92-2.26.86-.48,2-.72,3.41-.72h2.78v3.46h-3.02c-1.7,0-3.06.47-4.1,1.42s-1.56,2.41-1.56,4.39v14.3h-3.79Z" fill="#6f7173"/>
  <path d="M944.87,157.87c-2.37,0-4.45-.5-6.24-1.51-1.79-1.01-3.18-2.42-4.18-4.25-.99-1.82-1.49-3.94-1.49-6.34v-.58c0-2.43.5-4.56,1.49-6.38.99-1.82,2.37-3.24,4.13-4.25,1.76-1.01,3.78-1.51,6.05-1.51s4.16.47,5.86,1.42c1.7.94,3.02,2.3,3.98,4.06.96,1.76,1.44,3.82,1.44,6.19v1.73h-19.15c.1,2.53.91,4.5,2.45,5.93,1.54,1.42,3.46,2.14,5.76,2.14,2.02,0,3.57-.46,4.66-1.39,1.09-.93,1.92-2.03,2.5-3.31l3.26,1.58c-.48.99-1.14,1.98-1.99,2.98-.85.99-1.95,1.82-3.31,2.5-1.36.67-3.1,1.01-5.21,1.01ZM936.81,143.32h15.26c-.13-2.18-.87-3.87-2.23-5.09-1.36-1.22-3.1-1.82-5.21-1.82s-3.9.61-5.28,1.82c-1.38,1.22-2.22,2.91-2.54,5.09Z" fill="#6f7173"/>
  <path d="M973.34,157.87c-2.27,0-4.31-.49-6.12-1.46-1.81-.98-3.23-2.38-4.27-4.2-1.04-1.82-1.56-3.98-1.56-6.48v-.53c0-2.53.52-4.7,1.56-6.5,1.04-1.81,2.46-3.2,4.27-4.18,1.81-.98,3.85-1.46,6.12-1.46s4.15.42,5.74,1.25c1.58.83,2.83,1.94,3.74,3.31.91,1.38,1.5,2.85,1.75,4.42l-3.7.77c-.16-1.15-.54-2.21-1.13-3.17-.59-.96-1.42-1.73-2.47-2.3-1.06-.58-2.35-.86-3.89-.86s-2.97.35-4.2,1.06c-1.23.7-2.21,1.71-2.93,3.02-.72,1.31-1.08,2.88-1.08,4.7v.43c0,1.82.36,3.39,1.08,4.7.72,1.31,1.7,2.32,2.93,3.02,1.23.7,2.63,1.06,4.2,1.06,2.34,0,4.12-.61,5.35-1.82,1.23-1.22,1.99-2.72,2.28-4.51l3.74.82c-.35,1.54-.99,2.99-1.92,4.37-.93,1.38-2.18,2.48-3.77,3.31-1.58.83-3.5,1.25-5.74,1.25Z" fill="#6f7173"/>
  <path d="M993.16,130.08c-.83,0-1.53-.27-2.09-.82-.56-.54-.84-1.23-.84-2.06s.28-1.57.84-2.11c.56-.54,1.25-.82,2.09-.82s1.52.27,2.06.82c.54.54.82,1.25.82,2.11s-.27,1.52-.82,2.06c-.54.54-1.23.82-2.06.82ZM991.24,157.2v-23.47h3.79v23.47h-3.79Z" fill="#6f7173"/>
  <path d="M1012.46,157.87c-2.91,0-5.37-.67-7.37-2.02-2-1.34-3.21-3.47-3.62-6.38l3.6-.82c.26,1.54.74,2.74,1.44,3.62.7.88,1.58,1.5,2.62,1.87,1.04.37,2.15.55,3.34.55,1.76,0,3.15-.35,4.18-1.06,1.02-.7,1.54-1.65,1.54-2.83s-.49-2.07-1.46-2.57c-.98-.5-2.31-.9-4.01-1.22l-1.97-.34c-1.5-.26-2.88-.66-4.13-1.2-1.25-.54-2.24-1.28-2.98-2.21-.74-.93-1.1-2.1-1.1-3.5,0-2.11.82-3.76,2.45-4.94,1.63-1.18,3.79-1.78,6.48-1.78s4.81.6,6.46,1.8c1.65,1.2,2.71,2.89,3.19,5.06l-3.55.91c-.29-1.7-.98-2.89-2.06-3.58-1.09-.69-2.43-1.03-4.03-1.03s-2.86.3-3.79.89c-.93.59-1.39,1.46-1.39,2.62s.44,1.94,1.32,2.47c.88.53,2.06.92,3.53,1.18l1.97.34c1.66.29,3.16.68,4.49,1.18,1.33.5,2.38,1.21,3.17,2.14.78.93,1.18,2.14,1.18,3.65,0,2.3-.86,4.08-2.57,5.33-1.71,1.25-4.01,1.87-6.89,1.87Z" fill="#6f7173"/>
  <path d="M1030.22,130.08c-.83,0-1.53-.27-2.09-.82-.56-.54-.84-1.23-.84-2.06s.28-1.57.84-2.11c.56-.54,1.25-.82,2.09-.82s1.52.27,2.06.82c.54.54.82,1.25.82,2.11s-.27,1.52-.82,2.06c-.54.54-1.23.82-2.06.82ZM1028.3,157.2v-23.47h3.79v23.47h-3.79Z" fill="#6f7173"/>
  <path d="M1050.9,157.87c-2.37,0-4.46-.5-6.26-1.49-1.81-.99-3.22-2.39-4.22-4.2-1.01-1.81-1.51-3.94-1.51-6.41v-.62c0-2.43.5-4.56,1.51-6.38,1.01-1.82,2.42-3.23,4.22-4.22,1.81-.99,3.9-1.49,6.26-1.49s4.46.5,6.26,1.49c1.81.99,3.22,2.4,4.22,4.22,1.01,1.82,1.51,3.95,1.51,6.38v.62c0,2.46-.5,4.6-1.51,6.41-1.01,1.81-2.42,3.21-4.22,4.2-1.81.99-3.9,1.49-6.26,1.49ZM1050.9,154.46c2.5,0,4.49-.79,5.98-2.38,1.49-1.58,2.23-3.72,2.23-6.41v-.43c0-2.69-.74-4.82-2.23-6.41-1.49-1.58-3.48-2.38-5.98-2.38s-4.45.79-5.95,2.38-2.26,3.72-2.26,6.41v.43c0,2.69.75,4.82,2.26,6.41s3.49,2.38,5.95,2.38Z" fill="#6f7173"/>
  <path d="M1069.72,157.2v-23.47h3.7v3.98h.67c.51-1.12,1.38-2.14,2.62-3.05,1.23-.91,3.05-1.37,5.45-1.37,1.76,0,3.34.37,4.73,1.1,1.39.74,2.5,1.82,3.34,3.26.83,1.44,1.25,3.22,1.25,5.33v14.21h-3.79v-13.92c0-2.34-.58-4.03-1.75-5.09-1.17-1.06-2.74-1.58-4.73-1.58-2.27,0-4.12.74-5.54,2.21-1.42,1.47-2.14,3.66-2.14,6.58v11.81h-3.79Z" fill="#6f7173"/>
  <path d="M1117.43,157.2l-8.4-23.47h4.03l7.06,20.64h.67l7.06-20.64h4.03l-8.4,23.47h-6.05Z" fill="#6f7173"/>
  <path d="M1147.24,157.87c-2.37,0-4.46-.5-6.26-1.49-1.81-.99-3.22-2.39-4.22-4.2-1.01-1.81-1.51-3.94-1.51-6.41v-.62c0-2.43.5-4.56,1.51-6.38,1.01-1.82,2.42-3.23,4.22-4.22,1.81-.99,3.9-1.49,6.26-1.49s4.46.5,6.26,1.49c1.81.99,3.22,2.4,4.22,4.22,1.01,1.82,1.51,3.95,1.51,6.38v.62c0,2.46-.5,4.6-1.51,6.41-1.01,1.81-2.42,3.21-4.22,4.2-1.81.99-3.9,1.49-6.26,1.49ZM1147.24,154.46c2.5,0,4.49-.79,5.98-2.38,1.49-1.58,2.23-3.72,2.23-6.41v-.43c0-2.69-.74-4.82-2.23-6.41-1.49-1.58-3.48-2.38-5.98-2.38s-4.45.79-5.95,2.38-2.26,3.72-2.26,6.41v.43c0,2.69.75,4.82,2.26,6.41s3.49,2.38,5.95,2.38Z" fill="#6f7173"/>
  <path d="M1176.86,157.87c-2.27,0-4.31-.49-6.12-1.46-1.81-.98-3.23-2.38-4.27-4.2-1.04-1.82-1.56-3.98-1.56-6.48v-.53c0-2.53.52-4.7,1.56-6.5,1.04-1.81,2.46-3.2,4.27-4.18,1.81-.98,3.85-1.46,6.12-1.46s4.15.42,5.74,1.25c1.58.83,2.83,1.94,3.74,3.31.91,1.38,1.5,2.85,1.75,4.42l-3.7.77c-.16-1.15-.54-2.21-1.13-3.17-.59-.96-1.42-1.73-2.47-2.3-1.06-.58-2.35-.86-3.89-.86s-2.97.35-4.2,1.06c-1.23.7-2.21,1.71-2.93,3.02-.72,1.31-1.08,2.88-1.08,4.7v.43c0,1.82.36,3.39,1.08,4.7.72,1.31,1.7,2.32,2.93,3.02,1.23.7,2.63,1.06,4.2,1.06,2.34,0,4.12-.61,5.35-1.82,1.23-1.22,1.99-2.72,2.28-4.51l3.74.82c-.35,1.54-.99,2.99-1.92,4.37-.93,1.38-2.18,2.48-3.77,3.31-1.58.83-3.5,1.25-5.74,1.25Z" fill="#6f7173"/>
  <path d="M1222.36,157.2v-33.6h3.79v33.6h-3.79Z" fill="#6f7173"/>
  <path d="M1246.45,157.2v-33.6h3.79v33.6h-3.79Z" fill="#6f7173"/>
  <path d="M1268.97,157.87c-2.37,0-4.45-.5-6.24-1.51-1.79-1.01-3.18-2.42-4.18-4.25-.99-1.82-1.49-3.94-1.49-6.34v-.58c0-2.43.5-4.56,1.49-6.38.99-1.82,2.37-3.24,4.13-4.25,1.76-1.01,3.78-1.51,6.05-1.51s4.16.47,5.86,1.42c1.7.94,3.02,2.3,3.98,4.06.96,1.76,1.44,3.82,1.44,6.19v1.73h-19.15c.1,2.53.91,4.5,2.45,5.93,1.54,1.42,3.46,2.14,5.76,2.14,2.02,0,3.57-.46,4.66-1.39,1.09-.93,1.92-2.03,2.5-3.31l3.26,1.58c-.48.99-1.14,1.98-1.99,2.98-.85.99-1.95,1.82-3.31,2.5-1.36.67-3.1,1.01-5.21,1.01ZM1260.9,143.32h15.26c-.13-2.18-.87-3.87-2.23-5.09-1.36-1.22-3.1-1.82-5.21-1.82s-3.9.61-5.28,1.82c-1.38,1.22-2.22,2.91-2.54,5.09Z" fill="#6f7173"/>
  <path d="M1339,157.2v-33.6h3.79v33.6h-3.79Z" fill="#6f7173"/>
  <path d="M1362.52,157.2v-23.47h3.7v3.98h.67c.51-1.12,1.38-2.14,2.62-3.05,1.23-.91,3.05-1.37,5.45-1.37,1.76,0,3.34.37,4.73,1.1,1.39.74,2.5,1.82,3.34,3.26.83,1.44,1.25,3.22,1.25,5.33v14.21h-3.79v-13.92c0-2.34-.58-4.03-1.75-5.09-1.17-1.06-2.74-1.58-4.73-1.58-2.27,0-4.12.74-5.54,2.21-1.42,1.47-2.14,3.66-2.14,6.58v11.81h-3.79Z" fill="#6f7173"/>
</svg>)";
        
        auto xmlDoc = juce::XmlDocument::parse(svgContent);
        if (xmlDoc != nullptr)
        {
            logoDrawable = juce::Drawable::createFromSVG(*xmlDoc);
        }
    }
    
    std::unique_ptr<juce::Drawable> logoDrawable;
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
    void setWindowSize(WindowSize size);
    void setScale(int scalePercent);

    #if JucePlugin_Build_Standalone
    void loadFileButtonClicked();
    void playStopButtonClicked();
    void updatePlaybackUI();
    #endif

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
    static constexpr int bottomBarHeight = 26;  // More padding

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
    TooltipSlider rangeSlider;
    TooltipSlider speedSlider;
    
    juce::Label targetLabel;
    juce::Label rangeLabel;
    juce::Label speedLabel;
    
    // Mini gain meter
    MiniGainMeter miniGainMeter;
    juce::Label gainMeterLabel;
    
    // Toggle buttons in bottom bar (with icons)
    BottomBarIconButton naturalToggle { "NATURAL", BottomBarIconButton::Natural };
    BottomBarIconButton silenceToggle { "SMART SILENCE", BottomBarIconButton::Silence };
    BottomBarIconButton speedButton { "10s", BottomBarIconButton::Speed };
    BottomBarIconButton autoTargetButton { "AUTO-TARGET", BottomBarIconButton::Auto };
    
    // Help button (question mark toggle)
    HelpButton helpButton;
    bool helpModeActive = false;
    
    // A/B Compare state storage
    struct ParameterState {
        float target = -18.0f;
        float range = 6.0f;
        float speed = 50.0f;
        float attack = 10.0f;
        float release = 100.0f;
        float hold = 50.0f;
        float breathReduction = 0.0f;
        float transientPreservation = 50.0f;
        float outputTrim = 0.0f;
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
    
    // Learn button for auto-analysis (with pulsing animation)
    LearnButton learnButton;
    int learnCountdown = 0;
    
    // Learning analysis data
    float learnMinDb = 0.0f;
    float learnMaxDb = -100.0f;
    float learnSumDb = 0.0f;
    int learnSampleCount = 0;
    
    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> targetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rangeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttachment;

    //==============================================================================
    // Advanced panel (slide-down from top)
    bool advancedPanelVisible = false;
    AdvancedPanelComponent advancedPanel;
    
    juce::ComboBox lookAheadComboBox;
    juce::ComboBox detectionModeComboBox;
    
    
    TooltipSlider attackSlider;
    TooltipSlider releaseSlider;
    TooltipSlider holdSlider;
    TooltipSlider breathReductionSlider;
    TooltipSlider transientPreservationSlider;
    // Output Trim - adjustable MiniGainMeter-style fader
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
            auto bounds = getLocalBounds().toFloat().reduced(1.0f);
            
            // Background
            g.setColour(CustomLookAndFeel::getSurfaceDarkColour());
            g.fillRoundedRectangle(bounds, 3.0f);
            
            // Border
            g.setColour(CustomLookAndFeel::getBorderColour().withAlpha(0.5f));
            g.drawRoundedRectangle(bounds, 3.0f, 0.5f);
            
            // Center line (0 dB)
            float centerY = bounds.getCentreY();
            g.setColour(CustomLookAndFeel::getDimTextColour().withAlpha(0.3f));
            g.fillRect(bounds.getX() + 2.0f, centerY - 0.5f, bounds.getWidth() - 4.0f, 1.0f);
            
            // Value bar (similar to MiniGainMeter)
            float normalizedValue = currentValueDb / 12.0f;  // -1 to +1
            float barHeight = std::abs(normalizedValue) * (bounds.getHeight() / 2.0f - 2.0f);
            
            if (currentValueDb > 0.1f)
            {
                // Positive - draw upward from center
                g.setColour(CustomLookAndFeel::getGainBoostColour().withAlpha(0.85f));
                g.fillRoundedRectangle(bounds.getX() + 2.0f, centerY - barHeight, 
                                        bounds.getWidth() - 4.0f, barHeight, 2.0f);
            }
            else if (currentValueDb < -0.1f)
            {
                // Negative - draw downward from center
                g.setColour(CustomLookAndFeel::getGainCutColour().withAlpha(0.85f));
                g.fillRoundedRectangle(bounds.getX() + 2.0f, centerY,
                                        bounds.getWidth() - 4.0f, barHeight, 2.0f);
            }
        }
        
        void mouseDown(const juce::MouseEvent& e) override
        {
            mouseDrag(e);
        }
        
        void mouseDrag(const juce::MouseEvent& e) override
        {
            auto bounds = getLocalBounds().toFloat();
            float y = static_cast<float>(e.y);
            float normalized = 1.0f - (y / bounds.getHeight());  // 0 at bottom, 1 at top
            normalized = juce::jlimit(0.0f, 1.0f, normalized);
            
            // Map to -12 to +12 dB (center at 0.5 = 0 dB)
            float newValue = (normalized - 0.5f) * 24.0f;  // -12 to +12
            
            if (std::abs(newValue - currentValueDb) > 0.1f)
            {
                currentValueDb = newValue;
                repaint();
                if (onValueChanged)
                    onValueChanged(currentValueDb);
            }
        }
        
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
        {
            float delta = wheel.deltaY * 2.0f;  // Adjust sensitivity
            float newValue = juce::jlimit(-12.0f, 12.0f, currentValueDb - delta);
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
    };
    
    AdjustableGainMeter outputTrimMeter;
    
    juce::Label advancedHeaderLabel;  // "ADVANCED SETTINGS" header
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label holdLabel;
    juce::Label breathLabel;
    juce::Label transientLabel;
    juce::Label outputTrimLabel;
    
    // Output trim range labels (around the meter)
    juce::Label trimMinLabel;   // -12 dB at bottom
    juce::Label trimMidLabel;   // 0 dB in middle
    juce::Label trimMaxLabel;   // +12 dB at top

    //==============================================================================
    #if JucePlugin_Build_Standalone
    juce::TextButton loadFileButton { "📁" };
    juce::TextButton playStopButton { "▶" };
    juce::TextButton rewindButton { "⟲" };
    juce::Label fileNameLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool wasPlaying = false;
    #endif

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalRiderAudioProcessorEditor)
};
