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
    // Header
    
    titleLabel.setText("VOCAL RIDER", juce::dontSendNotification);
    titleLabel.setFont(CustomLookAndFeel::getPluginFont(14.0f, true));
    titleLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getAccentColour());
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

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

    advancedButton.setClickingTogglesState(true);
    advancedButton.onClick = [this] { toggleAdvancedPanel(); };
    addAndMakeVisible(advancedButton);
    
    // Bottom bar with resize button
    addAndMakeVisible(bottomBar);
    
    // Resize button - shows popup menu with size options
    resizeButton.onSizeSelected = [this](int sizeIndex) {
        switch (sizeIndex)
        {
            case 0: setWindowSize(WindowSize::Small); break;
            case 1: setWindowSize(WindowSize::Medium); break;
            case 2: setWindowSize(WindowSize::Large); break;
        }
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
        if (targetSlider.isMouseOverOrDragging())
            valueTooltip.showValue("TARGET", juce::String(targetSlider.getValue(), 1) + " dB", &targetSlider);
    };
    targetSlider.onMouseEnter = [this]() {
        valueTooltip.showValue("TARGET", juce::String(targetSlider.getValue(), 1) + " dB", &targetSlider);
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
        if (rangeSlider.isMouseOverOrDragging())
            valueTooltip.showValue("RANGE", juce::String(rangeSlider.getValue(), 1) + " dB", &rangeSlider);
    };
    rangeSlider.onMouseEnter = [this]() {
        valueTooltip.showValue("RANGE", juce::String(rangeSlider.getValue(), 1) + " dB", &rangeSlider);
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
        if (speedSlider.isMouseOverOrDragging())
            valueTooltip.showValue("SPEED", juce::String(static_cast<int>(speedSlider.getValue())) + "%", &speedSlider);
    };
    speedSlider.onMouseEnter = [this]() {
        valueTooltip.showValue("SPEED", juce::String(static_cast<int>(speedSlider.getValue())) + "%", &speedSlider);
    };
    speedSlider.onMouseExit = [this]() {
        valueTooltip.hideTooltip();
    };
    
    speedLabel.setText("SPEED", juce::dontSendNotification);
    speedLabel.setFont(CustomLookAndFeel::getPluginFont(9.0f, true));
    speedLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getTextColour());
    speedLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(speedLabel);
    
    // Natural toggle
    naturalToggle.setColour(juce::ToggleButton::textColourId, CustomLookAndFeel::getTextColour());
    naturalToggle.onClick = [this] {
        audioProcessor.setNaturalModeEnabled(naturalToggle.getToggleState());
    };
    addAndMakeVisible(naturalToggle);
    
    // Parameter attachments
    targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::targetLevelParamId, targetSlider);
    rangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::rangeParamId, rangeSlider);
    speedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getApvts(), VocalRiderAudioProcessor::speedParamId, speedSlider);

    //==============================================================================
    // Advanced Panel with fade animation
    
    advancedPanel.setVisible(false);
    advancedPanel.onAnimationUpdate = [this] {
        // Update child component opacity during fade animation
        float alpha = advancedPanel.getCurrentOpacity();
        lookAheadComboBox.setAlpha(alpha);
        detectionModeComboBox.setAlpha(alpha);
        attackSlider.setAlpha(alpha);
        releaseSlider.setAlpha(alpha);
        holdSlider.setAlpha(alpha);
        breathReductionSlider.setAlpha(alpha);
        transientPreservationSlider.setAlpha(alpha);
        attackLabel.setAlpha(alpha);
        releaseLabel.setAlpha(alpha);
        holdLabel.setAlpha(alpha);
        breathLabel.setAlpha(alpha);
        transientLabel.setAlpha(alpha);
        
        // Hide components when fully faded out
        if (advancedPanel.isFullyHidden() && !advancedPanelVisible)
        {
            advancedPanel.setVisible(false);
            lookAheadComboBox.setVisible(false);
            detectionModeComboBox.setVisible(false);
            attackSlider.setVisible(false);
            releaseSlider.setVisible(false);
            holdSlider.setVisible(false);
            breathReductionSlider.setVisible(false);
            transientPreservationSlider.setVisible(false);
            attackLabel.setVisible(false);
            releaseLabel.setVisible(false);
            holdLabel.setVisible(false);
            breathLabel.setVisible(false);
            transientLabel.setVisible(false);
        }
        
        repaint();
    };
    addAndMakeVisible(advancedPanel);
    
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
    
    setupAdvSlider(attackSlider, 1.0, 500.0, "ms");
    attackSlider.onValueChange = [this] {
        audioProcessor.setAttackMs(static_cast<float>(attackSlider.getValue()));
        if (attackSlider.isMouseOverOrDragging())
            valueTooltip.showValue("ATTACK", juce::String(static_cast<int>(attackSlider.getValue())) + " ms", &attackSlider);
    };
    attackSlider.onMouseEnter = [this]() {
        valueTooltip.showValue("ATTACK", juce::String(static_cast<int>(attackSlider.getValue())) + " ms", &attackSlider);
    };
    attackSlider.onMouseExit = [this]() { valueTooltip.hideTooltip(); };
    
    setupAdvSlider(releaseSlider, 10.0, 2000.0, "ms");
    releaseSlider.onValueChange = [this] {
        audioProcessor.setReleaseMs(static_cast<float>(releaseSlider.getValue()));
        if (releaseSlider.isMouseOverOrDragging())
            valueTooltip.showValue("RELEASE", juce::String(static_cast<int>(releaseSlider.getValue())) + " ms", &releaseSlider);
    };
    releaseSlider.onMouseEnter = [this]() {
        valueTooltip.showValue("RELEASE", juce::String(static_cast<int>(releaseSlider.getValue())) + " ms", &releaseSlider);
    };
    releaseSlider.onMouseExit = [this]() { valueTooltip.hideTooltip(); };
    
    setupAdvSlider(holdSlider, 0.0, 500.0, "ms");
    holdSlider.onValueChange = [this] {
        audioProcessor.setHoldMs(static_cast<float>(holdSlider.getValue()));
        if (holdSlider.isMouseOverOrDragging())
            valueTooltip.showValue("HOLD", juce::String(static_cast<int>(holdSlider.getValue())) + " ms", &holdSlider);
    };
    holdSlider.onMouseEnter = [this]() {
        valueTooltip.showValue("HOLD", juce::String(static_cast<int>(holdSlider.getValue())) + " ms", &holdSlider);
    };
    holdSlider.onMouseExit = [this]() { valueTooltip.hideTooltip(); };
    
    setupAdvSlider(breathReductionSlider, 0.0, 12.0, "dB");
    breathReductionSlider.onValueChange = [this] {
        audioProcessor.setBreathReduction(static_cast<float>(breathReductionSlider.getValue()));
        if (breathReductionSlider.isMouseOverOrDragging())
            valueTooltip.showValue("BREATH", juce::String(breathReductionSlider.getValue(), 1) + " dB", &breathReductionSlider);
    };
    breathReductionSlider.onMouseEnter = [this]() {
        valueTooltip.showValue("BREATH", juce::String(breathReductionSlider.getValue(), 1) + " dB", &breathReductionSlider);
    };
    breathReductionSlider.onMouseExit = [this]() { valueTooltip.hideTooltip(); };
    
    setupAdvSlider(transientPreservationSlider, 0.0, 100.0, "%");
    transientPreservationSlider.onValueChange = [this] {
        audioProcessor.setTransientPreservation(static_cast<float>(transientPreservationSlider.getValue()) / 100.0f);
        if (transientPreservationSlider.isMouseOverOrDragging())
            valueTooltip.showValue("TRANSIENT", juce::String(static_cast<int>(transientPreservationSlider.getValue())) + "%", &transientPreservationSlider);
    };
    transientPreservationSlider.onMouseEnter = [this]() {
        valueTooltip.showValue("TRANSIENT", juce::String(static_cast<int>(transientPreservationSlider.getValue())) + "%", &transientPreservationSlider);
    };
    transientPreservationSlider.onMouseExit = [this]() { valueTooltip.hideTooltip(); };
    
    setupAdvLabel(attackLabel, "ATTACK");
    setupAdvLabel(releaseLabel, "RELEASE");
    setupAdvLabel(holdLabel, "HOLD");
    setupAdvLabel(breathLabel, "BREATH REDUCTION");
    setupAdvLabel(transientLabel, "TRANSIENT PRESERVE");

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
    advancedPanel.setTargetOpacity(advancedPanelVisible ? 1.0f : 0.0f);
    
    // Always show panel and children during animation (visibility controlled by opacity)
    advancedPanel.setVisible(true);
    lookAheadComboBox.setVisible(true);
    detectionModeComboBox.setVisible(true);
    attackSlider.setVisible(true);
    releaseSlider.setVisible(true);
    holdSlider.setVisible(true);
    breathReductionSlider.setVisible(true);
    transientPreservationSlider.setVisible(true);
    attackLabel.setVisible(true);
    releaseLabel.setVisible(true);
    holdLabel.setVisible(true);
    breathLabel.setVisible(true);
    transientLabel.setVisible(true);
    
    // Set child component opacity via alpha
    float alpha = advancedPanel.getCurrentOpacity();
    lookAheadComboBox.setAlpha(alpha);
    detectionModeComboBox.setAlpha(alpha);
    attackSlider.setAlpha(alpha);
    releaseSlider.setAlpha(alpha);
    holdSlider.setAlpha(alpha);
    breathReductionSlider.setAlpha(alpha);
    transientPreservationSlider.setAlpha(alpha);
    attackLabel.setAlpha(alpha);
    releaseLabel.setAlpha(alpha);
    holdLabel.setAlpha(alpha);
    breathLabel.setAlpha(alpha);
    transientLabel.setAlpha(alpha);
    
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
    
    setSize(width, height);
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
    // Background with subtle gradient
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient bgGradient(
        CustomLookAndFeel::getBackgroundColour().brighter(0.03f),
        bounds.getX(), bounds.getY(),
        CustomLookAndFeel::getBackgroundColour().darker(0.02f),
        bounds.getX(), bounds.getBottom(),
        false
    );
    g.setGradientFill(bgGradient);
    g.fillAll();
    
    // Subtle radial gradient overlay (top-left)
    juce::ColourGradient radialGradient(
        CustomLookAndFeel::getAccentColour().withAlpha(0.08f),
        bounds.getX() + bounds.getWidth() * 0.2f, bounds.getY() + bounds.getHeight() * 0.2f,
        juce::Colours::transparentBlack,
        bounds.getX() + bounds.getWidth() * 0.5f, bounds.getY() + bounds.getHeight() * 0.5f,
        true
    );
    g.setGradientFill(radialGradient);
    g.fillAll();
    
    // Grain texture removed for cleaner look
    
    // Control panel floating tab
    auto panelBounds = controlPanel.getBounds().toFloat();
    
    // Shadow under panel
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(panelBounds.translated(0.0f, -3.0f).expanded(2.0f), 12.0f);
    
    // Panel background
    g.setColour(CustomLookAndFeel::getSurfaceColour());
    g.fillRoundedRectangle(panelBounds.withTop(panelBounds.getY() - 12.0f), 12.0f);
    
    // Panel border
    g.setColour(CustomLookAndFeel::getBorderColour());
    g.drawRoundedRectangle(panelBounds.withTop(panelBounds.getY() - 12.0f), 12.0f, 1.0f);
    
    // Top edge accent line
    g.setColour(CustomLookAndFeel::getAccentColour().withAlpha(0.5f));
    g.fillRoundedRectangle(panelBounds.getX() + 30.0f, panelBounds.getY() - 12.0f, 
                            panelBounds.getWidth() - 60.0f, 2.0f, 1.0f);
    
    // Bottom bar background
    auto bottomBounds = bottomBar.getBounds().toFloat();
    g.setColour(CustomLookAndFeel::getSurfaceDarkColour());
    g.fillRect(bottomBounds);
    
    // Bottom bar top edge line
    g.setColour(CustomLookAndFeel::getBorderColour().withAlpha(0.5f));
    g.drawHorizontalLine(static_cast<int>(bottomBounds.getY()), 0.0f, bounds.getWidth());
    
    // Advanced panel is now painted by its own component (AdvancedPanelComponent)
}

void VocalRiderAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    // Header bar
    auto headerArea = bounds.removeFromTop(32).reduced(10, 4);
    titleLabel.setBounds(headerArea.removeFromLeft(110));
    
    advancedButton.setBounds(headerArea.removeFromRight(26));
    headerArea.removeFromRight(4);
    presetComboBox.setBounds(headerArea.removeFromRight(100));
    
    #if JucePlugin_Build_Standalone
    headerArea.removeFromRight(10);
    rewindButton.setBounds(headerArea.removeFromRight(32));
    headerArea.removeFromRight(2);
    playStopButton.setBounds(headerArea.removeFromRight(45));
    headerArea.removeFromRight(2);
    loadFileButton.setBounds(headerArea.removeFromRight(45));
    headerArea.removeFromRight(6);
    fileNameLabel.setBounds(headerArea);
    #endif
    
    // Bottom bar at very bottom
    auto bottomArea = bounds.removeFromBottom(bottomBarHeight);
    bottomBar.setBounds(bottomArea);
    
    // Resize button in bottom bar (right side)
    int resizeButtonSize = 20;
    resizeButton.setBounds(bottomArea.getWidth() - resizeButtonSize - 4, 1, 
                           resizeButtonSize, resizeButtonSize);
    
    // Control panel at bottom (above bottom bar)
    auto controlArea = bounds.removeFromBottom(controlPanelHeight);
    controlPanel.setBounds(controlArea);
    
    // Advanced panel (fade animation - fixed position, opacity animated)
    bool showingPanel = advancedPanelVisible || advancedPanel.isAnimating();
    
    if (showingPanel)
    {
        int advHeight = 120;  // Fixed height
        auto advArea = bounds.removeFromTop(advHeight).reduced(12, 0);
        advancedPanel.setBounds(advArea);
        
        auto advContent = advArea.reduced(12, 10);
        
        // Dropdowns in row
        auto dropdownRow = advContent.removeFromTop(24);
        lookAheadComboBox.setBounds(dropdownRow.removeFromLeft(140));
        dropdownRow.removeFromLeft(12);
        detectionModeComboBox.setBounds(dropdownRow.removeFromLeft(130));
        
        advContent.removeFromTop(6);
        
        // Knobs - LABELS BELOW knobs
        int advKnobSize = 50;
        int labelHeight = 12;
        int labelWidth = 90;  // Wider labels for "BREATH REDUCTION" etc.
        int numKnobs = 5;
        int totalWidth = advContent.getWidth();
        int spacing = (totalWidth - advKnobSize * numKnobs) / (numKnobs + 1);
        int y = advContent.getY();
        int x = advContent.getX() + spacing;
        
        // Knob first, then label below
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
    }
    
    // Waveform fills area below header and above bottom bar
    waveformDisplay.setBounds(getLocalBounds().withTrimmedTop(32).withTrimmedBottom(bottomBarHeight));
    
    // Control panel contents - knobs at TOP of panel, Natural toggle BELOW knobs
    auto ctrlContent = controlArea.reduced(12, 4);
    
    int targetKnobSize = 60;
    int smallKnobSize = 46;
    int labelHeight = 12;
    
    int centerX = ctrlContent.getCentreX();
    int knobY = ctrlContent.getY() - 2;  // Start at very top
    
    // Target in center (larger) - LABEL BELOW knob
    targetSlider.setBounds(centerX - targetKnobSize / 2, knobY, targetKnobSize, targetKnobSize);
    targetLabel.setBounds(centerX - targetKnobSize / 2, knobY + targetKnobSize + 2, targetKnobSize, labelHeight);
    
    // Range to the left - LABEL BELOW knob
    int rangeX = centerX - targetKnobSize / 2 - smallKnobSize - 25;
    rangeSlider.setBounds(rangeX, knobY + 5, smallKnobSize, smallKnobSize);
    rangeLabel.setBounds(rangeX, knobY + 5 + smallKnobSize + 2, smallKnobSize, labelHeight);
    
    // Speed to the right - LABEL BELOW knob
    int speedX = centerX + targetKnobSize / 2 + 25;
    speedSlider.setBounds(speedX, knobY + 5, smallKnobSize, smallKnobSize);
    speedLabel.setBounds(speedX, knobY + 5 + smallKnobSize + 2, smallKnobSize, labelHeight);
    
    // Natural toggle - positioned BELOW the knobs, centered
    int toggleY = knobY + targetKnobSize + labelHeight + 8;
    naturalToggle.setBounds(centerX - 47, toggleY, 95, 20);
}

void VocalRiderAudioProcessorEditor::timerCallback()
{
    waveformDisplay.setInputLevel(audioProcessor.getInputLevelDb());
    waveformDisplay.setOutputLevel(audioProcessor.getOutputLevelDb());
    
    if (auto* param = audioProcessor.getApvts().getRawParameterValue(VocalRiderAudioProcessor::targetLevelParamId))
        waveformDisplay.setTargetLevel(param->load());
    if (auto* param = audioProcessor.getApvts().getRawParameterValue(VocalRiderAudioProcessor::rangeParamId))
        waveformDisplay.setRange(param->load());
    
    // Hide tooltip when no slider is being interacted with
    bool anySliderActive = targetSlider.isMouseOverOrDragging() ||
                           rangeSlider.isMouseOverOrDragging() ||
                           speedSlider.isMouseOverOrDragging();
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
