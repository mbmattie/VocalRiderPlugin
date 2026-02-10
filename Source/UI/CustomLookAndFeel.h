/*
  ==============================================================================

    CustomLookAndFeel.h
    Created: 2026
    Author:  MBM Audio

    Dark, high-contrast design with glowing accents.
    Inspired by D5.0, Retrograde, and X-Rider aesthetics.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel();
    ~CustomLookAndFeel() override = default;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    // Large featured knob (for Target) with hover support
    static void drawLargeKnob(juce::Graphics& g, juce::Rectangle<float> bounds,
                               float sliderPosProportional, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Colour accentColour,
                               bool isHovered = false);

    void drawLabel(juce::Graphics& g, juce::Label& label) override;
    juce::Font getLabelFont(juce::Label& label) override;
    
    // Plugin-wide fonts
    static juce::String getFontName() { return "Space Grotesk"; }
    static juce::String getBrandFontName() { return "Syne"; }  // For MBM Audio branding
    
    static juce::Font getPluginFont(float size, bool bold = false) 
    { 
        auto font = juce::Font(juce::FontOptions(getFontName(), size, bold ? juce::Font::bold : juce::Font::plain));
        // Fallback to system default if font not found (older macOS, missing fonts)
        if (font.getTypefaceName() != getFontName())
            return juce::Font(juce::FontOptions(size).withStyle(bold ? "Bold" : "Regular"));
        return font;
    }
    
    static juce::Font getBrandFont(float size, bool bold = true) 
    { 
        auto font = juce::Font(juce::FontOptions(getBrandFontName(), size, bold ? juce::Font::bold : juce::Font::plain));
        // Fallback to system default if font not found
        if (font.getTypefaceName() != getBrandFontName())
            return juce::Font(juce::FontOptions(size).withStyle(bold ? "Bold" : "Regular"));
        return font;
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;
    
    juce::Font getComboBoxFont(juce::ComboBox&) override { return getPluginFont(12.0f); }
    
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        // Properly vertically centered text
        label.setBounds(8, 0, box.getWidth() - 24, box.getHeight());
        label.setFont(getPluginFont(11.0f));
        label.setJustificationType(juce::Justification::centredLeft);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
    
    // Popup menu styling
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                          bool isSeparator, bool isActive, bool isHighlighted,
                          bool isTicked, bool hasSubMenu,
                          const juce::String& text, const juce::String& shortcutKeyText,
                          const juce::Drawable* icon, const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;

    //==========================================================================
    // Dark High-Contrast Color Palette
    
    // Base colors - lighter gray for prominent vignette effect
    static juce::Colour getBackgroundColour()      { return juce::Colour(0xFF252830); }  // Medium gray
    static juce::Colour getSurfaceColour()         { return juce::Colour(0xFF2C3038); }  // Slightly lighter surface
    static juce::Colour getSurfaceLightColour()    { return juce::Colour(0xFF3A3F4A); }  // Highlight
    static juce::Colour getSurfaceDarkColour()     { return juce::Colour(0xFF0D0E11); }  // Deep shadow
    static juce::Colour getBorderColour()          { return juce::Colour(0xFF2E3138); }  // Subtle border
    
    // Accent colors - glowing, vibrant
    static juce::Colour getAccentColour()          { return juce::Colour(0xFFB48EFF); }  // Purple glow
    static juce::Colour getAccentBrightColour()    { return juce::Colour(0xFFD4B8FF); }  // Bright purple
    static juce::Colour getAccentDimColour()       { return juce::Colour(0xFF7B5CAD); }  // Dim purple
    
    // Secondary accent (cyan for contrast)
    static juce::Colour getSecondaryAccentColour() { return juce::Colour(0xFF5BC4D4); }  // Cyan
    
    // Text colors with clear hierarchy
    static juce::Colour getTextColour()            { return juce::Colour(0xFFE8EAF0); }  // Bright white
    static juce::Colour getDimTextColour()         { return juce::Colour(0xFF8890A0); }  // Medium gray
    static juce::Colour getVeryDimTextColour()     { return juce::Colour(0xFF4A5060); }  // Dark gray
    
    // Waveform and visualization colors
    static juce::Colour getWaveformColour()        { return juce::Colour(0xFF5BC4D4); }  // Cyan
    static juce::Colour getWaveformDimColour()     { return juce::Colour(0xFF2A5560); }  // Dim cyan
    static juce::Colour getGainCurveColour()       { return juce::Colour(0xFF5BC4D4); }  // Cyan for gain curve (like input)
    static juce::Colour getGainBoostColour()       { return juce::Colour(0xFF6A7080); }  // Muted gray for boost range
    static juce::Colour getGainCutColour()         { return juce::Colour(0xFF8A7BC0); }  // Subtle purple for cut waveform
    static juce::Colour getTargetLineColour()      { return juce::Colour(0xFFE8E0F0); }  // Very light purple, almost white with a hint of lavender
    static juce::Colour getRangeLineColour()       { return juce::Colour(0xFF5A5E68); }  // Muted gray for range lines
    
    // Status colors
    static juce::Colour getWarningColour()         { return juce::Colour(0xFFE87B7B); }
    static juce::Colour getSuccessColour()         { return juce::Colour(0xFF7BE8A8); }
    
    //==========================================================================
    // Drawing helpers
    
    static void drawNeumorphicInset(juce::Graphics& g, juce::Rectangle<float> bounds, 
                                     float cornerRadius = 8.0f, float depth = 4.0f);
    
    static void drawNeumorphicRaised(juce::Graphics& g, juce::Rectangle<float> bounds,
                                      float cornerRadius = 8.0f, float depth = 4.0f);
    
    static void drawSoftPanel(juce::Graphics& g, juce::Rectangle<float> bounds, 
                               float cornerRadius = 8.0f);
    
    // Grainy texture overlay
    static void drawGrainTexture(juce::Graphics& g, juce::Rectangle<int> bounds, float opacity = 0.03f);
    
    // Glowing arc for knobs
    static void drawGlowingArc(juce::Graphics& g, float centreX, float centreY, float radius,
                                float startAngle, float endAngle, float thickness,
                                juce::Colour colour, float glowRadius = 8.0f);
    
    // Legacy compatibility
    static juce::Colour getPanelColour() { return getSurfaceColour(); }
    static void drawPanel(juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius = 6.0f);
    static void drawPanelWithBorder(juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius = 6.0f);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomLookAndFeel)
};
