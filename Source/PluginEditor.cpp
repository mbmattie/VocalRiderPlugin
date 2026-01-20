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

//==============================================================================
VocalRiderAudioProcessorEditor::VocalRiderAudioProcessorEditor(VocalRiderAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p)
{
    setLookAndFeel(&customLookAndFeel);
    // Disable manual resizing - only allow preset sizes
    setResizable(false, false);

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
    
    // Footer text
    footerLabel.setText("v1.0", juce::dontSendNotification);
    footerLabel.setFont(CustomLookAndFeel::getPluginFont(9.0f));
    footerLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getVeryDimTextColour());
    footerLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(footerLabel);

    // Preset navigation arrows (clean arrow buttons)
    prevPresetButton.onClick = [this] {
        int current = presetComboBox.getSelectedId();
        if (current > 1)
            presetComboBox.setSelectedId(current - 1);
    };
    addAndMakeVisible(prevPresetButton);
    
    nextPresetButton.onClick = [this] {
        int current = presetComboBox.getSelectedId();
        int numItems = presetComboBox.getNumItems();
        if (current < numItems)
            presetComboBox.setSelectedId(current + 1);
    };
    addAndMakeVisible(nextPresetButton);

    presetComboBox.setTextWhenNothingSelected("Preset");
    const auto& presets = VocalRiderAudioProcessor::getFactoryPresets();
    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
        presetComboBox.addItem(presets[static_cast<size_t>(i)].name, i + 1);
    presetComboBox.onChange = [this] {
        int idx = presetComboBox.getSelectedId() - 1;
        if (idx >= 0)
        {
            audioProcessor.loadPreset(idx);
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
    };

    //==============================================================================
    // Control Panel (floating tab)
    
    addAndMakeVisible(controlPanel);
    
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
        if (targetSlider.isMouseOverOrDragging()) {
            juce::String text = helpModeActive ? getHelpText("TARGET") : juce::String(targetSlider.getValue(), 1) + " dB";
            valueTooltip.showValue("TARGET", text, &targetSlider, false, helpModeActive);
        }
    };
    targetSlider.onDragEnd = [this]() { saveStateForUndo(); };  // Save on mouse release
    targetSlider.onMouseEnter = [this]() {
        juce::String text = helpModeActive ? getHelpText("TARGET") : juce::String(targetSlider.getValue(), 1) + " dB";
        valueTooltip.showValue("TARGET", text, &targetSlider, false, helpModeActive);
    };
    targetSlider.onMouseExit = [this]() {
        valueTooltip.hideTooltip();
    };
    
    targetLabel.setText("TARGET", juce::dontSendNotification);
    targetLabel.setFont(CustomLookAndFeel::getPluginFont(10.0f, true));
    targetLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getTextColour());
    targetLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(targetLabel);
    
    // Range knob
    setupSliderWithTooltip(rangeSlider, " dB");
    rangeSlider.onValueChange = [this]() {
        if (rangeSlider.isMouseOverOrDragging()) {
            juce::String text = helpModeActive ? getHelpText("RANGE") : juce::String(rangeSlider.getValue(), 1) + " dB";
            valueTooltip.showValue("RANGE", text, &rangeSlider, false, helpModeActive);
        }
    };
    rangeSlider.onDragEnd = [this]() { saveStateForUndo(); };  // Save on mouse release
    rangeSlider.onMouseEnter = [this]() {
        juce::String text = helpModeActive ? getHelpText("RANGE") : juce::String(rangeSlider.getValue(), 1) + " dB";
        valueTooltip.showValue("RANGE", text, &rangeSlider, false, helpModeActive);
    };
    rangeSlider.onMouseExit = [this]() {
        valueTooltip.hideTooltip();
    };
    
    rangeLabel.setText("RANGE", juce::dontSendNotification);
    rangeLabel.setFont(CustomLookAndFeel::getPluginFont(9.0f, true));
    rangeLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getTextColour());
    rangeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(rangeLabel);
    
    // Speed knob
    setupSliderWithTooltip(speedSlider, "%");
    speedSlider.onValueChange = [this] {
        audioProcessor.updateAttackReleaseFromSpeed(static_cast<float>(speedSlider.getValue()));
        if (advancedPanelVisible) updateAdvancedControls();
        if (speedSlider.isMouseOverOrDragging()) {
            juce::String text = helpModeActive ? getHelpText("SPEED") : juce::String(static_cast<int>(speedSlider.getValue())) + "%";
            valueTooltip.showValue("SPEED", text, &speedSlider, false, helpModeActive);
        }
    };
    speedSlider.onDragEnd = [this]() { saveStateForUndo(); };  // Save on mouse release
    speedSlider.onMouseEnter = [this]() {
        juce::String text = helpModeActive ? getHelpText("SPEED") : juce::String(static_cast<int>(speedSlider.getValue())) + "%";
        valueTooltip.showValue("SPEED", text, &speedSlider, false, helpModeActive);
    };
    speedSlider.onMouseExit = [this]() {
        valueTooltip.hideTooltip();
    };
    
    speedLabel.setText("SPEED", juce::dontSendNotification);
    speedLabel.setFont(CustomLookAndFeel::getPluginFont(9.0f, true));
    speedLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getTextColour());
    speedLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(speedLabel);
    
    // Mini gain meter
    addAndMakeVisible(miniGainMeter);
    gainMeterLabel.setText("GAIN", juce::dontSendNotification);
    gainMeterLabel.setFont(CustomLookAndFeel::getPluginFont(8.0f, true));
    gainMeterLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getDimTextColour());
    gainMeterLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(gainMeterLabel);
    
    // Natural toggle - default OFF (in bottom bar)
    naturalToggle.setToggleState(false, juce::dontSendNotification);
    audioProcessor.setNaturalModeEnabled(false);
    naturalToggle.onClick = [this] {
        audioProcessor.setNaturalModeEnabled(naturalToggle.getToggleState());
    };
    addAndMakeVisible(naturalToggle);
    
    // Smart Silence toggle - default OFF (now called silenceToggle)
    silenceToggle.setToggleState(false, juce::dontSendNotification);
    audioProcessor.setSmartSilenceEnabled(false);
    silenceToggle.onClick = [this] {
        audioProcessor.setSmartSilenceEnabled(silenceToggle.getToggleState());
    };
    addAndMakeVisible(silenceToggle);
    
    // Auto-Target button in bottom bar (replaces old Learn button)
    autoTargetButton.onClick = [this] {
        if (!autoTargetButton.getToggleState()) {
            autoTargetButton.setToggleState(true, juce::dontSendNotification);
            learnCountdown = 180;  // 3 seconds at 60Hz (faster analysis)
            // Reset learning stats
            learnMinDb = 0.0f;
            learnMaxDb = -100.0f;
            learnSumDb = 0.0f;
            learnSampleCount = 0;
        }
    };
    addAndMakeVisible(autoTargetButton);
    
    // Old Learn button - hidden (functionality moved to autoTargetButton)
    // learnButton kept for backwards compatibility but not shown
    
    // Speed button in bottom bar - controls waveform scroll speed
    waveformDisplay.setScrollSpeed(0.5f);  // Default to medium speed
    speedButton.onClick = [this] {
        juce::PopupMenu menu;
        menu.setLookAndFeel(&getLookAndFeel());
        float currentSpeed = waveformDisplay.getScrollSpeed();
        menu.addItem(1, "Fast (5s)", true, currentSpeed > 0.8f);
        menu.addItem(2, "Medium (10s)", true, currentSpeed > 0.4f && currentSpeed <= 0.8f);
        menu.addItem(3, "Slow (15s)", true, currentSpeed <= 0.4f);
        
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&speedButton),
            [this](int result) {
                if (result == 1) { waveformDisplay.setScrollSpeed(1.0f); speedButton.setLabel("5s"); }
                else if (result == 2) { waveformDisplay.setScrollSpeed(0.5f); speedButton.setLabel("10s"); }
                else if (result == 3) { waveformDisplay.setScrollSpeed(0.33f); speedButton.setLabel("15s"); }
            });
    };
    speedButton.onMouseEnter = [this]() {
        if (helpModeActive)
            valueTooltip.showValue("SPEED", getHelpText("SPEED_BTN"), &speedButton, false, true);
    };
    speedButton.onMouseExit = [this]() {
        if (helpModeActive)
            valueTooltip.hideTooltip();
    };
    addAndMakeVisible(speedButton);
    
    // Natural toggle help
    naturalToggle.onMouseEnter = [this]() {
        if (helpModeActive)
            valueTooltip.showValue("NATURAL", getHelpText("NATURAL"), &naturalToggle, false, true);
    };
    naturalToggle.onMouseExit = [this]() {
        if (helpModeActive)
            valueTooltip.hideTooltip();
    };
    
    // Silence toggle help
    silenceToggle.onMouseEnter = [this]() {
        if (helpModeActive)
            valueTooltip.showValue("SILENCE", getHelpText("SILENCE"), &silenceToggle, false, true);
    };
    silenceToggle.onMouseExit = [this]() {
        if (helpModeActive)
            valueTooltip.hideTooltip();
    };
    
    // Help button toggle
    helpButton.onClick = [this] {
        helpModeActive = helpButton.getToggleState();
        // When help mode is on, tooltips will show descriptions
        if (!helpModeActive)
            valueTooltip.hideTooltip();
    };
    addAndMakeVisible(helpButton);
    
    // Resize button callbacks are set up earlier where it's added to bottomBar
    
    // Parameter attachments
    targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::targetLevelParamId, targetSlider);
    rangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::rangeParamId, rangeSlider);
    speedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::speedParamId, speedSlider);

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
        outputTrimMeter.setAlpha(alpha);
        attackLabel.setAlpha(alpha);
        releaseLabel.setAlpha(alpha);
        holdLabel.setAlpha(alpha);
        breathLabel.setAlpha(alpha);
        transientLabel.setAlpha(alpha);
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
            outputTrimMeter.setVisible(false);
            attackLabel.setVisible(false);
            releaseLabel.setVisible(false);
            holdLabel.setVisible(false);
            breathLabel.setVisible(false);
            transientLabel.setVisible(false);
            outputTrimLabel.setVisible(false);
            trimMinLabel.setVisible(false);
            trimMidLabel.setVisible(false);
            trimMaxLabel.setVisible(false);
            
            #if JucePlugin_Build_Standalone
            loadFileButton.setVisible(false);
            playStopButton.setVisible(false);
            rewindButton.setVisible(false);
            fileNameLabel.setVisible(false);
            #endif
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
            juce::String text = helpModeActive ? getHelpText("ATTACK") : juce::String(static_cast<int>(attackSlider.getValue())) + " ms";
            valueTooltip.showValue("ATTACK", text, &attackSlider, false, helpModeActive);
        }
    };
    attackSlider.onMouseEnter = [this]() {
        juce::String text = helpModeActive ? getHelpText("ATTACK") : juce::String(static_cast<int>(attackSlider.getValue())) + " ms";
        valueTooltip.showValue("ATTACK", text, &attackSlider, false, helpModeActive);
    };
    attackSlider.onMouseExit = []() { /* Don't hide - let timer handle it */ };
    
    setupAdvSlider(releaseSlider, 10.0, 2000.0, "ms");
    releaseSlider.onValueChange = [this] {
        audioProcessor.setReleaseMs(static_cast<float>(releaseSlider.getValue()));
        if (releaseSlider.isMouseOverOrDragging()) {
            juce::String text = helpModeActive ? getHelpText("RELEASE") : juce::String(static_cast<int>(releaseSlider.getValue())) + " ms";
            valueTooltip.showValue("RELEASE", text, &releaseSlider, false, helpModeActive);
        }
    };
    releaseSlider.onMouseEnter = [this]() {
        juce::String text = helpModeActive ? getHelpText("RELEASE") : juce::String(static_cast<int>(releaseSlider.getValue())) + " ms";
        valueTooltip.showValue("RELEASE", text, &releaseSlider, false, helpModeActive);
    };
    releaseSlider.onMouseExit = []() { /* Don't hide - let timer handle it */ };
    
    setupAdvSlider(holdSlider, 0.0, 500.0, "ms");
    holdSlider.onValueChange = [this] {
        audioProcessor.setHoldMs(static_cast<float>(holdSlider.getValue()));
        if (holdSlider.isMouseOverOrDragging()) {
            juce::String text = helpModeActive ? getHelpText("HOLD") : juce::String(static_cast<int>(holdSlider.getValue())) + " ms";
            valueTooltip.showValue("HOLD", text, &holdSlider, false, helpModeActive);
        }
    };
    holdSlider.onMouseEnter = [this]() {
        juce::String text = helpModeActive ? getHelpText("HOLD") : juce::String(static_cast<int>(holdSlider.getValue())) + " ms";
        valueTooltip.showValue("HOLD", text, &holdSlider, false, helpModeActive);
    };
    holdSlider.onMouseExit = []() { /* Don't hide - let timer handle it */ };
    
    setupAdvSlider(breathReductionSlider, 0.0, 12.0, "dB");
    breathReductionSlider.onValueChange = [this] {
        audioProcessor.setBreathReduction(static_cast<float>(breathReductionSlider.getValue()));
        if (breathReductionSlider.isMouseOverOrDragging()) {
            juce::String text = helpModeActive ? getHelpText("BREATH") : juce::String(breathReductionSlider.getValue(), 1) + " dB";
            valueTooltip.showValue("BREATH", text, &breathReductionSlider, false, helpModeActive);
        }
    };
    breathReductionSlider.onMouseEnter = [this]() {
        juce::String text = helpModeActive ? getHelpText("BREATH") : juce::String(breathReductionSlider.getValue(), 1) + " dB";
        valueTooltip.showValue("BREATH", text, &breathReductionSlider, false, helpModeActive);
    };
    breathReductionSlider.onMouseExit = []() { /* Don't hide - let timer handle it */ };
    
    setupAdvSlider(transientPreservationSlider, 0.0, 100.0, "%");
    transientPreservationSlider.onValueChange = [this] {
        audioProcessor.setTransientPreservation(static_cast<float>(transientPreservationSlider.getValue()) / 100.0f);
        if (transientPreservationSlider.isMouseOverOrDragging()) {
            juce::String text = helpModeActive ? getHelpText("TRANSIENT") : juce::String(static_cast<int>(transientPreservationSlider.getValue())) + "%";
            valueTooltip.showValue("TRANSIENT", text, &transientPreservationSlider, false, helpModeActive);
        }
    };
    transientPreservationSlider.onMouseEnter = [this]() {
        juce::String text = helpModeActive ? getHelpText("TRANSIENT") : juce::String(static_cast<int>(transientPreservationSlider.getValue())) + "%";
        valueTooltip.showValue("TRANSIENT", text, &transientPreservationSlider, false, helpModeActive);
    };
    transientPreservationSlider.onMouseExit = []() { /* Don't hide - let timer handle it */ };
    
    // Output Trim - AdjustableGainMeter style (similar to MiniGainMeter but adjustable)
    outputTrimMeter.setValue(0.0f);  // Start at 0 dB
    outputTrimMeter.onValueChanged = [this](float valueDb) {
        audioProcessor.setOutputTrim(valueDb);
        // Show tooltip (save undo handled on mouse up)
        juce::String valStr = (valueDb >= 0 ? "+" : "") + juce::String(valueDb, 1) + " dB";
        juce::String text = helpModeActive ? getHelpText("OUTPUT") : valStr;
        valueTooltip.showValue("OUTPUT", text, &outputTrimMeter, false, helpModeActive);
    };
    outputTrimMeter.onMouseEnter = [this]() {
        float val = outputTrimMeter.getValue();
        juce::String valStr = (val >= 0 ? "+" : "") + juce::String(val, 1) + " dB";
        juce::String text = helpModeActive ? getHelpText("OUTPUT") : valStr;
        valueTooltip.showValue("OUTPUT", text, &outputTrimMeter, false, helpModeActive);
    };
    outputTrimMeter.onMouseExit = []() { /* Don't hide - let timer handle it */ };
    addAndMakeVisible(outputTrimMeter);
    outputTrimMeter.setVisible(false);
    // Output Trim meter mouse enter/exit for tooltip
    outputTrimMeter.addMouseListener(this, false);
    
    setupAdvLabel(attackLabel, "ATTACK");
    setupAdvLabel(releaseLabel, "RELEASE");
    setupAdvLabel(holdLabel, "HOLD");
    setupAdvLabel(breathLabel, "BREATH");
    setupAdvLabel(transientLabel, "TRANSIENT");
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

    //==============================================================================
    #if JucePlugin_Build_Standalone
    loadFileButton.setButtonText("Load");
    loadFileButton.onClick = [this] { loadFileButtonClicked(); };
    addAndMakeVisible(loadFileButton);

    playStopButton.setButtonText("Play");
    playStopButton.onClick = [this] { playStopButtonClicked(); };
    playStopButton.setEnabled(false);
    addAndMakeVisible(playStopButton);

    rewindButton.setButtonText("<<");
    rewindButton.onClick = [this] { audioProcessor.rewindPlayback(); };
    rewindButton.setEnabled(false);
    addAndMakeVisible(rewindButton);

    fileNameLabel.setText("Load an audio file", juce::dontSendNotification);
    fileNameLabel.setFont(CustomLookAndFeel::getPluginFont(10.0f));
    fileNameLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getDimTextColour());
    fileNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(fileNameLabel);
    #endif

    // Set initial size to medium
    setWindowSize(WindowSize::Medium);
    startTimerHz(30);
    updateAdvancedControls();
}

