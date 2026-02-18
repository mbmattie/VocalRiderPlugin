/*
  ==============================================================================

    DualRangeKnob.cpp
    Created: 2026
    Author:  MBM Audio

  ==============================================================================
*/

#include "DualRangeKnob.h"
#include <cmath>

//==============================================================================
// DualRangeKnob

DualRangeKnob::DualRangeKnob()
{
    setRepaintsOnMouseActivity(true);
}

DualRangeKnob::DragRing DualRangeKnob::hitTestRing(const juce::Point<float>& pos) const
{
    auto bounds = getLocalBounds().toFloat();
    float centreX = bounds.getCentreX();
    float centreY = bounds.getCentreY();
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f * 0.92f;

    float dist = std::sqrt((pos.x - centreX) * (pos.x - centreX) +
                           (pos.y - centreY) * (pos.y - centreY));

    if (dist > radius) return DragRing::None;

    if (locked)
        return DragRing::Boost; // when locked, any click moves both

    float knobBodyRadius = radius * 0.68f;
    float midpoint = (radius + knobBodyRadius) / 2.0f;
    if (dist >= midpoint)
        return DragRing::Boost;
    else
        return DragRing::Cut;
}

void DualRangeKnob::setBoostValue(float dB)
{
    float clamped = juce::jlimit(minDb, maxDb, dB);
    if (std::abs(boostValue - clamped) > 0.01f)
    {
        boostValue = clamped;
        repaint();
    }
}

void DualRangeKnob::setCutValue(float dB)
{
    float clamped = juce::jlimit(minDb, maxDb, dB);
    if (std::abs(cutValue - clamped) > 0.01f)
    {
        cutValue = clamped;
        repaint();
    }
}

void DualRangeKnob::setLocked(bool isLocked)
{
    if (locked != isLocked)
    {
        locked = isLocked;
        repaint();
    }
}

//==============================================================================
// Mouse interaction

void DualRangeKnob::mouseDown(const juce::MouseEvent& event)
{
    currentDragRing = hitTestRing(event.position);
    if (currentDragRing == DragRing::None) return;

    if (currentDragRing == DragRing::Boost)
        dragStartValue = boostValue;
    else
        dragStartValue = cutValue;

    dragStartY = event.position.y;
}

void DualRangeKnob::mouseDrag(const juce::MouseEvent& event)
{
    if (currentDragRing == DragRing::None) return;

    float deltaY = event.position.y - dragStartY;
    float deltaDrag = -deltaY * dragSensitivity;

    if (locked)
    {
        float newVal = juce::jlimit(minDb, maxDb, dragStartValue + deltaDrag);
        boostValue = newVal;
        cutValue = newVal;
        if (onBoostChanged) onBoostChanged(newVal);
        if (onCutChanged) onCutChanged(newVal);
    }
    else
    {
        float newVal = juce::jlimit(minDb, maxDb, dragStartValue + deltaDrag);
        if (currentDragRing == DragRing::Boost)
        {
            boostValue = newVal;
            if (onBoostChanged) onBoostChanged(newVal);
        }
        else
        {
            cutValue = newVal;
            if (onCutChanged) onCutChanged(newVal);
        }
    }

    repaint();
}

void DualRangeKnob::mouseUp(const juce::MouseEvent&)
{
    if (currentDragRing != DragRing::None)
    {
        if (onDragEnd) onDragEnd();
    }
    currentDragRing = DragRing::None;
}

void DualRangeKnob::mouseMove(const juce::MouseEvent& event)
{
    auto newHover = hitTestRing(event.position);
    if (newHover != hoverRing)
    {
        hoverRing = newHover;
        repaint();
    }
}

void DualRangeKnob::mouseExit(const juce::MouseEvent& e)
{
    juce::Component::mouseExit(e);
    mouseHovering = false;
    hoverRing = DragRing::None;
    repaint();
    if (onMouseExit) onMouseExit();
}

//==============================================================================
// Painting

static constexpr float rotaryStartAngle = juce::MathConstants<float>::pi * 1.2f;
static constexpr float rotaryEndAngle   = juce::MathConstants<float>::pi * 2.8f;

static float valueToAngle(float value, float minVal, float maxVal)
{
    float proportion = (value - minVal) / (maxVal - minVal);
    return rotaryStartAngle + proportion * (rotaryEndAngle - rotaryStartAngle);
}

void DualRangeKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    mouseHovering = isMouseOver();
    drawDualArcKnob(g, bounds);
}

