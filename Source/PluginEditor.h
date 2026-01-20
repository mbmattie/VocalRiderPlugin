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
        int tooltipWidth = helpMode ? 160 : 48;  // Compact tooltips
        int tooltipHeight = helpMode ? 52 : 26;  // Shorter height
        
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
        startTimerHz(60);  // Higher framerate for smooth pulsing
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
            iconDrawable->replaceColour(juce::Colours::black, iconColour);
            iconDrawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
        }
    }
    
private:
    std::unique_ptr<juce::Drawable> iconDrawable;
};

//==============================================================================
// Logo Component - draws branding with proper padding, noise texture, and text rendering
class LogoComponent : public juce::Component
{
public:
    LogoComponent()
    {
        // Parse the waveform icon SVG only
        parseIconSVG();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Draw noise texture on the logo area background
        juce::Random rng(42);
        for (int y = 0; y < getHeight(); y += 2)
        {
            for (int x = 0; x < getWidth(); x += 2)
            {
                float noise = rng.nextFloat();
                if (noise > 0.75f)
                {
                    g.setColour(juce::Colours::white.withAlpha(0.03f));
                    g.fillRect(x, y, 1, 1);
                }
                else if (noise < 0.25f)
                {
                    g.setColour(juce::Colours::black.withAlpha(0.03f));
                    g.fillRect(x, y, 1, 1);
                }
            }
        }
        
        // More horizontal padding, less vertical padding
        float leftPad = 12.0f;
        float rightPad = 8.0f;
        float topPad = 2.0f;
        float bottomPad = 2.0f;
        
        auto contentBounds = bounds.reduced(leftPad, topPad);
        contentBounds.removeFromRight(rightPad - leftPad);
        contentBounds.removeFromBottom(bottomPad - topPad);
        
        // Subtle white/grey colors (toned down from pure white)
        juce::Colour subtleWhite(0xFFCCCDD0);  // Toned down white
        juce::Colour subtleGrey(0xFFA0A5B0);   // Mid grey for "magic"
        juce::Colour accentPurple(0xFFB48EFF); // Purple for "RIDE"
        juce::Colour dimGrey(0xFF6A6E78);      // Dim grey for subtitle
        
        float iconSize = contentBounds.getHeight() - 4.0f;
        
        // Draw waveform icon
        if (iconDrawable != nullptr)
        {
            auto iconBounds = juce::Rectangle<float>(contentBounds.getX(), contentBounds.getCentreY() - iconSize / 2, iconSize, iconSize);
            iconDrawable->replaceColour(juce::Colours::white, subtleWhite);
            iconDrawable->drawWithin(g, iconBounds, juce::RectanglePlacement::centred, 1.0f);
        }
        
        float textX = contentBounds.getX() + iconSize + 10.0f;
        float textWidth = contentBounds.getWidth() - iconSize - 10.0f;
        
        // MBM AUDIO - top left, small
        g.setColour(subtleWhite);
        g.setFont(CustomLookAndFeel::getBrandFont(10.0f, true));
        g.drawText("MBM AUDIO", static_cast<int>(textX), static_cast<int>(contentBounds.getY() + 4), 
                   static_cast<int>(textWidth), 12, juce::Justification::centredLeft);
        
        // magic.RIDE - main title, "magic." in grey, "RIDE" in purple
        float titleY = contentBounds.getY() + 16.0f;
        g.setFont(CustomLookAndFeel::getPluginFont(18.0f, true));
        
        // "magic." part
        g.setColour(subtleGrey);
        float magicWidth = g.getCurrentFont().getStringWidth("magic.");
        g.drawText("magic.", static_cast<int>(textX), static_cast<int>(titleY), 
                   static_cast<int>(magicWidth + 2), 22, juce::Justification::centredLeft);
        
        // "RIDE" part in purple
        g.setColour(accentPurple);
        g.drawText("RIDE", static_cast<int>(textX + magicWidth), static_cast<int>(titleY), 
                   static_cast<int>(textWidth - magicWidth), 22, juce::Justification::centredLeft);
        
        // precision vocal leveling - subtitle
        g.setColour(dimGrey);
        g.setFont(CustomLookAndFeel::getPluginFont(9.0f, false));
        g.drawText("precision vocal leveling", static_cast<int>(textX), static_cast<int>(contentBounds.getBottom() - 14), 
                   static_cast<int>(textWidth), 12, juce::Justification::centredLeft);
    }
    
private:
    void parseIconSVG()
    {
        // Simplified waveform icon SVG
        const char* iconSVG = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 186 186">
  <rect x="0" y="0" width="186" height="186" rx="10" fill="#15181E"/>
  <path d="M177.43,95.21c-.17,4.44-3.37,7.69-8.1,7.73-7.03.05-13.97.69-20.93-.06-3.25-.35-5.72-1.3-7.81-3.02-1.11-.91-1.97-2.1-2.55-3.42v.73c-2.21-3.26-3.16-1.97-3.46-1.06-.31.91-3.37,19.05-3.37,19.05-2.33,13.14-3.06,31.72-13.55,39.87-.4.31-5.88,2.83-10.68-3.07-.11-.14-.22-.27-.33-.42-.04-.05-.07-.1-.11-.14-3.89-5.4-6.43-18.71-7.04-23.05l-1.19-8.31c-.49-3.41-.86-6.32-1.31-9.74l-3.11-23.84c-1.42-10.91-2.78-21.44-4.85-32.25-.21-1.1-.94-1.7-1.81-1.66-.43.02-1.26.16-1.47,1.01-5.29,22.09-5.83,42.34-13.22,65.72-1.34,4.23-3.64,8.6-7.56,10.63-4.99,2.57-10.37-.62-12.93-5.17-4.97-8.85-8.08-25.23-9.68-35.61-.21-1.39-.5-2.27-1.8-2.31-1.58-.04-1.38.98-1.62,2.42l-8.34,48.71c-1.23,7.17-3.13,13.89-5.92,20.38-1.6,3.71-3.54,6.47-6.93,8.55-5.63,3.45-10.27-1.66-10.44-2.23-.48-1.61.09-2.43.94-3.75,11.07-17.12,7.22-27.02,11.43-46.93.57-2.69.74-5.47,1.16-8.24,2.18-14.09,3.86-27.85,8.55-41.28,1.63-4.66,5.62-9.53,10.76-9.56,11.57-.07,14.71,21.5,16.58,29.4.89,3.75,1.26,7.33,1.97,11.12.25,1.32,1.37,1.86,2.19,1.81,1.28-.08,1.68-.84,1.91-2.24l3.72-22.73c1.5-9.14,7.64-53.29,23.29-47.23,5.36,2.07,8.56,7.29,10.4,12.78,4.15,12.32,6.03,24.79,7.66,37.73l4.81,38.4c.15,1.22.43,2.36,1.63,2.36,1.16,0,1.48-1.14,1.67-2.37l2.41-16.03c1-6.69,2.73-14.84,6.44-20.49,2.45-3.74,8.91-7.01,14.67-3.86,6.77,3.71,6.47,14.28,12.38,14.2l16.91-.23c4.58-.06,8.83,3.31,8.66,7.67Z" fill="#fff"/>
</svg>)";
        if (auto xml = juce::XmlDocument::parse(iconSVG))
            iconDrawable = juce::Drawable::createFromSVG(*xml);
    }
    
    std::unique_ptr<juce::Drawable> iconDrawable;
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
