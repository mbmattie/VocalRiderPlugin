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
    
    void showValue(const juce::String& label, const juce::String& value, juce::Component* sourceComponent)
    {
        if (label != currentLabel || value != currentValue || sourceComponent != currentSource)
        {
            currentLabel = label;
            currentValue = value;
            currentSource = sourceComponent;
        }
        targetOpacity = 1.0f;
        fadeSpeed = 0.25f;  // Fast fade-in
        setVisible(true);
        toFront(false);  // Bring tooltip to front
        updatePosition();  // Update position immediately
    }
    
    void hideTooltip()
    {
        targetOpacity = 0.0f;
        fadeSpeed = 0.08f;  // Slow fade-out
    }
    
    void updatePosition()
    {
        if (currentSource == nullptr || getParentComponent() == nullptr) return;
        
        int tooltipWidth = 75;
        int tooltipHeight = 32;  // Taller for two lines
        
        // Position centered below the source component
        auto sourceBounds = getParentComponent()->getLocalArea(currentSource, currentSource->getLocalBounds());
        int x = sourceBounds.getCentreX() - tooltipWidth / 2;
        int y = sourceBounds.getBottom() + 4;
        
        // Make sure tooltip stays within parent bounds
        auto parentBounds = getParentComponent()->getLocalBounds();
        if (x < 4) x = 4;
        if (x + tooltipWidth > parentBounds.getWidth() - 4) x = parentBounds.getWidth() - tooltipWidth - 4;
        if (y + tooltipHeight > parentBounds.getHeight() - 4) y = sourceBounds.getY() - tooltipHeight - 4;
        
        setBounds(x, y, tooltipWidth, tooltipHeight);
    }
    
    void paint(juce::Graphics& g) override
    {
        if (currentOpacity < 0.01f) return;
        
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        
        // Background with rounded corners
        g.setColour(CustomLookAndFeel::getSurfaceColour().withAlpha(0.95f * currentOpacity));
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Border
        g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(0.5f * currentOpacity));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        
        // Label (top, white, ALL CAPS)
        auto labelBounds = bounds.removeFromTop(bounds.getHeight() * 0.45f);
        g.setColour(CustomLookAndFeel::getTextColour().withAlpha(currentOpacity));
        g.setFont(CustomLookAndFeel::getPluginFont(9.0f, true));
        g.drawText(currentLabel.toUpperCase(), labelBounds, juce::Justification::centred);
        
        // Value (bottom, purple accent)
        g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(currentOpacity));
        g.setFont(CustomLookAndFeel::getPluginFont(11.0f, true));
        g.drawText(currentValue, bounds, juce::Justification::centred);
    }
    
    bool isShowing() const { return currentOpacity > 0.01f; }
    
private:
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
};

//==============================================================================
// Advanced Panel Component with opaque background and FADE animation
class AdvancedPanelComponent : public juce::Component, private juce::Timer
{
public:
    AdvancedPanelComponent()
    {
        setOpaque(false);  // Not opaque during fade
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
        g.setColour(juce::Colours::black.withAlpha(0.4f * currentOpacity));
        g.fillRoundedRectangle(bounds.translated(0.0f, 3.0f), 10.0f);
        
        // Fully opaque background that covers everything underneath
        g.setColour(CustomLookAndFeel::getSurfaceColour().withAlpha(currentOpacity));
        g.fillRoundedRectangle(bounds, 10.0f);
        
        // Border
        g.setColour(CustomLookAndFeel::getBorderColour().withAlpha(currentOpacity));
        g.drawRoundedRectangle(bounds, 10.0f, 1.0f);
        
        // Bottom edge accent line
        g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(0.3f * currentOpacity));
        g.fillRoundedRectangle(bounds.getX() + 30.0f, bounds.getBottom() - 4.0f,
                                bounds.getWidth() - 60.0f, 2.0f, 1.0f);
    }
    