VocalRiderAudioProcessorEditor::~VocalRiderAudioProcessorEditor()
{
    stopTimer();
    audioProcessor.setWaveformDisplay(nullptr);
    setLookAndFeel(nullptr);
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
    outputTrimMeter.setVisible(true);
    attackLabel.setVisible(true);
    releaseLabel.setVisible(true);
    holdLabel.setVisible(true);
    breathLabel.setVisible(true);
    transientLabel.setVisible(true);
    outputTrimLabel.setVisible(true);
    trimMinLabel.setVisible(true);
    trimMidLabel.setVisible(true);
    trimMaxLabel.setVisible(true);
    
    #if JucePlugin_Build_Standalone
    loadFileButton.setVisible(true);
    playStopButton.setVisible(true);
    rewindButton.setVisible(true);
    fileNameLabel.setVisible(true);
    #endif
    
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
        outputTrimMeter.setAlpha(alpha);
    attackLabel.setAlpha(alpha);
    releaseLabel.setAlpha(alpha);
    holdLabel.setAlpha(alpha);
    breathLabel.setAlpha(alpha);
    transientLabel.setAlpha(alpha);
    outputTrimLabel.setAlpha(alpha);
    trimMinLabel.setAlpha(alpha);
    trimMidLabel.setAlpha(alpha);
    trimMaxLabel.setAlpha(alpha);
    
    #if JucePlugin_Build_Standalone
    loadFileButton.setAlpha(alpha);
    playStopButton.setAlpha(alpha);
    rewindButton.setAlpha(alpha);
    fileNameLabel.setAlpha(alpha);
    #endif
    
    if (advancedPanelVisible) updateAdvancedControls();
    
    resized();
    repaint();
}

