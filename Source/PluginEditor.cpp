/*
  ==============================================================================

    PluginEditor.cpp
    Created: 2026
    Author:  MBM Audio

    Full-screen waveform with floating tab controls.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <map>

//==============================================================================
VocalRiderAudioProcessorEditor::VocalRiderAudioProcessorEditor(VocalRiderAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p)
{
    setLookAndFeel(&customLookAndFeel);
    // Disable manual resizing - only allow preset sizes
    setResizable(false, false);
    // Enable keyboard focus for shortcuts (Cmd+Z, etc.)
    setWantsKeyboardFocus(true);

    //==============================================================================
    // Header branding - compact, single line
    
    // Logo icon (waveform graphic)
    addAndMakeVisible(logoIcon);
    
    // Brand name "MBM Audio" in Syne font - FabFilter style (smaller, gray)
    brandLabel.setText("MBM Audio", juce::dontSendNotification);
    brandLabel.setFont(CustomLookAndFeel::getBrandFont(11.0f, false));
    brandLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getDimTextColour());
    brandLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(brandLabel);
    
    // Plugin name "MagicRide" in Space Grotesk - FabFilter style (larger, accent color)
    titleLabel.setText("MagicRide", juce::dontSendNotification);
    titleLabel.setFont(CustomLookAndFeel::getPluginFont(16.0f, true));
    titleLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getAccentColour());
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);
    
    // Footer text (doubles as status bar for help descriptions on hover)
    versionString = "v" + juce::String(JucePlugin_VersionString);
    footerLabel.setText(versionString, juce::dontSendNotification);
    footerLabel.setFont(CustomLookAndFeel::getPluginFont(9.0f));
    footerLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getVeryDimTextColour());
    footerLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(footerLabel);

    // Preset navigation arrows - cycle through all loadable presets
    prevPresetButton.onClick = [this] {
        auto allIds = getNavigablePresetIds();
        int current = presetComboBox.getSelectedId();
        for (int i = static_cast<int>(allIds.size()) - 1; i >= 0; --i) {
            if (allIds[static_cast<size_t>(i)] < current) {
                presetComboBox.setSelectedId(allIds[static_cast<size_t>(i)]);
                return;
            }
        }
        if (!allIds.empty())
            presetComboBox.setSelectedId(allIds.back());  // Wrap to last
    };
    addAndMakeVisible(prevPresetButton);
    
    nextPresetButton.onClick = [this] {
        auto allIds = getNavigablePresetIds();
        int current = presetComboBox.getSelectedId();
        for (size_t i = 0; i < allIds.size(); ++i) {
            if (allIds[i] > current) {
                presetComboBox.setSelectedId(allIds[i]);
                return;
            }
        }
        if (!allIds.empty())
            presetComboBox.setSelectedId(allIds.front());  // Wrap to first
    };
    addAndMakeVisible(nextPresetButton);

    presetComboBox.setTextWhenNothingSelected("Preset");
    rebuildPresetMenu();
    
    presetComboBox.onChange = [this] {
        int selectedId = presetComboBox.getSelectedId();
        if (selectedId == 1000)
        {
            // Init preset - reset to defaults
            audioProcessor.resetToDefaults();
            audioProcessor.setCurrentPresetIndex(1000);
            presetComboBox.setText("Init", juce::dontSendNotification);
            updateAdvancedControls();
        }
        else if (selectedId == 2000)
        {
            // "Save As..." action - show save dialog
            presetComboBox.setSelectedId(0, juce::dontSendNotification);  // Deselect
            showSavePresetDialog();
        }
        else if (selectedId == 2001)
        {
            // "Open Preset Folder..." - open the user presets folder in Finder
            presetComboBox.setSelectedId(0, juce::dontSendNotification);  // Deselect
            VocalRiderAudioProcessor::getUserPresetsFolder().startAsProcess();
        }
        else if (selectedId >= 3000)
        {
            // User preset: ID = 3000 + index into cachedUserPresets
            int userIdx = selectedId - 3000;
            if (userIdx >= 0 && userIdx < static_cast<int>(cachedUserPresets.size()))
            {
                audioProcessor.loadPresetFromData(cachedUserPresets[static_cast<size_t>(userIdx)]);
                audioProcessor.setCurrentPresetIndex(selectedId);
                presetComboBox.setText(cachedUserPresets[static_cast<size_t>(userIdx)].name, juce::dontSendNotification);
                updateAdvancedControls();
            }
        }
        else if (selectedId > 0)
        {
            int idx = selectedId - 1;
            audioProcessor.loadPreset(idx);
            audioProcessor.setCurrentPresetIndex(selectedId);
            updateAdvancedControls();
        }
    };
    addAndMakeVisible(presetComboBox);
    
    // A/B Compare button
    abCompareButton.onClick = [this] {
        if (abCompareButton.getToggleState()) {
            // Switching to B - save current state as A, apply B
            stateA = getCurrentState();
            applyState(stateB);
            isStateB = true;
        } else {
            // Switching to A - save current state as B, apply A
            stateB = getCurrentState();
            applyState(stateA);
            isStateB = false;
        }
    };
    addAndMakeVisible(abCompareButton);
    
    // Undo button
    undoButton.onClick = [this] { performUndo(); };
    addAndMakeVisible(undoButton);
    
    // Redo button
    redoButton.onClick = [this] { performRedo(); };
    addAndMakeVisible(redoButton);
    
    advancedButton.setClickingTogglesState(true);
    advancedButton.onClick = [this] { toggleAdvancedPanel(); };
    addAndMakeVisible(advancedButton);
    
    // Bottom bar with resize button
    bottomBar.setInterceptsMouseClicks(false, true);  // Don't intercept clicks, but allow children to receive them
    addAndMakeVisible(bottomBar);
    
    // Resize button - shows popup menu with size and scale options
    resizeButton.onSizeSelected = [this](int sizeIndex) {
        switch (sizeIndex)
        {
            case 0: setWindowSize(WindowSize::Small); break;
            case 1: setWindowSize(WindowSize::Medium); break;
            case 2: setWindowSize(WindowSize::Large); break;
        }
    };
    resizeButton.onScaleSelected = [this](int scalePercent) {
        setScale(scalePercent);
    };
    bottomBar.addAndMakeVisible(resizeButton);

    //==============================================================================
    // Waveform display
    
    addAndMakeVisible(waveformDisplay);
    audioProcessor.setWaveformDisplay(&waveformDisplay);
    
    waveformDisplay.onTargetChanged = [this](float newTarget) {
        if (auto* param = audioProcessor.getApvts().getParameter(VocalRiderAudioProcessor::targetLevelParamId))
            param->setValueNotifyingHost(param->convertTo0to1(newTarget));
    };
    
    waveformDisplay.onRangeChanged = [this](float newRange) {
        if (auto* param = audioProcessor.getApvts().getParameter(VocalRiderAudioProcessor::rangeParamId))
            param->setValueNotifyingHost(param->convertTo0to1(newRange));
        if (auto* param = audioProcessor.getApvts().getParameter(VocalRiderAudioProcessor::boostRangeParamId))
            param->setValueNotifyingHost(param->convertTo0to1(newRange));
        if (auto* param = audioProcessor.getApvts().getParameter(VocalRiderAudioProcessor::cutRangeParamId))
            param->setValueNotifyingHost(param->convertTo0to1(newRange));
    };
    
    waveformDisplay.onBoostRangeChanged = [this](float newRange) {
        if (auto* param = audioProcessor.getApvts().getParameter(VocalRiderAudioProcessor::boostRangeParamId))
            param->setValueNotifyingHost(param->convertTo0to1(newRange));
    };
    
    waveformDisplay.onCutRangeChanged = [this](float newRange) {
        if (auto* param = audioProcessor.getApvts().getParameter(VocalRiderAudioProcessor::cutRangeParamId))
            param->setValueNotifyingHost(param->convertTo0to1(newRange));
    };
    
    waveformDisplay.setRangeLocked(audioProcessor.isRangeLocked());
    
    waveformDisplay.setZoomButtonCallbacks(
        [this]() {
            setStatusBarText(waveformDisplay.isZoomEnabled()
                ? "Adaptive zoom is on \u2014 click to show full range"
                : "Click to zoom into the active signal range");
        },
        [this]() {
            clearStatusBarText();
        }
    );

    //==============================================================================
    // Control Panel (floating tab)
    
    // Control panel is invisible - just used for layout reference
    // The knobs are placed directly on the editor, not in this panel
    controlPanel.setVisible(false);
    controlPanel.setInterceptsMouseClicks(false, false);
    
    // Animated tooltip (added to be on top)
    addAndMakeVisible(valueTooltip);
    
    // Helper to setup slider with custom tooltip
    auto setupSliderWithTooltip = [this](juce::Slider& slider, const juce::String& suffix) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setTextValueSuffix(suffix);
        slider.setPopupDisplayEnabled(false, false, nullptr);  // Disable built-in popup
        addAndMakeVisible(slider);
    };
    
    // Target knob (LARGE, center)
    setupSliderWithTooltip(targetSlider, " dB");
    targetSlider.onValueChange = [this]() {
        // Update waveform display immediately when target changes
        waveformDisplay.setTargetLevel(static_cast<float>(targetSlider.getValue()));
        
        if (targetSlider.isMouseOverOrDragging()) {
            valueTooltip.showValue("TARGET", juce::String(targetSlider.getValue(), 1) + " dB", &targetSlider);
        }
    };
    targetSlider.onDragEnd = [this]() { saveStateForUndo(); };
    targetSlider.onMouseEnter = [this]() {
        currentHoveredComponent = &targetSlider;
        setStatusBarText(getShortHelpText("TARGET"));
        valueTooltip.showValue("TARGET", juce::String(targetSlider.getValue(), 1) + " dB", &targetSlider);
    };
    targetSlider.onMouseExit = [this]() {
        if (currentHoveredComponent == &targetSlider) { currentHoveredComponent = nullptr; }
        clearStatusBarText();
        valueTooltip.hideTooltip();
    };
    
    targetLabel.setText("TARGET", juce::dontSendNotification);
    targetLabel.setFont(CustomLookAndFeel::getPluginFont(10.0f, true));
    targetLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getTextColour());
    targetLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(targetLabel);
    
    // Range knob - hidden sliders for APVTS attachment
    setupSliderWithTooltip(rangeSlider, " dB");
    rangeSlider.setVisible(false);
    setupSliderWithTooltip(boostRangeSlider, " dB");
    boostRangeSlider.setVisible(false);
    setupSliderWithTooltip(cutRangeSlider, " dB");
    cutRangeSlider.setVisible(false);
    addChildComponent(rangeSlider);
    addChildComponent(boostRangeSlider);
    addChildComponent(cutRangeSlider);

    // Dual range knob (visible, custom component)
    addAndMakeVisible(dualRangeKnob);
    dualRangeKnob.setLocked(audioProcessor.isRangeLocked());
    dualRangeKnob.onBoostChanged = [this](float newVal) {
        boostRangeSlider.setValue(newVal, juce::sendNotification);
        if (dualRangeKnob.isLocked())
        {
            cutRangeSlider.setValue(newVal, juce::sendNotification);
            rangeSlider.setValue(newVal, juce::sendNotification);
        }
        updateRangeLabel();
        valueTooltip.showValue("RANGE", getRangeTooltipText(), &dualRangeKnob);
    };
    dualRangeKnob.onCutChanged = [this](float newVal) {
        cutRangeSlider.setValue(newVal, juce::sendNotification);
        if (dualRangeKnob.isLocked())
        {
            boostRangeSlider.setValue(newVal, juce::sendNotification);
            rangeSlider.setValue(newVal, juce::sendNotification);
        }
        updateRangeLabel();
        valueTooltip.showValue("RANGE", getRangeTooltipText(), &dualRangeKnob);
    };
    dualRangeKnob.onDragEnd = [this]() { saveStateForUndo(); };
    dualRangeKnob.onMouseEnter = [this]() {
        currentHoveredComponent = &dualRangeKnob;
        setStatusBarText(getShortHelpText("RANGE"));
        valueTooltip.showValue("RANGE", getRangeTooltipText(), &dualRangeKnob);
    };
    dualRangeKnob.onMouseExit = [this]() {
        if (currentHoveredComponent == &dualRangeKnob) { currentHoveredComponent = nullptr; }
        clearStatusBarText();
        valueTooltip.hideTooltip();
    };

    // Lock/unlock button for range
    addAndMakeVisible(rangeLockButton);
    rangeLockButton.setLocked(audioProcessor.isRangeLocked());
    rangeLockButton.onLockChanged = [this](bool isLocked) {
        audioProcessor.setRangeLocked(isLocked);
        dualRangeKnob.setLocked(isLocked);
        if (isLocked)
        {
            float avg = (dualRangeKnob.getBoostValue() + dualRangeKnob.getCutValue()) / 2.0f;
            dualRangeKnob.setBoostValue(avg);
            dualRangeKnob.setCutValue(avg);
            boostRangeSlider.setValue(avg, juce::sendNotification);
            cutRangeSlider.setValue(avg, juce::sendNotification);
            rangeSlider.setValue(avg, juce::sendNotification);
        }
        waveformDisplay.setRangeLocked(isLocked);
        updateRangeLabel();
        saveStateForUndo();
    };
    rangeLockButton.onMouseEnterCb = [this]() {
        currentHoveredComponent = &rangeLockButton;
        setStatusBarText(rangeLockButton.isLocked()
            ? "Unlock to set boost and cut range independently"
            : "Lock to move boost and cut range together");
    };
    rangeLockButton.onMouseExitCb = [this]() {
        if (currentHoveredComponent == &rangeLockButton) currentHoveredComponent = nullptr;
        clearStatusBarText();
    };

    rangeLabel.setText("RANGE", juce::dontSendNotification);
    rangeLabel.setFont(CustomLookAndFeel::getPluginFont(9.0f, true));
    rangeLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getTextColour());
    rangeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(rangeLabel);
    
    // Speed knob
    setupSliderWithTooltip(speedSlider, "%");
    speedSlider.onValueChange = [this] {
        // Only update attack/release from speed when the user is actively dragging,
        // not during programmatic value changes (e.g. state restoration via APVTS attachment)
        if (speedSlider.isMouseButtonDown())
        {
            audioProcessor.updateAttackReleaseFromSpeed(static_cast<float>(speedSlider.getValue()), true);
            if (advancedPanelVisible) updateAdvancedControls();
        }
        if (speedSlider.isMouseOverOrDragging()) {
            valueTooltip.showValue("SPEED", juce::String(static_cast<int>(speedSlider.getValue())) + "%", &speedSlider);
        }
    };
    speedSlider.onDragEnd = [this]() { saveStateForUndo(); };
    speedSlider.onMouseEnter = [this]() {
        currentHoveredComponent = &speedSlider;
        setStatusBarText(getShortHelpText("SPEED"));
        valueTooltip.showValue("SPEED", juce::String(static_cast<int>(speedSlider.getValue())) + "%", &speedSlider);
    };
    speedSlider.onMouseExit = [this]() {
        if (currentHoveredComponent == &speedSlider) { currentHoveredComponent = nullptr; }
        clearStatusBarText();
        valueTooltip.hideTooltip();
    };
    
    speedLabel.setText("SPEED", juce::dontSendNotification);
    speedLabel.setFont(CustomLookAndFeel::getPluginFont(9.0f, true));
    speedLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getTextColour());
    speedLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(speedLabel);
    
    // Speed icons (turtle = slow on left, rabbit = fast on right)
    addAndMakeVisible(turtleIcon);
    addAndMakeVisible(rabbitIcon);
    
    // Mini gain meter
    addAndMakeVisible(miniGainMeter);
    gainMeterLabel.setText("GAIN", juce::dontSendNotification);
    gainMeterLabel.setFont(CustomLookAndFeel::getPluginFont(8.0f, true));
    gainMeterLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getDimTextColour());
    gainMeterLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(gainMeterLabel);
    
    // Natural toggle - synced via ButtonAttachment to APVTS parameter
    // The APVTS parameter is read in processBlock to set naturalModeEnabled
    addAndMakeVisible(naturalToggle);
    
    // Smart Silence toggle - synced from processor state in updateAdvancedControls()
    silenceToggle.onClick = [this] {
        audioProcessor.setSmartSilenceEnabled(silenceToggle.getToggleState());
    };
    addAndMakeVisible(silenceToggle);
    
    // Auto-Target button in bottom bar - single click to activate
    autoTargetButton.setClickingTogglesState(false);  // Don't toggle, just click
    autoTargetButton.onClick = [this] {
        // Always start learning on click
        autoTargetButton.setToggleState(true, juce::dontSendNotification);
        autoTargetButton.setPulsing(true);  // Start pulsing animation
        learnCountdown = 90;   // 3 seconds at 30Hz timer
        // Reset learning stats - min starts HIGH so actual audio is captured
        learnMinDb = 6.0f;     // Start high - actual audio will be lower
        learnMaxDb = -100.0f;  // Start low - actual audio will be higher
        learnSumDb = 0.0f;
        learnSampleCount = 0;
    };
    addAndMakeVisible(autoTargetButton);
    
    // Old Learn button - hidden (functionality moved to autoTargetButton)
    // learnButton kept for backwards compatibility but not shown
    
    // Speed button removed - scroll speed is now fixed at 8 seconds
    // speedButton hidden but kept for backwards compatibility
    
    // Automation mode selector - matches enum order!
    // Enum: Off=0, Read=1, Touch=2, Latch=3, Write=4
    automationModeComboBox.addItem("OFF", 1);     // ID 1 = mode 0 = Off
    automationModeComboBox.addItem("READ", 2);    // ID 2 = mode 1 = Read
    automationModeComboBox.addItem("TOUCH", 3);   // ID 3 = mode 2 = Touch
    automationModeComboBox.addItem("LATCH", 4);   // ID 4 = mode 3 = Latch
    automationModeComboBox.addItem("WRITE", 5);   // ID 5 = mode 4 = Write
    automationModeComboBox.setSelectedId(static_cast<int>(audioProcessor.getAutomationMode()) + 1, juce::dontSendNotification);
    automationModeComboBox.onChange = [this] {
        int mode = automationModeComboBox.getSelectedId() - 1;
        audioProcessor.setAutomationMode(static_cast<VocalRiderAudioProcessor::AutomationMode>(mode));
    };
    automationModeComboBox.setColour(juce::ComboBox::backgroundColourId, CustomLookAndFeel::getPanelColour());
    automationModeComboBox.setColour(juce::ComboBox::textColourId, CustomLookAndFeel::getTextColour());
    automationModeComboBox.setColour(juce::ComboBox::outlineColourId, CustomLookAndFeel::getDimTextColour().withAlpha(0.3f));
    addAndMakeVisible(automationModeComboBox);
    
    // Automation label next to dropdown
    automationLabel.setText("AUTO", juce::dontSendNotification);
    automationLabel.setFont(CustomLookAndFeel::getPluginFont(8.0f, true));
    automationLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getDimTextColour());
    automationLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(automationLabel);
    
    // Natural toggle - status bar help on hover
    naturalToggle.onMouseEnter = [this]() {
        currentHoveredComponent = &naturalToggle;
        setStatusBarText(getShortHelpText("NATURAL"));
    };
    naturalToggle.onMouseExit = [this]() {
        if (currentHoveredComponent == &naturalToggle) { currentHoveredComponent = nullptr; }
        clearStatusBarText();
    };
    
    // Silence toggle - status bar help on hover
    silenceToggle.onMouseEnter = [this]() {
        currentHoveredComponent = &silenceToggle;
        setStatusBarText(getShortHelpText("SILENCE"));
    };
    silenceToggle.onMouseExit = [this]() {
        if (currentHoveredComponent == &silenceToggle) { currentHoveredComponent = nullptr; }
        clearStatusBarText();
    };
    
    // Auto-target button - status bar help on hover
    autoTargetButton.onMouseEnter = [this]() {
        currentHoveredComponent = &autoTargetButton;
        setStatusBarText(getShortHelpText("AUTOTARGET"));
    };
    autoTargetButton.onMouseExit = [this]() {
        if (currentHoveredComponent == &autoTargetButton) { currentHoveredComponent = nullptr; }
        clearStatusBarText();
    };
    
    // Help button opens About dialog (not toggle - only X closes it)
    helpButton.setClickingTogglesState(false);
    helpButton.onClick = [this] {
        if (!aboutDialog.isShowing())
            aboutDialog.show();
    };
    addAndMakeVisible(helpButton);
    
    // Setup About dialog panel (initially hidden, modal-like behavior)
    aboutDialog.setVersion(JucePlugin_VersionString);
    aboutDialog.setVisible(false);
    aboutDialog.onClose = [this] {
        // No toggle state to sync - button is not a toggle anymore
    };
    addAndMakeVisible(aboutDialog);
    
    // Resize button callbacks are set up earlier where it's added to bottomBar
    
    // Parameter attachments
    targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::targetLevelParamId, targetSlider);
    rangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::rangeParamId, rangeSlider);
    boostRangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::boostRangeParamId, boostRangeSlider);
    cutRangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::cutRangeParamId, cutRangeSlider);
    speedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::speedParamId, speedSlider);
    
    // Advanced parameter attachments
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::attackParamId, attackSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::releaseParamId, releaseSlider);
    holdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::holdParamId, holdSlider);
    breathReductionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::breathReductionParamId, breathReductionSlider);
    transientPreservationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::transientPreservationParamId, transientPreservationSlider);
    naturalModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::naturalModeParamId, naturalToggle);
    smartSilenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::smartSilenceParamId, silenceToggle);
    noiseFloorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::noiseFloorParamId, noiseFloorSlider);

    // Initialize undo history with current state
    undoHistory.push_back(getCurrentState());
    undoIndex = 0;
    
    // Initialize A/B states
    stateA = getCurrentState();
    stateB = getCurrentState();

    //==============================================================================
    // Advanced Panel with fade animation
    
    advancedPanel.setVisible(false);
    advancedPanel.onAnimationUpdate = [this] {
        // Update child component opacity during fade animation
        float alpha = advancedPanel.getCurrentOpacity();
        advancedHeaderLabel.setAlpha(alpha);
        lookAheadComboBox.setAlpha(alpha);
        detectionModeComboBox.setAlpha(alpha);
        attackSlider.setAlpha(alpha);
        releaseSlider.setAlpha(alpha);
        holdSlider.setAlpha(alpha);
        breathReductionSlider.setAlpha(alpha);
        transientPreservationSlider.setAlpha(alpha);
        noiseFloorSlider.setAlpha(alpha);
        outputTrimMeter.setAlpha(alpha);
        attackLabel.setAlpha(alpha);
        releaseLabel.setAlpha(alpha);
        holdLabel.setAlpha(alpha);
        breathLabel.setAlpha(alpha);
        transientLabel.setAlpha(alpha);
        noiseFloorLabel.setAlpha(alpha);
        outputTrimLabel.setAlpha(alpha);
    trimMinLabel.setAlpha(alpha);
    trimMidLabel.setAlpha(alpha);
    trimMaxLabel.setAlpha(alpha);
    
    // Hide components when fully faded out
    if (advancedPanel.isFullyHidden() && !advancedPanelVisible)
    {
        advancedPanel.setVisible(false);
        advancedHeaderLabel.setVisible(false);
        lookAheadComboBox.setVisible(false);
        detectionModeComboBox.setVisible(false);
        attackSlider.setVisible(false);
        releaseSlider.setVisible(false);
        holdSlider.setVisible(false);
        breathReductionSlider.setVisible(false);
        transientPreservationSlider.setVisible(false);
        noiseFloorSlider.setVisible(false);
        outputTrimMeter.setVisible(false);
        attackLabel.setVisible(false);
        releaseLabel.setVisible(false);
        holdLabel.setVisible(false);
        breathLabel.setVisible(false);
        transientLabel.setVisible(false);
        noiseFloorLabel.setVisible(false);
        outputTrimLabel.setVisible(false);
        trimMinLabel.setVisible(false);
        trimMidLabel.setVisible(false);
        trimMaxLabel.setVisible(false);
    }
        
        repaint();
    };
    addAndMakeVisible(advancedPanel);
    
    // Advanced panel header
    advancedHeaderLabel.setText("ADVANCED SETTINGS", juce::dontSendNotification);
    advancedHeaderLabel.setFont(CustomLookAndFeel::getPluginFont(10.0f, true));
    advancedHeaderLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getDimTextColour());
    advancedHeaderLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(advancedHeaderLabel);
    advancedHeaderLabel.setVisible(false);
    
    lookAheadComboBox.addItem("Look-Ahead: Off", 1);
    lookAheadComboBox.addItem("Look-Ahead: 10ms", 2);
    lookAheadComboBox.addItem("Look-Ahead: 20ms", 3);
    lookAheadComboBox.addItem("Look-Ahead: 30ms", 4);
    lookAheadComboBox.setSelectedId(1);
    lookAheadComboBox.onChange = [this] {
        audioProcessor.setLookAheadMode(lookAheadComboBox.getSelectedId() - 1);
    };
    addAndMakeVisible(lookAheadComboBox);
    lookAheadComboBox.setVisible(false);
    
    detectionModeComboBox.addItem("Detection: RMS", 1);
    detectionModeComboBox.addItem("Detection: LUFS", 2);
    detectionModeComboBox.setSelectedId(1);
    detectionModeComboBox.onChange = [this] {
        audioProcessor.setUseLufs(detectionModeComboBox.getSelectedId() == 2);
    };
    addAndMakeVisible(detectionModeComboBox);
    detectionModeComboBox.setVisible(false);
    
    auto setupAdvSlider = [this](TooltipSlider& slider, double min, double max, const juce::String& suffix) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);  // Hide text, use custom tooltip
        slider.setRange(min, max, 1.0);
        slider.setTextValueSuffix(suffix);
        slider.setPopupDisplayEnabled(false, false, nullptr);  // Disable built-in popup
        addAndMakeVisible(slider);
        slider.setVisible(false);
    };
    
    auto setupAdvLabel = [this](juce::Label& label, const juce::String& text) {
        label.setText(text, juce::dontSendNotification);
        label.setFont(CustomLookAndFeel::getPluginFont(8.0f, true));
        label.setColour(juce::Label::textColourId, CustomLookAndFeel::getTextColour());  // White
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
        label.setVisible(false);
    };
    
    // Advanced panel sliders - tooltips appear BELOW the knobs
    setupAdvSlider(attackSlider, 1.0, 500.0, "ms");
    attackSlider.onValueChange = [this] {
        audioProcessor.setAttackMs(static_cast<float>(attackSlider.getValue()));
        if (attackSlider.isMouseOverOrDragging()) {
            valueTooltip.showValue("ATTACK", juce::String(static_cast<int>(attackSlider.getValue())) + " ms", &attackSlider);
        }
    };
    attackSlider.onMouseEnter = [this]() {
        currentHoveredComponent = &attackSlider;
        setStatusBarText(getShortHelpText("ATTACK"));
        valueTooltip.showValue("ATTACK", juce::String(static_cast<int>(attackSlider.getValue())) + " ms", &attackSlider);
    };
    attackSlider.onMouseExit = [this]() {
        if (currentHoveredComponent == &attackSlider) { currentHoveredComponent = nullptr; }
        clearStatusBarText();
    };
    
    setupAdvSlider(releaseSlider, 10.0, 2000.0, "ms");
    releaseSlider.onValueChange = [this] {
        audioProcessor.setReleaseMs(static_cast<float>(releaseSlider.getValue()));
        if (releaseSlider.isMouseOverOrDragging()) {
            valueTooltip.showValue("RELEASE", juce::String(static_cast<int>(releaseSlider.getValue())) + " ms", &releaseSlider);
        }
    };
    releaseSlider.onMouseEnter = [this]() {
        currentHoveredComponent = &releaseSlider;
        setStatusBarText(getShortHelpText("RELEASE"));
        valueTooltip.showValue("RELEASE", juce::String(static_cast<int>(releaseSlider.getValue())) + " ms", &releaseSlider);
    };
    releaseSlider.onMouseExit = [this]() {
        if (currentHoveredComponent == &releaseSlider) { currentHoveredComponent = nullptr; }
        clearStatusBarText();
    };
    
    setupAdvSlider(holdSlider, 0.0, 500.0, "ms");
    holdSlider.onValueChange = [this] {
        audioProcessor.setHoldMs(static_cast<float>(holdSlider.getValue()));
        if (holdSlider.isMouseOverOrDragging()) {
            valueTooltip.showValue("HOLD", juce::String(static_cast<int>(holdSlider.getValue())) + " ms", &holdSlider);
        }
    };
    holdSlider.onMouseEnter = [this]() {
        currentHoveredComponent = &holdSlider;
        setStatusBarText(getShortHelpText("HOLD"));
        valueTooltip.showValue("HOLD", juce::String(static_cast<int>(holdSlider.getValue())) + " ms", &holdSlider);
    };
    holdSlider.onMouseExit = [this]() {
        if (currentHoveredComponent == &holdSlider) { currentHoveredComponent = nullptr; }
        clearStatusBarText();
    };
    
    setupAdvSlider(breathReductionSlider, 0.0, 12.0, "dB");
    breathReductionSlider.onValueChange = [this] {
        audioProcessor.setBreathReduction(static_cast<float>(breathReductionSlider.getValue()));
        if (breathReductionSlider.isMouseOverOrDragging()) {
            double val = breathReductionSlider.getValue();
            juce::String valStr = (val > 0.05 ? "-" : "") + juce::String(val, 1) + " dB";
            valueTooltip.showValue("BREATH", valStr, &breathReductionSlider);
        }
    };
    breathReductionSlider.onMouseEnter = [this]() {
        currentHoveredComponent = &breathReductionSlider;
        setStatusBarText(getShortHelpText("BREATH"));
        double val = breathReductionSlider.getValue();
        juce::String valStr = (val > 0.05 ? "-" : "") + juce::String(val, 1) + " dB";
        valueTooltip.showValue("BREATH", valStr, &breathReductionSlider);
    };
    breathReductionSlider.onMouseExit = [this]() {
        if (currentHoveredComponent == &breathReductionSlider) { currentHoveredComponent = nullptr; }
        clearStatusBarText();
    };
    
    setupAdvSlider(transientPreservationSlider, 0.0, 100.0, "%");
    transientPreservationSlider.onValueChange = [this] {
        audioProcessor.setTransientPreservation(static_cast<float>(transientPreservationSlider.getValue()) / 100.0f);
        if (transientPreservationSlider.isMouseOverOrDragging()) {
            valueTooltip.showValue("TRANSIENT", juce::String(static_cast<int>(transientPreservationSlider.getValue())) + "%", &transientPreservationSlider);
        }
    };
    transientPreservationSlider.onMouseEnter = [this]() {
        currentHoveredComponent = &transientPreservationSlider;
        setStatusBarText(getShortHelpText("TRANSIENT"));
        valueTooltip.showValue("TRANSIENT", juce::String(static_cast<int>(transientPreservationSlider.getValue())) + "%", &transientPreservationSlider);
    };
    transientPreservationSlider.onMouseExit = [this]() {
        if (currentHoveredComponent == &transientPreservationSlider) { currentHoveredComponent = nullptr; }
        clearStatusBarText();
    };
    
    setupAdvSlider(noiseFloorSlider, -60.0, -20.0, "dB");
    noiseFloorSlider.setRange(-60.0, -20.0, 0.5);  // Override: smoother 0.5 dB steps
    noiseFloorSlider.onValueChange = [this] {
        audioProcessor.setNoiseFloor(static_cast<float>(noiseFloorSlider.getValue()));
        if (noiseFloorSlider.isMouseOverOrDragging()) {
            juce::String valStr = juce::String(noiseFloorSlider.getValue(), 1) + " dB";
            if (noiseFloorSlider.getValue() <= -59.9) valStr = "OFF";
            valueTooltip.showValue("NOISE FLOOR", valStr, &noiseFloorSlider);
        }
    };
    noiseFloorSlider.onMouseEnter = [this]() {
        currentHoveredComponent = &noiseFloorSlider;
        setStatusBarText(getShortHelpText("NOISEFLOOR"));
        juce::String valStr = juce::String(noiseFloorSlider.getValue(), 1) + " dB";
        if (noiseFloorSlider.getValue() <= -59.9) valStr = "OFF";
        valueTooltip.showValue("NOISE FLOOR", valStr, &noiseFloorSlider);
    };
    noiseFloorSlider.onMouseExit = [this]() {
        if (currentHoveredComponent == &noiseFloorSlider) { currentHoveredComponent = nullptr; }
        clearStatusBarText();
    };
    
    // Output Trim - AdjustableGainMeter style (similar to MiniGainMeter but adjustable)
    outputTrimMeter.setValue(0.0f);  // Start at 0 dB
    outputTrimMeter.onValueChanged = [this](float valueDb) {
        audioProcessor.setOutputTrim(valueDb);
        juce::String valStr = (valueDb >= 0 ? "+" : "") + juce::String(valueDb, 1) + " dB";
        valueTooltip.showValue("OUTPUT", valStr, &outputTrimMeter);
    };
    outputTrimMeter.onMouseEnter = [this]() {
        currentHoveredComponent = &outputTrimMeter;
        setStatusBarText(getShortHelpText("OUTPUT"));
        float val = outputTrimMeter.getValue();
        juce::String valStr = (val >= 0 ? "+" : "") + juce::String(val, 1) + " dB";
        valueTooltip.showValue("OUTPUT", valStr, &outputTrimMeter);
    };
    outputTrimMeter.onMouseExit = [this]() {
        if (currentHoveredComponent == &outputTrimMeter) { currentHoveredComponent = nullptr; }
        clearStatusBarText();
    };
    addAndMakeVisible(outputTrimMeter);
    outputTrimMeter.setVisible(false);
    // Output Trim meter mouse enter/exit for tooltip
    outputTrimMeter.addMouseListener(this, false);
    
    setupAdvLabel(attackLabel, "ATTACK");
    setupAdvLabel(releaseLabel, "RELEASE");
    setupAdvLabel(holdLabel, "HOLD");
    setupAdvLabel(breathLabel, "BREATH");
    setupAdvLabel(transientLabel, "TRANSIENT");
    setupAdvLabel(noiseFloorLabel, "NOISE FL.");
    setupAdvLabel(outputTrimLabel, "OUTPUT");
    
    // Output trim range indicator labels (faint, around knob)
    auto setupTrimRangeLabel = [this](juce::Label& label, const juce::String& text) {
        label.setText(text, juce::dontSendNotification);
        label.setFont(CustomLookAndFeel::getPluginFont(7.0f, false));
        label.setColour(juce::Label::textColourId, CustomLookAndFeel::getDimTextColour().withAlpha(0.5f));
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
    };
    setupTrimRangeLabel(trimMinLabel, "-12");
    setupTrimRangeLabel(trimMidLabel, "0");
    setupTrimRangeLabel(trimMaxLabel, "+12");
    
    // Sidechain UI (infrastructure kept, hidden for now — future update)
    sidechainToggle.setColour(juce::ToggleButton::textColourId, CustomLookAndFeel::getTextColour());
    sidechainToggle.setColour(juce::ToggleButton::tickColourId, CustomLookAndFeel::getAccentColour());
    sidechainToggle.setToggleState(audioProcessor.isSidechainEnabled(), juce::dontSendNotification);
    sidechainToggle.onClick = [this] {
        bool on = sidechainToggle.getToggleState();
        audioProcessor.setSidechainEnabled(on);
        waveformDisplay.setSidechainActive(on);
    };
    addChildComponent(sidechainToggle);
    
    setupAdvSlider(sidechainOffsetSlider, 0.0, 18.0, "dB");
    sidechainOffsetSlider.setValue(audioProcessor.getSidechainAmount(), juce::dontSendNotification);
    sidechainOffsetSlider.onValueChange = [this] {
        audioProcessor.setSidechainAmount(static_cast<float>(sidechainOffsetSlider.getValue()));
    };
    addChildComponent(sidechainOffsetSlider);
    setupAdvLabel(sidechainOffsetLabel, "SC OFFSET");
    sidechainOffsetLabel.setVisible(false);

    // Restore window size from saved state (default to Medium)
    int savedSizeIndex = audioProcessor.getWindowSizeIndex();
    switch (savedSizeIndex)
    {
        case 0: setWindowSize(WindowSize::Small); break;
        case 2: setWindowSize(WindowSize::Large); break;
        default: setWindowSize(WindowSize::Medium); break;
    }
    startTimerHz(30);
    updateAdvancedControls();
}

VocalRiderAudioProcessorEditor::~VocalRiderAudioProcessorEditor()
{
    // FIRST: Stop audio thread from accessing our waveform display
    // Must happen before anything else to prevent use-after-free
    audioProcessor.setWaveformDisplay(nullptr);
    
    stopTimer();
    
    // Clear ALL attachments BEFORE sliders/buttons are destroyed
    // This prevents crash when attachment tries to remove listener from destroyed component
    targetAttachment.reset();
    rangeAttachment.reset();
    speedAttachment.reset();
    attackAttachment.reset();
    releaseAttachment.reset();
    holdAttachment.reset();
    breathReductionAttachment.reset();
    transientPreservationAttachment.reset();
    naturalModeAttachment.reset();
    smartSilenceAttachment.reset();
    outputTrimAttachment.reset();
    noiseFloorAttachment.reset();
    
    setLookAndFeel(nullptr);
}

//==============================================================================
void VocalRiderAudioProcessorEditor::rebuildPresetMenu()
{
    const auto& presets = VocalRiderAudioProcessor::getFactoryPresets();
    cachedUserPresets = VocalRiderAudioProcessor::loadUserPresets();
    
    auto* rootMenu = presetComboBox.getRootMenu();
    rootMenu->clear();
    
    // Add "Init" preset at the very beginning (ID = 1000 to avoid conflict)
    rootMenu->addItem(1000, "Init");
    rootMenu->addSeparator();
    
    // Factory presets organized by category (IDs start at 1)
    std::map<juce::String, juce::PopupMenu> categoryMenus;
    
    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
    {
        const auto& preset = presets[static_cast<size_t>(i)];
        categoryMenus[preset.category].addItem(i + 1, preset.name);
    }
    
    const std::vector<juce::String> categoryOrder = { "Vocals", "Speaking", "Mattie's Favorites" };
    for (const auto& category : categoryOrder)
    {
        if (categoryMenus.find(category) != categoryMenus.end())
        {
            rootMenu->addSubMenu(category, categoryMenus[category]);
        }
    }
    
    // User presets section (if any exist)
    if (!cachedUserPresets.empty())
    {
        rootMenu->addSeparator();
        juce::PopupMenu userMenu;
        for (int i = 0; i < static_cast<int>(cachedUserPresets.size()); ++i)
        {
            userMenu.addItem(3000 + i, cachedUserPresets[static_cast<size_t>(i)].name);
        }
        rootMenu->addSubMenu("User", userMenu);
    }
    
    // Save As and Open Folder options at the bottom
    rootMenu->addSeparator();
    rootMenu->addItem(2000, "Save As...");
    rootMenu->addItem(2001, "Open Preset Folder...");
}

std::vector<int> VocalRiderAudioProcessorEditor::getNavigablePresetIds() const
{
    std::vector<int> ids;
    // Init
    ids.push_back(1000);
    // Factory presets (1-based)
    const auto& factory = VocalRiderAudioProcessor::getFactoryPresets();
    for (int i = 0; i < static_cast<int>(factory.size()); ++i)
        ids.push_back(i + 1);
    // User presets
    for (int i = 0; i < static_cast<int>(cachedUserPresets.size()); ++i)
        ids.push_back(3000 + i);
    // Sort so navigation is in order: factory (1-18), Init (1000), User (3000+)
    std::sort(ids.begin(), ids.end());
    return ids;
}

void VocalRiderAudioProcessorEditor::showSavePresetDialog()
{
    // Create a custom styled overlay component matching the plugin aesthetic
    // Stored in a unique_ptr member so it's cleaned up if the editor is destroyed
    saveDialogOverlay = std::make_unique<juce::Component>();
    auto* overlay = saveDialogOverlay.get();
    overlay->setBounds(getLocalBounds());
    overlay->setAlwaysOnTop(true);
    addAndMakeVisible(overlay);
    overlay->toFront(true);
    
    // Text editor for preset name
    auto* nameEditor = new juce::TextEditor();
    nameEditor->setFont(CustomLookAndFeel::getPluginFont(12.0f, false));
    nameEditor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1A1D22));
    nameEditor->setColour(juce::TextEditor::textColourId, juce::Colour(0xFFD0D4DC));
    nameEditor->setColour(juce::TextEditor::outlineColourId, CustomLookAndFeel::getAccentColour().withAlpha(0.5f));
    nameEditor->setColour(juce::TextEditor::focusedOutlineColourId, CustomLookAndFeel::getAccentColour());
    nameEditor->setColour(juce::CaretComponent::caretColourId, CustomLookAndFeel::getAccentColour());
    nameEditor->setJustification(juce::Justification::centredLeft);
    nameEditor->setTextToShowWhenEmpty("Enter preset name...", juce::Colour(0xFF606878));
    overlay->addAndMakeVisible(nameEditor);
    
    // Save button
    auto* saveBtn = new juce::TextButton("Save");
    saveBtn->setColour(juce::TextButton::buttonColourId, CustomLookAndFeel::getAccentColour().withAlpha(0.4f));
    saveBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFD0D4DC));
    overlay->addAndMakeVisible(saveBtn);
    
    // Cancel button
    auto* cancelBtn = new juce::TextButton("Cancel");
    cancelBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2D35));
    cancelBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF8A8F98));
    overlay->addAndMakeVisible(cancelBtn);
    
    // Layout
    float dialogW = 260.0f;
    float dialogH = 130.0f;
    float cx = static_cast<float>(getWidth()) / 2.0f;
    float cy = static_cast<float>(getHeight()) / 2.0f;
    auto dialogBounds = juce::Rectangle<float>(cx - dialogW / 2.0f, cy - dialogH / 2.0f, dialogW, dialogH);
    
    nameEditor->setBounds(static_cast<int>(dialogBounds.getX()) + 20,
                           static_cast<int>(dialogBounds.getY()) + 50,
                           static_cast<int>(dialogW) - 40, 26);
    saveBtn->setBounds(static_cast<int>(dialogBounds.getX()) + 20,
                        static_cast<int>(dialogBounds.getBottom()) - 38,
                        static_cast<int>(dialogW / 2.0f) - 30, 26);
    cancelBtn->setBounds(static_cast<int>(dialogBounds.getCentreX()) + 10,
                          static_cast<int>(dialogBounds.getBottom()) - 38,
                          static_cast<int>(dialogW / 2.0f) - 30, 26);
    
    // Paint custom background
    overlay->setPaintingIsUnclipped(true);
    auto dialogBoundsCopy = dialogBounds;
    overlay->setBufferedToImage(false);
    
    struct SaveDialogOverlay : public juce::Component
    {
        juce::Rectangle<float> dlgBounds;
        void paint(juce::Graphics& g) override
        {
            // Dark semi-transparent backdrop
            g.setColour(juce::Colours::black.withAlpha(0.4f));
            g.fillRect(getLocalBounds());
            
            // Dialog shadow
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.fillRoundedRectangle(dlgBounds.translated(0.0f, 6.0f), 10.0f);
            
            // Dialog background
            juce::ColourGradient bgGrad(
                juce::Colour(0xFF282C34), dlgBounds.getX(), dlgBounds.getY(),
                juce::Colour(0xFF1A1D22), dlgBounds.getX(), dlgBounds.getBottom(), false);
            g.setGradientFill(bgGrad);
            g.fillRoundedRectangle(dlgBounds, 10.0f);
            
            // Border
            g.setColour(CustomLookAndFeel::getBorderColour().withAlpha(0.6f));
            g.drawRoundedRectangle(dlgBounds, 10.0f, 1.0f);
            
            // Accent line
            g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(0.5f));
            g.fillRoundedRectangle(dlgBounds.getX() + 30, dlgBounds.getY() + 4, dlgBounds.getWidth() - 60, 2.0f, 1.0f);
            
            // Title
            g.setColour(juce::Colour(0xFFD0D4DC));
            g.setFont(CustomLookAndFeel::getPluginFont(13.0f, true));
            g.drawText("Save Preset", dlgBounds.withHeight(40.0f).translated(0, 10), juce::Justification::centred);
        }
        void mouseDown(const juce::MouseEvent&) override {} // Block clicks behind
    };
    
    auto* bg = new SaveDialogOverlay();
    bg->dlgBounds = dialogBounds;
    bg->setBounds(getLocalBounds());
    bg->setInterceptsMouseClicks(true, true);
    overlay->addAndMakeVisible(bg);
    bg->toBack();  // Behind the text editor and buttons
    
    nameEditor->grabKeyboardFocus();
    
    // Callbacks
    // Use callAsync with SafePointer to defer deletion safely.
    // SafePointer prevents crash if the editor is destroyed before async call executes.
    juce::Component::SafePointer<juce::Component> safeOverlay(overlay);
    juce::Component::SafePointer<VocalRiderAudioProcessorEditor> safeEditor(this);
    
    saveBtn->onClick = [safeEditor, nameEditor]() {
        if (safeEditor != nullptr)
        {
            auto name = nameEditor->getText().trim();
            if (name.isNotEmpty())
            {
                safeEditor->audioProcessor.saveUserPreset(name);
                safeEditor->rebuildPresetMenu();
                safeEditor->presetComboBox.setText(name, juce::dontSendNotification);
            }
            // Release the overlay (destroys it and all children safely)
            safeEditor->saveDialogOverlay.reset();
        }
    };
    
    cancelBtn->onClick = [safeEditor]() {
        if (safeEditor != nullptr)
            safeEditor->saveDialogOverlay.reset();
    };
    
    // Enter key saves
    nameEditor->onReturnKey = [saveBtn]() {
        saveBtn->triggerClick();
    };
    
    // Escape key cancels
    nameEditor->onEscapeKey = [cancelBtn]() {
        cancelBtn->triggerClick();
    };
}

//==============================================================================
void VocalRiderAudioProcessorEditor::updateAdvancedControls()
{
    attackSlider.setValue(audioProcessor.getAttackMs(), juce::dontSendNotification);
    releaseSlider.setValue(audioProcessor.getReleaseMs(), juce::dontSendNotification);
    holdSlider.setValue(audioProcessor.getHoldMs(), juce::dontSendNotification);
    lookAheadComboBox.setSelectedId(audioProcessor.getLookAheadMode() + 1, juce::dontSendNotification);
    naturalToggle.setToggleState(audioProcessor.isNaturalModeEnabled(), juce::dontSendNotification);
    detectionModeComboBox.setSelectedId(audioProcessor.getUseLufs() ? 2 : 1, juce::dontSendNotification);
    breathReductionSlider.setValue(audioProcessor.getBreathReduction(), juce::dontSendNotification);
    transientPreservationSlider.setValue(audioProcessor.getTransientPreservation() * 100.0f, juce::dontSendNotification);
    outputTrimMeter.setValue(audioProcessor.getOutputTrim());
    noiseFloorSlider.setValue(audioProcessor.getNoiseFloor(), juce::dontSendNotification);
    // Sidechain UI hidden for now — force disabled to prevent stale state
    audioProcessor.setSidechainEnabled(false);
    waveformDisplay.setSidechainActive(false);
    sidechainToggle.setToggleState(false, juce::dontSendNotification);
    sidechainOffsetSlider.setValue(audioProcessor.getSidechainAmount(), juce::dontSendNotification);
    
    // Sync range lock state and dual range knob from processor
    bool locked = audioProcessor.isRangeLocked();
    rangeLockButton.setLocked(locked);
    dualRangeKnob.setLocked(locked);
    waveformDisplay.setRangeLocked(locked);
    dualRangeKnob.setBoostValue(static_cast<float>(boostRangeSlider.getValue()));
    dualRangeKnob.setCutValue(static_cast<float>(cutRangeSlider.getValue()));
    
    // Bottom bar UI state
    silenceToggle.setToggleState(audioProcessor.isSmartSilenceEnabled(), juce::dontSendNotification);
    automationModeComboBox.setSelectedId(static_cast<int>(audioProcessor.getAutomationMode()) + 1, juce::dontSendNotification);
    
    // Scroll speed restoration
    // Scroll speed is now fixed - no restoration needed
    
    // Preset restoration
    int presetIdx = audioProcessor.getCurrentPresetIndex();
    if (presetIdx > 0)
        presetComboBox.setSelectedId(presetIdx, juce::dontSendNotification);
}

void VocalRiderAudioProcessorEditor::toggleAdvancedPanel()
{
    advancedPanelVisible = advancedButton.getToggleState();
    
    // Fade animation - set target opacity
    advancedPanel.setTargetOpacity(advancedPanelVisible ? 0.92f : 0.0f);
    
    // Always show panel and children during animation (visibility controlled by opacity)
    advancedPanel.setVisible(true);
    advancedHeaderLabel.setVisible(true);
    lookAheadComboBox.setVisible(true);
    detectionModeComboBox.setVisible(true);
    attackSlider.setVisible(true);
    releaseSlider.setVisible(true);
    holdSlider.setVisible(true);
    breathReductionSlider.setVisible(true);
    transientPreservationSlider.setVisible(true);
    noiseFloorSlider.setVisible(true);
    outputTrimMeter.setVisible(true);
    attackLabel.setVisible(true);
    releaseLabel.setVisible(true);
    holdLabel.setVisible(true);
    breathLabel.setVisible(true);
    transientLabel.setVisible(true);
    noiseFloorLabel.setVisible(true);
    outputTrimLabel.setVisible(true);
    trimMinLabel.setVisible(true);
    trimMidLabel.setVisible(true);
    trimMaxLabel.setVisible(true);
    
    // Set child component opacity via alpha
    float alpha = advancedPanel.getCurrentOpacity();
    advancedHeaderLabel.setAlpha(alpha);
    lookAheadComboBox.setAlpha(alpha);
    detectionModeComboBox.setAlpha(alpha);
    attackSlider.setAlpha(alpha);
    releaseSlider.setAlpha(alpha);
    holdSlider.setAlpha(alpha);
    breathReductionSlider.setAlpha(alpha);
    transientPreservationSlider.setAlpha(alpha);
    noiseFloorSlider.setAlpha(alpha);
    outputTrimMeter.setAlpha(alpha);
    attackLabel.setAlpha(alpha);
    releaseLabel.setAlpha(alpha);
    holdLabel.setAlpha(alpha);
    breathLabel.setAlpha(alpha);
    transientLabel.setAlpha(alpha);
    noiseFloorLabel.setAlpha(alpha);
    outputTrimLabel.setAlpha(alpha);
    trimMinLabel.setAlpha(alpha);
    trimMidLabel.setAlpha(alpha);
    trimMaxLabel.setAlpha(alpha);
    
    if (advancedPanelVisible) updateAdvancedControls();
    
    resized();
    repaint();
}

void VocalRiderAudioProcessorEditor::updateGainStats()
{
}

void VocalRiderAudioProcessorEditor::updateRangeLabel()
{
    // Label always stays "RANGE" — tooltip shows the +/- dB detail
}

juce::String VocalRiderAudioProcessorEditor::getRangeTooltipText() const
{
    if (dualRangeKnob.isLocked())
    {
        return juce::String(dualRangeKnob.getBoostValue(), 1) + " dB";
    }
    else
    {
        return "+" + juce::String(dualRangeKnob.getBoostValue(), 1)
               + " / -" + juce::String(dualRangeKnob.getCutValue(), 1) + " dB";
    }
}

void VocalRiderAudioProcessorEditor::setWindowSize(WindowSize size)
{
    currentWindowSize = size;
    
    int width = mediumWidth, height = mediumHeight;
    int sizeIndex = 1;  // Default to Medium
    switch (size)
    {
        case WindowSize::Small:
            width = smallWidth;
            height = smallHeight;
            resizeButton.currentSize = 0;
            sizeIndex = 0;
            break;
        case WindowSize::Medium:
            width = mediumWidth;
            height = mediumHeight;
            resizeButton.currentSize = 1;
            sizeIndex = 1;
            break;
        case WindowSize::Large:
            width = largeWidth;
            height = largeHeight;
            resizeButton.currentSize = 2;
            sizeIndex = 2;
            break;
    }
    
    // Save window size to processor state
    audioProcessor.setWindowSizeIndex(sizeIndex);
    
    setSize(static_cast<int>(width * uiScaleFactor), static_cast<int>(height * uiScaleFactor));
    resized();
}

void VocalRiderAudioProcessorEditor::setScale(int scalePercent)
{
    uiScaleFactor = scalePercent / 100.0f;
    resizeButton.currentScale = scalePercent;
    
    // Recalculate size based on current window preset and new scale
    int baseWidth = mediumWidth, baseHeight = mediumHeight;
    switch (currentWindowSize)
    {
        case WindowSize::Small:
            baseWidth = smallWidth;
            baseHeight = smallHeight;
            break;
        case WindowSize::Medium:
            baseWidth = mediumWidth;
            baseHeight = mediumHeight;
            break;
        case WindowSize::Large:
            baseWidth = largeWidth;
            baseHeight = largeHeight;
            break;
    }
    
    setSize(static_cast<int>(baseWidth * uiScaleFactor), static_cast<int>(baseHeight * uiScaleFactor));
    
    // Apply scale transform to entire UI
    setTransform(juce::AffineTransform::scale(uiScaleFactor));
    resized();
}

//==============================================================================
void VocalRiderAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float cornerRadius = 12.0f;  // Rounded corners
    
    // Clip to rounded rectangle for rounded corners effect
    juce::Path roundedBg;
    roundedBg.addRoundedRectangle(bounds, cornerRadius);
    g.reduceClipRegion(roundedBg);
    
    // Base background with subtle texture
    g.fillAll(CustomLookAndFeel::getBackgroundColour());
    
    // Cached noise texture for brushed metal feel (regenerated only on resize)
    if (cachedNoiseTexture.isNull() || cachedNoiseTexture.getWidth() != getWidth() || cachedNoiseTexture.getHeight() != getHeight())
    {
        cachedNoiseTexture = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
        juce::Graphics noiseG(cachedNoiseTexture);
        juce::Random rng(123);
        noiseG.setColour(juce::Colours::white.withAlpha(0.02f));
        for (int y = 0; y < getHeight(); y += 4)
        {
            for (int x = 0; x < getWidth(); x += 4)
            {
                if (rng.nextFloat() > 0.75f)
                    noiseG.fillRect(x, y, 2, 2);
            }
        }
    }
    g.drawImageAt(cachedNoiseTexture, 0, 0);
    
    //==========================================================================
    // FabFilter-style header bar
    float headerHeight = 52.0f;  // Compact header
    auto headerBounds = bounds.removeFromTop(headerHeight);
    
    // Draw header background - darker, integrated
    g.setColour(juce::Colour(0xFF1A1D24));
    g.fillRect(headerBounds);
    
    // Branded "tab" section on the left (logo + brand + plugin name)
    float brandTabWidth = 300.0f;  // Width to match resized() width
    auto brandTab = headerBounds.removeFromLeft(brandTabWidth);
    
    // Brand tab has slightly different/darker background
    g.setColour(juce::Colour(0xFF15181E));
    g.fillRect(brandTab);
    
    // Subtle bottom border on brand tab to make it feel like a "tab"
    g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(0.3f));
    g.drawLine(brandTab.getX(), brandTab.getBottom(), brandTab.getRight(), brandTab.getBottom(), 1.0f);
    
    // Vertical separator after brand tab
    g.setColour(juce::Colour(0xFF333842));
    g.drawVerticalLine(static_cast<int>(brandTab.getRight()), headerBounds.getY(), headerBounds.getBottom());
    
    // Subtle bottom edge on entire header
    g.setColour(juce::Colour(0xFF333842).withAlpha(0.5f));
    g.drawHorizontalLine(static_cast<int>(headerHeight - 1), 0.0f, bounds.getWidth());
    
    //==========================================================================
    // Vignette effects for main area (below header)
    float vignetteWidth = bounds.getWidth() * 0.25f;
    
    // Left edge vignette
    juce::ColourGradient leftVignette(
        juce::Colour(0x60000000),
        bounds.getX(), bounds.getCentreY(),
        juce::Colours::transparentBlack,
        bounds.getX() + vignetteWidth, bounds.getCentreY(),
        false
    );
    g.setGradientFill(leftVignette);
    g.fillRect(bounds.getX(), bounds.getY(), vignetteWidth, bounds.getHeight());
    
    // Right edge vignette
    juce::ColourGradient rightVignette(
        juce::Colours::transparentBlack,
        bounds.getRight() - vignetteWidth, bounds.getCentreY(),
        juce::Colour(0x60000000),
        bounds.getRight(), bounds.getCentreY(),
        false
    );
    g.setGradientFill(rightVignette);
    g.fillRect(bounds.getRight() - vignetteWidth, bounds.getY(), vignetteWidth, bounds.getHeight());
    
    // Top vignette (below header)
    float topVignetteHeight = bounds.getHeight() * 0.1f;
    juce::ColourGradient topVignette(
        juce::Colour(0x50000000),
        bounds.getCentreX(), bounds.getY(),
        juce::Colours::transparentBlack,
        bounds.getCentreX(), bounds.getY() + topVignetteHeight,
        false
    );
    g.setGradientFill(topVignette);
    g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), topVignetteHeight);
    
    // Bottom vignette
    float bottomVignetteHeight = bounds.getHeight() * 0.25f;
    juce::ColourGradient bottomVignette(
        juce::Colours::transparentBlack,
        bounds.getCentreX(), bounds.getBottom() - bottomVignetteHeight,
        juce::Colour(0x55000000),
        bounds.getCentreX(), bounds.getBottom(),
        false
    );
    g.setGradientFill(bottomVignette);
    g.fillRect(bounds.getX(), bounds.getBottom() - bottomVignetteHeight, bounds.getWidth(), bottomVignetteHeight);
    
    // Bottom bar - very subtle, integrated with meter area
    auto bottomBounds = bottomBar.getBounds().toFloat();
    
    // Subtle dark gradient from transparent to dark at bottom
    juce::ColourGradient bottomBarGradient(
        juce::Colours::transparentBlack, bottomBounds.getX(), bottomBounds.getY(),
        juce::Colour(0x60000000), bottomBounds.getX(), bottomBounds.getBottom(),
        false
    );
    g.setGradientFill(bottomBarGradient);
    g.fillRect(bottomBounds);
    
    // Very subtle top edge
    g.setColour(CustomLookAndFeel::getBorderColour().withAlpha(0.15f));
    g.drawHorizontalLine(static_cast<int>(bottomBounds.getY()), 0.0f, bounds.getWidth());
    
    // Advanced panel is now painted by its own component (AdvancedPanelComponent)
}

void VocalRiderAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    //==========================================================================
    // FabFilter-style header bar (compact height)
    int headerHeight = 52;  // Compact header
    auto headerArea = bounds.removeFromTop(headerHeight);
    
    // Brand section on the left - Full SVG logo with all branding
    auto brandTab = headerArea.removeFromLeft(300);  // Width to fit logo
    brandTab = brandTab.reduced(4, 3);  // Slightly more padding
    
    // Full logo component (SVG with icon + MBM AUDIO + magic.RIDE + subtitle)
    logoIcon.setBounds(brandTab);
    
    // Hide separate titleLabel and brandLabel - they're now part of LogoComponent
    titleLabel.setBounds(-100, -100, 1, 1);
    brandLabel.setBounds(-100, -100, 1, 1);
    
    // Right controls area (everything else)
    auto controlsArea = headerArea.reduced(10, 0);
    int buttonHeight = 22;
    int smallButtonSize = 16;
    int centerY = (controlsArea.getHeight() - buttonHeight) / 2;
    int smallCenterY = (controlsArea.getHeight() - smallButtonSize) / 2;
    
    // From RIGHT to LEFT:
    // Help/About button (rightmost - question mark)
    helpButton.setBounds(controlsArea.removeFromRight(20).withY(smallCenterY).withHeight(smallButtonSize));
    controlsArea.removeFromRight(8);
    
    // Settings/Advanced button (gear icon, to the left of help)
    advancedButton.setBounds(controlsArea.removeFromRight(22).withY(smallCenterY).withHeight(smallButtonSize));
    controlsArea.removeFromRight(15);
    
    // Preset section: [◄] [Default Setting ▼] [►]
    nextPresetButton.setBounds(controlsArea.removeFromRight(18).withY(centerY).withHeight(buttonHeight));
    controlsArea.removeFromRight(2);
    presetComboBox.setBounds(controlsArea.removeFromRight(110).withY(centerY).withHeight(buttonHeight));
    controlsArea.removeFromRight(2);
    prevPresetButton.setBounds(controlsArea.removeFromRight(18).withY(centerY).withHeight(buttonHeight));
    controlsArea.removeFromRight(15);
    
    // A/B Compare
    abCompareButton.setBounds(controlsArea.removeFromRight(36).withY(centerY).withHeight(buttonHeight));
    controlsArea.removeFromRight(10);
    
    // Undo/Redo (small arrows, like FabFilter)
    redoButton.setBounds(controlsArea.removeFromRight(smallButtonSize).withY(smallCenterY).withHeight(smallButtonSize));
    controlsArea.removeFromRight(4);
    undoButton.setBounds(controlsArea.removeFromRight(smallButtonSize).withY(smallCenterY).withHeight(smallButtonSize));
    
    // Bottom bar at very bottom
    auto bottomArea = bounds.removeFromBottom(bottomBarHeight);
    bottomBar.setBounds(bottomArea);
    
    // Left side: Footer label (version / status bar help text)
    footerLabel.setBounds(bottomArea.getX() + 8, bottomArea.getY() + 5, 200, 16);
    
    // Center: Toggle buttons - Smart Silence centered under target knob, others around it
    int naturalW = 72;
    int silenceW = 95;  // Wider for "SMART SILENCE"
    int autoTargetW = 90;
    int btnGap = 8;
    int toggleY = bottomArea.getY() + 4;
    
    // Center smart silence under target knob (which is at center of window)
    int btnCenterX = bottomArea.getWidth() / 2;
    int silenceX = btnCenterX - silenceW / 2;
    
    silenceToggle.setBounds(silenceX, toggleY, silenceW, 18);
    naturalToggle.setBounds(silenceX - btnGap - naturalW, toggleY, naturalW, 18);
    autoTargetButton.setBounds(silenceX + silenceW + btnGap, toggleY, autoTargetW, 18);
    
    // Automation mode selector (right side, before resize button)
    int autoModeW = 65;
    int autoModeH = 18;
    int autoLabelW = 30;
    int resizeSize = 14;
    int autoModeX = bottomArea.getWidth() - autoModeW - resizeSize - 16;
    automationModeComboBox.setBounds(autoModeX, toggleY, autoModeW, autoModeH);
    automationLabel.setBounds(autoModeX - autoLabelW - 4, toggleY, autoLabelW, autoModeH);
    
    // Resize button in bottom right corner (smaller icon)
    // Note: resizeButton is a child of bottomBar, so use local coordinates
    resizeButton.setBounds(bottomArea.getWidth() - resizeSize - 8, 4, resizeSize, resizeSize);
    
    // Control panel at bottom (above bottom bar)
    auto controlArea = bounds.removeFromBottom(controlPanelHeight);
    controlPanel.setBounds(controlArea);
    
    // Advanced panel (fade animation - fixed position, opacity animated)
    bool showingPanel = advancedPanelVisible || advancedPanel.isAnimating();
    
    if (showingPanel)
    {
        int advHeight = 130;
        auto advArea = bounds.removeFromTop(advHeight).reduced(12, 0);
        advancedPanel.setBounds(advArea);
        
        auto advContent = advArea.reduced(12, 8);
        
        // Header row
        auto headerRow = advContent.removeFromTop(16);
        advancedHeaderLabel.setBounds(headerRow);
        advContent.removeFromTop(4);
        
        // Dropdowns in row
        auto dropdownRow = advContent.removeFromTop(24);
        lookAheadComboBox.setBounds(dropdownRow.removeFromLeft(140));
        dropdownRow.removeFromLeft(12);
        detectionModeComboBox.setBounds(dropdownRow.removeFromLeft(130));
        
        advContent.removeFromTop(6);
        
        // LEFT SIDE: 7 Knobs with labels below
        int advKnobSize = 40;
        int labelHeight = 12;
        int labelWidth = 65;
        int numKnobs = 6;  // Attack, Release, Hold, Breath, Transient, Noise Floor
        
        // Reserve space for output trim fader on right
        int faderAreaWidth = 50;  // Fader + labels on right
        auto knobArea = advContent.withTrimmedRight(faderAreaWidth);
        auto faderArea = advContent.removeFromRight(faderAreaWidth);
        
        int totalKnobWidth = knobArea.getWidth();
        int spacing = (totalKnobWidth - advKnobSize * numKnobs) / (numKnobs + 1);
        int y = knobArea.getY();
        int x = knobArea.getX() + spacing;
        
        attackSlider.setBounds(x, y, advKnobSize, advKnobSize);
        attackLabel.setBounds(x - (labelWidth - advKnobSize) / 2, y + advKnobSize + 2, labelWidth, labelHeight);
        x += advKnobSize + spacing;
        
        releaseSlider.setBounds(x, y, advKnobSize, advKnobSize);
        releaseLabel.setBounds(x - (labelWidth - advKnobSize) / 2, y + advKnobSize + 2, labelWidth, labelHeight);
        x += advKnobSize + spacing;
        
        holdSlider.setBounds(x, y, advKnobSize, advKnobSize);
        holdLabel.setBounds(x - (labelWidth - advKnobSize) / 2, y + advKnobSize + 2, labelWidth, labelHeight);
        x += advKnobSize + spacing;
        
        breathReductionSlider.setBounds(x, y, advKnobSize, advKnobSize);
        breathLabel.setBounds(x - (labelWidth - advKnobSize) / 2, y + advKnobSize + 2, labelWidth, labelHeight);
        x += advKnobSize + spacing;
        
        transientPreservationSlider.setBounds(x, y, advKnobSize, advKnobSize);
        transientLabel.setBounds(x - (labelWidth - advKnobSize) / 2, y + advKnobSize + 2, labelWidth, labelHeight);
        x += advKnobSize + spacing;
        
        noiseFloorSlider.setBounds(x, y, advKnobSize, advKnobSize);
        noiseFloorLabel.setBounds(x - (labelWidth - advKnobSize) / 2, y + advKnobSize + 2, labelWidth, labelHeight);
        
        // Sidechain UI hidden for now (infrastructure kept for future update)
        sidechainToggle.setBounds(-100, -100, 1, 1);
        sidechainOffsetSlider.setBounds(-100, -100, 1, 1);
        sidechainOffsetLabel.setBounds(-100, -100, 1, 1);
        
        // RIGHT SIDE: Output Trim Vertical Fader (spans FULL advanced panel height)
        // Use advArea bounds directly so the fader isn't limited by the knob row
        int faderW = 16;
        int faderX = faderArea.getX() + (faderAreaWidth - faderW) / 2;
        int faderY = advArea.getY() + 10;  // Start near the very top of the panel
        int faderBottom = advArea.getBottom() - 10 - labelHeight - 2;  // Near panel bottom, leave label room
        int faderHeight = faderBottom - faderY;
        
        outputTrimMeter.setBounds(faderX, faderY, faderW, faderHeight);
        outputTrimLabel.setBounds(faderArea.getX(), faderBottom + 2, faderAreaWidth, labelHeight);
        
        // Position trim range labels on the RIGHT side of the fader
        int trimLabelW = 22;
        int trimLabelH = 10;
        int trimLabelX = faderX + faderW + 1;
        
        // +12 at top of fader (right side)
        trimMaxLabel.setBounds(trimLabelX, faderY + 8, trimLabelW, trimLabelH);
        // 0 in middle of fader (right side)
        trimMidLabel.setBounds(trimLabelX, faderY + faderHeight / 2 - trimLabelH / 2, trimLabelW, trimLabelH);
        // -12 at bottom of fader (right side)
        trimMinLabel.setBounds(trimLabelX, faderY + faderHeight - trimLabelH - 8, trimLabelW, trimLabelH);
    }
    
    // Waveform fills area below header and above bottom bar (transparent control area)
    auto waveformBounds = getLocalBounds().withTrimmedTop(headerHeight).withTrimmedBottom(bottomBarHeight);
    waveformDisplay.setBounds(waveformBounds);
    
    // Control panel contents - PROPORTIONALLY BIGGER knobs for -60dB range
    auto ctrlContent = controlArea.reduced(12, 4);
    
    int targetKnobSize = 95;   // Even larger target knob
    int smallKnobSize = 72;    // Larger side knobs  
    int labelHeight = 12;
    int miniMeterWidth = 58;   // Square-ish for circular arc
    int miniMeterHeight = 58;  // Same as width for proper arc
    
    int centerX = ctrlContent.getCentreX();
    int knobY = ctrlContent.getY();
    
    // Target in center (larger) - LABEL BELOW knob
    targetSlider.setBounds(centerX - targetKnobSize / 2, knobY, targetKnobSize, targetKnobSize);
    int targetLabelY = knobY + targetKnobSize + 2;
    targetLabel.setBounds(centerX - targetKnobSize / 2, targetLabelY, targetKnobSize, labelHeight);
    
    // Old learnButton hidden - functionality moved to bottom bar autoTargetButton
    learnButton.setBounds(-100, -100, 1, 1);
    
    // Range and Speed knobs - CENTERED around target knob (now that learn button moved)
    int spacing = 50;  // Spacing between knobs
    int rangeX = centerX - targetKnobSize / 2 - smallKnobSize - spacing;
    int speedX = centerX + targetKnobSize / 2 + spacing;
    
    // Range to the left - LABEL BELOW knob (hidden sliders off-screen)
    rangeSlider.setBounds(-100, -100, 1, 1);
    boostRangeSlider.setBounds(-100, -100, 1, 1);
    cutRangeSlider.setBounds(-100, -100, 1, 1);
    
    // Dual range knob (visible) — same size as speed knob for symmetry
    dualRangeKnob.setBounds(rangeX, knobY + 8, smallKnobSize, smallKnobSize);
    
    // RANGE label + lock icon centered under knob
    // Match SPEED label padding: label top = knobBottom + 2
    int lockSize = 11;
    int labelTextW = 38;
    int totalLabelW = labelTextW + lockSize + 3;
    int labelGroupX = rangeX + (smallKnobSize - totalLabelW) / 2;
    int rangeLabelY = knobY + 8 + smallKnobSize + 2;
    rangeLabel.setBounds(labelGroupX, rangeLabelY, labelTextW, labelHeight);
    // Lock icon vertically centered with RANGE text
    int labelMidY = rangeLabelY + labelHeight / 2 + 1;
    rangeLockButton.setBounds(labelGroupX + labelTextW + 3,
                              labelMidY - lockSize / 2 - 3,
                              lockSize, lockSize);
    
    
    // Speed to the right - LABEL BELOW knob
    speedSlider.setBounds(speedX, knobY + 8, smallKnobSize, smallKnobSize);
    speedLabel.setBounds(speedX, knobY + 8 + smallKnobSize + 2, smallKnobSize, labelHeight);
    
    // Turtle (slow) at 7 o'clock position, Rabbit (fast) at 5 o'clock position
    int iconSize = 16;
    int knobCenterX = speedX + smallKnobSize / 2;
    int knobCenterY = knobY + 8 + smallKnobSize / 2;
    int iconRadius = smallKnobSize / 2 + 8;  // Padding from knob edge
    
    // 7 o'clock position (bottom-left) - turtle moved 2-3px more left for visual balance
    int turtleX = knobCenterX + static_cast<int>(-0.82f * iconRadius) - iconSize / 2;
    int turtleY = knobCenterY + static_cast<int>(0.62f * iconRadius) - iconSize / 2;
    turtleIcon.setBounds(turtleX, turtleY, iconSize, iconSize);
    
    // 5 o'clock position (bottom-right) - moved 1px left for balance
    int rabbitX = knobCenterX + static_cast<int>(0.76f * iconRadius) - iconSize / 2;
    int rabbitY = knobCenterY + static_cast<int>(0.62f * iconRadius) - iconSize / 2;
    rabbitIcon.setBounds(rabbitX, rabbitY, iconSize, iconSize);
    
    // Mini gain meter - hidden (keeping code in case we want it back)
    miniGainMeter.setVisible(false);
    gainMeterLabel.setVisible(false);
    
    // Natural and Silence toggles now in bottom bar (positioned elsewhere)
    
    // About dialog covers entire window (for click-to-close overlay)
    // Only update bounds - do NOT call toFront() here as it disrupts z-order
    // of header buttons. toFront() is called in aboutDialog.show() instead.
    aboutDialog.setBounds(getLocalBounds());
}

void VocalRiderAudioProcessorEditor::timerCallback()
{
    // Automation mode pulsing animation when writing
    if (audioProcessor.isAutomationWriting())
    {
        automationPulsePhase += 0.15f;
        if (automationPulsePhase > juce::MathConstants<float>::twoPi)
            automationPulsePhase -= juce::MathConstants<float>::twoPi;
        
        float pulse = 0.5f + 0.5f * std::sin(automationPulsePhase);
        auto pulseColour = CustomLookAndFeel::getAccentColour().interpolatedWith(
            CustomLookAndFeel::getAccentBrightColour(), pulse);
        automationModeComboBox.setColour(juce::ComboBox::outlineColourId, pulseColour);
        automationModeComboBox.setColour(juce::ComboBox::textColourId, pulseColour);
    }
    else
    {
        // Reset to normal colors when not writing
        automationModeComboBox.setColour(juce::ComboBox::outlineColourId, CustomLookAndFeel::getDimTextColour().withAlpha(0.3f));
        automationModeComboBox.setColour(juce::ComboBox::textColourId, CustomLookAndFeel::getTextColour());
        automationPulsePhase = 0.0f;
    }
    
    waveformDisplay.setInputLevel(audioProcessor.getInputLevelDb());
    waveformDisplay.setOutputLevel(audioProcessor.getOutputLevelDb());
    waveformDisplay.setSidechainLevel(audioProcessor.getSidechainLevelDb());
    waveformDisplay.setSidechainActive(audioProcessor.isSidechainEnabled());
    
    // Natural Mode indicator: label visible when toggle is ON,
    // green dot reflects actual phrase detection from audio thread
    bool naturalEnabled = audioProcessor.isNaturalModeEnabled();
    waveformDisplay.setNaturalModeEnabled(naturalEnabled);
    
    if (!naturalEnabled)
    {
        waveformDisplay.setInPhrase(false);
        phraseIndicatorSilenceCount = 0;
    }
    else
    {
        // Track silence to handle DAW stop (processBlock stops being called)
        float currentInputForPhrase = audioProcessor.getInputLevelDb();
        if (currentInputForPhrase < -50.0f)
        {
            phraseIndicatorSilenceCount++;
            if (phraseIndicatorSilenceCount > 30)  // ~500ms at 60Hz
                waveformDisplay.setInPhrase(false);
            else
                waveformDisplay.setInPhrase(audioProcessor.isInPhrase());
        }
        else
        {
            phraseIndicatorSilenceCount = 0;
            waveformDisplay.setInPhrase(audioProcessor.isInPhrase());
        }
    }
    
    
    // Update noise floor display
    float nfDb = audioProcessor.getNoiseFloor();
    waveformDisplay.setNoiseFloorDb(nfDb);
    waveformDisplay.setNoiseFloorActive(nfDb > -59.9f);
    
    if (auto* param = audioProcessor.getApvts().getRawParameterValue(VocalRiderAudioProcessor::targetLevelParamId))
        waveformDisplay.setTargetLevel(param->load());
    
    float currentBoostRange = 6.0f;
    float currentCutRange = 6.0f;
    if (auto* param = audioProcessor.getApvts().getRawParameterValue(VocalRiderAudioProcessor::boostRangeParamId))
        currentBoostRange = param->load();
    if (auto* param = audioProcessor.getApvts().getRawParameterValue(VocalRiderAudioProcessor::cutRangeParamId))
        currentCutRange = param->load();
    
    waveformDisplay.setBoostRange(currentBoostRange);
    waveformDisplay.setCutRange(currentCutRange);
    
    // Sync DualRangeKnob with APVTS (in case automation or preset changed values)
    if (!dualRangeKnob.isMouseButtonDown())
    {
        dualRangeKnob.setBoostValue(currentBoostRange);
        dualRangeKnob.setCutValue(currentCutRange);
    }
    
    float currentRange = (currentBoostRange + currentCutRange) / 2.0f;
    
    // Update mini gain meter with decay when no audio
    float currentGain = audioProcessor.getCurrentGainDb();
    float inputLevel = audioProcessor.getInputLevelDb();
    
    // If input is very quiet (silence), decay displayed gain toward zero
    if (inputLevel < -50.0f)
    {
        // Decay toward zero - smooth exponential decay
        displayedGainDb *= 0.92f;  // ~3dB per 60Hz frame = fast decay
        if (std::abs(displayedGainDb) < 0.1f)
            displayedGainDb = 0.0f;
    }
    else
    {
        // Follow the actual gain
        displayedGainDb = currentGain;
    }
    
    miniGainMeter.setGainDb(displayedGainDb);
    miniGainMeter.setRangeDb(currentRange);
    
    // Auto-Target learning mode - collect data and analyze
    if (autoTargetButton.getToggleState() && learnCountdown > 0)
    {
        learnCountdown--;
        
        // Collect level data - use a threshold to avoid counting silence/noise
        float inputDb = audioProcessor.getInputLevelDb();
        if (inputDb > -55.0f)  // Only count actual audio above noise floor
        {
            if (inputDb < learnMinDb) learnMinDb = inputDb;
            if (inputDb > learnMaxDb) learnMaxDb = inputDb;
            learnSumDb += inputDb;
            learnSampleCount++;
        }
        
        if (learnCountdown == 0)
        {
            // Stop button state and pulsing animation
            autoTargetButton.setToggleState(false, juce::dontSendNotification);
            autoTargetButton.setPulsing(false);
            
            // Calculate and apply learned target - need at least 5 samples
            if (learnSampleCount >= 5)
            {
                float avgDb = learnSumDb / static_cast<float>(learnSampleCount);
                
                // Set target to average level (where vocal typically sits)
                // Only adjusts target - leaves range and speed untouched
                float targetLevel = juce::jlimit(-40.0f, -6.0f, avgDb);
                if (auto* param = audioProcessor.getApvts().getParameter(VocalRiderAudioProcessor::targetLevelParamId))
                    param->setValueNotifyingHost(param->convertTo0to1(targetLevel));
                    
                // Update the UI
                updateAdvancedControls();
            }
        }
    }
    else if (!autoTargetButton.getToggleState() && autoTargetButton.getPulsing())
    {
        // Ensure pulsing is stopped if button is manually turned off
        autoTargetButton.setPulsing(false);
    }
    
    // Hover tracking is now only for value tooltip display
    
    // Continuous tooltip update during drag for advanced sliders
    // (handles potential conflicts with APVTS SliderAttachment overriding onValueChange)
    if (attackSlider.isMouseButtonDown())
        valueTooltip.showValue("ATTACK", juce::String(static_cast<int>(attackSlider.getValue())) + " ms", &attackSlider);
    else if (releaseSlider.isMouseButtonDown())
        valueTooltip.showValue("RELEASE", juce::String(static_cast<int>(releaseSlider.getValue())) + " ms", &releaseSlider);
    else if (holdSlider.isMouseButtonDown())
        valueTooltip.showValue("HOLD", juce::String(static_cast<int>(holdSlider.getValue())) + " ms", &holdSlider);
    else if (breathReductionSlider.isMouseButtonDown()) {
        double val = breathReductionSlider.getValue();
        juce::String valStr = (val > 0.05 ? "-" : "") + juce::String(val, 1) + " dB";
        valueTooltip.showValue("BREATH", valStr, &breathReductionSlider);
    }
    else if (transientPreservationSlider.isMouseButtonDown())
        valueTooltip.showValue("TRANSIENT", juce::String(static_cast<int>(transientPreservationSlider.getValue())) + "%", &transientPreservationSlider);
    else if (noiseFloorSlider.isMouseButtonDown()) {
        juce::String valStr = juce::String(noiseFloorSlider.getValue(), 1) + " dB";
        if (noiseFloorSlider.getValue() <= -59.9) valStr = "OFF";
        valueTooltip.showValue("NOISE FLOOR", valStr, &noiseFloorSlider);
    }
    
    // Hide tooltip when no slider is being interacted with
    bool anySliderActive = targetSlider.isMouseOverOrDragging() ||
                           rangeSlider.isMouseOverOrDragging() ||
                           speedSlider.isMouseOverOrDragging() ||
                           attackSlider.isMouseOverOrDragging() ||
                           releaseSlider.isMouseOverOrDragging() ||
                           holdSlider.isMouseOverOrDragging() ||
                           breathReductionSlider.isMouseOverOrDragging() ||
                           transientPreservationSlider.isMouseOverOrDragging() ||
                           noiseFloorSlider.isMouseOverOrDragging() ||
                           outputTrimMeter.isMouseOver();
    if (!anySliderActive && valueTooltip.isShowing() && currentHoveredComponent == nullptr)
    {
        valueTooltip.hideTooltip();
    }
    

}

//==============================================================================
// Keyboard shortcuts
bool VocalRiderAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    // Cmd+Z (Mac) or Ctrl+Z (Windows) = Undo
    if (key.isKeyCode('Z') && key.getModifiers().isCommandDown() && !key.getModifiers().isShiftDown())
    {
        performUndo();
        return true;
    }
    
    // Cmd+Shift+Z (Mac) or Ctrl+Shift+Z (Windows) = Redo
    if (key.isKeyCode('Z') && key.getModifiers().isCommandDown() && key.getModifiers().isShiftDown())
    {
        performRedo();
        return true;
    }
    
    // Cmd+Y (alternative Redo on Windows)
    if (key.isKeyCode('Y') && key.getModifiers().isCommandDown())
    {
        performRedo();
        return true;
    }
    
    return false;  // Key not handled
}

//==============================================================================
// A/B Compare, Undo/Redo implementation

VocalRiderAudioProcessorEditor::ParameterState VocalRiderAudioProcessorEditor::getCurrentState()
{
    ParameterState state;
    state.target = static_cast<float>(targetSlider.getValue());
    state.range = static_cast<float>(rangeSlider.getValue());
    state.boostRange = dualRangeKnob.getBoostValue();
    state.cutRange = dualRangeKnob.getCutValue();
    state.speed = static_cast<float>(speedSlider.getValue());
    state.attack = static_cast<float>(attackSlider.getValue());
    state.release = static_cast<float>(releaseSlider.getValue());
    state.hold = static_cast<float>(holdSlider.getValue());
    state.breathReduction = static_cast<float>(breathReductionSlider.getValue());
    state.transientPreservation = static_cast<float>(transientPreservationSlider.getValue());
    state.outputTrim = outputTrimMeter.getValue();
    state.noiseFloor = static_cast<float>(noiseFloorSlider.getValue());
    return state;
}

void VocalRiderAudioProcessorEditor::applyState(const ParameterState& state)
{
    targetSlider.setValue(state.target, juce::sendNotification);
    rangeSlider.setValue(state.range, juce::sendNotification);
    dualRangeKnob.setBoostValue(state.boostRange);
    dualRangeKnob.setCutValue(state.cutRange);
    boostRangeSlider.setValue(state.boostRange, juce::sendNotification);
    cutRangeSlider.setValue(state.cutRange, juce::sendNotification);
    speedSlider.setValue(state.speed, juce::sendNotification);
    attackSlider.setValue(state.attack, juce::sendNotification);
    releaseSlider.setValue(state.release, juce::sendNotification);
    holdSlider.setValue(state.hold, juce::sendNotification);
    breathReductionSlider.setValue(state.breathReduction, juce::sendNotification);
    transientPreservationSlider.setValue(state.transientPreservation, juce::sendNotification);
    outputTrimMeter.setValue(state.outputTrim);
    noiseFloorSlider.setValue(state.noiseFloor, juce::sendNotification);
}

void VocalRiderAudioProcessorEditor::saveStateForUndo()
{
    // Trim redo history when adding new state
    if (undoIndex < static_cast<int>(undoHistory.size()) - 1)
        undoHistory.resize(static_cast<size_t>(undoIndex + 1));
    
    undoHistory.push_back(getCurrentState());
    undoIndex = static_cast<int>(undoHistory.size()) - 1;
    
    // Limit history size to 5 states (current + 4 previous)
    while (undoHistory.size() > 5)
    {
        undoHistory.erase(undoHistory.begin());
        undoIndex--;
    }
}

void VocalRiderAudioProcessorEditor::performUndo()
{
    if (undoIndex > 0)
    {
        undoIndex--;
        applyState(undoHistory[static_cast<size_t>(undoIndex)]);
    }
}

void VocalRiderAudioProcessorEditor::performRedo()
{
    if (undoIndex < static_cast<int>(undoHistory.size()) - 1)
    {
        undoIndex++;
        applyState(undoHistory[static_cast<size_t>(undoIndex)]);
    }
}

juce::String VocalRiderAudioProcessorEditor::getHelpText(const juce::String& controlName)
{
    // Help text with line breaks for better readability (2 lines each)
    if (controlName == "TARGET")
        return "Target level for your vocal.\nSet to your desired average loudness.";
    if (controlName == "RANGE")
        return "Maximum gain adjustment.\nHigher values = more correction.";
    if (controlName == "SPEED")
        return "How quickly gain responds.\nFaster = aggressive, Slower = natural.";
    if (controlName == "ATTACK")
        return "How quickly gain increases\nwhen vocal gets louder.";
    if (controlName == "RELEASE")
        return "How quickly gain decreases\nwhen vocal gets quieter.";
    if (controlName == "HOLD")
        return "Time to hold gain before release.\nPrevents pumping artifacts.";
    if (controlName == "BREATH")
        return "Reduces gain on breaths.\nHigher = more breath reduction.";
    if (controlName == "TRANSIENT")
        return "Preserves vocal attacks.\nHigher = less gain on transients.";
    if (controlName == "NOISEFLOOR")
        return "Ignore audio below threshold.\nDrag up to set noise floor level.";
    if (controlName == "OUTPUT")
        return "Makeup gain after riding.\nBoost or cut overall level.";
    if (controlName == "NATURAL")
        return "Phrase-based processing.\nSmoother, more musical results.";
    if (controlName == "SILENCE")
        return "Reduces gain on silent sections.\nLowers noise floor by -6dB.";
    if (controlName == "SPEED_BTN")
        return "Waveform scroll speed.\nSlower = see more history.";
    if (controlName == "AUTOTARGET")
        return "Auto-analyze audio for 3 sec.\nSets target level only.";
    if (controlName == "INPUT_METER")
        return "Input level meter.\nShows your incoming signal level.";
    if (controlName == "OUTPUT_METER")
        return "Output level meter.\nShows level after gain riding.";
    if (controlName == "GAIN_METER")
        return "Current gain adjustment.\nGreen = boost, Purple = cut.";
    return "";
}

void VocalRiderAudioProcessorEditor::setStatusBarText(const juce::String& text)
{
    footerLabel.setText(text, juce::dontSendNotification);
    footerLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getDimTextColour());
}

void VocalRiderAudioProcessorEditor::clearStatusBarText()
{
    footerLabel.setText(versionString, juce::dontSendNotification);
    footerLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getVeryDimTextColour());
}

juce::String VocalRiderAudioProcessorEditor::getShortHelpText(const juce::String& controlName)
{
    if (controlName == "TARGET")    return "Target level for your vocal loudness";
    if (controlName == "RANGE")     return "Maximum gain adjustment (+/- dB)";
    if (controlName == "SPEED")     return "How quickly gain responds to changes";
    if (controlName == "ATTACK")    return "Gain increase speed when vocal gets louder";
    if (controlName == "RELEASE")   return "Gain decrease speed when vocal gets quieter";
    if (controlName == "HOLD")      return "Hold time before release - prevents pumping";
    if (controlName == "BREATH")    return "Reduces gain on breaths and sibilance";
    if (controlName == "TRANSIENT") return "Preserves vocal attacks and consonants";
    if (controlName == "NOISEFLOOR") return "Ignore audio below this threshold";
    if (controlName == "OUTPUT")    return "Makeup gain after riding (+/- 12 dB)";
    if (controlName == "NATURAL")   return "Phrase-based processing for smoother results";
    if (controlName == "SILENCE")   return "Reduces gain on silent sections (-6 dB)";
    if (controlName == "AUTOTARGET") return "Auto-analyze audio to set target level";
    return "";
}
