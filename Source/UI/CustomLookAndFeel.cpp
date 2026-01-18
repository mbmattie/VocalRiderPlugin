/*
  ==============================================================================

    CustomLookAndFeel.cpp
    Created: 2026
    Author:  MBM Audio

    Dark, high-contrast design with glowing accents.
    Inspired by D5.0, Retrograde, and X-Rider aesthetics.

  ==============================================================================
*/

#include "CustomLookAndFeel.h"
#include <random>

CustomLookAndFeel::CustomLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, getBackgroundColour());
    setColour(juce::Label::textColourId, getTextColour());
    setColour(juce::Slider::textBoxTextColourId, getTextColour());
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0x00000000));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
    setColour(juce::TextButton::buttonColourId, getSurfaceColour());
    setColour(juce::TextButton::textColourOffId, getTextColour());
    setColour(juce::ComboBox::backgroundColourId, getSurfaceColour());
    setColour(juce::ComboBox::textColourId, getTextColour());
    setColour(juce::ComboBox::outlineColourId, getBorderColour());
    setColour(juce::ComboBox::arrowColourId, getDimTextColour());
    setColour(juce::PopupMenu::backgroundColourId, getSurfaceColour());
    setColour(juce::PopupMenu::textColourId, getTextColour());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, getAccentColour().withAlpha(0.2f));
    setColour(juce::PopupMenu::highlightedTextColourId, getTextColour());
    setColour(juce::ToggleButton::textColourId, getTextColour());
    setColour(juce::ToggleButton::tickColourId, getAccentColour());
}

//==============================================================================
// Grain Texture

void CustomLookAndFeel::drawGrainTexture(juce::Graphics& g, juce::Rectangle<int> bounds, float opacity)
{
    // Simple noise pattern using random pixels
    std::mt19937 rng(42);  // Fixed seed for consistent grain
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    int step = 3;  // Sample every 3 pixels for performance
    for (int y = bounds.getY(); y < bounds.getBottom(); y += step)
    {
        for (int x = bounds.getX(); x < bounds.getRight(); x += step)
        {
            float noise = dist(rng);
            if (noise > 0.5f)
            {
                float brightness = (noise - 0.5f) * 2.0f * opacity;
                g.setColour(juce::Colours::white.withAlpha(brightness));
                g.fillRect(x, y, step, step);
            }
        }
    }
}

//==============================================================================
// Glowing Arc