void VocalRiderAudioProcessorEditor::updateGainStats()
{
}

void VocalRiderAudioProcessorEditor::setWindowSize(WindowSize size)
{
    currentWindowSize = size;
    
    int width, height;
    switch (size)
    {
        case WindowSize::Small:
            width = smallWidth;
            height = smallHeight;
            resizeButton.currentSize = 0;
            break;
        case WindowSize::Medium:
            width = mediumWidth;
            height = mediumHeight;
            resizeButton.currentSize = 1;
            break;
        case WindowSize::Large:
            width = largeWidth;
            height = largeHeight;
            resizeButton.currentSize = 2;
            break;
    }
    
    setSize(static_cast<int>(width * uiScaleFactor), static_cast<int>(height * uiScaleFactor));
    resized();
}

void VocalRiderAudioProcessorEditor::setScale(int scalePercent)
{
    uiScaleFactor = scalePercent / 100.0f;
    resizeButton.currentScale = scalePercent;
    
    // Recalculate size based on current window preset and new scale
    int baseWidth, baseHeight;
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

#if JucePlugin_Build_Standalone
void VocalRiderAudioProcessorEditor::loadFileButtonClicked()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select a vocal file...", juce::File{},
        "*.wav;*.mp3;*.aif;*.aiff;*.flac;*.ogg"
    );

    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                if (audioProcessor.loadAudioFile(file))
                {
                    fileNameLabel.setText(audioProcessor.getLoadedFileName(), juce::dontSendNotification);
                    fileNameLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getTextColour());
                    playStopButton.setEnabled(true);
                    rewindButton.setEnabled(true);
                    waveformDisplay.clear();
                }
            }
        });
}

