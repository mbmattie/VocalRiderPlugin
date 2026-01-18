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
    titleLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
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
    
    // Resize button (cycles through sizes)
    resizeButton.onClick = [this] {
        // Cycle through sizes: Small -> Medium -> Large -> Small
        switch (currentWindowSize)
        {
            case WindowSize::Small:
                setWindowSize(WindowSize::Medium);
                break;
            case WindowSize::Medium:
                setWindowSize(WindowSize::Large);
                break;
            case WindowSize::Large:
                setWindowSize(WindowSize::Small);
                break;
        }
    };
    addAndMakeVisible(resizeButton);

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
    
    // Target knob (LARGE, center)
    targetSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    targetSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    targetSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(targetSlider);
    
    targetLabel.setText("TARGET", juce::dontSendNotification);
    targetLabel.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    targetLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getTargetLineColour());
    targetLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(targetLabel);
    
    // Range knob
    rangeSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    rangeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    rangeSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(rangeSlider);
    
    rangeLabel.setText("RANGE", juce::dontSendNotification);
    rangeLabel.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    rangeLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getRangeLineColour());
    rangeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(rangeLabel);
    
    // Speed knob
    speedSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    speedSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    speedSlider.setTextValueSuffix("%");
    addAndMakeVisible(speedSlider);
    
    speedLabel.setText("SPEED", juce::dontSendNotification);
    speedLabel.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    speedLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getDimTextColour());
    speedLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(speedLabel);
    
    speedSlider.onValueChange = [this] {
        audioProcessor.updateAttackReleaseFromSpeed(static_cast<float>(speedSlider.getValue()));
        if (advancedPanelVisible) updateAdvancedControls();
    };
    
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
    // Advanced Panel
    
    advancedPanel.setVisible(false);
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
    
    automationModeComboBox.addItem("Auto: Read", 1);
    automationModeComboBox.addItem("Auto: Touch", 2);
    automationModeComboBox.addItem("Auto: Latch", 3);
    automationModeComboBox.addItem("Auto: Write", 4);
    automationModeComboBox.setSelectedId(1);
    automationModeComboBox.onChange = [this] {
        audioProcessor.setAutomationMode(
            static_cast<VocalRiderAudioProcessor::AutomationMode>(automationModeComboBox.getSelectedId() - 1));
    };
    addAndMakeVisible(automationModeComboBox);
    automationModeComboBox.setVisible(false);
    
    auto setupAdvSlider = [this](juce::Slider& slider, double min, double max, const juce::String& suffix) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 12);
        slider.setRange(min, max, 1.0);
        slider.setTextValueSuffix(suffix);
        addAndMakeVisible(slider);
        slider.setVisible(false);
    };
    
    auto setupAdvLabel = [this](juce::Label& label, const juce::String& text) {
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::Font(juce::FontOptions(8.0f)));
        label.setColour(juce::Label::textColourId, CustomLookAndFeel::getVeryDimTextColour());
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
        label.setVisible(false);
    };
    
    setupAdvSlider(attackSlider, 1.0, 500.0, "ms");
    attackSlider.onValueChange = [this] { audioProcessor.setAttackMs(static_cast<float>(attackSlider.getValue())); };
    
    setupAdvSlider(releaseSlider, 10.0, 2000.0, "ms");
    releaseSlider.onValueChange = [this] { audioProcessor.setReleaseMs(static_cast<float>(releaseSlider.getValue())); };
    
    setupAdvSlider(holdSlider, 0.0, 500.0, "ms");
    holdSlider.onValueChange = [this] { audioProcessor.setHoldMs(static_cast<float>(holdSlider.getValue())); };
    
    setupAdvSlider(breathReductionSlider, 0.0, 12.0, "dB");
    breathReductionSlider.onValueChange = [this] { audioProcessor.setBreathReduction(static_cast<float>(breathReductionSlider.getValue())); };
    
    setupAdvSlider(transientPreservationSlider, 0.0, 100.0, "%");
    transientPreservationSlider.onValueChange = [this] { audioProcessor.setTransientPreservation(static_cast<float>(transientPreservationSlider.getValue()) / 100.0f); };
    
    setupAdvLabel(attackLabel, "ATK");
    setupAdvLabel(releaseLabel, "REL");
    setupAdvLabel(holdLabel, "HOLD");
    setupAdvLabel(breathLabel, "BREATH");
    setupAdvLabel(transientLabel, "TRANS");

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
    fileNameLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
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
    automationModeComboBox.setSelectedId(static_cast<int>(audioProcessor.getAutomationMode()) + 1, juce::dontSendNotification);
}

void VocalRiderAudioProcessorEditor::toggleAdvancedPanel()
{
    advancedPanelVisible = advancedButton.getToggleState();
    
    advancedPanel.setVisible(advancedPanelVisible);
    lookAheadComboBox.setVisible(advancedPanelVisible);
    detectionModeComboBox.setVisible(advancedPanelVisible);
    automationModeComboBox.setVisible(advancedPanelVisible);
    attackSlider.setVisible(advancedPanelVisible);
    releaseSlider.setVisible(advancedPanelVisible);
    holdSlider.setVisible(advancedPanelVisible);
    breathReductionSlider.setVisible(advancedPanelVisible);
    transientPreservationSlider.setVisible(advancedPanelVisible);
    attackLabel.setVisible(advancedPanelVisible);
    releaseLabel.setVisible(advancedPanelVisible);
    holdLabel.setVisible(advancedPanelVisible);
    breathLabel.setVisible(advancedPanelVisible);
    transientLabel.setVisible(advancedPanelVisible);
    
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
            break;
        case WindowSize::Medium:
            width = mediumWidth;
            height = mediumHeight;
            break;
        case WindowSize::Large:
            width = largeWidth;
            height = largeHeight;
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
    
    CustomLookAndFeel::drawGrainTexture(g, getLocalBounds(), 0.012f);
    
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
    
    // Advanced panel is now painted by its own component (AdvancedPanelComponent)
}

void VocalRiderAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    // Header bar
    auto headerArea = bounds.removeFromTop(32).reduced(10, 4);
    titleLabel.setBounds(headerArea.removeFromLeft(110));
    
    advancedButton.setBounds(headerArea.removeFromRight(30));
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
    
    // Control panel at bottom
    auto controlArea = bounds.removeFromBottom(controlPanelHeight);
    controlPanel.setBounds(controlArea);
    
    // Advanced panel (below header if visible)
    if (advancedPanelVisible)
    {
        int advHeight = 85;
        auto advArea = bounds.removeFromTop(advHeight).reduced(12, 0);
        advancedPanel.setBounds(advArea);
        
        auto advContent = advArea.reduced(10, 8);
        
        // Dropdowns
        auto dropdownRow = advContent.removeFromTop(22);
        lookAheadComboBox.setBounds(dropdownRow.removeFromLeft(130));
        dropdownRow.removeFromLeft(8);
        detectionModeComboBox.setBounds(dropdownRow.removeFromLeft(115));
        dropdownRow.removeFromLeft(8);
        automationModeComboBox.setBounds(dropdownRow.removeFromLeft(100));
        
        advContent.removeFromTop(4);
        
        // Knobs
        int advKnobSize = 44;
        int labelHeight = 10;
        int numKnobs = 5;
        int spacing = (advContent.getWidth() - advKnobSize * numKnobs) / (numKnobs + 1);
        int y = advContent.getY();
        int x = advContent.getX() + spacing;
        
        attackLabel.setBounds(x, y, advKnobSize, labelHeight);
        attackSlider.setBounds(x, y + labelHeight, advKnobSize, advKnobSize);
        x += advKnobSize + spacing;
        
        releaseLabel.setBounds(x, y, advKnobSize, labelHeight);
        releaseSlider.setBounds(x, y + labelHeight, advKnobSize, advKnobSize);
        x += advKnobSize + spacing;
        
        holdLabel.setBounds(x, y, advKnobSize, labelHeight);
        holdSlider.setBounds(x, y + labelHeight, advKnobSize, advKnobSize);
        x += advKnobSize + spacing;
        
        breathLabel.setBounds(x, y, advKnobSize, labelHeight);
        breathReductionSlider.setBounds(x, y + labelHeight, advKnobSize, advKnobSize);
        x += advKnobSize + spacing;
        
        transientLabel.setBounds(x, y, advKnobSize, labelHeight);
        transientPreservationSlider.setBounds(x, y + labelHeight, advKnobSize, advKnobSize);
    }
    
    // Waveform fills all remaining space (extends behind panels)
    waveformDisplay.setBounds(getLocalBounds().withTrimmedTop(32).withTrimmedBottom(controlPanelHeight - 15));
    
    // Control panel contents
    auto ctrlContent = controlArea.reduced(15, 10);
    
    int targetKnobSize = 80;
    int smallKnobSize = 58;
    int labelHeight = 12;
    
    int centerX = ctrlContent.getCentreX();
    int knobY = ctrlContent.getY() + 6;
    
    // Target in center (larger)
    targetLabel.setBounds(centerX - targetKnobSize / 2, knobY, targetKnobSize, labelHeight);
    targetSlider.setBounds(centerX - targetKnobSize / 2, knobY + labelHeight, targetKnobSize, targetKnobSize);
    
    // Range to the left
    int rangeX = centerX - targetKnobSize / 2 - smallKnobSize - 35;
    rangeLabel.setBounds(rangeX, knobY + 8, smallKnobSize, labelHeight);
    rangeSlider.setBounds(rangeX, knobY + labelHeight + 8, smallKnobSize, smallKnobSize);
    
    // Speed to the right
    int speedX = centerX + targetKnobSize / 2 + 35;
    speedLabel.setBounds(speedX, knobY + 8, smallKnobSize, labelHeight);
    speedSlider.setBounds(speedX, knobY + labelHeight + 8, smallKnobSize, smallKnobSize);
    
    // Natural toggle
    naturalToggle.setBounds(ctrlContent.getRight() - 90, ctrlContent.getCentreY() - 12, 85, 24);
    
    // Resize button in lower right corner
    int resizeButtonSize = 20;
    resizeButton.setBounds(getWidth() - resizeButtonSize - 8, getHeight() - resizeButtonSize - 8, 
                           resizeButtonSize, resizeButtonSize);
}

void VocalRiderAudioProcessorEditor::timerCallback()
{
    waveformDisplay.setInputLevel(audioProcessor.getInputLevelDb());
    waveformDisplay.setOutputLevel(audioProcessor.getOutputLevelDb());
    
    if (auto* param = audioProcessor.getApvts().getRawParameterValue(VocalRiderAudioProcessor::targetLevelParamId))
        waveformDisplay.setTargetLevel(param->load());
    if (auto* param = audioProcessor.getApvts().getRawParameterValue(VocalRiderAudioProcessor::rangeParamId))
        waveformDisplay.setRange(param->load());

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