void DualRangeKnob::drawDualArcKnob(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f * 0.92f;
    bool isHovered = mouseHovering;

    auto purpleColour = CustomLookAndFeel::getAccentColour();
    auto outerPurpleColour = purpleColour.withSaturation(0.35f).brighter(0.25f);  // Lighter, desaturated purple for outer arc

    float boostAngle = valueToAngle(boostValue, minDb, maxDb);
    float cutAngle = valueToAngle(cutValue, minDb, maxDb);

    auto outerRadius = radius;
    auto knobBodyRadius = outerRadius * 0.68f;    // Larger inner body

    // Outer arc: thin ring for boost
    float outerArcOuter = outerRadius;
    float outerArcInner = outerRadius * 0.85f;
    float outerArcRadius = (outerArcOuter + outerArcInner) / 2.0f;
    float outerArcThickness = (outerArcOuter - outerArcInner) * 0.50f;

    // Inner arc: between outer arc and knob body for cut (purple)
    float innerArcOuter = outerRadius * 0.82f;
    float innerArcRadius = (innerArcOuter + knobBodyRadius) / 2.0f;
    float innerArcThickness = (innerArcOuter - knobBodyRadius) * 0.70f;

    // Opaque circular background
    g.setColour(juce::Colour(0xFF0D0E11));
    g.fillEllipse(centreX - outerRadius, centreY - outerRadius,
                  outerRadius * 2.0f, outerRadius * 2.0f);

    // Outer ring border
    g.setColour(isHovered ? juce::Colour(0xFF4A4D55) : juce::Colour(0xFF3A3D45));
    g.drawEllipse(centreX - outerRadius, centreY - outerRadius,
                  outerRadius * 2.0f, outerRadius * 2.0f, 1.0f);

    // === OUTER ARC GROOVE (boost) ===
    juce::Path outerGroove;
    outerGroove.addCentredArc(centreX, centreY, outerArcRadius, outerArcRadius,
                               0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xFF151619));
    g.strokePath(outerGroove, juce::PathStrokeType(outerArcThickness,
                 juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // === INNER ARC GROOVE (cut) ===
    juce::Path innerGroove;
    innerGroove.addCentredArc(centreX, centreY, innerArcRadius, innerArcRadius,
                               0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xFF151619));
    g.strokePath(innerGroove, juce::PathStrokeType(innerArcThickness,
                 juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Main knob body with gradient
    auto knobBounds = juce::Rectangle<float>(
        centreX - knobBodyRadius, centreY - knobBodyRadius,
        knobBodyRadius * 2.0f, knobBodyRadius * 2.0f);

    juce::ColourGradient knobGradient(
        juce::Colour(0xFF1E2028), centreX, centreY - knobBodyRadius,
        juce::Colour(0xFF353840), centreX, centreY + knobBodyRadius, false);
    juce::Path knobPath;
    knobPath.addEllipse(knobBounds);
    g.setGradientFill(knobGradient);
    g.fillPath(knobPath);

    // Hover glow
    if (isHovered && !locked)
    {
        if (hoverRing == DragRing::Boost)
        {
            g.setColour(outerPurpleColour.withAlpha(0.06f));
            g.fillEllipse(centreX - outerRadius, centreY - outerRadius,
                          outerRadius * 2.0f, outerRadius * 2.0f);
        }
        else if (hoverRing == DragRing::Cut)
        {
            g.setColour(purpleColour.withAlpha(0.06f));
            g.fillEllipse(knobBounds.expanded(3.0f));
        }
    }
    else if (isHovered && locked)
    {
        g.setColour(purpleColour.withAlpha(0.05f));
        g.fillEllipse(centreX - outerRadius, centreY - outerRadius,
                      outerRadius * 2.0f, outerRadius * 2.0f);
    }

    // === OUTER VALUE ARC (boost - cyan) ===
    float boostProportion = (boostValue - minDb) / (maxDb - minDb);
    if (boostProportion > 0.001f)
    {
        juce::Path boostArc;
        boostArc.addCentredArc(centreX, centreY, outerArcRadius, outerArcRadius,
                                0.0f, rotaryStartAngle, boostAngle, true);

        juce::ColourGradient arcGrad(
            outerPurpleColour.darker(0.3f),
            centreX + std::sin(rotaryStartAngle) * outerArcRadius,
            centreY - std::cos(rotaryStartAngle) * outerArcRadius,
            outerPurpleColour.brighter(0.1f),
            centreX + std::sin(boostAngle) * outerArcRadius,
            centreY - std::cos(boostAngle) * outerArcRadius, false);

        g.setGradientFill(arcGrad);
        g.strokePath(boostArc, juce::PathStrokeType(outerArcThickness * 0.85f,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // === INNER VALUE ARC (cut - purple) ===
    float cutProportion = (cutValue - minDb) / (maxDb - minDb);
    if (cutProportion > 0.001f)
    {
        juce::Path cutArc;
        cutArc.addCentredArc(centreX, centreY, innerArcRadius, innerArcRadius,
                              0.0f, rotaryStartAngle, cutAngle, true);

        juce::ColourGradient arcGrad(
            purpleColour.darker(0.3f),
            centreX + std::sin(rotaryStartAngle) * innerArcRadius,
            centreY - std::cos(rotaryStartAngle) * innerArcRadius,
            purpleColour.brighter(0.1f),
            centreX + std::sin(cutAngle) * innerArcRadius,
            centreY - std::cos(cutAngle) * innerArcRadius, false);

        g.setGradientFill(arcGrad);
        g.strokePath(cutArc, juce::PathStrokeType(innerArcThickness * 0.85f,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Position indicator dots (FabFilter style)
    float dotRadius = 2.2f;

    // Outer dot (boost)
    float outerDotDist = outerArcRadius;
    float odx = centreX + std::sin(boostAngle) * outerDotDist;
    float ody = centreY - std::cos(boostAngle) * outerDotDist;
    g.setColour(outerPurpleColour);
    g.fillEllipse(odx - dotRadius, ody - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);

    // Inner dot (cut)
    float innerDotDist = innerArcRadius;
    float idx = centreX + std::sin(cutAngle) * innerDotDist;
    float idy = centreY - std::cos(cutAngle) * innerDotDist;
    g.setColour(purpleColour);
    g.fillEllipse(idx - dotRadius, idy - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
}

//==============================================================================
// RangeLockButton

RangeLockButton::RangeLockButton()
{
    setRepaintsOnMouseActivity(true);
}

void RangeLockButton::setLocked(bool isLocked)
{
    if (locked != isLocked)
    {
        locked = isLocked;
        repaint();
    }
}

void RangeLockButton::mouseDown(const juce::MouseEvent&)
{
    locked = !locked;
    repaint();
    if (onLockChanged) onLockChanged(locked);
}

void RangeLockButton::mouseEnter(const juce::MouseEvent& event)
{
    juce::Component::mouseEnter(event);
    hovering = true;
    repaint();
}

void RangeLockButton::mouseExit(const juce::MouseEvent& event)
{
    juce::Component::mouseExit(event);
    hovering = false;
    repaint();
}

void RangeLockButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    if (hovering)
    {
        g.setColour(CustomLookAndFeel::getSurfaceLightColour().withAlpha(0.3f));
        g.fillRoundedRectangle(bounds, 3.0f);
    }

    drawSvgLockIcon(g, bounds, locked);
}

void RangeLockButton::ensureSvgsCached()
{
    auto lockedCol = CustomLookAndFeel::getAccentColour();
    auto unlockedCol = CustomLookAndFeel::getDimTextColour();

    if (cachedLockedSvg != nullptr && cachedLockedColour == lockedCol
        && cachedUnlockedSvg != nullptr && cachedUnlockedColour == unlockedCol)
        return;

    cachedLockedColour = lockedCol;
    cachedUnlockedColour = unlockedCol;

    auto makeSvg = [](const juce::String& pathData, juce::Colour col) -> std::unique_ptr<juce::Drawable>
    {
        juce::String hex = col.toDisplayString(false);
        juce::String svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\">"
                           "<path fill=\"#" + hex + "\" d=\"" + pathData + "\"/></svg>";
        auto xml = juce::XmlDocument::parse(svg);
        if (xml == nullptr) return nullptr;
        return juce::Drawable::createFromSVG(*xml);
    };

    // MaterialSymbolsLock filled (closed lock)
    cachedLockedSvg = makeSvg(
        "M6 22q-.825 0-1.412-.587T4 20V10q0-.825.588-1.412T6 8h1V6"
        "q0-2.075 1.463-3.537T12 1t3.538 1.463T17 6v2h1q.825 0 1.413.588T20 10v10"
        "q0 .825-.587 1.413T18 22zm6-5q.825 0 1.413-.587T14 15"
        "t-.587-1.412T12 13t-1.412.588T10 15t.588 1.413T12 17"
        "M9 8h6V6q0-1.25-.875-2.125T12 3t-2.125.875T9 6z",
        lockedCol);

    // MaterialSymbolsLockOpenRightRounded filled (open lock, right-opening)
    cachedUnlockedSvg = makeSvg(
        "M12 17q.825 0 1.413-.587T14 15t-.587-1.412T12 13"
        "t-1.412.588T10 15t.588 1.413T12 17m-6 5"
        "q-.825 0-1.412-.587T4 20V10q0-.825.588-1.412T6 8h7V6"
        "q0-2.075 1.463-3.537T18 1q1.875 0 3.263 1.213T22.925 5.2"
        "q.05.325-.225.563T22 6t-.7-.175t-.4-.575"
        "q-.275-.95-1.062-1.6T18 3q-1.25 0-2.125.875T15 6v2h3"
        "q.825 0 1.413.588T20 10v10q0 .825-.587 1.413T18 22z",
        unlockedCol);
}

void RangeLockButton::drawSvgLockIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isLocked)
{
    ensureSvgsCached();

    auto* drawable = isLocked ? cachedLockedSvg.get() : cachedUnlockedSvg.get();
    if (drawable == nullptr) return;

    float iconSize = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.85f;
    auto iconBounds = bounds.withSizeKeepingCentre(iconSize, iconSize);
    drawable->setTransformToFit(iconBounds, juce::RectanglePlacement::centred);
    drawable->draw(g, 1.0f);
}