void VocalRiderAudioProcessorEditor::playStopButtonClicked()
{
    audioProcessor.togglePlayback();
    updatePlaybackUI();
}

void VocalRiderAudioProcessorEditor::updatePlaybackUI()
{
    playStopButton.setButtonText(audioProcessor.isPlaying() ? "Stop" : "Play");
}
#endif

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
    
    // Subtle noise texture for brushed metal feel
    juce::Random rng(123);
    for (int y = 0; y < getHeight(); y += 4)
    {
        for (int x = 0; x < getWidth(); x += 4)
        {
            float noise = rng.nextFloat();
            if (noise > 0.75f)
            {
                g.setColour(juce::Colours::white.withAlpha(0.02f));
                g.fillRect(x, y, 2, 2);
            }
        }
    }
    
    //==========================================================================
    // FabFilter-style header bar
    float headerHeight = 70.0f;  // Increased to match resized() height
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
    // FabFilter-style header bar (taller height for full logo)
    int headerHeight = 70;  // Increased height to prevent logo cutoff
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
    // Settings/Advanced button (rightmost - like FabFilter)
    advancedButton.setBounds(controlsArea.removeFromRight(22).withY(smallCenterY).withHeight(smallButtonSize));
    controlsArea.removeFromRight(8);
    
    // Help button (to the left of settings)
    helpButton.setBounds(controlsArea.removeFromRight(20).withY(smallCenterY).withHeight(smallButtonSize));
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
    
    #if JucePlugin_Build_Standalone
    // Audio player controls hidden from header - will be in advanced panel
    // For now, place them off-screen
    rewindButton.setBounds(-100, -100, 28, buttonHeight);
    playStopButton.setBounds(-100, -100, 40, buttonHeight);
    loadFileButton.setBounds(-100, -100, 40, buttonHeight);
    fileNameLabel.setBounds(-100, -100, 100, buttonHeight);
    #endif
    
    // Bottom bar at very bottom
    auto bottomArea = bounds.removeFromBottom(bottomBarHeight);
    bottomBar.setBounds(bottomArea);
    
    // Left side: Footer label (version)
    footerLabel.setBounds(bottomArea.getX() + 8, bottomArea.getY() + 5, 50, 16);
    
    // Center: Toggle buttons with icons - order: Natural, Smart Silence, Auto-Target, Speed
    int naturalW = 72;
    int silenceW = 95;  // Wider for "SMART SILENCE"
    int autoTargetW = 90;
    int speedW = 50;
    int btnGap = 8;
    int toggleWidth = naturalW + btnGap + silenceW + btnGap + autoTargetW + btnGap + speedW;
    int toggleStartX = (bottomArea.getWidth() - toggleWidth) / 2;
    int toggleY = bottomArea.getY() + 4;
    
    naturalToggle.setBounds(toggleStartX, toggleY, naturalW, 18);
    silenceToggle.setBounds(toggleStartX + naturalW + btnGap, toggleY, silenceW, 18);
    autoTargetButton.setBounds(toggleStartX + naturalW + btnGap + silenceW + btnGap, toggleY, autoTargetW, 18);
    speedButton.setBounds(toggleStartX + naturalW + btnGap + silenceW + btnGap + autoTargetW + btnGap, toggleY, speedW, 18);
    
    // Resize button in bottom right corner (smaller icon)
    // Note: resizeButton is a child of bottomBar, so use local coordinates
    int resizeSize = 14;
    resizeButton.setBounds(bottomArea.getWidth() - resizeSize - 8, 4, resizeSize, resizeSize);
    
    // Control panel at bottom (above bottom bar)
    auto controlArea = bounds.removeFromBottom(controlPanelHeight);
    controlPanel.setBounds(controlArea);
    
    // Advanced panel (fade animation - fixed position, opacity animated)
    bool showingPanel = advancedPanelVisible || advancedPanel.isAnimating();
    
    if (showingPanel)
    {
        #if JucePlugin_Build_Standalone
        int advHeight = 160;  // Taller for audio controls
        #else
        int advHeight = 130;
        #endif
        auto advArea = bounds.removeFromTop(advHeight).reduced(12, 0);
        advancedPanel.setBounds(advArea);
        
        auto advContent = advArea.reduced(12, 8);
        
        // Header row
        auto headerRow = advContent.removeFromTop(16);
        advancedHeaderLabel.setBounds(headerRow);
        advContent.removeFromTop(4);
        
        #if JucePlugin_Build_Standalone
        // Audio player controls row (for standalone testing)
        auto playerRow = advContent.removeFromTop(24);
        loadFileButton.setBounds(playerRow.removeFromLeft(50));
        playerRow.removeFromLeft(4);
        playStopButton.setBounds(playerRow.removeFromLeft(50));
        playerRow.removeFromLeft(4);
        rewindButton.setBounds(playerRow.removeFromLeft(30));
        playerRow.removeFromLeft(8);
        fileNameLabel.setBounds(playerRow);
        advContent.removeFromTop(6);
        #endif
        
        // Dropdowns in row
        auto dropdownRow = advContent.removeFromTop(24);
        lookAheadComboBox.setBounds(dropdownRow.removeFromLeft(140));
        dropdownRow.removeFromLeft(12);
        detectionModeComboBox.setBounds(dropdownRow.removeFromLeft(130));
        
        advContent.removeFromTop(6);
        
        // LEFT SIDE: 5 Knobs with labels below
        int advKnobSize = 46;
        int labelHeight = 12;
        int labelWidth = 80;
        int numKnobs = 5;  // Attack, Release, Hold, Breath, Transient
        
        // Reserve space for output trim fader on right (narrower)
        int faderWidth = 16;  // Narrower fader
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
        
        // RIGHT SIDE: Output Trim Vertical Fader (narrower, with more bottom padding)
        int faderHeight = advKnobSize;  // Same height as knobs
        int faderX = faderArea.getX() + 4;  // Left side of fader area
        int faderY = y;
        
        outputTrimMeter.setBounds(faderX, faderY, faderWidth, faderHeight);
        outputTrimLabel.setBounds(faderArea.getX(), faderY + faderHeight + 4, faderAreaWidth, labelHeight);  // More padding
        
        // Position trim range labels on the RIGHT side of the fader
        int trimLabelW = 22;
        int trimLabelH = 10;
        int labelX = faderX + faderWidth + 2;  // Labels on RIGHT side
        
        // +12 at top of fader (right side)
        trimMaxLabel.setBounds(labelX, faderY - 1, trimLabelW, trimLabelH);
        // 0 in middle of fader (right side)
        trimMidLabel.setBounds(labelX, faderY + faderHeight / 2 - trimLabelH / 2, trimLabelW, trimLabelH);
        // -12 at bottom of fader (right side)
        trimMinLabel.setBounds(labelX, faderY + faderHeight - trimLabelH + 1, trimLabelW, trimLabelH);
    }
    
    // Waveform fills area below header and above bottom bar (transparent control area)
    auto waveformBounds = getLocalBounds().withTrimmedTop(36).withTrimmedBottom(bottomBarHeight);
    waveformDisplay.setBounds(waveformBounds);
    
    // Control panel contents - PROPORTIONALLY BIGGER knobs for -60dB range
    auto ctrlContent = controlArea.reduced(12, 4);
    
    int targetKnobSize = 95;   // Even larger target knob
    int smallKnobSize = 72;    // Larger side knobs  
    int labelHeight = 12;
    int miniMeterWidth = 18;
    int miniMeterHeight = 60;
    
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
    
    // Range to the left - LABEL BELOW knob
    rangeSlider.setBounds(rangeX, knobY + 8, smallKnobSize, smallKnobSize);
    rangeLabel.setBounds(rangeX, knobY + 8 + smallKnobSize + 2, smallKnobSize, labelHeight);
    
    // Speed to the right - LABEL BELOW knob
    speedSlider.setBounds(speedX, knobY + 8, smallKnobSize, smallKnobSize);
    speedLabel.setBounds(speedX, knobY + 8 + smallKnobSize + 2, smallKnobSize, labelHeight);
    
    // Mini gain meter to the right of speed knob
    int gainMeterX = speedX + smallKnobSize + 16;
    miniGainMeter.setBounds(gainMeterX, knobY + 8, miniMeterWidth, miniMeterHeight);
    gainMeterLabel.setBounds(gainMeterX - 5, knobY + 8 + miniMeterHeight + 2, miniMeterWidth + 10, labelHeight);
    
    // Natural and Silence toggles now in bottom bar (positioned elsewhere)
}

