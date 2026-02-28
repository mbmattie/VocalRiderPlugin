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
// Large Featured Knob (for Target) - FabFilter style with gradient

void CustomLookAndFeel::drawLargeKnob(juce::Graphics& g, juce::Rectangle<float> bounds,
                                       float sliderPosProportional, float rotaryStartAngle,
                                       float rotaryEndAngle, juce::Colour accentColour,
                                       bool isHovered)
{
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f * 0.92f;
    
    float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    
    auto outerRadius = radius;
    auto knobRadius = outerRadius * 0.82f;  // Bigger inner knob (was 0.72)
    float arcRadius = (outerRadius + knobRadius) / 2.0f;
    float arcThickness = (outerRadius - knobRadius) * 0.85f;  // Thinner arc
    
    // FULLY OPAQUE circular background that covers the entire knob area
    g.setColour(juce::Colour(0xFF0D0E11));
    g.fillEllipse(centreX - outerRadius, centreY - outerRadius, 
                  outerRadius * 2.0f, outerRadius * 2.0f);
    
    // Outer ring - thinner grey ring (brighter on hover)
    g.setColour(isHovered ? juce::Colour(0xFF4A4D55) : juce::Colour(0xFF3A3D45));
    g.drawEllipse(centreX - outerRadius, centreY - outerRadius, 
                  outerRadius * 2.0f, outerRadius * 2.0f, 1.0f);
    
    // Main knob body with gradient (darker top, lighter bottom)
    auto knobBounds = juce::Rectangle<float>(
        centreX - knobRadius, centreY - knobRadius,
        knobRadius * 2.0f, knobRadius * 2.0f
    );
    
    // Arc groove background (thinner ring between outer and inner)
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(centreX, centreY, arcRadius, arcRadius, 
                                 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xFF151619));
    g.strokePath(backgroundArc, juce::PathStrokeType(arcThickness, 
                 juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    
    // Create gradient: darker grey at top, lighter grey at bottom
    juce::ColourGradient knobGradient(
        juce::Colour(0xFF1E2028),
        centreX, centreY - knobRadius,
        juce::Colour(0xFF353840),
        centreX, centreY + knobRadius,
        false
    );
    
    // Fill with gradient
    juce::Path knobPath;
    knobPath.addEllipse(knobBounds);
    g.setGradientFill(knobGradient);
    g.fillPath(knobPath);
    
    // Hover glow effect
    if (isHovered)
    {
        g.setColour(accentColour.withAlpha(0.06f));
        g.fillEllipse(knobBounds.expanded(2.0f));
    }
    
    // Draw value arc with gradient
    if (sliderPosProportional > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, arcRadius, arcRadius, 
                               0.0f, rotaryStartAngle, angle, true);
        
        // Purple gradient for the value arc
        juce::ColourGradient arcGradient(
            accentColour.darker(0.3f),
            centreX + std::sin(rotaryStartAngle) * arcRadius,
            centreY - std::cos(rotaryStartAngle) * arcRadius,
            accentColour.brighter(0.1f),
            centreX + std::sin(angle) * arcRadius,
            centreY - std::cos(angle) * arcRadius,
            false
        );
        
        g.setGradientFill(arcGradient);
        g.strokePath(valueArc, juce::PathStrokeType(arcThickness * 0.75f,  // Thinner arc
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    
    // Position indicator LINE - THINNER line from center toward edge
    auto lineStartDist = knobRadius * 0.2f;
    auto lineEndDist = knobRadius * 0.8f;
    
    auto lineStartX = centreX + std::sin(angle) * lineStartDist;
    auto lineStartY = centreY - std::cos(angle) * lineStartDist;
    auto lineEndX = centreX + std::sin(angle) * lineEndDist;
    auto lineEndY = centreY - std::cos(angle) * lineEndDist;
    
    // Subtle indicator line
    g.setColour(accentColour.withAlpha(0.85f));
    g.drawLine(lineStartX, lineStartY, lineEndX, lineEndY, 1.2f);
}

//==============================================================================
// FabFilter-style Rotary Slider (clean, minimal design with gradient)

void CustomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPosProportional, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f * 0.92f;
    
    float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    bool isDisabled = !slider.isEnabled();
    auto accentColour = isDisabled ? juce::Colour(0xFF3A3D48) : getAccentColour();
    
    // Check if mouse is over for hover effect
    bool isHovered = !isDisabled && slider.isMouseOver();
    
    auto outerRadius = radius;
    auto knobRadius = outerRadius * 0.82f;  // Bigger inner knob (was 0.72)
    float arcRadius = (outerRadius + knobRadius) / 2.0f;
    float arcThickness = (outerRadius - knobRadius) * 0.85f;  // Thinner arc
    
    // FULLY OPAQUE circular background that covers the entire knob area
    // This prevents the waveform from showing through the gap
    g.setColour(juce::Colour(0xFF0D0E11));  // Very dark, matches background
    g.fillEllipse(centreX - outerRadius, centreY - outerRadius, 
                  outerRadius * 2.0f, outerRadius * 2.0f);
    
    // Outer ring - thinner grey ring (brighter on hover)
    g.setColour(isHovered ? juce::Colour(0xFF4A4D55) : juce::Colour(0xFF3A3D45));
    g.drawEllipse(centreX - outerRadius, centreY - outerRadius, 
                  outerRadius * 2.0f, outerRadius * 2.0f, 1.0f);
    
    // Arc groove background (darker ring between outer and inner)
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(centreX, centreY, arcRadius, arcRadius, 
                                 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xFF151619));  // Dark groove
    g.strokePath(backgroundArc, juce::PathStrokeType(arcThickness, 
                 juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    
    // Main knob body with gradient (darker top, lighter bottom)
    auto knobBounds = juce::Rectangle<float>(
        centreX - knobRadius, centreY - knobRadius,
        knobRadius * 2.0f, knobRadius * 2.0f
    );
    
    // Create gradient: darker grey at top, lighter grey at bottom
    juce::ColourGradient knobGradient(
        juce::Colour(0xFF1E2028),  // Darker at top
        centreX, centreY - knobRadius,
        juce::Colour(0xFF353840),  // Lighter at bottom
        centreX, centreY + knobRadius,
        false
    );
    
    // Fill knob with gradient using a path
    juce::Path knobPath;
    knobPath.addEllipse(knobBounds);
    g.setGradientFill(knobGradient);
    g.fillPath(knobPath);
    
    // Hover glow effect
    if (isHovered)
    {
        g.setColour(accentColour.withAlpha(0.1f));
        g.fillEllipse(knobBounds.expanded(3.0f));
    }
    
    // Draw value arc with gradient (darker purple to lighter purple)
    if (sliderPosProportional > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, arcRadius, arcRadius, 
                               0.0f, rotaryStartAngle, angle, true);
        
        // Create purple gradient for the value arc
        juce::ColourGradient arcGradient(
            accentColour.darker(0.3f),  // Darker purple at start
            centreX + std::sin(rotaryStartAngle) * arcRadius,
            centreY - std::cos(rotaryStartAngle) * arcRadius,
            accentColour.brighter(0.1f),  // Lighter purple at end
            centreX + std::sin(angle) * arcRadius,
            centreY - std::cos(angle) * arcRadius,
            false
        );
        
        g.setGradientFill(arcGradient);
        g.strokePath(valueArc, juce::PathStrokeType(arcThickness * 0.85f, 
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    
    // Position indicator LINE - THINNER line from center toward edge
    auto lineStartDist = knobRadius * 0.2f;
    auto lineEndDist = knobRadius * 0.8f;
    
    auto lineStartX = centreX + std::sin(angle) * lineStartDist;
    auto lineStartY = centreY - std::cos(angle) * lineStartDist;
    auto lineEndX = centreX + std::sin(angle) * lineEndDist;
    auto lineEndY = centreY - std::cos(angle) * lineEndDist;
    
    g.setColour(accentColour.withAlpha(isDisabled ? 0.5f : 0.85f));
    g.drawLine(lineStartX, lineStartY, lineEndX, lineEndY, 1.2f);
}

//==============================================================================
void CustomLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (!label.isBeingEdited())
    {
        auto textColour = label.findColour(juce::Label::textColourId);
        if (!label.isEnabled())
            textColour = textColour.withMultipliedSaturation(0.3f).withMultipliedBrightness(0.5f);
        g.setColour(textColour);
        g.setFont(getLabelFont(label));
        
        auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         juce::jmax(1, static_cast<int>(textArea.getHeight() / 12.0f)),
                         label.getMinimumHorizontalScale());
    }
}

juce::Font CustomLookAndFeel::getLabelFont(juce::Label& label)
{
    // Respect the label's own font if it has been set to a non-default size
    auto labelFont = label.getFont();
    if (labelFont.getHeight() > 12.0f)  // If larger than default, use it
        return labelFont;
    return getPluginFont(11.0f);  // Default small font
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
void CustomLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                        bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/)
{
    // Use plugin font for all button text
    g.setFont(getPluginFont(12.0f, true));
    
    auto textColour = button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                                 : juce::TextButton::textColourOffId);
    if (!button.isEnabled())
        textColour = textColour.withAlpha(0.5f);
    
    g.setColour(textColour);
    g.drawText(button.getButtonText(), button.getLocalBounds().reduced(4),
               juce::Justification::centred);
}

//==============================================================================
void CustomLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                      int buttonX, int buttonY, int buttonW, int buttonH,
                                      juce::ComboBox&)
{
    auto cornerSize = 5.0f;
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(1.0f);
    
    // Darker gray background
    g.setColour(juce::Colour(0xFF2D3038));  // Darker gray
    g.fillRoundedRectangle(bounds, cornerSize);
    
    // Subtle shine/highlight at top
    juce::ColourGradient shine(
        juce::Colours::white.withAlpha(0.06f), bounds.getX(), bounds.getY(),
        juce::Colours::transparentWhite, bounds.getX(), bounds.getY() + bounds.getHeight() * 0.5f,
        false
    );
    g.setGradientFill(shine);
    g.fillRoundedRectangle(bounds, cornerSize);
    
    // Border - subtle
    g.setColour(isButtonDown ? getAccentColour().withAlpha(0.5f) : juce::Colour(0xFF454550));
    g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
    
    // Arrow
    auto arrowZone = juce::Rectangle<int>(buttonX, buttonY, buttonW, buttonH).toFloat();
    juce::Path arrow;
    auto arrowSize = 4.0f;
    arrow.addTriangle(arrowZone.getCentreX() - arrowSize, arrowZone.getCentreY() - 1.5f,
                      arrowZone.getCentreX() + arrowSize, arrowZone.getCentreY() - 1.5f,
                      arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);
    
    g.setColour(isButtonDown ? getAccentColour() : getDimTextColour().brighter(0.3f));
    g.fillPath(arrow);
}

//==============================================================================
void CustomLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    
    // Fill entire background first to avoid white corners
    g.fillAll(juce::Colour(0xFF1A1C22));
    
    // Simple dark gradient background
    juce::ColourGradient bgGrad(
        juce::Colour(0xFF2A2D35), bounds.getX(), bounds.getY(),
        juce::Colour(0xFF1A1C22), bounds.getX(), bounds.getBottom(),
        false
    );
    g.setGradientFill(bgGrad);
    g.fillRect(bounds);
}

void CustomLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                           bool isSeparator, bool isActive, bool isHighlighted,
                                           bool isTicked, bool hasSubMenu,
                                           const juce::String& text, const juce::String& /*shortcutKeyText*/,
                                           const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/)
{
    auto bounds = area.toFloat().reduced(4.0f, 1.0f);
    
    if (isSeparator)
    {
        g.setColour(getBorderColour().withAlpha(0.5f));
        g.fillRect(bounds.getX() + 8.0f, bounds.getCentreY() - 0.5f, bounds.getWidth() - 16.0f, 1.0f);
        return;
    }
    
    if (isHighlighted && isActive)
    {
        // Simple highlight - subtle background
        g.setColour(juce::Colour(0xFF3A3D45));
        g.fillRoundedRectangle(bounds, 3.0f);
    }
    
    // Text with consistent margin
    g.setFont(getPluginFont(12.0f));
    
    // When ticked, highlight text in accent color
    if (isTicked)
        g.setColour(getAccentColour());
    else
        g.setColour(isActive ? (isHighlighted ? getTextColour() : getDimTextColour().brighter(0.2f)) 
                             : getDimTextColour().withAlpha(0.4f));
    
    float textLeftMargin = 12.0f;
    float arrowWidth = hasSubMenu ? 16.0f : 0.0f;  // Reserve space for arrow
    auto textBounds = bounds;
    textBounds.setX(bounds.getX() + textLeftMargin);
    textBounds.setWidth(bounds.getWidth() - textLeftMargin - 8.0f - arrowWidth);
    
    if (isTicked)
    {
        // Subtle accent bar on left edge when selected
        g.setColour(getAccentColour());
        g.fillRoundedRectangle(bounds.getX() + 2.0f, bounds.getCentreY() - 6.0f, 2.0f, 12.0f, 1.0f);
    }
    
    g.drawText(text, textBounds, juce::Justification::centredLeft);
    
    // Draw submenu arrow on the right
    if (hasSubMenu)
    {
        float arrowX = bounds.getRight() - 12.0f;
        float arrowY = bounds.getCentreY();
        float arrowSize = 4.0f;
        
        juce::Path arrow;
        arrow.addTriangle(arrowX, arrowY - arrowSize,
                          arrowX, arrowY + arrowSize,
                          arrowX + arrowSize, arrowY);
        
        g.setColour(isHighlighted ? getTextColour() : getDimTextColour());
        g.fillPath(arrow);
    }
}

juce::Font CustomLookAndFeel::getPopupMenuFont()
{
    return getPluginFont(12.0f);
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
    g.setFont(getPluginFont(11.0f));
    g.drawText(button.getButtonText(), textBounds, juce::Justification::centredLeft);
}
