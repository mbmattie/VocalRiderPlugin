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
// Advanced Panel Component with opaque background
class AdvancedPanelComponent : public juce::Component
{
public:
    AdvancedPanelComponent()
    {
        setOpaque(true);  // Tell JUCE we're painting an opaque background
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Shadow effect (drawn first, behind everything)
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 3.0f), 10.0f);
        
        // Fully opaque background that covers everything underneath
        g.setColour(CustomLookAndFeel::getSurfaceColour());
        g.fillRoundedRectangle(bounds, 10.0f);
        
        // Border
        g.setColour(CustomLookAndFeel::getBorderColour());
        g.drawRoundedRectangle(bounds, 10.0f, 1.0f);
        
        // Bottom edge accent line
        g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(0.3f));
        g.fillRoundedRectangle(bounds.getX() + 30.0f, bounds.getBottom() - 4.0f,
                                bounds.getWidth() - 60.0f, 2.0f, 1.0f);
    }
};

//==============================================================================
// Resize Button Component (FabFilter-style with expanding arrows)
class ResizeButton : public juce::Button
{
public:
    ResizeButton() : juce::Button("Resize") {}
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto accentColour = CustomLookAndFeel::getAccentColour();
        
        // Background (subtle, only visible on hover)
        if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(CustomLookAndFeel::getSurfaceLightColour().withAlpha(0.4f));
            g.fillRoundedRectangle(bounds, 3.0f);
        }
        
        // Draw two diagonal arrows pointing outward (bottom-right direction)
        // Style: Two diagonal lines with arrowheads pointing outward
        float padding = 4.0f;
        float lineLength = 7.0f;
        float arrowHeadSize = 2.0f;
        float spacing = 2.5f;
        
        float left = bounds.getX() + padding;
        float bottom = bounds.getBottom() - padding;
        
        // Arrow color - purple accent, brighter when highlighted
        juce::Colour arrowColour = shouldDrawButtonAsHighlighted 
            ? accentColour.brighter(0.4f) 
            : accentColour.withAlpha(0.85f);
        
        g.setColour(arrowColour);
        
        // First arrow: diagonal line from top-left to bottom-right
        float x1_start = left;
        float y1_start = bottom - lineLength;
        float x1_end = left + lineLength;
        float y1_end = bottom;
        
        // Draw line
        g.drawLine(x1_start, y1_start, x1_end, y1_end, 1.5f);
        
        // Draw arrowhead (triangle pointing bottom-right)
        juce::Path arrowHead1;
        arrowHead1.addTriangle(
            x1_end, y1_end,                                    // Tip (bottom-right)
            x1_end - arrowHeadSize, y1_end - arrowHeadSize,   // Top-left of arrowhead
            x1_end - arrowHeadSize, y1_end                    // Bottom-left of arrowhead
        );
        g.fillPath(arrowHead1);
        
        // Second arrow: offset diagonal line
        float x2_start = left + spacing;
        float y2_start = bottom - lineLength - spacing;
        float x2_end = left + lineLength + spacing;
        float y2_end = bottom - spacing;
        
        // Draw line
        g.drawLine(x2_start, y2_start, x2_end, y2_end, 1.5f);
        
        // Draw arrowhead (triangle pointing bottom-right)
        juce::Path arrowHead2;
        arrowHead2.addTriangle(
            x2_end, y2_end,                                    // Tip (bottom-right)
            x2_end - arrowHeadSize, y2_end - arrowHeadSize,   // Top-left of arrowhead
            x2_end - arrowHeadSize, y2_end                    // Bottom-left of arrowhead
        );
        g.fillPath(arrowHead2);
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
    juce::TextButton advancedButton { "⚙" };
    
    // Resize button (lower right corner)
    ResizeButton resizeButton;

    //==============================================================================
    // Main waveform display (fills most of the window)
    WaveformDisplay waveformDisplay;

    //==============================================================================
    // Bottom control panel (tab overlay)
    juce::Component controlPanel;
    
    // Main knobs: Target (large, center), Range and Speed (smaller, sides)
    juce::Slider targetSlider;
    juce::Slider rangeSlider;
    juce::Slider speedSlider;
    
    juce::Label targetLabel;
    juce::Label rangeLabel;
    juce::Label speedLabel;
    
    // Natural toggle
    juce::ToggleButton naturalToggle { "Natural" };
    
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
    juce::ComboBox automationModeComboBox;
    
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider holdSlider;
    juce::Slider breathReductionSlider;
    juce::Slider transientPreservationSlider;
    
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
    
    static constexpr int controlPanelHeight = 100;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalRiderAudioProcessorEditor)
};