void VocalRiderAudioProcessorEditor::timerCallback()
{
    waveformDisplay.setInputLevel(audioProcessor.getInputLevelDb());
    waveformDisplay.setOutputLevel(audioProcessor.getOutputLevelDb());
    
    if (auto* param = audioProcessor.getApvts().getRawParameterValue(VocalRiderAudioProcessor::targetLevelParamId))
        waveformDisplay.setTargetLevel(param->load());
    
    float currentRange = 12.0f;
    if (auto* param = audioProcessor.getApvts().getRawParameterValue(VocalRiderAudioProcessor::rangeParamId))
    {
        currentRange = param->load();
        waveformDisplay.setRange(currentRange);
    }
    
    // Update mini gain meter
    float currentGain = audioProcessor.getCurrentGainDb();
    miniGainMeter.setGainDb(currentGain);
    miniGainMeter.setRangeDb(currentRange);
    
    // Auto-Target learning mode - collect data and analyze
    if (autoTargetButton.getToggleState() && learnCountdown > 0)
    {
        learnCountdown--;
        
        // Collect level data
        float inputDb = audioProcessor.getInputLevelDb();
        if (inputDb > -60.0f)  // Only count actual audio
        {
            if (inputDb < learnMinDb) learnMinDb = inputDb;
            if (inputDb > learnMaxDb) learnMaxDb = inputDb;
            learnSumDb += inputDb;
            learnSampleCount++;
        }
        
        if (learnCountdown == 0)
        {
            autoTargetButton.setToggleState(false, juce::dontSendNotification);
            
            // Calculate and apply learned values
            if (learnSampleCount > 30)  // Need enough samples
            {
                float avgDb = learnSumDb / static_cast<float>(learnSampleCount);
                float dynamicRange = learnMaxDb - learnMinDb;
                
                // Set target to average level
                float targetLevel = juce::jlimit(-40.0f, -6.0f, avgDb);
                if (auto* param = audioProcessor.getApvts().getParameter("targetLevel"))
                    param->setValueNotifyingHost(param->convertTo0to1(targetLevel));
                
                // Set range based on dynamic range (half of dynamic range, clamped)
                float range = juce::jlimit(3.0f, 18.0f, dynamicRange / 2.0f);
                if (auto* param = audioProcessor.getApvts().getParameter("range"))
                    param->setValueNotifyingHost(param->convertTo0to1(range));
                
                // Set speed based on how "jumpy" the audio is
                // More dynamic range = slower speed to be smoother
                float speed = juce::jmap(dynamicRange, 6.0f, 24.0f, 70.0f, 30.0f);
                speed = juce::jlimit(20.0f, 80.0f, speed);
                if (auto* param = audioProcessor.getApvts().getParameter("speed"))
                    param->setValueNotifyingHost(param->convertTo0to1(speed));
            }
        }
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
                           outputTrimMeter.isMouseOver();
    if (!anySliderActive && valueTooltip.isShowing())
    {
        valueTooltip.hideTooltip();
    }

    #if JucePlugin_Build_Standalone
    if (audioProcessor.hasFileLoaded())
    {
        bool currentlyPlaying = audioProcessor.isPlaying();
        
        if (wasPlaying && !currentlyPlaying && audioProcessor.hasPlaybackFinished())
            updatePlaybackUI();
        wasPlaying = currentlyPlaying;

        if (currentlyPlaying != (playStopButton.getButtonText() == "Stop"))
            updatePlaybackUI();
    }
    #endif
}

//==============================================================================
// A/B Compare, Undo/Redo implementation

VocalRiderAudioProcessorEditor::ParameterState VocalRiderAudioProcessorEditor::getCurrentState()
{
    ParameterState state;
    state.target = static_cast<float>(targetSlider.getValue());
    state.range = static_cast<float>(rangeSlider.getValue());
    state.speed = static_cast<float>(speedSlider.getValue());
    state.attack = static_cast<float>(attackSlider.getValue());
    state.release = static_cast<float>(releaseSlider.getValue());
    state.hold = static_cast<float>(holdSlider.getValue());
    state.breathReduction = static_cast<float>(breathReductionSlider.getValue());
    state.transientPreservation = static_cast<float>(transientPreservationSlider.getValue());
    state.outputTrim = outputTrimMeter.getValue();
    return state;
}

void VocalRiderAudioProcessorEditor::applyState(const ParameterState& state)
{
    targetSlider.setValue(state.target, juce::sendNotification);
    rangeSlider.setValue(state.range, juce::sendNotification);
    speedSlider.setValue(state.speed, juce::sendNotification);
    attackSlider.setValue(state.attack, juce::sendNotification);
    releaseSlider.setValue(state.release, juce::sendNotification);
    holdSlider.setValue(state.hold, juce::sendNotification);
    breathReductionSlider.setValue(state.breathReduction, juce::sendNotification);
    transientPreservationSlider.setValue(state.transientPreservation, juce::sendNotification);
    outputTrimMeter.setValue(state.outputTrim);
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
    if (controlName == "OUTPUT")
        return "Makeup gain after riding.\nBoost or cut overall level.";
    if (controlName == "NATURAL")
        return "Phrase-based processing.\nSmoother, more musical results.";
    if (controlName == "SILENCE")
        return "Reduces gain on silent sections.\nLowers noise floor by -6dB.";
    if (controlName == "SPEED_BTN")
        return "Waveform scroll speed.\nSlower = see more history.";
    return "";
}