private:
    void timerCallback() override
    {
        if (std::abs(currentOpacity - targetOpacity) > 0.01f)
        {
            currentOpacity += (targetOpacity - currentOpacity) * 0.18f;  // Smooth easing
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
// Slider with hover callbacks for tooltip support
class TooltipSlider : public juce::Slider
{
public:
    TooltipSlider() = default;
    
    std::function<void()> onMouseEnter;
    std::function<void()> onMouseExit;
    
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
    ResizeButton() : juce::Button("Resize") {}
    
    std::function<void(int)> onSizeSelected;
    int currentSize = 1; // 0=Small, 1=Medium, 2=Large
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        auto accentColour = CustomLookAndFeel::getAccentColour();
        
        // Background - always clearly visible with solid dark background
        g.setColour(CustomLookAndFeel::getSurfaceDarkColour());
        g.fillRoundedRectangle(bounds, 5.0f);
        
        // Border - brighter on hover
        g.setColour(shouldDrawButtonAsHighlighted 
            ? accentColour.withAlpha(0.8f)
            : CustomLookAndFeel::getBorderColour().withAlpha(0.8f));
        g.drawRoundedRectangle(bounds, 5.0f, 1.5f);
        
        // Draw diagonal resize arrows (⤢ style)
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float arrowLen = 6.0f;
        float headSize = 3.0f;
        
        juce::Colour arrowColour = shouldDrawButtonAsHighlighted 
            ? accentColour.brighter(0.3f) 
            : accentColour.withAlpha(0.9f);
        
        g.setColour(arrowColour);
        
        // Arrow pointing bottom-right (from center outward)
        float x1End = cx + arrowLen;
        float y1End = cy + arrowLen;
        g.drawLine(cx - 1.0f, cy - 1.0f, x1End, y1End, 2.0f);
        
        // Arrowhead bottom-right
        juce::Path head1;
        head1.addTriangle(x1End, y1End, 
                          x1End - headSize, y1End,
                          x1End, y1End - headSize);
        g.fillPath(head1);
        
        // Arrow pointing top-left (from center outward)
        float x2End = cx - arrowLen;
        float y2End = cy - arrowLen;
        g.drawLine(cx + 1.0f, cy + 1.0f, x2End, y2End, 2.0f);
        
        // Arrowhead top-left
        juce::Path head2;
        head2.addTriangle(x2End, y2End, 
                          x2End + headSize, y2End,
                          x2End, y2End + headSize);
        g.fillPath(head2);
    }
    
    void clicked() override
    {
        juce::PopupMenu menu;
        menu.addSectionHeader("Resize");
        menu.addSeparator();
        menu.addItem(1, "Small", true, currentSize == 0);
        menu.addItem(2, "Medium", true, currentSize == 1);
        menu.addItem(3, "Large", true, currentSize == 2);
        
        menu.showMenuAsync(juce::PopupMenu::Options()
            .withTargetComponent(this)
            .withMinimumWidth(100),
            [this](int result) {
                if (result > 0 && onSizeSelected)
                {
                    currentSize = result - 1;
                    onSizeSelected(result - 1);
                }
            });
    }
};

//==============================================================================
// Gear icon button (replaces Unicode character that may not render)
class GearButton : public juce::Button
{
public:
    GearButton() : juce::Button("Settings") {}
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        auto centre = bounds.getCentre();
        float outerRadius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        float innerRadius = outerRadius * 0.55f;
        
        juce::Colour iconColour = shouldDrawButtonAsHighlighted 
            ? CustomLookAndFeel::getAccentColour().brighter(0.3f)
            : (getToggleState() ? CustomLookAndFeel::getAccentColour() : CustomLookAndFeel::getDimTextColour());
        
        g.setColour(iconColour);
        
        // Draw gear teeth
        juce::Path gearPath;
        int numTeeth = 8;
        for (int i = 0; i < numTeeth; ++i)
        {
            float angle1 = juce::MathConstants<float>::twoPi * i / numTeeth;
            float angle2 = angle1 + juce::MathConstants<float>::twoPi * 0.3f / numTeeth;
            float angle3 = angle1 + juce::MathConstants<float>::twoPi * 0.5f / numTeeth;
            float angle4 = angle1 + juce::MathConstants<float>::twoPi * 0.8f / numTeeth;
            
            float toothOuter = outerRadius;
            float toothInner = outerRadius * 0.75f;
            
            if (i == 0)
                gearPath.startNewSubPath(centre.x + std::cos(angle1) * toothInner, 
                                         centre.y + std::sin(angle1) * toothInner);
            
            gearPath.lineTo(centre.x + std::cos(angle2) * toothInner, centre.y + std::sin(angle2) * toothInner);
            gearPath.lineTo(centre.x + std::cos(angle2) * toothOuter, centre.y + std::sin(angle2) * toothOuter);
            gearPath.lineTo(centre.x + std::cos(angle3) * toothOuter, centre.y + std::sin(angle3) * toothOuter);
            gearPath.lineTo(centre.x + std::cos(angle4) * toothInner, centre.y + std::sin(angle4) * toothInner);
        }
        gearPath.closeSubPath();
        
        g.fillPath(gearPath);
        
        // Center hole
        g.setColour(CustomLookAndFeel::getSurfaceColour());
        g.fillEllipse(centre.x - innerRadius, centre.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);
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

    #if JucePlugin_Build_Standalone
    void loadFileButtonClicked();
    void playStopButtonClicked();
    void updatePlaybackUI();
    #endif

    VocalRiderAudioProcessor& audioProcessor;
    CustomLookAndFeel customLookAndFeel;

    //==============================================================================
    // Header (minimal)
    juce::Label titleLabel;
    juce::ComboBox presetComboBox;
    GearButton advancedButton;
    
    // Bottom bar with resize button
    juce::Component bottomBar;
    ResizeButton resizeButton;
    static constexpr int bottomBarHeight = 22;

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
    
    // Natural toggle (animated)
    AnimatedToggleButton naturalToggle { "NATURAL" };
    
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
    
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label holdLabel;
    juce::Label breathLabel;
    juce::Label transientLabel;

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
    
    // Fixed window dimensions for each preset
    static constexpr int smallWidth = 550;
    static constexpr int smallHeight = 380;
    static constexpr int mediumWidth = 700;
    static constexpr int mediumHeight = 480;
    static constexpr int largeWidth = 900;
    static constexpr int largeHeight = 600;
    
    static constexpr int controlPanelHeight = 110;  // Extra room for tooltip below knobs

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalRiderAudioProcessorEditor)
};
