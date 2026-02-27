/*
  ==============================================================================

    DualRangeKnob.h
    Created: 2026
    Author:  MBM Audio

    Dual concentric arc knob for independent boost/cut range control.
    - Locked: single arc, both values move together
    - Unlocked: outer arc (boost, cyan) + inner arc (cut, purple)
    - Hit zones: outer half of knob = boost, inner half = cut
    - Lock button with SVG icon

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CustomLookAndFeel.h"

class DualRangeKnob : public juce::Component
{
public:
    DualRangeKnob();
    ~DualRangeKnob() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void setBoostValue(float dB);
    void setCutValue(float dB);
    float getBoostValue() const { return boostValue; }
    float getCutValue() const { return cutValue; }

    void setLocked(bool locked);
    bool isLocked() const { return locked; }

    std::function<void(float)> onBoostChanged;
    std::function<void(float)> onCutChanged;
    std::function<void()> onDragEnd;

    std::function<void()> onMouseEnter;
    std::function<void()> onMouseExit;

    void mouseEnter(const juce::MouseEvent& e) override
    {
        juce::Component::mouseEnter(e);
        if (onMouseEnter) onMouseEnter();
    }

    bool isMouseOver() const { return mouseHovering; }

private:
    enum class DragRing { None, Boost, Cut };

    DragRing hitTestRing(const juce::Point<float>& pos) const;

    void drawDualArcKnob(juce::Graphics& g, juce::Rectangle<float> bounds);

    float boostValue = 6.0f;   // 0-12 dB
    float cutValue = 6.0f;     // 0-12 dB
    bool locked = true;

    DragRing currentDragRing = DragRing::None;
    DragRing hoverRing = DragRing::None;
    float dragStartValue = 0.0f;
    float dragStartY = 0.0f;
    bool mouseHovering = false;

    static constexpr float minDb = 0.0f;
    static constexpr float maxDb = 12.0f;
    static constexpr float dragSensitivity = 0.15f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DualRangeKnob)
};

//==============================================================================
// Lock/Unlock toggle button with Material Design lock icon
class RangeLockButton : public juce::Component
{
public:
    RangeLockButton();
    ~RangeLockButton() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void setLocked(bool isLocked);
    bool isLocked() const { return locked; }

    std::function<void(bool)> onLockChanged;
    std::function<void()> onMouseEnterCb;
    std::function<void()> onMouseExitCb;

private:
    void drawSvgLockIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isLocked);
    void ensureSvgsCached();

    bool locked = true;
    bool hovering = false;

    std::unique_ptr<juce::Drawable> cachedLockedSvg;
    std::unique_ptr<juce::Drawable> cachedUnlockedSvg;
    juce::Colour cachedLockedColour, cachedUnlockedColour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RangeLockButton)
};