void CustomLookAndFeel::drawGlowingArc(juce::Graphics& g, float centreX, float centreY, float radius,
                                        float startAngle, float endAngle, float thickness,
                                        juce::Colour colour, float glowRadius)
{
    juce::Path arc;
    arc.addCentredArc(centreX, centreY, radius, radius, 0.0f, startAngle, endAngle, true);
    
    // Multi-layer glow effect
    for (float i = glowRadius; i > 0.0f; i -= 1.5f)
    {
        float alpha = 0.08f * (i / glowRadius);
        g.setColour(colour.withAlpha(alpha));
        g.strokePath(arc, juce::PathStrokeType(thickness + i * 2.0f, 
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    
    // Core arc
    g.setColour(colour);
    g.strokePath(arc, juce::PathStrokeType(thickness, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    
    // Bright center
    g.setColour(colour.brighter(0.4f));
    g.strokePath(arc, juce::PathStrokeType(thickness * 0.5f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
}

//==============================================================================
// Drawing Helpers

void CustomLookAndFeel::drawNeumorphicInset(juce::Graphics& g, juce::Rectangle<float> bounds,
                                             float cornerRadius, float depth)
{
    // Dark inset with strong shadow
    g.setColour(getSurfaceDarkColour());
    g.fillRoundedRectangle(bounds, cornerRadius);
    
    // Inner shadow gradient from top
    juce::ColourGradient shadow(
        juce::Colours::black.withAlpha(0.4f),
        bounds.getX(), bounds.getY(),
        juce::Colours::transparentBlack,
        bounds.getX(), bounds.getY() + depth * 3.0f,
        false
    );
    g.setGradientFill(shadow);
    g.fillRoundedRectangle(bounds, cornerRadius);
    
    // Subtle edge highlight at bottom
    g.setColour(getSurfaceLightColour().withAlpha(0.1f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerRadius, 0.5f);
}

void CustomLookAndFeel::drawNeumorphicRaised(juce::Graphics& g, juce::Rectangle<float> bounds,
                                              float cornerRadius, float depth)
{
    // Shadow
    g.setColour(getSurfaceDarkColour());
    g.fillRoundedRectangle(bounds.translated(depth * 0.5f, depth * 0.5f), cornerRadius);
    
    // Main surface
    g.setColour(getSurfaceColour());
    g.fillRoundedRectangle(bounds, cornerRadius);
    
    // Top-left highlight
    g.setColour(getSurfaceLightColour().withAlpha(0.15f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerRadius, 0.5f);
}

void CustomLookAndFeel::drawSoftPanel(juce::Graphics& g, juce::Rectangle<float> bounds,
                                       float cornerRadius)
{
    g.setColour(getSurfaceColour());
    g.fillRoundedRectangle(bounds, cornerRadius);
    
    g.setColour(getBorderColour().withAlpha(0.5f));
    g.drawRoundedRectangle(bounds, cornerRadius, 0.5f);
}

void CustomLookAndFeel::drawPanel(juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius)
{
    drawSoftPanel(g, bounds, cornerRadius);
}

void CustomLookAndFeel::drawPanelWithBorder(juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius)
{
    drawSoftPanel(g, bounds, cornerRadius);
}

//==============================================================================
// Large Featured Knob (for Target)

void CustomLookAndFeel::drawLargeKnob(juce::Graphics& g, juce::Rectangle<float> bounds,
                                       float sliderPosProportional, float rotaryStartAngle,
                                       float rotaryEndAngle, juce::Colour accentColour)
{
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f * 0.9f;
    
    float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    
    // Outer ring shadow
    g.setColour(getSurfaceDarkColour());
    g.fillEllipse(centreX - radius - 2.0f, centreY - radius + 2.0f, radius * 2.0f + 4.0f, radius * 2.0f + 4.0f);
    
    // Outer ring
    g.setColour(getSurfaceColour().darker(0.2f));
    g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
    
    // Outer ring edge
    g.setColour(getBorderColour());
    g.drawEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 1.5f);
    
    // Inner knob
    auto innerRadius = radius * 0.72f;
    auto innerBounds = juce::Rectangle<float>(
        centreX - innerRadius, centreY - innerRadius,
        innerRadius * 2.0f, innerRadius * 2.0f
    );
    
    // Inner knob shadow (inset look)
    g.setColour(getSurfaceDarkColour());
    g.fillEllipse(innerBounds.translated(1.0f, 1.0f));
    
    // Inner knob surface
    juce::ColourGradient knobGradient(
        getSurfaceColour().brighter(0.05f),
        centreX, centreY - innerRadius * 0.5f,
        getSurfaceColour().darker(0.1f),
        centreX, centreY + innerRadius * 0.5f,
        false
    );
    g.setGradientFill(knobGradient);
    g.fillEllipse(innerBounds);
    
    // Inner knob edge
    g.setColour(getBorderColour().withAlpha(0.5f));
    g.drawEllipse(innerBounds.reduced(0.5f), 0.5f);
    
    // Glowing value arc in the groove
    float arcRadius = (radius + innerRadius) / 2.0f;
    float arcThickness = (radius - innerRadius) * 0.4f;
    
    // Background arc (dim)
    juce::Path bgArc;
    bgArc.addCentredArc(centreX, centreY, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(getSurfaceDarkColour());
    g.strokePath(bgArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    
    // Value arc with glow
    if (sliderPosProportional > 0.001f)
    {
        drawGlowingArc(g, centreX, centreY, arcRadius, rotaryStartAngle, angle, 
                       arcThickness, accentColour, 12.0f);
    }
    
    // Position indicator dot on knob
    auto dotRadius = 4.0f;
    auto dotDistance = innerRadius * 0.6f;
    auto dotX = centreX + std::sin(angle) * dotDistance;
    auto dotY = centreY - std::cos(angle) * dotDistance;
    
    // Dot glow
    g.setColour(accentColour.withAlpha(0.3f));
    g.fillEllipse(dotX - dotRadius - 3.0f, dotY - dotRadius - 3.0f,
                  (dotRadius + 3.0f) * 2.0f, (dotRadius + 3.0f) * 2.0f);
    
    // Dot
    g.setColour(accentColour);
    g.fillEllipse(dotX - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
}

//==============================================================================
// Modern Rotary Slider (redesigned with gradients and shadows)

void CustomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPosProportional, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f * 0.88f;
    
    float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    
    // Outer shadow (soft, offset)
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillEllipse(centreX - radius + 1.5f, centreY - radius + 2.0f, radius * 2.0f, radius * 2.0f);
    
    // Outer ring with gradient
    auto outerRadius = radius;
    juce::ColourGradient outerGradient(
        getSurfaceColour().brighter(0.15f),
        centreX - outerRadius * 0.6f, centreY - outerRadius * 0.6f,
        getSurfaceDarkColour().darker(0.2f),
        centreX + outerRadius * 0.6f, centreY + outerRadius * 0.6f,
        false
    );
    g.setGradientFill(outerGradient);
    g.fillEllipse(centreX - outerRadius, centreY - outerRadius, outerRadius * 2.0f, outerRadius * 2.0f);
    
    // Outer ring highlight (top-left)
    g.setColour(getSurfaceLightColour().withAlpha(0.3f));
    g.drawEllipse(centreX - outerRadius, centreY - outerRadius, outerRadius * 2.0f, outerRadius * 2.0f, 1.5f);
    
    // Inner knob with modern gradient
    auto innerRadius = outerRadius * 0.72f;
    auto innerBounds = juce::Rectangle<float>(
        centreX - innerRadius, centreY - innerRadius,
        innerRadius * 2.0f, innerRadius * 2.0f
    );
    
    // Inner shadow (inset effect)
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillEllipse(innerBounds.translated(0.0f, 1.5f));
    
    // Inner knob - radial gradient for 3D effect
    juce::ColourGradient knobGradient(
        getSurfaceColour().brighter(0.2f),
        centreX - innerRadius * 0.4f, centreY - innerRadius * 0.4f,
        getSurfaceColour().darker(0.15f),
        centreX + innerRadius * 0.4f, centreY + innerRadius * 0.4f,
        false
    );
    g.setGradientFill(knobGradient);
    g.fillEllipse(innerBounds);
    
    // Inner highlight ring (top-left)
    g.setColour(getSurfaceLightColour().withAlpha(0.4f));
    g.drawEllipse(innerBounds.reduced(0.5f), 1.0f);
    
    // Value arc track (background)
    float arcRadius = (outerRadius + innerRadius) / 2.0f;
    float arcThickness = (outerRadius - innerRadius) * 0.6f;
    
    juce::Path trackArc;
    trackArc.addCentredArc(centreX, centreY, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(getSurfaceDarkColour().withAlpha(0.6f));
    g.strokePath(trackArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    
    // Value arc with modern glow
    if (sliderPosProportional > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
        
        // Glow layers
        for (float i = 8.0f; i > 0.0f; i -= 1.5f)
        {
            float alpha = 0.12f * (i / 8.0f);
            g.setColour(getAccentColour().withAlpha(alpha));
            g.strokePath(valueArc, juce::PathStrokeType(arcThickness + i * 1.5f,
                         juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        
        // Main arc with gradient
        juce::ColourGradient arcGradient(
            getAccentBrightColour(),
            centreX - arcRadius * 0.5f, centreY - arcRadius * 0.5f,
            getAccentColour(),
            centreX + arcRadius * 0.5f, centreY + arcRadius * 0.5f,
            false
        );
        g.setGradientFill(arcGradient);
        g.strokePath(valueArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
    }
    
    // Modern pointer (dot indicator)
    auto dotRadius = 5.0f;
    auto dotDistance = innerRadius * 0.65f;
    auto dotX = centreX + std::sin(angle) * dotDistance;
    auto dotY = centreY - std::cos(angle) * dotDistance;
    
    // Dot shadow
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillEllipse(dotX - dotRadius + 0.5f, dotY - dotRadius + 1.0f, dotRadius * 2.0f, dotRadius * 2.0f);
    
    // Dot with gradient
    juce::ColourGradient dotGradient(
        getAccentBrightColour(),
        dotX - dotRadius * 0.5f, dotY - dotRadius * 0.5f,
        getAccentColour(),
        dotX + dotRadius * 0.5f, dotY + dotRadius * 0.5f,
        false
    );
    g.setGradientFill(dotGradient);
    g.fillEllipse(dotX - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
    
    // Dot highlight
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.fillEllipse(dotX - dotRadius * 0.4f, dotY - dotRadius * 0.6f, dotRadius * 0.8f, dotRadius * 0.8f);
}

//==============================================================================
void CustomLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (!label.isBeingEdited())
    {
        auto alpha = label.isEnabled() ? 1.0f : 0.5f;
        g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
        g.setFont(getLabelFont(label));
        
        auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         juce::jmax(1, static_cast<int>(textArea.getHeight() / 12.0f)),
                         label.getMinimumHorizontalScale());
    }
}

juce::Font CustomLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(juce::FontOptions(11.0f));
}

//==============================================================================
void CustomLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                              const juce::Colour&,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    auto cornerSize = 5.0f;
    
    if (shouldDrawButtonAsDown || button.getToggleState())
    {
        // Pressed state
        g.setColour(getSurfaceDarkColour());
        g.fillRoundedRectangle(bounds, cornerSize);
        
        if (button.getToggleState())
        {
            g.setColour(getAccentColour().withAlpha(0.2f));
            g.fillRoundedRectangle(bounds.reduced(1.0f), cornerSize - 1.0f);
            
            g.setColour(getAccentColour().withAlpha(0.6f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        }
    }
    else
    {
        g.setColour(getSurfaceColour());
        g.fillRoundedRectangle(bounds, cornerSize);
        
        if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(getAccentColour().withAlpha(0.15f));
            g.fillRoundedRectangle(bounds, cornerSize);
        }
        
        g.setColour(getBorderColour());
        g.drawRoundedRectangle(bounds, cornerSize, 0.5f);
    }
}

//==============================================================================
void CustomLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                      int buttonX, int buttonY, int buttonW, int buttonH,
                                      juce::ComboBox&)
{
    auto cornerSize = 5.0f;
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(1.0f);
    
    g.setColour(isButtonDown ? getSurfaceDarkColour() : getSurfaceColour());
    g.fillRoundedRectangle(bounds, cornerSize);
    
    g.setColour(getBorderColour());
    g.drawRoundedRectangle(bounds, cornerSize, 0.5f);
    
    // Arrow
    auto arrowZone = juce::Rectangle<int>(buttonX, buttonY, buttonW, buttonH).toFloat();
    juce::Path arrow;
    auto arrowSize = 4.0f;
    arrow.addTriangle(arrowZone.getCentreX() - arrowSize, arrowZone.getCentreY() - 1.5f,
                      arrowZone.getCentreX() + arrowSize, arrowZone.getCentreY() - 1.5f,
                      arrowZone.getCentreX(), arrowZone.getCentreY() + 2.5f);
    
    g.setColour(getDimTextColour());
    g.fillPath(arrow);
}

//==============================================================================
void CustomLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                          bool shouldDrawButtonAsHighlighted,
                                          bool)
{
    auto bounds = button.getLocalBounds().toFloat();
    
    // Pill toggle
    auto toggleWidth = 32.0f;
    auto toggleHeight = 16.0f;
    auto pillBounds = juce::Rectangle<float>(
        bounds.getX() + 2.0f, 
        bounds.getCentreY() - toggleHeight / 2.0f,
        toggleWidth, toggleHeight
    );
    
    auto cornerRadius = toggleHeight / 2.0f;
    
    if (button.getToggleState())
    {
        // Active - glowing
        g.setColour(getAccentColour().withAlpha(0.3f));
        g.fillRoundedRectangle(pillBounds.expanded(2.0f), cornerRadius + 2.0f);
        
        g.setColour(getAccentColour());
        g.fillRoundedRectangle(pillBounds, cornerRadius);
        
        // Knob (right)
        auto knobSize = toggleHeight - 4.0f;
        auto knobX = pillBounds.getRight() - knobSize - 2.0f;
        g.setColour(getTextColour());
        g.fillEllipse(knobX, pillBounds.getCentreY() - knobSize / 2.0f, knobSize, knobSize);
    }
    else
    {
        // Inactive
        g.setColour(getSurfaceDarkColour());
        g.fillRoundedRectangle(pillBounds, cornerRadius);
        
        g.setColour(getBorderColour());
        g.drawRoundedRectangle(pillBounds, cornerRadius, 0.5f);
        
        // Knob (left)
        auto knobSize = toggleHeight - 4.0f;
        auto knobX = pillBounds.getX() + 2.0f;
        g.setColour(getDimTextColour());
        g.fillEllipse(knobX, pillBounds.getCentreY() - knobSize / 2.0f, knobSize, knobSize);
    }
    
    if (shouldDrawButtonAsHighlighted)
    {
        g.setColour(getAccentColour().withAlpha(0.1f));
        g.fillRoundedRectangle(pillBounds.expanded(3.0f), cornerRadius + 3.0f);
    }
    
    // Text
    auto textBounds = bounds;
    textBounds.removeFromLeft(toggleWidth + 10.0f);
    g.setColour(button.findColour(juce::ToggleButton::textColourId));
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText(button.getButtonText(), textBounds, juce::Justification::centredLeft);
}
