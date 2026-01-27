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

//==============================================================================
// Factory Presets
const std::vector<VocalRiderAudioProcessor::Preset>& VocalRiderAudioProcessor::getFactoryPresets()
{
    // Category, Name, Target, Speed, Range, Attack, Release, Hold
    static const std::vector<Preset> presets = {
        // Vocals - for singing and music production
        { "Vocals",    "Gentle Lead",      -18.0f, 30.0f,  6.0f, 100.0f, 400.0f,  50.0f },
        { "Vocals",    "Tight Lead",       -16.0f, 55.0f,  8.0f,  40.0f, 150.0f,  30.0f },
        { "Vocals",    "Dynamic Lead",     -17.0f, 45.0f, 10.0f,  60.0f, 250.0f,  40.0f },
        { "Vocals",    "Backing Vocals",   -22.0f, 35.0f,  5.0f,  80.0f, 350.0f,  60.0f },
        { "Vocals",    "Breathy Vocal",    -19.0f, 40.0f,  7.0f,  70.0f, 300.0f,  80.0f },
        { "Vocals",    "Aggressive Mix",   -14.0f, 75.0f, 12.0f,  15.0f,  60.0f,  10.0f },
        
        // Speaking/Dialogue - for podcasts, voiceovers, etc.
        { "Speaking",  "Podcast",          -18.0f, 50.0f,  9.0f,  50.0f, 200.0f,  30.0f },
        { "Speaking",  "Broadcast",        -16.0f, 60.0f, 10.0f,  30.0f, 150.0f,  20.0f },
        { "Speaking",  "Dialogue",         -20.0f, 40.0f,  8.0f,  80.0f, 300.0f, 100.0f },
        { "Speaking",  "Voiceover",        -17.0f, 55.0f,  8.0f,  45.0f, 180.0f,  40.0f },
        { "Speaking",  "Interview",        -19.0f, 45.0f,  7.0f,  60.0f, 250.0f,  50.0f },
        { "Speaking",  "Audiobook",        -21.0f, 35.0f,  6.0f,  90.0f, 400.0f,  80.0f },
        
        // Mattie's Favorites - curated collection
        { "Mattie's Favorites", "Smooth Rider",     -18.0f, 42.0f,  7.0f,  65.0f, 280.0f,  45.0f },
        { "Mattie's Favorites", "Crystal Clear",    -17.0f, 52.0f,  8.0f,  50.0f, 200.0f,  35.0f },
        { "Mattie's Favorites", "Magic Touch",      -19.0f, 38.0f,  6.0f,  75.0f, 320.0f,  55.0f },
        { "Mattie's Favorites", "Radio Ready",      -15.0f, 65.0f, 10.0f,  25.0f, 120.0f,  15.0f },
        { "Mattie's Favorites", "Natural Flow",     -20.0f, 32.0f,  5.0f,  85.0f, 380.0f,  70.0f },
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

    #if JucePlugin_Build_Standalone
    formatManager.registerBasicFormats();
    #endif
}

VocalRiderAudioProcessor::~VocalRiderAudioProcessor()
{
    #if JucePlugin_Build_Standalone
    transportSource.setSource(nullptr);
    #endif
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VocalRiderAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Target Level: -40 to 0 dB, default -18 dB
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(targetLevelParamId, 1),
        "Target Level",
        juce::NormalisableRange<float>(-40.0f, 0.0f, 0.1f),
        -18.0f,
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

    // Range: 0-12 dB, default 6 dB
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(rangeParamId, 1),
        "Range",
        juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f),
        6.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")
    ));

    // Gain Output: -12 to +12 dB, automatable (for writing automation)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(gainOutputParamId, 1),
        "Gain Output",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB").withAutomatable(true)
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
    return isLookAheadEnabled() ? (static_cast<double>(lookAheadSamples) / currentSampleRate) : 0.0;
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
    
    updateAttackReleaseFromSpeed(speed);
    
    lastSpeed = speed;

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
    inPhrase = false;
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

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    #if JucePlugin_Build_Standalone
    if (fileLoaded.load() && transportPlaying.load())
    {
        juce::AudioSourceChannelInfo info(&buffer, 0, numSamples);
        transportSource.getNextAudioBlock(info);
    }
    #endif

    // Get parameters with smoothing to prevent clicks/pops on rapid changes
    float targetLevelRaw = targetLevelParam->load();
    float speed = speedParam->load();
    float rangeRaw = rangeParam->load();
    bool useLookAhead = isLookAheadEnabled();
    
    // Smooth target and range parameters (prevents clicks on rapid UI changes)
    smoothedTargetLevel = smoothedTargetLevel * paramSmoothingCoeff + targetLevelRaw * (1.0f - paramSmoothingCoeff);
    smoothedRange = smoothedRange * paramSmoothingCoeff + rangeRaw * (1.0f - paramSmoothingCoeff);
    float targetLevel = smoothedTargetLevel;
    float range = smoothedRange;

    // Update speed-dependent settings
    if (std::abs(speed - lastSpeed) > 0.5f)
    {
        float windowMs = juce::jmap(speed, 0.0f, 100.0f, 100.0f, 10.0f);
        rmsDetector.setWindowSize(windowMs);
        updateAttackReleaseFromSpeed(speed);
        lastSpeed = speed;
    }

    // Sync advanced parameters from automation
    if (attackParam != nullptr) attackMs.store(attackParam->load());
    if (releaseParam != nullptr) releaseMs.store(releaseParam->load());
    if (holdParam != nullptr) holdMs.store(holdParam->load());
    if (breathReductionParam != nullptr) breathReductionDb.store(breathReductionParam->load());
    if (transientPreservationParam != nullptr) transientPreservation.store(transientPreservationParam->load() / 100.0f);
    if (naturalModeParam != nullptr) naturalModeEnabled.store(naturalModeParam->load() > 0.5f);
    if (smartSilenceParam != nullptr) smartSilenceEnabled.store(smartSilenceParam->load() > 0.5f);
    if (outputTrimParam != nullptr) outputTrimDb.store(outputTrimParam->load());

    // Apply advanced attack/release/hold
    gainSmoother.setAttackTime(attackMs.load());
    gainSmoother.setReleaseTime(releaseMs.load());
    gainSmoother.setHoldTime(holdMs.load());

    // Create mono sum for level detection
    juce::AudioBuffer<float> monoBuffer(1, numSamples);
    monoBuffer.clear();
    
    for (int channel = 0; channel < juce::jmin(totalNumInputChannels, 2); ++channel)
    {
        monoBuffer.addFrom(0, 0, buffer, channel, 0, numSamples,
                           1.0f / static_cast<float>(juce::jmin(totalNumInputChannels, 2)));
    }

    // Store original samples for waveform display
    std::vector<float> inputSamples(static_cast<size_t>(numSamples));
    std::vector<float> gainSamples(static_cast<size_t>(numSamples));
    const float* monoRead = monoBuffer.getReadPointer(0);
    std::copy(monoRead, monoRead + numSamples, inputSamples.begin());

    // Input metering (before HPF)
    float inputRms = monoBuffer.getRMSLevel(0, 0, numSamples);
    float inputDb = juce::Decibels::gainToDecibels(inputRms, -100.0f);
    inputLevelDb.store(inputDb);

    // === SIDECHAIN INPUT PROCESSING ===
    bool useSidechain = sidechainEnabled.load() && hasSidechainInput();
    float sidechainBlend = sidechainAmount.load() / 100.0f;
    float sidechainLevel = -100.0f;
    
    if (useSidechain)
    {
        // Get sidechain input (channels 2+ are sidechain)
        juce::AudioBuffer<float> sidechainBuffer(1, numSamples);
        sidechainBuffer.clear();
        
        int numSidechainChannels = getTotalNumInputChannels() - 2;
        for (int ch = 0; ch < numSidechainChannels && ch < 2; ++ch)
        {
            sidechainBuffer.addFrom(0, 0, buffer, ch + 2, 0, numSamples, 
                                    1.0f / static_cast<float>(juce::jmin(numSidechainChannels, 2)));
        }
        
        // Measure sidechain level
        float scRms = sidechainBuffer.getRMSLevel(0, 0, numSamples);
        sidechainLevel = juce::Decibels::gainToDecibels(scRms, -100.0f);
        sidechainLevelDb.store(sidechainLevel);
    }

    // === VOCAL FOCUS FILTER (frequency-weighted detection) ===
    // Create filtered copy for detection (isolates vocal fundamentals)
    juce::AudioBuffer<float> filteredBuffer(1, numSamples);
    filteredBuffer.copyFrom(0, 0, monoBuffer, 0, 0, numSamples);
    
    bool useVocalFocus = vocalFocusEnabled.load();
    
    if (useVocalFocus)
    {
        // Apply vocal focus filters to detection signal
        juce::dsp::AudioBlock<float> filteredBlock(filteredBuffer);
        juce::dsp::ProcessContextReplacing<float> filterContext(filteredBlock);
        vocalFocusHighPass.process(filterContext);  // Cut below 180Hz
        vocalFocusLowPass.process(filterContext);   // Cut above 5kHz
    }
    else
    {
        // Use original spectral focus filters (legacy behavior)
        juce::dsp::AudioBlock<float> filteredBlock(filteredBuffer);
        juce::dsp::ProcessContextReplacing<float> filterContext(filteredBlock);
        sidechainHPF.process(filterContext);  // High-pass at 200Hz
        sidechainLPF.process(filterContext);  // Low-pass at 4kHz
    }
    
    const float* filteredRead = filteredBuffer.getReadPointer(0);
    
    // === LUFS CALCULATION (if enabled) ===
    bool useLufs = useLufsMode.load();
    float measuredLufs = -100.0f;
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
            suggestedTarget = juce::jlimit(-40.0f, -6.0f, suggestedTarget);
            
            if (auto* param = apvts.getParameter(targetLevelParamId))
            {
                param->setValueNotifyingHost(param->convertTo0to1(suggestedTarget));
            }
            
            autoCalibrating.store(false);
        }
    }

    // === PREDICTIVE LOOK-AHEAD: Pre-scan for peaks ===
    std::vector<float> peakAheadLevels(static_cast<size_t>(numSamples), -100.0f);
    if (useLookAhead)
    {
        int scanWindow = juce::jmin(lookAheadSamples, numSamples);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float peakAhead = 0.0f;
            int scanEnd = juce::jmin(sample + scanWindow, numSamples);
            for (int j = sample; j < scanEnd; ++j)
            {
                peakAhead = juce::jmax(peakAhead, std::abs(filteredRead[j]));
            }
            peakAheadLevels[static_cast<size_t>(sample)] = 
                juce::Decibels::gainToDecibels(peakAhead, -100.0f);
        }
    }

    // Pre-compute gain values
    std::vector<float> precomputedGains(static_cast<size_t>(numSamples));
    
    // Gate smoothing coefficient (fast attack, slower release)
    const float gateSmoothAttack = 0.99f;
    const float gateSmoothRelease = 0.9995f;
    
    // === SIDECHAIN TARGET ADJUSTMENT ===
    // When sidechain is enabled, blend the fixed target with matching the sidechain level
    float effectiveTarget = targetLevel;
    if (useSidechain && sidechainLevel > -60.0f)
    {
        // Sidechain level becomes the dynamic target
        // Blend between fixed target (0%) and sidechain-following (100%)
        effectiveTarget = targetLevel * (1.0f - sidechainBlend) + sidechainLevel * sidechainBlend;
    }
    targetLevel = effectiveTarget;
    
    // Check if Natural (phrase-based) mode is enabled
    bool useNaturalMode = naturalModeEnabled.load();
    
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
        
        if (useNaturalMode)
        {
            // === PHRASE-BASED (NATURAL) MODE ===
            // Detect phrases using intelligent boundary detection and apply consistent gain per phrase
            // Now integrates with advanced features: breath detection, transient preservation
            
            float sampleValue = filteredRead[sample];
            float sampleLevelDb = juce::Decibels::gainToDecibels(std::abs(sampleValue), -100.0f);
            
            // === INTELLIGENT PHRASE BOUNDARY DETECTION ===
            // Use multiple signals: level, spectral characteristics, energy changes
            bool audioPresent = sampleLevelDb > gateThresholdDb;
            
            // Track energy changes for phrase detection
            float energyDelta = std::abs(sampleLevelDb - phraseLastLevelDb);
            phraseLastLevelDb = sampleLevelDb;
            
            // Large energy jump can indicate new phrase even without full silence
            bool energyJump = (energyDelta > 12.0f && sampleLevelDb > gateThresholdDb + 6.0f);
            
            if (audioPresent)
            {
                // Audio present
                silenceSampleCount = 0;
                
                // Start new phrase on: first audio after silence, OR significant energy jump
                if (!inPhrase || (energyJump && phraseSampleCount > phraseMinSamples))
                {
                    if (!inPhrase || energyJump)
                    {
                        // Start new phrase
                        bool wasInPhrase = inPhrase;
                        inPhrase = true;
                        phraseStartSample = sample;
                        
                        if (!wasInPhrase)
                        {
                            phraseAccumulator = 0.0f;
                            phraseSampleCount = 0;
                            lastPhraseGainDb = currentPhraseGainDb;
                        }
                        else if (energyJump)
                        {
                            // Soft reset for energy-based phrase change
                            phraseAccumulator *= 0.3f;  // Keep some history
                            phraseSampleCount = static_cast<int>(phraseSampleCount * 0.3f);
                        }
                    }
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
                    
                    currentPhraseGainDb = juce::jlimit(-range, range, gainNeeded);
                }
            }
            else
            {
                // Silence detected
                silenceSampleCount++;
                
                // Use hold time concept: don't end phrase immediately
                double sr = getSampleRate();
                int holdSamplesForPhrase = static_cast<int>(holdMs.load() * sr / 1000.0);
                holdSamplesForPhrase = juce::jmax(holdSamplesForPhrase, silenceMinSamples);
                
                if (inPhrase && silenceSampleCount > holdSamplesForPhrase)
                {
                    // End of phrase detected
                    inPhrase = false;
                    phraseEndSample = sample;
                }
            }
            
            // Target gain: phrase gain when in phrase, silence gain otherwise
            float targetPhraseGain = inPhrase ? currentPhraseGainDb : getSilenceGainDb();
            
            // Use attack/release coefficients instead of fixed smoothing
            // Attack when gain is increasing (getting louder), release when decreasing
            float gainDelta = targetPhraseGain - phraseGainSmoother;
            double sr = getSampleRate();
            float phraseSmooth;
            if (gainDelta > 0)
            {
                // Gain increasing (boost) - use attack time
                phraseSmooth = std::exp(-1.0f / (attackMs.load() * static_cast<float>(sr) / 1000.0f));
            }
            else
            {
                // Gain decreasing (cut) - use release time  
                phraseSmooth = std::exp(-1.0f / (releaseMs.load() * static_cast<float>(sr) / 1000.0f));
            }
            
            phraseGainSmoother = phraseSmooth * phraseGainSmoother + (1.0f - phraseSmooth) * targetPhraseGain;
            
            targetGainDb = phraseGainSmoother;
            
            // Don't boost silence (apply smart silence reduction if enabled)
            if (!gateOpen)
            {
                targetGainDb = juce::jmin(targetGainDb, getSilenceGainDb());
            }
        }
        else
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
            
            // Soft knee
            if (std::abs(gainNeeded) < kneeWidthDb)
            {
                float ratio = gainNeeded / kneeWidthDb;
                gainNeeded = gainNeeded * (0.5f + 0.5f * ratio * ratio * (gainNeeded > 0 ? 1.0f : -1.0f));
            }
            
            targetGainDb = juce::jlimit(-range, range, gainNeeded);
            
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
    if (useLookAhead && lookAheadSamples > 0)
    {
        // LOOK-AHEAD PROCESSING
        // We delay the audio by lookAheadSamples while applying gains computed from "future" audio
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            int bufferSize = lookAheadDelayBuffer.getNumSamples();
            int readPos = (lookAheadWritePos + 1) % bufferSize;
            
            // Store current gain for the delayed audio
            lookAheadGainBuffer[static_cast<size_t>(lookAheadWritePos)] = precomputedGains[static_cast<size_t>(sample)];
            
            for (int channel = 0; channel < juce::jmin(totalNumInputChannels, 2); ++channel)
            {
                float* channelData = buffer.getWritePointer(channel);
                float* delayData = lookAheadDelayBuffer.getWritePointer(channel);
                
                // Write current sample to delay buffer
                delayData[lookAheadWritePos] = channelData[sample];
                
                // Read delayed sample and apply the pre-computed gain
                float delayedSample = delayData[readPos];
                // Use precomputed gain immediately if buffer not filled yet (don't wait)
                float gain = lookAheadBufferFilled ? lookAheadGainBuffer[static_cast<size_t>(readPos)] 
                                                   : precomputedGains[static_cast<size_t>(sample)];
                
                float processed;
                if (doTransientPreservation && transientPres > 0.0f)
                {
                    // Preserve transients: blend between full gain and unity
                    float blendedGain = 1.0f + (gain - 1.0f) * (1.0f - transientPres);
                    processed = delayedSample * blendedGain;
                }
                else
                {
                    processed = delayedSample * gain;
                }
                
                channelData[sample] = softClip(processed);
            }
            
            lookAheadWritePos = (lookAheadWritePos + 1) % bufferSize;
            
            // Mark buffer as filled once we've written enough samples
            if (!lookAheadBufferFilled && lookAheadWritePos >= lookAheadSamples)
                lookAheadBufferFilled = true;
        }
    }
    else
    {
        // NORMAL PROCESSING (no look-ahead) - DIRECT GAIN APPLICATION
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float gainLinear = precomputedGains[static_cast<size_t>(sample)];
            
            for (int channel = 0; channel < juce::jmin(totalNumInputChannels, 2); ++channel)
            {
                float* channelData = buffer.getWritePointer(channel);
                float inputSample = channelData[sample];
                float processed;
                
                if (doTransientPreservation && transientPres > 0.0f)
                {
                    // Blend between unity and full gain based on transient preservation
                    float blendedGain = 1.0f + (gainLinear - 1.0f) * (1.0f - transientPres);
                    processed = inputSample * blendedGain;
                }
                else
                {
                    // Standard gain application
                    processed = inputSample * gainLinear;
                }
                
                channelData[sample] = softClip(processed);
            }
        }
    }

    float finalGainDb = gainSmoother.getCurrentGainDb();
    currentGainDb.store(finalGainDb);
    
    // === AUTOMATION OUTPUT ===
    // Update the gain output parameter for DAW automation
    gainOutputParam.store(finalGainDb);
    
    // Only write automation in Touch/Latch/Write modes (not Off or Read)
    bool shouldWriteAutomation = (autoMode == AutomationMode::Touch || 
                                   autoMode == AutomationMode::Latch || 
                                   autoMode == AutomationMode::Write);
    
    if (shouldWriteAutomation)
    {
        // Write the computed gain to the automatable parameter
        if (auto* param = apvts.getParameter(gainOutputParamId))
        {
            float normalizedGain = param->convertTo0to1(finalGainDb);
            bool shouldWrite = false;
            
            switch (autoMode)
            {
                case AutomationMode::Write:
                    // Continuous write - ALWAYS output, no threshold
                    shouldWrite = true;
                    break;
                    
                case AutomationMode::Latch:
                    // Write once any gain change occurs, then continue
                    if (automationWriteActive || std::abs(finalGainDb) > 0.05f)
                    {
                        shouldWrite = true;
                        automationWriteActive = true;
                    }
                    break;
                    
                case AutomationMode::Touch:
                    // Write when there's any meaningful gain change
                    shouldWrite = (std::abs(finalGainDb) > 0.05f);
                    break;
                    
                case AutomationMode::Off:
                case AutomationMode::Read:
                    break;
            }
            
            if (shouldWrite)
            {
                // Signal gesture start if not already writing
                if (!automationGestureActive)
                {
                    param->beginChangeGesture();
                    automationGestureActive = true;
                }
                param->setValueNotifyingHost(normalizedGain);
            }
            else if (automationGestureActive && autoMode == AutomationMode::Touch)
            {
                // End gesture when Touch mode stops writing
                param->endChangeGesture();
                automationGestureActive = false;
            }
        }
    }
    else if (automationGestureActive)
    {
        // End any active gesture when switching away from write modes
        if (auto* param = apvts.getParameter(gainOutputParamId))
        {
            param->endChangeGesture();
            automationGestureActive = false;
        }
    }

    // Apply output trim (makeup/reduction gain, -12 to +12 dB)
    float trimGain = juce::Decibels::decibelsToGain(outputTrimDb.load());
    if (std::abs(trimGain - 1.0f) > 0.001f)  // Only if trim is not unity
    {
        for (int sample = 0; sample < numSamples; ++sample)
        {
            for (int channel = 0; channel < juce::jmin(totalNumInputChannels, 2); ++channel)
            {
                float* channelData = buffer.getWritePointer(channel);
                channelData[sample] = softClip(channelData[sample] * trimGain);
            }
        }
    }

    // Output samples for waveform
    std::vector<float> outputSamples(static_cast<size_t>(numSamples));
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float sum = 0.0f;
        for (int channel = 0; channel < juce::jmin(totalNumInputChannels, 2); ++channel)
        {
            sum += buffer.getSample(channel, sample);
        }
        outputSamples[static_cast<size_t>(sample)] = sum / static_cast<float>(juce::jmin(totalNumInputChannels, 2));
    }

    // Push to waveform display (thread-safe access)
    if (auto* display = waveformDisplay.load())
    {
        display->pushSamples(inputSamples.data(), outputSamples.data(), 
                             gainSamples.data(), numSamples);
        // Use raw parameter value for immediate visual response
        display->setTargetLevel(targetLevelRaw);
    }

    // Output metering - use PEAK for display (matches DAW meters)
    float outputPeak = buffer.getMagnitude(0, 0, numSamples);
    if (totalNumInputChannels > 1)
        outputPeak = juce::jmax(outputPeak, buffer.getMagnitude(1, 0, numSamples));
    outputLevelDb.store(juce::Decibels::gainToDecibels(outputPeak, -100.0f));
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
    
    // UI state persistence
    state.setProperty("smartSilence", smartSilenceEnabled.load(), nullptr);
    state.setProperty("scrollSpeed", scrollSpeedSetting.load(), nullptr);
    state.setProperty("presetIndex", currentPresetIndex.load(), nullptr);
    state.setProperty("windowSizeIndex", windowSizeIndex.load(), nullptr);
    
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VocalRiderAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto state = juce::ValueTree::fromXml(*xmlState);
        apvts.replaceState(state);
        
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
            setAutomationMode(static_cast<AutomationMode>(static_cast<int>(state.getProperty("automationMode"))));
        
        // UI state restoration
        if (state.hasProperty("smartSilence"))
            setSmartSilenceEnabled(static_cast<bool>(state.getProperty("smartSilence")));
        if (state.hasProperty("scrollSpeed"))
            setScrollSpeed(static_cast<float>(state.getProperty("scrollSpeed")));
        if (state.hasProperty("presetIndex"))
            setCurrentPresetIndex(static_cast<int>(state.getProperty("presetIndex")));
        if (state.hasProperty("windowSizeIndex"))
            setWindowSizeIndex(static_cast<int>(state.getProperty("windowSizeIndex")));
    }
}

