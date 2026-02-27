/*
  ==============================================================================

    PluginProcessor.cpp
    Created: 2026
    Author:  MBM Audio

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Parameter IDs
const juce::String VocalRiderAudioProcessor::targetLevelParamId = "targetLevel";
const juce::String VocalRiderAudioProcessor::speedParamId = "speed";
const juce::String VocalRiderAudioProcessor::rangeParamId = "range";
const juce::String VocalRiderAudioProcessor::boostRangeParamId = "boostRange";
const juce::String VocalRiderAudioProcessor::cutRangeParamId = "cutRange";
const juce::String VocalRiderAudioProcessor::gainOutputParamId = "gainOutput";

// Advanced parameter IDs
const juce::String VocalRiderAudioProcessor::attackParamId = "attack";
const juce::String VocalRiderAudioProcessor::releaseParamId = "release";
const juce::String VocalRiderAudioProcessor::holdParamId = "hold";
const juce::String VocalRiderAudioProcessor::breathReductionParamId = "breathReduction";
const juce::String VocalRiderAudioProcessor::transientPreservationParamId = "transientPreservation";
const juce::String VocalRiderAudioProcessor::naturalModeParamId = "naturalMode";
const juce::String VocalRiderAudioProcessor::smartSilenceParamId = "smartSilence";
const juce::String VocalRiderAudioProcessor::outputTrimParamId = "outputTrim";
const juce::String VocalRiderAudioProcessor::noiseFloorParamId = "noiseFloor";

//==============================================================================
// Factory Presets
const std::vector<VocalRiderAudioProcessor::Preset>& VocalRiderAudioProcessor::getFactoryPresets()
{
    // Category, Name, Target, Speed, Range, Attack, Release, Hold, Natural, SmartSilence, LUFS, Breath, Transient, NoiseFloor
    // Extended: lookAheadMode, outputTrim, boostRange (-1=same as range), cutRange (-1=same), rangeLocked
    static const std::vector<Preset> presets = {
        // Vocals - for singing and music production
        { "Vocals",    "Gentle Lead",      -18.0f, 30.0f,  6.0f, 100.0f, 400.0f,  50.0f, true,  false, false, 0.0f,  0.0f, -100.0f },
        { "Vocals",    "Tight Lead",       -16.0f, 55.0f,  8.0f,  40.0f, 150.0f,  30.0f, false, false, false, 0.0f, 30.0f, -100.0f },
        { "Vocals",    "Dynamic Lead",     -17.0f, 45.0f, 10.0f,  60.0f, 250.0f,  40.0f, true,  false, false, 0.0f, 20.0f, -100.0f },
        { "Vocals",    "Backing Vocals",   -22.0f, 35.0f,  5.0f,  80.0f, 350.0f,  60.0f, true,  false, false, 3.0f,  0.0f, -45.0f },
        { "Vocals",    "Breathy Vocal",    -19.0f, 40.0f,  7.0f,  70.0f, 300.0f,  80.0f, true,  true,  false, 6.0f,  0.0f, -42.0f },
        { "Vocals",    "Aggressive Mix",   -14.0f, 75.0f, 12.0f,  15.0f,  60.0f,  10.0f, false, false, false, 0.0f, 50.0f, -100.0f },
        { "Vocals",    "Boost Only",       -20.0f, 50.0f,  8.0f,  50.0f, 200.0f,  40.0f, true,  false, false, 0.0f,  0.0f, -100.0f, 0, 0.0f, 10.0f, 2.0f, false },
        { "Vocals",    "Cut Only",         -16.0f, 50.0f,  8.0f,  40.0f, 180.0f,  30.0f, false, false, false, 0.0f, 20.0f, -100.0f, 0, 0.0f, 2.0f, 10.0f, false },
        
        // Speaking/Dialogue - for podcasts, voiceovers, etc.
        { "Speaking",  "Podcast",          -18.0f, 50.0f,  9.0f,  50.0f, 200.0f,  30.0f, false, true,  true,  4.0f,  0.0f, -48.0f },
        { "Speaking",  "Broadcast",        -16.0f, 60.0f, 10.0f,  30.0f, 150.0f,  20.0f, false, true,  true,  3.0f,  0.0f, -50.0f },
        { "Speaking",  "Dialogue",         -20.0f, 40.0f,  8.0f,  80.0f, 300.0f, 100.0f, true,  true,  false, 5.0f,  0.0f, -100.0f },
        { "Speaking",  "Voiceover",        -17.0f, 55.0f,  8.0f,  45.0f, 180.0f,  40.0f, false, true,  true,  4.0f,  0.0f, -46.0f },
        { "Speaking",  "Interview",        -19.0f, 45.0f,  7.0f,  60.0f, 250.0f,  50.0f, true,  true,  false, 6.0f,  0.0f, -44.0f },
        { "Speaking",  "Audiobook",        -21.0f, 35.0f,  6.0f,  90.0f, 400.0f,  80.0f, true,  true,  true,  5.0f,  0.0f, -50.0f },
        { "Speaking",  "Lift Whispers",    -22.0f, 45.0f,  8.0f,  70.0f, 280.0f,  60.0f, true,  true,  true,  4.0f,  0.0f, -46.0f, 0, 0.0f, 12.0f, 3.0f, false },
        
        // Mattie's Favorites - curated collection (natural + LUFS focused)
        { "Mattie's Favorites", "Natural LUFS",     -18.0f, 60.0f,  7.0f,  40.0f, 180.0f,  35.0f, true,  false, true,  0.0f,  0.0f, -100.0f },
        { "Mattie's Favorites", "Smooth & Natural", -18.0f, 45.0f,  6.0f,  55.0f, 240.0f,  45.0f, true,  false, true,  0.0f, 15.0f, -100.0f },
        { "Mattie's Favorites", "Fast Natural",     -17.0f, 70.0f,  8.0f,  25.0f, 120.0f,  20.0f, true,  false, true,  0.0f, 25.0f, -100.0f },
        { "Mattie's Favorites", "Clean Podcast",    -16.0f, 55.0f,  9.0f,  35.0f, 160.0f,  30.0f, true,  true,  true,  5.0f,  0.0f, -48.0f },
        { "Mattie's Favorites", "Transparent",      -19.0f, 40.0f,  5.0f,  70.0f, 300.0f,  50.0f, true,  false, true,  0.0f,  0.0f, -100.0f },
        { "Mattie's Favorites", "Punchy Vocal",     -16.0f, 65.0f, 10.0f,  30.0f, 140.0f,  25.0f, true,  false, false, 0.0f, 40.0f, -100.0f },
        { "Mattie's Favorites", "Gentle Lift",      -19.0f, 40.0f,  6.0f,  60.0f, 260.0f,  45.0f, true,  false, true,  0.0f, 10.0f, -100.0f, 0, 0.0f, 8.0f, 3.0f, false },
        { "Mattie's Favorites", "Tame Peaks",       -17.0f, 55.0f,  8.0f,  35.0f, 150.0f,  25.0f, true,  false, true,  0.0f, 30.0f, -100.0f, 0, 0.0f, 3.0f, 10.0f, false },
    };
    return presets;
}

std::vector<juce::String> VocalRiderAudioProcessor::getPresetCategories()
{
    return { "Vocals", "Speaking", "Mattie's Favorites" };
}

//==============================================================================
VocalRiderAudioProcessor::VocalRiderAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                     .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Cache parameter pointers for real-time access
    targetLevelParam = apvts.getRawParameterValue(targetLevelParamId);
    speedParam = apvts.getRawParameterValue(speedParamId);
    rangeParam = apvts.getRawParameterValue(rangeParamId);
    boostRangeParam = apvts.getRawParameterValue(boostRangeParamId);
    cutRangeParam = apvts.getRawParameterValue(cutRangeParamId);
    gainOutputParamPtr = apvts.getRawParameterValue(gainOutputParamId);
    
    // Advanced parameter pointers
    attackParam = apvts.getRawParameterValue(attackParamId);
    releaseParam = apvts.getRawParameterValue(releaseParamId);
    holdParam = apvts.getRawParameterValue(holdParamId);
    breathReductionParam = apvts.getRawParameterValue(breathReductionParamId);
    transientPreservationParam = apvts.getRawParameterValue(transientPreservationParamId);
    naturalModeParam = apvts.getRawParameterValue(naturalModeParamId);
    smartSilenceParam = apvts.getRawParameterValue(smartSilenceParamId);
    outputTrimParam = apvts.getRawParameterValue(outputTrimParamId);
    noiseFloorParam = apvts.getRawParameterValue(noiseFloorParamId);

    #if JucePlugin_Build_Standalone
    formatManager.registerBasicFormats();
    #endif
    
    startTimerHz(30);
}

VocalRiderAudioProcessor::~VocalRiderAudioProcessor()
{
    stopTimer();
    
    #if JucePlugin_Build_Standalone
    transportSource.setSource(nullptr);
    #endif
}

//==============================================================================
void VocalRiderAudioProcessor::timerCallback()
{
    // Relay gain output to host from the message thread.
    // In VST3, beginEdit/performEdit/endEdit only work from the message thread.
    // The audio thread path (outputParameterChanges) is treated as display-only
    // by Cubase, so we must also push values here for automation recording.
    //
    // In Read mode the DAW drives the parameter — do NOT write back.
    if (automationMode.load() == AutomationMode::Read)
    {
        if (messageThreadGestureActive)
        {
            if (auto* param = apvts.getParameter(gainOutputParamId))
                param->endChangeGesture();
            messageThreadGestureActive = false;
        }
        return;
    }
    
    float currentGain = gainOutputParam.load();
    bool gainIsActive = std::abs(currentGain) > 0.05f;
    
    if (auto* param = apvts.getParameter(gainOutputParamId))
    {
        bool valueChanged = std::abs(currentGain - lastSentGainOutput) > 0.005f;
        
        if (gainIsActive || messageThreadGestureActive)
        {
            if (!messageThreadGestureActive)
            {
                param->beginChangeGesture();
                messageThreadGestureActive = true;
            }
            
            if (valueChanged)
            {
                param->setValueNotifyingHost(param->convertTo0to1(currentGain));
                lastSentGainOutput = currentGain;
            }
            
            if (!gainIsActive)
            {
                param->setValueNotifyingHost(param->convertTo0to1(0.0f));
                lastSentGainOutput = 0.0f;
                param->endChangeGesture();
                messageThreadGestureActive = false;
            }
        }
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VocalRiderAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Gain Output FIRST: -12 to +12 dB, automatable (for Cubase and other DAWs to record automation)
    // Placing this first ensures it's the primary/default automation target for hosts like Cubase.
    // Marked as non-automatable from the user side so hosts like Cubase treat it as output-only;
    // the plugin drives this value via setValueNotifyingHost on every block.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(gainOutputParamId, 1),
        "Gain Output",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB").withAutomatable(true)
    ));

    // Target Level: -50 to 0 dB, default -22 dB
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(targetLevelParamId, 1),
        "Target Level",
        juce::NormalisableRange<float>(-50.0f, 0.0f, 0.1f),
        -22.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")
    ));

    // Speed: 0-100%, default 50%
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(speedParamId, 1),
        "Speed",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")
    ));

    // Range: 0-12 dB, default 6 dB (legacy linked range, kept for backward compat)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(rangeParamId, 1),
        "Range",
        juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f),
        6.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")
    ));

    // Boost Range: 0-12 dB (max gain boost allowed)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(boostRangeParamId, 1),
        "Boost Range",
        juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f),
        6.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")
    ));

    // Cut Range: 0-12 dB (max gain cut allowed)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(cutRangeParamId, 1),
        "Cut Range",
        juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f),
        6.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")
    ));

    //==============================================================================
    // Advanced Parameters
    
    // Attack: 1ms to 500ms, default 50ms
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(attackParamId, 1),
        "Attack",
        juce::NormalisableRange<float>(1.0f, 500.0f, 0.1f, 0.4f),  // Skewed for finer control at lower values
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")
    ));

    // Release: 10ms to 2000ms, default 200ms
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(releaseParamId, 1),
        "Release",
        juce::NormalisableRange<float>(10.0f, 2000.0f, 0.1f, 0.4f),  // Skewed
        200.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")
    ));

    // Hold: 0ms to 500ms, default 50ms
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(holdParamId, 1),
        "Hold",
        juce::NormalisableRange<float>(0.0f, 500.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")
    ));

    // Breath Reduction: 0 to 12 dB, default 0 (off)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(breathReductionParamId, 1),
        "Breath Reduction",
        juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")
    ));

    // Transient Preservation: 0 to 100%, default 50%
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(transientPreservationParamId, 1),
        "Transient Preservation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")
    ));

    // Natural Mode: On/Off, default On
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(naturalModeParamId, 1),
        "Natural Mode",
        true  // default on
    ));

    // Smart Silence: On/Off, default Off
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(smartSilenceParamId, 1),
        "Smart Silence",
        false  // default off
    ));

    // Output Trim: -12 to +12 dB, default 0
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(outputTrimParamId, 1),
        "Output Trim",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")
    ));

    // Noise Floor: -60 to -20 dB, default -100 (off)
    // When set above -60dB, signals below this threshold are ignored by the gain rider
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(noiseFloorParamId, 1),
        "Noise Floor",
        juce::NormalisableRange<float>(-60.0f, -20.0f, 0.1f),
        -60.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")
    ));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String VocalRiderAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool VocalRiderAudioProcessor::acceptsMidi() const { return false; }
bool VocalRiderAudioProcessor::producesMidi() const { return false; }
bool VocalRiderAudioProcessor::isMidiEffect() const { return false; }

double VocalRiderAudioProcessor::getTailLengthSeconds() const
{
    if (currentSampleRate <= 0.0)
        return 0.0;
    return isLookAheadEnabled() ? (static_cast<double>(lookAheadSamples.load()) / currentSampleRate) : 0.0;
}

void VocalRiderAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer,
                                                      juce::MidiBuffer& midiMessages)
{
    // IMPORTANT: Do NOT modify ANY DSP state here (gain smoother, gate, phrase detection,
    // atomics, etc.). Some DAWs call processBlockBypassed during plugin initialization,
    // bus reconfiguration, or other internal state transitions - even when the user hasn't
    // bypassed the plugin. Any state modification here can corrupt the gain calculation
    // in processBlock, causing boost bias and incorrect behavior.
    //
    // Signal that look-ahead buffer should be cleared on next active processBlock
    // (avoids playing stale delayed audio when un-bypassing).
    lookAheadNeedsClear.store(true);
    
    // Just let the base class handle it (clears extra output channels, passes audio through).
    juce::AudioProcessor::processBlockBypassed(buffer, midiMessages);
}

int VocalRiderAudioProcessor::getNumPrograms() { return 1; }
int VocalRiderAudioProcessor::getCurrentProgram() { return 0; }
void VocalRiderAudioProcessor::setCurrentProgram(int) {}
const juce::String VocalRiderAudioProcessor::getProgramName(int) { return {}; }
void VocalRiderAudioProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
void VocalRiderAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    float speed = speedParam->load();
    float windowMs = juce::jmap(speed, 0.0f, 100.0f, 100.0f, 10.0f);
    rmsDetector.prepare(sampleRate, windowMs);
    
    gainSmoother.prepare(sampleRate);
    peakDetector.prepare(sampleRate);
    peakDetector.setAttackTime(0.1f);
    peakDetector.setReleaseTime(50.0f);
    
    // Sidechain bandpass for vocal spectral focus (200Hz - 4kHz)
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;
    
    sidechainHPF.prepare(spec);
    sidechainHPF.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    sidechainHPF.setCutoffFrequency(200.0f);  // Focus on vocal fundamentals and up
    sidechainHPF.setResonance(0.707f);
    
    sidechainLPF.prepare(spec);
    sidechainLPF.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    sidechainLPF.setCutoffFrequency(4000.0f);  // Cut high-frequency sibilance/noise
    sidechainLPF.setResonance(0.707f);
    
    // Transient detection filter (fast HPF for detecting sharp attacks)
    transientHPF.prepare(spec);
    transientHPF.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    transientHPF.setCutoffFrequency(2000.0f);
    transientHPF.setResonance(0.707f);
    transientEnvelope = 0.0f;
    sustainEnvelope = 0.0f;
    
    // LUFS K-weighting filters
    lufsPreFilter.prepare(spec);
    lufsPreFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    lufsPreFilter.setCutoffFrequency(38.0f);  // High-pass for K-weighting
    lufsPreFilter.setResonance(0.5f);
    
    lufsHighShelf.prepare(spec);
    lufsHighShelf.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    lufsHighShelf.setCutoffFrequency(1500.0f);  // Approximate high shelf boost
    lufsHighShelf.setResonance(0.707f);
    
    lufsIntegrator = 0.0f;
    
    // Vocal focus filters (frequency-weighted detection for better vocal tracking)
    vocalFocusHighPass.prepare(spec);
    vocalFocusHighPass.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    vocalFocusHighPass.setCutoffFrequency(180.0f);  // Cut low rumble below vocal range
    vocalFocusHighPass.setResonance(0.707f);
    
    vocalFocusLowPass.prepare(spec);
    vocalFocusLowPass.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    vocalFocusLowPass.setCutoffFrequency(5000.0f);  // Cut high frequencies above vocal presence
    vocalFocusLowPass.setResonance(0.707f);
    
    vocalFocusBandBoost.prepare(spec);
    vocalFocusBandBoost.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    vocalFocusBandBoost.setCutoffFrequency(2500.0f);  // Boost vocal presence region
    vocalFocusBandBoost.setResonance(1.0f);  // Moderate Q for presence boost
    
    // Sidechain RMS detector
    sidechainRmsDetector.prepare(sampleRate);
    sidechainRmsDetector.setWindowSize(0.05f);  // 50ms window for sidechain
    lufsSampleCount = 0;
    
    // Breath detection state
    isBreath = false;
    breathEnvelope = 0.0f;
    
    // Reset noise gate state
    gateOpen = false;
    gateSmoothedLevel = -100.0f;
    
    // Do NOT call updateAttackReleaseFromSpeed here - let processBlock use the
    // restored/saved values for attack/release/hold from APVTS. Only update from
    // speed when the user explicitly changes the speed slider.
    
    // Compute sample-rate-independent parameter smoothing (~3ms time constant)
    paramSmoothingCoeff = std::exp(-1.0f / (0.003f * static_cast<float>(sampleRate)));
    
    lastSpeed = speed;

    // Pre-allocate scratch buffers for processBlock (avoid heap allocs on audio thread)
    preparedBlockSize = samplesPerBlock * 2;  // 2x headroom for hosts that exceed samplesPerBlock
    scratchMonoBuffer.setSize(1, preparedBlockSize, false, true);  // clearExtraSpace=true
    scratchFilteredBuffer.setSize(1, preparedBlockSize, false, true);
    scratchSidechainBuffer.setSize(1, preparedBlockSize, false, true);
    scratchInputSamples.assign(static_cast<size_t>(preparedBlockSize), 0.0f);
    scratchGainSamples.assign(static_cast<size_t>(preparedBlockSize), 0.0f);
    scratchPeakAheadLevels.assign(static_cast<size_t>(preparedBlockSize), -100.0f);
    scratchPrecomputedGains.assign(static_cast<size_t>(preparedBlockSize), 1.0f);  // 1.0 = unity gain
    scratchOutputSamples.assign(static_cast<size_t>(preparedBlockSize), 0.0f);

    // Prepare look-ahead buffer (allocate for max 30ms)
    maxLookAheadSamples = static_cast<int>(0.030 * sampleRate);
    updateLookAheadSamples();
    lookAheadDelayBuffer.setSize(2, maxLookAheadSamples + samplesPerBlock);
    lookAheadDelayBuffer.clear();
    lookAheadGainBuffer.resize(static_cast<size_t>(maxLookAheadSamples + samplesPerBlock), 1.0f);
    lookAheadWritePos = 0;
    lookAheadBufferFilled = false;

    // Phrase detection parameters
    phraseMinSamples = static_cast<int>(0.1 * sampleRate);
    silenceMinSamples = static_cast<int>(0.15 * sampleRate);
    inPhrase.store(false);
    phraseAccumulator = 0.0f;
    phraseSampleCount = 0;
    currentPhraseGainDb = 0.0f;
    phraseLastLevelDb = -100.0f;
    phraseStartSample = 0;
    phraseEndSample = 0;
    lastPhraseGainDb = 0.0f;
    silenceSampleCount = 0;
    phraseGainSmoother = 0.0f;
    
    // Phrase lookahead buffer (500ms for phrase pre-analysis)
    phraseLookaheadSamples = static_cast<int>(0.5 * sampleRate);
    phraseLookaheadBuffer.resize(static_cast<size_t>(phraseLookaheadSamples), 0.0f);
    phraseLookaheadWritePos = 0;

    // Auto-calibrate duration
    autoCalibrateAccumulator = 0.0f;
    autoCalibrateSampleCount = 0;

    // Report latency to DAW for Plugin Delay Compensation
    setLatencySamples(getLookAheadLatency());

    #if JucePlugin_Build_Standalone
    currentBlockSize = samplesPerBlock;
    transportSource.prepareToPlay(samplesPerBlock, sampleRate);
    #endif
}

void VocalRiderAudioProcessor::releaseResources()
{
    rmsDetector.reset();
    gainSmoother.reset();
    peakDetector.reset();
    lookAheadDelayBuffer.clear();
    lookAheadBufferFilled = false;

    #if JucePlugin_Build_Standalone
    transportSource.releaseResources();
    #endif
}

bool VocalRiderAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    // Sidechain can be mono, stereo, or disabled
    auto sidechainSet = layouts.getChannelSet(true, 1);
    if (!sidechainSet.isDisabled() 
        && sidechainSet != juce::AudioChannelSet::mono()
        && sidechainSet != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

//==============================================================================
float VocalRiderAudioProcessor::softClip(float sample)
{
    if (sample > ceilingLinear)
    {
        float excess = sample - ceilingLinear;
        return ceilingLinear + std::tanh(excess * 2.0f) * (1.0f - ceilingLinear);
    }
    else if (sample < -ceilingLinear)
    {
        float excess = -sample - ceilingLinear;
        return -(ceilingLinear + std::tanh(excess * 2.0f) * (1.0f - ceilingLinear));
    }
    return sample;
}

void VocalRiderAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    // Clear stale look-ahead buffer after bypass
    if (lookAheadNeedsClear.exchange(false))
    {
        lookAheadDelayBuffer.clear();
        lookAheadWritePos = 0;
        lookAheadBufferFilled = false;
    }

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    #if JucePlugin_Build_Standalone
    if (fileLoaded.load() && transportPlaying.load())
    {
        juce::AudioSourceChannelInfo info(&buffer, 0, numSamples);
        transportSource.getNextAudioBlock(info);
    }
    #endif

    // Safe sample rate (fallback to 44100 if host hasn't called prepareToPlay yet)
    const double safeSampleRate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    
    // Get parameters with smoothing to prevent clicks/pops on rapid changes
    float targetLevelRaw = targetLevelParam->load();
    float speed = speedParam->load();
    float boostRangeRaw = boostRangeParam->load();
    float cutRangeRaw = cutRangeParam->load();
    bool useLookAhead = isLookAheadEnabled();
    
    // Smooth target and range parameters (prevents clicks on rapid UI changes)
    smoothedTargetLevel = smoothedTargetLevel * paramSmoothingCoeff + targetLevelRaw * (1.0f - paramSmoothingCoeff);
    smoothedBoostRange = smoothedBoostRange * paramSmoothingCoeff + boostRangeRaw * (1.0f - paramSmoothingCoeff);
    smoothedCutRange = smoothedCutRange * paramSmoothingCoeff + cutRangeRaw * (1.0f - paramSmoothingCoeff);
    float targetLevel = smoothedTargetLevel;
    float boostRange = smoothedBoostRange;
    float cutRange = smoothedCutRange;

    // Update speed-dependent RMS window (always keep in sync)
    if (std::abs(speed - lastSpeed) > 0.5f)
    {
        float windowMs = juce::jmap(speed, 0.0f, 100.0f, 100.0f, 10.0f);
        rmsDetector.setWindowSize(windowMs);
        lastSpeed = speed;
        // Note: attack/release are NOT overwritten here — they're driven by
        // the APVTS parameters below, which persist independently of speed.
    }

    // Sync advanced parameters from APVTS (source of truth for persistence)
    if (attackParam != nullptr) attackMs.store(attackParam->load());
    if (releaseParam != nullptr) releaseMs.store(releaseParam->load());
    if (holdParam != nullptr) holdMs.store(holdParam->load());
    if (breathReductionParam != nullptr) breathReductionDb.store(breathReductionParam->load());
    if (transientPreservationParam != nullptr) transientPreservation.store(transientPreservationParam->load() / 100.0f);
    if (naturalModeParam != nullptr) naturalModeEnabled.store(naturalModeParam->load() > 0.5f);
    if (smartSilenceParam != nullptr) smartSilenceEnabled.store(smartSilenceParam->load() > 0.5f);
    if (outputTrimParam != nullptr) outputTrimDb.store(outputTrimParam->load());
    if (noiseFloorParam != nullptr) noiseFloorDb.store(noiseFloorParam->load());

    // Apply advanced attack/release/hold (only update when changed to avoid needless exp() calls)
    {
        float atk = attackMs.load();
        float rel = releaseMs.load();
        float hld = holdMs.load();
        if (std::abs(atk - lastAttackMs) > 0.01f) { gainSmoother.setAttackTime(atk); lastAttackMs = atk; }
        if (std::abs(rel - lastReleaseMs) > 0.01f) { gainSmoother.setReleaseTime(rel); lastReleaseMs = rel; }
        if (std::abs(hld - lastHoldMs) > 0.01f) { gainSmoother.setHoldTime(hld); lastHoldMs = hld; }
    }

    // Quick silent-buffer check: if the entire buffer is silent and natural mode is on,
    // ensure phrase state is cleared (handles DAW stop where blocks become silence)
    if (naturalModeEnabled.load())
    {
        float maxSample = 0.0f;
        auto mainBusSilence = getBusBuffer(buffer, true, 0);
        const int mainInputChannels = mainBusSilence.getNumChannels();
        for (int ch = 0; ch < mainInputChannels; ++ch)
        {
            auto* channelData = mainBusSilence.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                maxSample = juce::jmax(maxSample, std::abs(channelData[i]));
        }
        if (maxSample < 0.0001f)
        {
            // Entire buffer is silence - increment silence counter and clear phrase if sustained
            processorSilenceBlockCount++;
            if (processorSilenceBlockCount > 10)  // ~10 blocks of pure silence (~100ms)
            {
                inPhrase.store(false);
                phraseGainSmoother = 0.0f;
                currentPhraseGainDb = 0.0f;
            }
        }
        else
        {
            processorSilenceBlockCount = 0;
        }
    }

    // Guard: if the host sends a buffer larger than what we pre-allocated,
    // pass audio through unprocessed rather than heap-allocating on the audio thread.
    // The next prepareToPlay call will resize properly.
    if (numSamples > preparedBlockSize)
    {
        return;
    }
    
    if (numSamples <= 0 || totalNumInputChannels <= 0)
        return;

    // Create mono sum for level detection from main input bus only (not sidechain)
    auto& monoBuffer = scratchMonoBuffer;
    monoBuffer.clear();
    
    auto mainBusBuffer = getBusBuffer(buffer, true, 0);
    const int numChannels = juce::jmax(1, mainBusBuffer.getNumChannels());
    for (int channel = 0; channel < numChannels; ++channel)
    {
        monoBuffer.addFrom(0, 0, mainBusBuffer, channel, 0, numSamples,
                           1.0f / static_cast<float>(numChannels));
    }

    // Mono read pointer (needed for LUFS, spectral analysis, auto-calibrate, etc.)
    const float* monoRead = monoBuffer.getReadPointer(0);
    
    // Store samples for waveform display (mono average for RMS-based display)
    auto& inputSamples = scratchInputSamples;
    auto& gainSamples = scratchGainSamples;
    std::fill(gainSamples.begin(), gainSamples.begin() + numSamples, 0.0f);
    for (int i = 0; i < numSamples; ++i)
    {
        inputSamples[static_cast<size_t>(i)] = std::abs(monoRead[i]);
    }

    // Input metering (before HPF)
    float inputRms = monoBuffer.getRMSLevel(0, 0, numSamples);
    float inputDb = juce::Decibels::gainToDecibels(inputRms, -100.0f);
    inputLevelDb.store(inputDb);

    // === SIDECHAIN INPUT PROCESSING ===
    bool useSidechain = sidechainEnabled.load() && hasSidechainInput();
    float sidechainLevel = -100.0f;
    
    if (useSidechain)
    {
        auto scBusBuffer = getBusBuffer(buffer, true, 1);
        int scChannels = scBusBuffer.getNumChannels();
        
        if (scChannels > 0)
        {
            auto& sidechainBuffer = scratchSidechainBuffer;
            sidechainBuffer.clear();
            
            float scInvCh = 1.0f / static_cast<float>(juce::jmin(scChannels, 2));
            for (int ch = 0; ch < scChannels && ch < 2; ++ch)
            {
                sidechainBuffer.addFrom(0, 0, scBusBuffer, ch, 0, numSamples, scInvCh);
            }
            
            float scRms = sidechainBuffer.getRMSLevel(0, 0, numSamples);
            sidechainLevel = juce::Decibels::gainToDecibels(scRms, -100.0f);
            sidechainLevelDb.store(sidechainLevel);
        }
    }
    else
    {
        sidechainLevelDb.store(-100.0f);
    }

    // === VOCAL FOCUS FILTER (frequency-weighted detection) ===
    // Create filtered copy for detection (isolates vocal fundamentals, pre-allocated)
    auto& filteredBuffer = scratchFilteredBuffer;
    filteredBuffer.copyFrom(0, 0, monoBuffer, 0, 0, numSamples);
    
    bool useVocalFocus = vocalFocusEnabled.load();
    
    // Use SubBlock to ensure filter only processes valid numSamples (not entire pre-allocated buffer)
    {
        auto filteredBlockSub = juce::dsp::AudioBlock<float>(filteredBuffer)
                                    .getSubBlock(0, static_cast<size_t>(numSamples));
        juce::dsp::ProcessContextReplacing<float> filterContext(filteredBlockSub);
        
        if (useVocalFocus)
        {
            vocalFocusHighPass.process(filterContext);  // Cut below 180Hz
            vocalFocusLowPass.process(filterContext);   // Cut above 5kHz
        }
        else
        {
            sidechainHPF.process(filterContext);  // High-pass at 200Hz
            sidechainLPF.process(filterContext);  // Low-pass at 4kHz
        }
    }
    
    const float* filteredRead = filteredBuffer.getReadPointer(0);
    
    // === LUFS CALCULATION (if enabled) ===
    bool useLufs = useLufsMode.load();
    float measuredLufs = -100.0f;
    if (lufsNeedsReset.exchange(false))
    {
        lufsIntegrator = 0.0f;
        lufsSampleCount = 0;
    }
    if (useLufs)
    {
        measuredLufs = calculateLufs(monoRead, numSamples);
        inputLufs.store(measuredLufs);
    }
    
    // === BREATH DETECTION ===
    float breathReduce = breathReductionDb.load();
    bool doBreathDetection = breathReduce > 0.0f;
    float spectralFlat = 0.0f;
    float zeroCross = 0.0f;
    if (doBreathDetection)
    {
        spectralFlat = calculateSpectralFlatness(monoRead, numSamples);
        zeroCross = calculateZeroCrossingRate(monoRead, numSamples);
        isBreath = detectBreath(spectralFlat, zeroCross);
    }
    
    // === TRANSIENT PRESERVATION ===
    float transientPres = transientPreservation.load();
    bool doTransientPreservation = transientPres > 0.0f;
    
    // === AUTOMATION MODE ===
    AutomationMode autoMode = automationMode.load();
    
    // === READ MODE: Use DAW automation instead of internal calculation ===
    float automationGainDb = 0.0f;
    bool useAutomationGain = (autoMode == AutomationMode::Read);
    if (useAutomationGain)
    {
        // Read the gain value from the DAW automation parameter
        if (auto* param = apvts.getParameter(gainOutputParamId))
        {
            float normalizedValue = param->getValue();  // 0-1 from DAW
            automationGainDb = param->convertFrom0to1(normalizedValue);  // Convert to dB
        }
    }

    // Auto-calibrate
    if (autoCalibrateNeedsReset.exchange(false))
    {
        autoCalibrateAccumulator = 0.0f;
        autoCalibrateSampleCount = 0;
    }
    if (autoCalibrating.load())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float squared = monoRead[i] * monoRead[i];
            autoCalibrateAccumulator += squared;
            autoCalibrateSampleCount++;
        }

        if (autoCalibrateSampleCount >= static_cast<int>(autoCalibrateSeconds * currentSampleRate))
        {
            float avgRms = std::sqrt(autoCalibrateAccumulator / static_cast<float>(autoCalibrateSampleCount));
            float suggestedTarget = juce::Decibels::gainToDecibels(avgRms, -60.0f);
            suggestedTarget = juce::jlimit(-50.0f, -6.0f, suggestedTarget);
            
            if (auto* param = apvts.getParameter(targetLevelParamId))
            {
                param->setValueNotifyingHost(param->convertTo0to1(suggestedTarget));
            }
            
            autoCalibrating.store(false);
        }
    }

    // === PREDICTIVE LOOK-AHEAD: Pre-scan for peaks (pre-allocated) ===
    auto& peakAheadLevels = scratchPeakAheadLevels;
    std::fill(peakAheadLevels.begin(), peakAheadLevels.begin() + numSamples, -100.0f);
    if (useLookAhead)
    {
        int scanWindow = juce::jmin(lookAheadSamples.load(), numSamples);
        // O(n) sliding window maximum using backward pass
        // Step 1: Forward pass to find max from each sample to end of its window
        float runningMax = 0.0f;
        for (int sample = numSamples - 1; sample >= 0; --sample)
        {
            // Reset running max at window boundaries
            if ((numSamples - 1 - sample) % scanWindow == 0)
                runningMax = 0.0f;
            runningMax = juce::jmax(runningMax, std::abs(filteredRead[sample]));
            peakAheadLevels[static_cast<size_t>(sample)] = runningMax;
        }
        // Step 2: Second pass to merge block boundaries (complete sliding window max)
        // and convert to dB
        float suffixMax = 0.0f;
        for (int sample = 0; sample < numSamples; ++sample)
        {
            int blockEnd = sample + scanWindow;
            if (blockEnd <= numSamples)
            {
                // Approximate: use the forward-pass max (covers most of the window)
                float peakVal = peakAheadLevels[static_cast<size_t>(sample)];
                peakAheadLevels[static_cast<size_t>(sample)] = 
                    juce::Decibels::gainToDecibels(peakVal, -100.0f);
            }
            else
            {
                float peakVal = peakAheadLevels[static_cast<size_t>(sample)];
                peakAheadLevels[static_cast<size_t>(sample)] = 
                    juce::Decibels::gainToDecibels(peakVal, -100.0f);
            }
        }
    }

    // Pre-compute gain values (pre-allocated, unity-initialized for safety)
    auto& precomputedGains = scratchPrecomputedGains;
    std::fill(precomputedGains.begin(), precomputedGains.begin() + numSamples, 1.0f);
    
    // Gate smoothing coefficient (fast attack, slower release)
    const float gateSmoothAttack = 0.99f;
    const float gateSmoothRelease = 0.9995f;
    
    // === SIDECHAIN TARGET ADJUSTMENT ===
    // When sidechain is enabled, dynamic target = sidechain RMS + offset
    float effectiveTarget = targetLevel;
    if (useSidechain && sidechainLevel > -60.0f)
    {
        float offsetDb = sidechainAmount.load();
        effectiveTarget = sidechainLevel + offsetDb;
        effectiveTarget = juce::jlimit(-50.0f, 0.0f, effectiveTarget);
    }
    effectiveTargetDb.store(effectiveTarget);
    targetLevel = effectiveTarget;
    
    // Check if Natural (phrase-based) mode is enabled
    bool useNaturalMode = naturalModeEnabled.load();
    
    // Handle thread-safe phrase state reset (triggered by UI toggle)
    if (phraseStateNeedsReset.exchange(false))
    {
        inPhrase.store(false);
        phraseAccumulator = 0.0f;
        phraseSampleCount = 0;
        currentPhraseGainDb = 0.0f;
        lastPhraseGainDb = 0.0f;
        silenceSampleCount = 0;
        phraseGainSmoother = 0.0f;
        phraseLastLevelDb = -100.0f;
    }
    
    // Noise floor threshold - signals below this are treated as silence
    float noiseFloorThreshold = noiseFloorDb.load();
    bool useNoiseFloor = noiseFloorThreshold > -59.9f;  // Active when above minimum (-60 dB)
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Use FILTERED signal for level detection (frequency-weighted)
        float rmsLevelDb = rmsDetector.processSample(filteredRead[sample]);
        float peakLevelDb = peakDetector.processSample(filteredRead[sample]);
        
        // === NOISE GATE LOGIC ===
        float currentLevel = juce::jmax(rmsLevelDb, peakLevelDb);
        float smoothCoeff = (currentLevel > gateSmoothedLevel) ? gateSmoothAttack : gateSmoothRelease;
        gateSmoothedLevel = smoothCoeff * gateSmoothedLevel + (1.0f - smoothCoeff) * currentLevel;
        
        // Gate with hysteresis
        if (!gateOpen && gateSmoothedLevel > gateThresholdDb + gateHysteresisDb)
            gateOpen = true;
        else if (gateOpen && gateSmoothedLevel < gateThresholdDb)
            gateOpen = false;
        
        float targetGainDb = 0.0f;
        
        // === NOISE FLOOR CHECK ===
        // If noise floor is active and current level is below it, skip gain calculation
        bool belowNoiseFloor = false;
        if (useNoiseFloor && currentLevel < noiseFloorThreshold)
        {
            belowNoiseFloor = true;
            targetGainDb = getSilenceGainDb();  // Apply silence gain (0 or -6dB with smart silence)
        }
        
        if (!belowNoiseFloor && useNaturalMode)
        {
            // === PHRASE-BASED (NATURAL) MODE ===
            // Use smoothed RMS level (already computed) for stable phrase detection
            // This avoids the noise from per-sample level analysis
            
            float sampleValue = filteredRead[sample];
            
            // Use the already-computed RMS level for phrase detection (much more stable)
            bool audioPresent = rmsLevelDb > gateThresholdDb;
            
            // Track smoothed energy changes for phrase boundary detection
            // Use exponential smoothing on RMS level for stable energy tracking
            float smoothCoeffPhrase = 0.995f;  // Very smooth tracking
            float smoothedPhraseLevel = smoothCoeffPhrase * phraseLastLevelDb + (1.0f - smoothCoeffPhrase) * rmsLevelDb;
            float energyDelta = std::abs(smoothedPhraseLevel - phraseLastLevelDb);
            phraseLastLevelDb = smoothedPhraseLevel;
            
            // Only detect energy jumps when the change is genuinely large AND sustained
            bool energyJump = (energyDelta > 6.0f && rmsLevelDb > gateThresholdDb + 8.0f 
                              && phraseSampleCount > phraseMinSamples * 2);
            
            if (audioPresent)
            {
                // Audio present
                silenceSampleCount = 0;
                
                // Start new phrase on: first audio after silence, OR very significant energy jump
                bool currentlyInPhrase = inPhrase.load();
                if (!currentlyInPhrase)
                {
                    // Start new phrase after silence
                    inPhrase.store(true);
                    phraseStartSample = sample;
                    phraseAccumulator = 0.0f;
                    phraseSampleCount = 0;
                    lastPhraseGainDb = currentPhraseGainDb;
                }
                else if (energyJump)
                {
                    // Soft reset for energy-based phrase change (keep some history)
                    phraseAccumulator *= 0.5f;
                    phraseSampleCount = static_cast<int>(phraseSampleCount * 0.5f);
                }
                
                // Accumulate for phrase level calculation
                phraseAccumulator += sampleValue * sampleValue;
                phraseSampleCount++;
                
                // Calculate running phrase level and gain
                if (phraseSampleCount > phraseMinSamples / 4)  // After initial samples
                {
                    float phraseRms = std::sqrt(phraseAccumulator / static_cast<float>(phraseSampleCount));
                    float phraseLevelDb = juce::Decibels::gainToDecibels(phraseRms, -100.0f);
                    float gainNeeded = targetLevel - phraseLevelDb;
                    
                    // === BREATH REDUCTION (integrated) ===
                    if (doBreathDetection && isBreath)
                    {
                        gainNeeded = juce::jmin(gainNeeded, -breathReduce);
                    }
                    
                    // === TRANSIENT PRESERVATION (integrated) ===
                    if (doTransientPreservation && peakLevelDb > rmsLevelDb + 6.0f)
                    {
                        // Reduce gain adjustment during transients to preserve dynamics
                        float transientAmount = (peakLevelDb - rmsLevelDb - 6.0f) / 12.0f;
                        transientAmount = juce::jlimit(0.0f, 1.0f, transientAmount) * transientPres;
                        gainNeeded *= (1.0f - transientAmount * 0.7f);
                    }
                    
                    // Soft knee for phrase gain
                    if (std::abs(gainNeeded) < kneeWidthDb)
                    {
                        float ratio = gainNeeded / kneeWidthDb;
                        gainNeeded = gainNeeded * (0.5f + 0.5f * ratio * ratio * (gainNeeded > 0 ? 1.0f : -1.0f));
                    }
                    
                    currentPhraseGainDb = juce::jlimit(-cutRange, boostRange, gainNeeded);
                    
                    // === PEAK-AWARE GAIN LIMITING (Natural Mode) ===
                    // Same protection as Standard mode: prevent boost from clipping peaks
                    if (currentPhraseGainDb > 0.0f)
                    {
                        static constexpr float peakSafeCeiling = -1.0f;
                        float peakAfterGain = peakLevelDb + currentPhraseGainDb;
                        if (peakAfterGain > peakSafeCeiling)
                        {
                            currentPhraseGainDb = juce::jmax(0.0f, peakSafeCeiling - peakLevelDb);
                        }
                    }
                }
            }
            else
            {
                // Silence detected
                silenceSampleCount++;
                
                // Use hold time concept: don't end phrase immediately
                int holdSamplesForPhrase = static_cast<int>(holdMs.load() * safeSampleRate / 1000.0);
                holdSamplesForPhrase = juce::jmax(holdSamplesForPhrase, silenceMinSamples);
                
                if (inPhrase.load() && silenceSampleCount > holdSamplesForPhrase)
                {
                    // End of phrase detected
                    inPhrase.store(false);
                    phraseEndSample = sample;
                }
            }
            
            // Target gain: phrase gain when in phrase, silence gain otherwise
            float targetPhraseGain = inPhrase.load() ? currentPhraseGainDb : getSilenceGainDb();
            
            // Use attack/release coefficients for smooth gain transitions
            float gainDelta = targetPhraseGain - phraseGainSmoother;
            float phraseSmooth;
            if (gainDelta > 0)
            {
                // Gain increasing (boost) - use attack time (slower for natural feel)
                float effectiveAttack = attackMs.load() * 1.5f;  // Natural mode uses slightly slower attack
                phraseSmooth = std::exp(-1.0f / (effectiveAttack * static_cast<float>(safeSampleRate) / 1000.0f));
            }
            else
            {
                // Gain decreasing (cut) - use release time (slower for natural feel)
                float effectiveRelease = releaseMs.load() * 1.5f;  // Natural mode uses slightly slower release
                phraseSmooth = std::exp(-1.0f / (effectiveRelease * static_cast<float>(safeSampleRate) / 1000.0f));
            }
            
            phraseGainSmoother = phraseSmooth * phraseGainSmoother + (1.0f - phraseSmooth) * targetPhraseGain;
            
            targetGainDb = phraseGainSmoother;
            
            // Don't boost silence (apply smart silence reduction if enabled)
            if (!gateOpen)
            {
                targetGainDb = juce::jmin(targetGainDb, getSilenceGainDb());
            }
        }
        else if (!belowNoiseFloor)
        {
            // === STANDARD MODE (sample-by-sample) ===
            
            // Blend peak and RMS for transient sensitivity
            float effectiveLevelDb;
            
            // Use LUFS or RMS based on mode
            float baseLevelDb = useLufs ? measuredLufs : rmsLevelDb;
            
            if (peakLevelDb > baseLevelDb + 3.0f)
            {
                effectiveLevelDb = baseLevelDb + (peakLevelDb - baseLevelDb) * 0.7f;
            }
            else
            {
                effectiveLevelDb = baseLevelDb;
            }
            
            // If using predictive look-ahead, blend with peak-ahead levels
            if (useLookAhead)
            {
                float peakAhead = peakAheadLevels[static_cast<size_t>(sample)];
                if (peakAhead > effectiveLevelDb)
                {
                    effectiveLevelDb = effectiveLevelDb + (peakAhead - effectiveLevelDb) * 0.6f;
                }
            }
            
            // Calculate target gain
            float gainNeeded = targetLevel - effectiveLevelDb;
            
            // === BREATH REDUCTION ===
            if (doBreathDetection && isBreath)
            {
                // If breath detected, reduce gain by breathReductionDb
                gainNeeded = juce::jmin(gainNeeded, -breathReduce);
            }
            
            // === TRANSIENT PRESERVATION (Standard Mode) ===
            if (doTransientPreservation && peakLevelDb > rmsLevelDb + 6.0f)
            {
                // Reduce gain adjustment during transients to preserve dynamics
                float transientAmount = (peakLevelDb - rmsLevelDb - 6.0f) / 12.0f;
                transientAmount = juce::jlimit(0.0f, 1.0f, transientAmount) * transientPres;
                gainNeeded *= (1.0f - transientAmount * 0.7f);
            }
            
            // Soft knee
            if (std::abs(gainNeeded) < kneeWidthDb)
            {
                float ratio = gainNeeded / kneeWidthDb;
                gainNeeded = gainNeeded * (0.5f + 0.5f * ratio * ratio * (gainNeeded > 0 ? 1.0f : -1.0f));
            }
            
            targetGainDb = juce::jlimit(-cutRange, boostRange, gainNeeded);
            
            // === PEAK-AWARE GAIN LIMITING ===
            // Prevent boost from pushing peaks past the soft clipper ceiling.
            // Without this, the RMS-based gain decision can boost hot signals into clipping
            // because RMS is always lower than peak (crest factor).
            if (targetGainDb > 0.0f)
            {
                static constexpr float peakSafeCeiling = -1.0f;  // dB headroom below 0 dBFS
                float peakAfterGain = peakLevelDb + targetGainDb;
                if (peakAfterGain > peakSafeCeiling)
                {
                    targetGainDb = juce::jmax(0.0f, peakSafeCeiling - peakLevelDb);
                }
            }
            
            // Noise gate: Apply smart silence reduction if enabled
            if (!gateOpen)
            {
                targetGainDb = juce::jmin(targetGainDb, getSilenceGainDb());
            }
            
            if (effectiveLevelDb < gateThresholdDb - 10.0f)
            {
                targetGainDb = juce::jmin(targetGainDb, 0.0f);
            }
        }
        
        // === READ MODE: Override calculated gain with DAW automation ===
        if (useAutomationGain)
        {
            targetGainDb = automationGainDb;  // Use the gain from DAW automation
        }
        
        float smoothedGainDb = gainSmoother.processSample(targetGainDb);
        
        gainSamples[static_cast<size_t>(sample)] = smoothedGainDb;
        precomputedGains[static_cast<size_t>(sample)] = juce::Decibels::decibelsToGain(smoothedGainDb);
    }

    // Apply gain with or without look-ahead
    const int currentLookAheadSamples = lookAheadSamples.load();  // Cache atomic for tight loop
    if (useLookAhead && currentLookAheadSamples > 0)
    {
        // LOOK-AHEAD PROCESSING
        // The audio is delayed by currentLookAheadSamples, but gains are computed from
        // the CURRENT (non-delayed) audio. This means the gain decisions are based on
        // audio that is "in the future" relative to the delayed output, giving the
        // algorithm time to react before transients actually arrive in the output.
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            int bufferSize = lookAheadDelayBuffer.getNumSamples();
            int readPos = (lookAheadWritePos - currentLookAheadSamples + bufferSize) % bufferSize;
            
            for (int channel = 0; channel < numChannels; ++channel)
            {
                float* channelData = buffer.getWritePointer(channel);
                float* delayData = lookAheadDelayBuffer.getWritePointer(channel);
                
                delayData[lookAheadWritePos] = channelData[sample];
                
                float delayedSample = lookAheadBufferFilled ? delayData[readPos] : 0.0f;
                
                float gain = precomputedGains[static_cast<size_t>(sample)];
                
                float processed = delayedSample * gain;
                channelData[sample] = softClip(processed);
            }
            
            lookAheadWritePos = (lookAheadWritePos + 1) % bufferSize;
            
            // Mark buffer as filled once we've written enough samples
            if (!lookAheadBufferFilled && lookAheadWritePos >= currentLookAheadSamples)
                lookAheadBufferFilled = true;
        }
    }
    else
    {
        // NORMAL PROCESSING (no look-ahead) - DIRECT GAIN APPLICATION
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float gainLinear = precomputedGains[static_cast<size_t>(sample)];
            
            for (int channel = 0; channel < numChannels; ++channel)
            {
                float* channelData = buffer.getWritePointer(channel);
                float inputSample = channelData[sample];
                
                float processed = inputSample * gainLinear;
                channelData[sample] = softClip(processed);
            }
        }
    }

    float finalGainDb = gainSmoother.getCurrentGainDb();
    currentGainDb.store(finalGainDb);
    
    // === AUTOMATION OUTPUT ===
    // Handle gesture end request from UI thread (must happen on audio thread for thread safety)
    if (automationGestureNeedsEnd.exchange(false))
    {
        if (automationGestureActive.load())
        {
            if (auto* param = apvts.getParameter(gainOutputParamId))
                param->endChangeGesture();
            automationGestureActive.store(false);
        }
    }
    
    // Update the gain output parameter for DAW automation.
    // In Read mode the DAW drives the parameter value — do NOT write back.
    gainOutputParam.store(finalGainDb);
    
    if (autoMode != AutomationMode::Read)
    {
        if (auto* param = apvts.getParameter(gainOutputParamId))
        {
            float normalizedGain = param->convertTo0to1(finalGainDb);
            bool gainIsActive = std::abs(finalGainDb) > 0.05f;
            
            // Cubase (VST3) requires beginChangeGesture/endChangeGesture pairs to
            // record automation. We bracket value changes with gestures so that all
            // hosts (Cubase, Pro Tools, Logic) can record the output parameter via
            // their own track automation modes.
            bool shouldWriteGesture = false;
            
            switch (autoMode)
            {
                case AutomationMode::Write:
                    shouldWriteGesture = true;
                    break;
                    
                case AutomationMode::Latch:
                    if (automationWriteActive || gainIsActive)
                    {
                        shouldWriteGesture = true;
                        automationWriteActive = true;
                    }
                    break;
                    
                case AutomationMode::Touch:
                    shouldWriteGesture = gainIsActive;
                    break;
                    
                case AutomationMode::Off:
                    shouldWriteGesture = gainIsActive;
                    break;
                    
                default:
                    break;
            }
            
            if (shouldWriteGesture)
            {
                if (!automationGestureActive)
                {
                    param->beginChangeGesture();
                    automationGestureActive = true;
                }
            }
            else if (automationGestureActive)
            {
                param->endChangeGesture();
                automationGestureActive = false;
            }
            
            param->setValueNotifyingHost(normalizedGain);
        }
    }
    else if (automationGestureActive)
    {
        // Switched to Read mode while a gesture was active — close it
        if (auto* param = apvts.getParameter(gainOutputParamId))
            param->endChangeGesture();
        automationGestureActive = false;
    }

    // Apply output trim (makeup/reduction gain, -12 to +12 dB)
    float trimGain = juce::Decibels::decibelsToGain(outputTrimDb.load());
    if (std::abs(trimGain - 1.0f) > 0.001f)
    {
        for (int sample = 0; sample < numSamples; ++sample)
        {
            for (int channel = 0; channel < numChannels; ++channel)
            {
                float* channelData = buffer.getWritePointer(channel);
                channelData[sample] = softClip(channelData[sample] * trimGain);
            }
        }
    }

    // Output samples for waveform display (mono average for RMS-based display)
    auto& outputSamples = scratchOutputSamples;
    {
        float invCh = 1.0f / static_cast<float>(numChannels);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float sum = 0.0f;
            for (int channel = 0; channel < numChannels; ++channel)
                sum += std::abs(buffer.getSample(channel, sample));
            outputSamples[static_cast<size_t>(sample)] = sum * invCh;
        }
    }

    // Push to waveform display (thread-safe access)
    if (auto* display = waveformDisplay.load())
    {
        display->pushSamples(inputSamples.data(), outputSamples.data(), 
                             gainSamples.data(), numSamples);
        display->setTargetLevel(targetLevelRaw);
    }

    // Output metering - use RMS from main output bus only
    float outputRmsLevel = buffer.getRMSLevel(0, 0, numSamples);
    if (numChannels > 1)
        outputRmsLevel = juce::jmax(outputRmsLevel, buffer.getRMSLevel(1, 0, numSamples));
    outputLevelDb.store(juce::Decibels::gainToDecibels(outputRmsLevel, -100.0f));
}

//==============================================================================
bool VocalRiderAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* VocalRiderAudioProcessor::createEditor()
{
    return new VocalRiderAudioProcessorEditor(*this);
}

//==============================================================================
void VocalRiderAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    
    // Add advanced settings to state
    state.setProperty("lookAheadMode", lookAheadMode.load(), nullptr);
    state.setProperty("naturalMode", naturalModeEnabled.load(), nullptr);
    state.setProperty("attackMs", attackMs.load(), nullptr);
    state.setProperty("releaseMs", releaseMs.load(), nullptr);
    state.setProperty("holdMs", holdMs.load(), nullptr);
    state.setProperty("useLufs", useLufsMode.load(), nullptr);
    state.setProperty("breathReduction", breathReductionDb.load(), nullptr);
    state.setProperty("transientPreservation", transientPreservation.load(), nullptr);
    state.setProperty("outputTrim", outputTrimDb.load(), nullptr);
    state.setProperty("automationMode", static_cast<int>(automationMode.load()), nullptr);
    state.setProperty("noiseFloor", noiseFloorDb.load(), nullptr);
    
    // UI state persistence
    state.setProperty("smartSilence", smartSilenceEnabled.load(), nullptr);
    state.setProperty("scrollSpeed", scrollSpeedSetting.load(), nullptr);
    state.setProperty("presetIndex", currentPresetIndex.load(), nullptr);
    state.setProperty("windowSizeIndex", windowSizeIndex.load(), nullptr);
    
    // Range lock state
    state.setProperty("rangeLocked", rangeLocked.load(), nullptr);
    
    // Sidechain / vocal focus settings
    state.setProperty("sidechainEnabled", sidechainEnabled.load(), nullptr);
    state.setProperty("sidechainAmount", static_cast<double>(sidechainAmount.load()), nullptr);
    state.setProperty("vocalFocusEnabled", vocalFocusEnabled.load(), nullptr);
    
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr)
        copyXmlToBinary(*xml, destData);
}

void VocalRiderAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    // Accept both current APVTS tag ("Parameters") and legacy tag ("VocalRiderState")
    // for backward compatibility with sessions saved by older plugin versions
    if (xmlState != nullptr && (xmlState->hasTagName(apvts.state.getType()) 
                                || xmlState->hasTagName("VocalRiderState")))
    {
        auto state = juce::ValueTree::fromXml(*xmlState);
        apvts.replaceState(state);
        
        // Backward compat: if old state has "range" but not "boostRange"/"cutRange",
        // copy range to both boost and cut
        if (!state.getChildWithProperty("id", "boostRange").isValid()
            && !state.hasProperty("boostRange"))
        {
            auto rangeVal = apvts.getRawParameterValue(rangeParamId)->load();
            if (auto* bp = apvts.getParameter(boostRangeParamId))
                bp->setValueNotifyingHost(bp->convertTo0to1(rangeVal));
            if (auto* cp = apvts.getParameter(cutRangeParamId))
                cp->setValueNotifyingHost(cp->convertTo0to1(rangeVal));
        }
        
        // Range lock state
        if (state.hasProperty("rangeLocked"))
            setRangeLocked(static_cast<bool>(state.getProperty("rangeLocked")));
        else
            setRangeLocked(true);
        
        // Restore advanced settings
        if (state.hasProperty("lookAheadMode"))
            setLookAheadMode(static_cast<int>(state.getProperty("lookAheadMode")));
        if (state.hasProperty("naturalMode"))
            setNaturalModeEnabled(static_cast<bool>(state.getProperty("naturalMode")));
        if (state.hasProperty("attackMs"))
            attackMs.store(static_cast<float>(state.getProperty("attackMs")));
        if (state.hasProperty("releaseMs"))
            releaseMs.store(static_cast<float>(state.getProperty("releaseMs")));
        if (state.hasProperty("holdMs"))
            holdMs.store(static_cast<float>(state.getProperty("holdMs")));
        if (state.hasProperty("useLufs"))
            setUseLufs(static_cast<bool>(state.getProperty("useLufs")));
        if (state.hasProperty("breathReduction"))
            setBreathReduction(static_cast<float>(state.getProperty("breathReduction")));
        if (state.hasProperty("transientPreservation"))
            setTransientPreservation(static_cast<float>(state.getProperty("transientPreservation")));
        if (state.hasProperty("outputTrim"))
            setOutputTrim(static_cast<float>(state.getProperty("outputTrim")));
        if (state.hasProperty("automationMode"))
        {
            int modeInt = static_cast<int>(state.getProperty("automationMode"));
            if (modeInt >= 0 && modeInt <= static_cast<int>(AutomationMode::Write))
                setAutomationMode(static_cast<AutomationMode>(modeInt));
        }
        if (state.hasProperty("noiseFloor"))
            setNoiseFloor(static_cast<float>(state.getProperty("noiseFloor")));
        
        // UI state restoration
        if (state.hasProperty("smartSilence"))
            setSmartSilenceEnabled(static_cast<bool>(state.getProperty("smartSilence")));
        if (state.hasProperty("scrollSpeed"))
            setScrollSpeed(static_cast<float>(state.getProperty("scrollSpeed")));
        if (state.hasProperty("presetIndex"))
            setCurrentPresetIndex(static_cast<int>(state.getProperty("presetIndex")));
        if (state.hasProperty("windowSizeIndex"))
            setWindowSizeIndex(static_cast<int>(state.getProperty("windowSizeIndex")));
        
        // Sidechain / vocal focus settings
        if (state.hasProperty("sidechainEnabled"))
            setSidechainEnabled(static_cast<bool>(state.getProperty("sidechainEnabled")));
        if (state.hasProperty("sidechainAmount"))
            setSidechainAmount(static_cast<float>(state.getProperty("sidechainAmount")));
        if (state.hasProperty("vocalFocusEnabled"))
            setVocalFocusEnabled(static_cast<bool>(state.getProperty("vocalFocusEnabled")));
    }
}

//==============================================================================
void VocalRiderAudioProcessor::startAutoCalibrate()
{
    autoCalibrateNeedsReset.store(true);  // Signal audio thread to reset accumulators
    autoCalibrating.store(true);
}

void VocalRiderAudioProcessor::stopAutoCalibrate()
{
    autoCalibrating.store(false);
}

float VocalRiderAudioProcessor::getAutoCalibrateProgress() const
{
    if (!autoCalibrating.load()) return 0.0f;
    float total = autoCalibrateSeconds * static_cast<float>(currentSampleRate);
    if (total <= 0.0f) return 0.0f;  // Guard against zero sample rate
    return juce::jmin(1.0f, static_cast<float>(autoCalibrateSampleCount) / total);
}

//==============================================================================
void VocalRiderAudioProcessor::setLookAheadMode(int mode)
{
    lookAheadMode.store(juce::jlimit(0, 3, mode));
    updateLookAheadSamples();
    setLatencySamples(getLookAheadLatency());
}

int VocalRiderAudioProcessor::getLookAheadLatency() const
{
    return lookAheadSamples.load();
}

void VocalRiderAudioProcessor::updateLookAheadSamples()
{
    int mode = lookAheadMode.load();
    int samples = 0;
    switch (mode)
    {
        case 0: samples = 0; break;                                          // Off
        case 1: samples = static_cast<int>(0.010 * currentSampleRate); break; // 10ms
        case 2: samples = static_cast<int>(0.020 * currentSampleRate); break; // 20ms
        case 3: samples = static_cast<int>(0.030 * currentSampleRate); break; // 30ms
        default: samples = 0; break;
    }
    lookAheadSamples.store(samples);
}

void VocalRiderAudioProcessor::setNaturalModeEnabled(bool enabled)
{
    bool wasEnabled = naturalModeEnabled.load();
    naturalModeEnabled.store(enabled);
    
    // Signal that phrase state should be reset (audio thread will handle it safely)
    if (enabled != wasEnabled)
        phraseStateNeedsReset.store(true);
}

void VocalRiderAudioProcessor::setUseLufs(bool useLufs)
{
    useLufsMode.store(useLufs);
    lufsNeedsReset.store(true);  // Signal audio thread to reset (avoids data race)
}

void VocalRiderAudioProcessor::setBreathReduction(float reductionDb)
{
    breathReductionDb.store(juce::jlimit(0.0f, 12.0f, reductionDb));
}

void VocalRiderAudioProcessor::setTransientPreservation(float amount)
{
    transientPreservation.store(juce::jlimit(0.0f, 1.0f, amount));
}

void VocalRiderAudioProcessor::setOutputTrim(float trimDb)
{
    float clamped = juce::jlimit(-12.0f, 12.0f, trimDb);
    outputTrimDb.store(clamped);
    
    // Also update the APVTS parameter so it syncs with processBlock
    if (auto* param = apvts.getParameter(outputTrimParamId))
    {
        param->setValueNotifyingHost(param->convertTo0to1(clamped));
    }
}

void VocalRiderAudioProcessor::setNoiseFloor(float thresholdDb)
{
    float clamped = juce::jlimit(-60.0f, -20.0f, thresholdDb);
    noiseFloorDb.store(clamped);
    
    if (auto* param = apvts.getParameter(noiseFloorParamId))
    {
        param->setValueNotifyingHost(param->convertTo0to1(clamped));
    }
}

bool VocalRiderAudioProcessor::hasSidechainInput() const
{
    auto* bus = getBus(true, 1);
    return bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0;
}

void VocalRiderAudioProcessor::setAutomationMode(AutomationMode mode)
{
    AutomationMode oldMode = automationMode.load();
    automationMode.store(mode);
    
    // Signal audio thread to end any active gesture (don't call endChangeGesture from UI thread)
    if (automationGestureActive.load() && mode != oldMode)
    {
        automationGestureNeedsEnd.store(true);
    }
    
    // Reset write active flag when switching modes
    automationWriteActive.store(false);
}

//==============================================================================
// Helper functions for advanced detection

float VocalRiderAudioProcessor::calculateLufs(const float* samples, int numSamples)
{
    // Simplified LUFS calculation with K-weighting approximation
    float sumSquared = 0.0f;
    
    for (int i = 0; i < numSamples; ++i)
    {
        // Apply K-weighting (simplified)
        float filtered = lufsPreFilter.processSample(0, samples[i]);
        filtered = lufsHighShelf.processSample(0, filtered) * 1.4f + filtered;  // Boost highs
        sumSquared += filtered * filtered;
    }
    
    lufsIntegrator += sumSquared;
    lufsSampleCount += numSamples;
    
    // Prevent unbounded accumulation: use ~3 second sliding window
    // Reset when we exceed the window, preserving recent average
    int maxLufsSamples = static_cast<int>(3.0 * currentSampleRate);
    if (maxLufsSamples > 0 && lufsSampleCount > maxLufsSamples)
    {
        // Decay integrator to approximate sliding window
        float ratio = static_cast<float>(maxLufsSamples) / static_cast<float>(lufsSampleCount);
        lufsIntegrator *= ratio;
        lufsSampleCount = maxLufsSamples;
    }
    
    if (lufsSampleCount > 0)
    {
        float meanSquared = lufsIntegrator / static_cast<float>(lufsSampleCount);
        float rms = std::sqrt(meanSquared);
        return juce::Decibels::gainToDecibels(rms, -100.0f) - 0.691f;  // LUFS offset
    }
    
    return -100.0f;
}

float VocalRiderAudioProcessor::calculateSpectralFlatness(const float* samples, int numSamples)
{
    // Spectral flatness: ratio of geometric mean to arithmetic mean
    // High value = noise-like (breath), Low value = tonal (voice)
    
    if (numSamples < 2) return 0.0f;
    
    float sumAbs = 0.0f;
    float sumLog = 0.0f;
    int validSamples = 0;
    
    // On older hardware, downsample for large blocks to reduce expensive log() calls
    int step = (numSamples > 256) ? 4 : 1;
    
    for (int i = 0; i < numSamples; i += step)
    {
        float absVal = std::abs(samples[i]);
        if (absVal > 1e-10f)
        {
            sumAbs += absVal;
            sumLog += std::log(absVal);
            validSamples++;
        }
    }
    
    if (validSamples < 2) return 0.0f;
    
    float arithmeticMean = sumAbs / static_cast<float>(validSamples);
    float geometricMean = std::exp(sumLog / static_cast<float>(validSamples));
    
    if (arithmeticMean < 1e-10f) return 0.0f;
    
    return geometricMean / arithmeticMean;
}

float VocalRiderAudioProcessor::calculateZeroCrossingRate(const float* samples, int numSamples)
{
    if (numSamples < 2) return 0.0f;
    
    int crossings = 0;
    for (int i = 1; i < numSamples; ++i)
    {
        if ((samples[i] >= 0.0f) != (samples[i - 1] >= 0.0f))
            crossings++;
    }
    
    return static_cast<float>(crossings) / static_cast<float>(numSamples - 1);
}

bool VocalRiderAudioProcessor::detectBreath(float spectralFlatness, float zeroCrossRate)
{
    // Breaths have: high spectral flatness (noisy), high zero-crossing rate
    // Voice has: low spectral flatness (tonal), moderate zero-crossing rate
    
    bool likelyBreath = (spectralFlatness > 0.3f && zeroCrossRate > 0.2f);
    
    // Smooth the breath detection
    float target = likelyBreath ? 1.0f : 0.0f;
    breathEnvelope = 0.95f * breathEnvelope + 0.05f * target;
    
    return breathEnvelope > 0.5f;
}

void VocalRiderAudioProcessor::separateTransientSustain(float sample, float& transient, float& sustain)
{
    // Separate transient and sustain components
    float highPassed = transientHPF.processSample(0, sample);
    float absHigh = std::abs(highPassed);
    
    // Transient envelope (fast attack, slow release)
    if (absHigh > transientEnvelope)
        transientEnvelope = 0.1f * transientEnvelope + 0.9f * absHigh;  // Fast attack
    else
        transientEnvelope = 0.999f * transientEnvelope;  // Slow release
    
    // Sustain envelope (slower overall)
    float absSample = std::abs(sample);
    sustainEnvelope = 0.999f * sustainEnvelope + 0.001f * absSample;
    
    // Calculate ratio
    float transientRatio = (sustainEnvelope > 0.0001f) ? 
                           juce::jmin(1.0f, transientEnvelope / sustainEnvelope) : 0.0f;
    
    transient = sample * transientRatio;
    sustain = sample * (1.0f - transientRatio);
}

void VocalRiderAudioProcessor::setAttackMs(float ms)
{
    attackMs.store(juce::jlimit(1.0f, 500.0f, ms));
}

void VocalRiderAudioProcessor::setReleaseMs(float ms)
{
    releaseMs.store(juce::jlimit(10.0f, 2000.0f, ms));
}

void VocalRiderAudioProcessor::setHoldMs(float ms)
{
    holdMs.store(juce::jlimit(0.0f, 500.0f, ms));
}

void VocalRiderAudioProcessor::updateAttackReleaseFromSpeed(float speed, bool updateUI)
{
    // Map speed to attack/release
    float normalizedSpeed = juce::jlimit(0.0f, 100.0f, speed) / 100.0f;
    float speedFactor = std::pow(normalizedSpeed, 0.5f);
    
    float attack = juce::jmap(speedFactor, 500.0f, 5.0f);
    float release = juce::jmap(speedFactor, 1000.0f, 20.0f);
    
    // Update internal values
    attackMs.store(attack);
    releaseMs.store(release);
    
    // Only update APVTS parameters when explicitly requested (user moving speed slider)
    // This prevents overwriting saved parameter values during initialization
    if (updateUI)
    {
        if (auto* param = apvts.getParameter(attackParamId))
            param->setValueNotifyingHost(param->convertTo0to1(attack));
        if (auto* param = apvts.getParameter(releaseParamId))
            param->setValueNotifyingHost(param->convertTo0to1(release));
    }
}

void VocalRiderAudioProcessor::loadPreset(int index)
{
    const auto& presets = getFactoryPresets();
    if (index >= 0 && index < static_cast<int>(presets.size()))
        loadPresetFromData(presets[static_cast<size_t>(index)]);
}

void VocalRiderAudioProcessor::resetToDefaults()
{
    // Reset main parameters to defaults
    if (auto* param = apvts.getParameter(targetLevelParamId))
        param->setValueNotifyingHost(param->convertTo0to1(-18.0f));  // Default target
    if (auto* param = apvts.getParameter(speedParamId))
        param->setValueNotifyingHost(param->convertTo0to1(50.0f));   // Default speed
    if (auto* param = apvts.getParameter(rangeParamId))
        param->setValueNotifyingHost(param->convertTo0to1(12.0f));   // Default range
    if (auto* param = apvts.getParameter(boostRangeParamId))
        param->setValueNotifyingHost(param->convertTo0to1(12.0f));
    if (auto* param = apvts.getParameter(cutRangeParamId))
        param->setValueNotifyingHost(param->convertTo0to1(12.0f));
    rangeLocked.store(true);
    
    // Reset advanced parameters
    attackMs.store(10.0f);
    releaseMs.store(100.0f);
    holdMs.store(50.0f);
    naturalModeEnabled.store(false);
    smartSilenceEnabled.store(false);
    breathReductionDb.store(0.0f);
    transientPreservation.store(0.0f);
    outputTrimDb.store(0.0f);
    noiseFloorDb.store(-60.0f);
    lookAheadMode.store(0);
    useLufsMode.store(false);
    automationMode.store(AutomationMode::Off);  // Default to Off (internal gain calculation, no automation I/O)
}

//==============================================================================
// User Presets

juce::File VocalRiderAudioProcessor::getUserPresetsFolder()
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    #if JUCE_MAC
    auto presetsDir = appData.getChildFile("Application Support").getChildFile("MBM Audio").getChildFile("magic.RIDE").getChildFile("User Presets");
    #elif JUCE_WINDOWS
    auto presetsDir = appData.getChildFile("MBM Audio").getChildFile("magic.RIDE").getChildFile("User Presets");
    #else
    auto presetsDir = appData.getChildFile("MBM Audio").getChildFile("magic.RIDE").getChildFile("User Presets");
    #endif
    presetsDir.createDirectory();
    return presetsDir;
}

VocalRiderAudioProcessor::Preset VocalRiderAudioProcessor::getCurrentSettingsAsPreset(const juce::String& name) const
{
    Preset p;
    p.category = "User";
    p.name = name;
    p.targetLevel = apvts.getRawParameterValue(targetLevelParamId)->load();
    p.speed = apvts.getRawParameterValue(speedParamId)->load();
    p.range = apvts.getRawParameterValue(rangeParamId)->load();
    p.attackMs = attackMs.load();
    p.releaseMs = releaseMs.load();
    p.holdMs = holdMs.load();
    p.naturalMode = naturalModeEnabled.load();
    p.smartSilence = smartSilenceEnabled.load();
    p.useLufs = useLufsMode.load();
    p.breathReduction = breathReductionDb.load();
    p.transientPreservation = transientPreservation.load() * 100.0f;  // Convert from 0-1 to %
    p.noiseFloor = noiseFloorDb.load();
    p.lookAheadMode = lookAheadMode.load();
    p.outputTrim = outputTrimDb.load();
    return p;
}

bool VocalRiderAudioProcessor::saveUserPreset(const juce::String& name)
{
    auto preset = getCurrentSettingsAsPreset(name);
    auto folder = getUserPresetsFolder();
    
    // Sanitize filename
    juce::String safeName = name.replaceCharacters("\\/:*?\"<>|", "_________");
    auto file = folder.getChildFile(safeName + ".xml");
    
    auto xml = std::make_unique<juce::XmlElement>("UserPreset");
    xml->setAttribute("name", preset.name);
    xml->setAttribute("targetLevel", static_cast<double>(preset.targetLevel));
    xml->setAttribute("speed", static_cast<double>(preset.speed));
    xml->setAttribute("range", static_cast<double>(preset.range));
    xml->setAttribute("attackMs", static_cast<double>(preset.attackMs));
    xml->setAttribute("releaseMs", static_cast<double>(preset.releaseMs));
    xml->setAttribute("holdMs", static_cast<double>(preset.holdMs));
    xml->setAttribute("naturalMode", preset.naturalMode);
    xml->setAttribute("smartSilence", preset.smartSilence);
    xml->setAttribute("useLufs", preset.useLufs);
    xml->setAttribute("breathReduction", static_cast<double>(preset.breathReduction));
    xml->setAttribute("transientPreservation", static_cast<double>(preset.transientPreservation));
    xml->setAttribute("noiseFloor", static_cast<double>(preset.noiseFloor));
    xml->setAttribute("lookAheadMode", preset.lookAheadMode);
    xml->setAttribute("outputTrim", static_cast<double>(preset.outputTrim));
    
    return xml->writeTo(file);
}

bool VocalRiderAudioProcessor::deleteUserPreset(const juce::String& name)
{
    auto folder = getUserPresetsFolder();
    juce::String safeName = name.replaceCharacters("\\/:*?\"<>|", "_________");
    auto file = folder.getChildFile(safeName + ".xml");
    return file.deleteFile();
}

std::vector<VocalRiderAudioProcessor::Preset> VocalRiderAudioProcessor::loadUserPresets()
{
    std::vector<Preset> userPresets;
    auto folder = getUserPresetsFolder();
    
    for (const auto& file : folder.findChildFiles(juce::File::findFiles, false, "*.xml"))
    {
        auto xml = juce::XmlDocument::parse(file);
        if (xml != nullptr && xml->getTagName() == "UserPreset")
        {
            Preset p;
            p.category = "User";
            p.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
            p.targetLevel = static_cast<float>(xml->getDoubleAttribute("targetLevel", -18.0));
            p.speed = static_cast<float>(xml->getDoubleAttribute("speed", 50.0));
            p.range = static_cast<float>(xml->getDoubleAttribute("range", 6.0));
            p.attackMs = static_cast<float>(xml->getDoubleAttribute("attackMs", 50.0));
            p.releaseMs = static_cast<float>(xml->getDoubleAttribute("releaseMs", 200.0));
            p.holdMs = static_cast<float>(xml->getDoubleAttribute("holdMs", 50.0));
            p.naturalMode = xml->getBoolAttribute("naturalMode", true);
            p.smartSilence = xml->getBoolAttribute("smartSilence", false);
            p.useLufs = xml->getBoolAttribute("useLufs", false);
            p.breathReduction = static_cast<float>(xml->getDoubleAttribute("breathReduction", 0.0));
            p.transientPreservation = static_cast<float>(xml->getDoubleAttribute("transientPreservation", 0.0));
            p.noiseFloor = static_cast<float>(xml->getDoubleAttribute("noiseFloor", -100.0));
            p.lookAheadMode = xml->getIntAttribute("lookAheadMode", 0);
            p.outputTrim = static_cast<float>(xml->getDoubleAttribute("outputTrim", 0.0));
            userPresets.push_back(p);
        }
    }
    
    // Sort alphabetically by name
    std::sort(userPresets.begin(), userPresets.end(), [](const Preset& a, const Preset& b) {
        return a.name.compareIgnoreCase(b.name) < 0;
    });
    
    return userPresets;
}

void VocalRiderAudioProcessor::loadPresetFromData(const Preset& preset)
{
    // Main knobs
    if (auto* param = apvts.getParameter(targetLevelParamId))
        param->setValueNotifyingHost(param->convertTo0to1(preset.targetLevel));
    if (auto* param = apvts.getParameter(speedParamId))
        param->setValueNotifyingHost(param->convertTo0to1(preset.speed));
    float boost = (preset.boostRange >= 0.0f) ? preset.boostRange : preset.range;
    float cut   = (preset.cutRange >= 0.0f)  ? preset.cutRange   : preset.range;
    if (auto* param = apvts.getParameter(rangeParamId))
        param->setValueNotifyingHost(param->convertTo0to1(preset.range));
    if (auto* param = apvts.getParameter(boostRangeParamId))
        param->setValueNotifyingHost(param->convertTo0to1(boost));
    if (auto* param = apvts.getParameter(cutRangeParamId))
        param->setValueNotifyingHost(param->convertTo0to1(cut));
    setRangeLocked(preset.rangeLocked);
    
    // Timing parameters (update APVTS so processBlock sync doesn't overwrite)
    if (auto* param = apvts.getParameter(attackParamId))
        param->setValueNotifyingHost(param->convertTo0to1(preset.attackMs));
    if (auto* param = apvts.getParameter(releaseParamId))
        param->setValueNotifyingHost(param->convertTo0to1(preset.releaseMs));
    if (auto* param = apvts.getParameter(holdParamId))
        param->setValueNotifyingHost(param->convertTo0to1(preset.holdMs));
    attackMs.store(preset.attackMs);
    releaseMs.store(preset.releaseMs);
    holdMs.store(preset.holdMs);
    
    // Toggle settings (update APVTS parameters)
    if (auto* param = apvts.getParameter(naturalModeParamId))
        param->setValueNotifyingHost(preset.naturalMode ? 1.0f : 0.0f);
    if (auto* param = apvts.getParameter(smartSilenceParamId))
        param->setValueNotifyingHost(preset.smartSilence ? 1.0f : 0.0f);
    naturalModeEnabled.store(preset.naturalMode);
    smartSilenceEnabled.store(preset.smartSilence);
    useLufsMode.store(preset.useLufs);
    
    // Advanced knobs (update APVTS parameters)
    if (auto* param = apvts.getParameter(breathReductionParamId))
        param->setValueNotifyingHost(param->convertTo0to1(preset.breathReduction));
    if (auto* param = apvts.getParameter(transientPreservationParamId))
        param->setValueNotifyingHost(param->convertTo0to1(preset.transientPreservation));
    breathReductionDb.store(preset.breathReduction);
    transientPreservation.store(preset.transientPreservation / 100.0f);
    
    // Noise floor: clamp to parameter range
    float nfClamped = juce::jmax(-60.0f, preset.noiseFloor);
    if (auto* param = apvts.getParameter(noiseFloorParamId))
        param->setValueNotifyingHost(param->convertTo0to1(nfClamped));
    noiseFloorDb.store(nfClamped);
    
    // Extended settings
    setLookAheadMode(preset.lookAheadMode);
    setOutputTrim(preset.outputTrim);
}

//==============================================================================
#if JucePlugin_Build_Standalone
bool VocalRiderAudioProcessor::loadAudioFile(const juce::File& file)
{
    stopPlayback();
    
    auto* reader = formatManager.createReaderFor(file);
    
    if (reader != nullptr)
    {
        auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
        readerSource = std::move(newSource);
        loadedFileName = file.getFileName();
        fileLoaded.store(true);
        return true;
    }
    
    return false;
}

void VocalRiderAudioProcessor::startPlayback()
{
    if (fileLoaded.load())
    {
        if (transportSource.getCurrentPosition() >= transportSource.getLengthInSeconds() - 0.1)
            transportSource.setPosition(0.0);
        transportSource.start();
        transportPlaying.store(true);
    }
}

void VocalRiderAudioProcessor::stopPlayback()
{
    transportSource.stop();
    transportSource.setPosition(0.0);
    transportPlaying.store(false);
}

void VocalRiderAudioProcessor::togglePlayback()
{
    if (transportPlaying.load())
    {
        transportSource.stop();
        transportPlaying.store(false);
    }
    else
    {
        startPlayback();
    }
}

void VocalRiderAudioProcessor::rewindPlayback()
{
    transportSource.setPosition(0.0);
}

bool VocalRiderAudioProcessor::hasPlaybackFinished() const
{
    if (!fileLoaded.load()) return false;
    return transportSource.getCurrentPosition() >= transportSource.getLengthInSeconds() - 0.05;
}

double VocalRiderAudioProcessor::getPlaybackPosition() const
{
    return transportSource.getCurrentPosition();
}

double VocalRiderAudioProcessor::getPlaybackLength() const
{
    return transportSource.getLengthInSeconds();
}
#endif

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalRiderAudioProcessor();
}