//==============================================================================
void VocalRiderAudioProcessor::startAutoCalibrate()
{
    autoCalibrateAccumulator = 0.0f;
    autoCalibrateSampleCount = 0;
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
    return lookAheadSamples;
}

void VocalRiderAudioProcessor::updateLookAheadSamples()
{
    int mode = lookAheadMode.load();
    switch (mode)
    {
        case 0: lookAheadSamples = 0; break;                                          // Off
        case 1: lookAheadSamples = static_cast<int>(0.010 * currentSampleRate); break; // 10ms
        case 2: lookAheadSamples = static_cast<int>(0.020 * currentSampleRate); break; // 20ms
        case 3: lookAheadSamples = static_cast<int>(0.030 * currentSampleRate); break; // 30ms
        default: lookAheadSamples = 0; break;
    }
}

void VocalRiderAudioProcessor::setNaturalModeEnabled(bool enabled)
{
    naturalModeEnabled.store(enabled);
    
    // Reset phrase state when toggling
    inPhrase = false;
    phraseAccumulator = 0.0f;
    phraseSampleCount = 0;
    currentPhraseGainDb = 0.0f;
    silenceSampleCount = 0;
}

void VocalRiderAudioProcessor::setUseLufs(bool useLufs)
{
    useLufsMode.store(useLufs);
    lufsIntegrator = 0.0f;
    lufsSampleCount = 0;
}

void VocalRiderAudioProcessor::setBreathReduction(float reductionDb)
{
    breathReductionDb.store(juce::jlimit(0.0f, 12.0f, reductionDb));
}

void VocalRiderAudioProcessor::setTransientPreservation(float amount)
{
    transientPreservation.store(juce::jlimit(0.0f, 1.0f, amount));
}

bool VocalRiderAudioProcessor::hasSidechainInput() const
{
    return getTotalNumInputChannels() > 2;  // More than stereo means sidechain is connected
}

void VocalRiderAudioProcessor::setAutomationMode(AutomationMode mode)
{
    AutomationMode oldMode = automationMode.load();
    automationMode.store(mode);
    
    // End any active gesture when switching modes
    if (automationGestureActive && mode != oldMode)
    {
        if (auto* param = apvts.getParameter(gainOutputParamId))
        {
            param->endChangeGesture();
        }
        automationGestureActive = false;
    }
    
    // Reset write active flag when switching modes
    automationWriteActive = false;
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
    
    for (int i = 0; i < numSamples; ++i)
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
    {
        const auto& preset = presets[static_cast<size_t>(index)];
        
        if (auto* param = apvts.getParameter(targetLevelParamId))
            param->setValueNotifyingHost(param->convertTo0to1(preset.targetLevel));
        if (auto* param = apvts.getParameter(speedParamId))
            param->setValueNotifyingHost(param->convertTo0to1(preset.speed));
        if (auto* param = apvts.getParameter(rangeParamId))
            param->setValueNotifyingHost(param->convertTo0to1(preset.range));
        
        attackMs.store(preset.attackMs);
        releaseMs.store(preset.releaseMs);
        holdMs.store(preset.holdMs);
    }
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
    
    // Reset advanced parameters
    attackMs.store(10.0f);
    releaseMs.store(100.0f);
    holdMs.store(50.0f);
    naturalModeEnabled.store(false);
    smartSilenceEnabled.store(false);
    breathReductionDb.store(0.0f);
    transientPreservation.store(0.0f);
    outputTrimDb.store(0.0f);
    lookAheadMode.store(0);
    useLufsMode.store(false);
    automationMode.store(AutomationMode::Off);  // Default to Off (internal gain calculation, no automation I/O)
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
