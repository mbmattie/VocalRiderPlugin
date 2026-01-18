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

//==============================================================================
// Factory Presets
const std::vector<VocalRiderAudioProcessor::Preset>& VocalRiderAudioProcessor::getFactoryPresets()
{
    static const std::vector<Preset> presets = {
        { "Gentle",      -18.0f, 30.0f,  6.0f, 100.0f, 400.0f,  50.0f },
        { "Broadcast",   -16.0f, 60.0f, 10.0f,  30.0f, 150.0f,  20.0f },
        { "Aggressive",  -14.0f, 80.0f, 12.0f,  10.0f,  50.0f,   0.0f },
        { "Dialogue",    -20.0f, 40.0f,  8.0f,  80.0f, 300.0f, 100.0f },
        { "Podcast",     -18.0f, 50.0f,  9.0f,  50.0f, 200.0f,  30.0f },
    };
    return presets;
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

    // Range: 0-12 dB, default 12 dB
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(rangeParamId, 1),
        "Range",
        juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f),
        12.0f,
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

    // Get parameters
    float targetLevel = targetLevelParam->load();
    float speed = speedParam->load();
    float range = rangeParam->load();
    bool useLookAhead = isLookAheadEnabled();

    // Update speed-dependent settings
    if (std::abs(speed - lastSpeed) > 0.5f)
    {
        float windowMs = juce::jmap(speed, 0.0f, 100.0f, 100.0f, 10.0f);
        rmsDetector.setWindowSize(windowMs);
        updateAttackReleaseFromSpeed(speed);
        lastSpeed = speed;
    }

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

    // === SPECTRAL FOCUS (200Hz - 4kHz vocal range) ===
    // Create filtered copy for detection (isolates vocal fundamentals)
    juce::AudioBuffer<float> filteredBuffer(1, numSamples);
    filteredBuffer.copyFrom(0, 0, monoBuffer, 0, 0, numSamples);
    
    juce::dsp::AudioBlock<float> filteredBlock(filteredBuffer);
    juce::dsp::ProcessContextReplacing<float> filterContext(filteredBlock);
    sidechainHPF.process(filterContext);  // High-pass at 200Hz
    sidechainLPF.process(filterContext);  // Low-pass at 4kHz
    
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
            // Detect phrases based on silence gaps and apply consistent gain per phrase
            
            float sampleValue = filteredRead[sample];
            float sampleLevelDb = juce::Decibels::gainToDecibels(std::abs(sampleValue), -100.0f);
            
            if (sampleLevelDb > gateThresholdDb)
            {
                // Audio present
                silenceSampleCount = 0;
                
                if (!inPhrase)
                {
                    // Start new phrase
                    inPhrase = true;
                    phraseAccumulator = 0.0f;
                    phraseSampleCount = 0;
                    lastPhraseGainDb = currentPhraseGainDb;
                }
                
                // Accumulate for phrase level calculation
                phraseAccumulator += sampleValue * sampleValue;
                phraseSampleCount++;
                
                // Calculate running phrase level and gain
                if (phraseSampleCount > phraseMinSamples / 4)  // After initial samples
                {
                    float phraseRms = std::sqrt(phraseAccumulator / static_cast<float>(phraseSampleCount));
                    float phraseLevelDb = juce::Decibels::gainToDecibels(phraseRms, -100.0f);
                    currentPhraseGainDb = juce::jlimit(-range, range, targetLevel - phraseLevelDb);
                }
            }
            else
            {
                // Silence
                silenceSampleCount++;
                
                if (inPhrase && silenceSampleCount > silenceMinSamples)
                {
                    // End of phrase detected
                    inPhrase = false;
                }
            }
            
            // Smoothly transition the phrase gain
            float targetPhraseGain = inPhrase ? currentPhraseGainDb : silenceGainDb;
            phraseGainSmoother = phraseGainSmoothCoeff * phraseGainSmoother + 
                                 (1.0f - phraseGainSmoothCoeff) * targetPhraseGain;
            
            targetGainDb = phraseGainSmoother;
            
            // Don't boost silence
            if (!gateOpen)
            {
                targetGainDb = juce::jmin(targetGainDb, silenceGainDb);
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
            
            // Noise gate: Don't boost silence
            if (!gateOpen)
            {
                targetGainDb = juce::jmin(targetGainDb, silenceGainDb);
            }
            
            if (effectiveLevelDb < gateThresholdDb - 10.0f)
            {
                targetGainDb = juce::jmin(targetGainDb, 0.0f);
            }
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
                float gain = lookAheadBufferFilled ? lookAheadGainBuffer[static_cast<size_t>(readPos)] : 1.0f;
                
                float processed;
                if (doTransientPreservation && transientPres > 0.0f)
                {
                    // Preserve transients: blend between full gain and unity
                    float transientPart, sustainPart;
                    separateTransientSustain(delayedSample, transientPart, sustainPart);
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
        // NORMAL PROCESSING (no look-ahead)
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
                    // Separate transient and sustain, only apply gain to sustain
                    float transientPart, sustainPart;
                    separateTransientSustain(inputSample, transientPart, sustainPart);
                    
                    // Preserve transients, ride the sustain
                    float blendedGain = 1.0f + (gainLinear - 1.0f) * (1.0f - transientPres);
                    processed = transientPart + sustainPart * gainLinear;
                    processed = inputSample * blendedGain;  // Blend based on preservation amount
                }
                else
                {
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
    
    if (autoMode != AutomationMode::Read && gainOutputParamPtr != nullptr)
    {
        // Write the computed gain to the automatable parameter
        if (auto* param = apvts.getParameter(gainOutputParamId))
        {
            float normalizedGain = param->convertTo0to1(finalGainDb);
            
            switch (autoMode)
            {
                case AutomationMode::Write:
                    // Continuous write
                    param->setValueNotifyingHost(normalizedGain);
                    break;
                    
                case AutomationMode::Latch:
                    // Write once touched, continue until stopped
                    if (automationWriteActive || std::abs(finalGainDb) > 0.1f)
                    {
                        param->setValueNotifyingHost(normalizedGain);
                        automationWriteActive = true;
                    }
                    break;
                    
                case AutomationMode::Touch:
                    // Only write when gain is changing significantly
                    if (std::abs(finalGainDb) > 0.1f)
                    {
                        param->setValueNotifyingHost(normalizedGain);
                    }
                    break;
                    
                case AutomationMode::Read:
                    // Read mode - do nothing (should not reach here due to outer if)
                    break;
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

    // Push to waveform display
    if (waveformDisplay != nullptr)
    {
        waveformDisplay->pushSamples(inputSamples.data(), outputSamples.data(), 
                                      gainSamples.data(), numSamples);
        waveformDisplay->setTargetLevel(targetLevel);
    }

    // Output metering
    float outputRms = buffer.getRMSLevel(0, 0, numSamples);
    if (totalNumInputChannels > 1)
        outputRms = (outputRms + buffer.getRMSLevel(1, 0, numSamples)) * 0.5f;
    outputLevelDb.store(juce::Decibels::gainToDecibels(outputRms, -100.0f));
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
    state.setProperty("automationMode", static_cast<int>(automationMode.load()), nullptr);
    
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
        if (state.hasProperty("automationMode"))
            setAutomationMode(static_cast<AutomationMode>(static_cast<int>(state.getProperty("automationMode"))));
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

void VocalRiderAudioProcessor::setAutomationMode(AutomationMode mode)
{
    automationMode.store(mode);
    automationWriteActive = (mode != AutomationMode::Read);
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

void VocalRiderAudioProcessor::updateAttackReleaseFromSpeed(float speed)
{
    // Map speed to attack/release
    float normalizedSpeed = juce::jlimit(0.0f, 100.0f, speed) / 100.0f;
    float speedFactor = std::pow(normalizedSpeed, 0.5f);
    
    float attack = juce::jmap(speedFactor, 500.0f, 5.0f);
    float release = juce::jmap(speedFactor, 1000.0f, 20.0f);
    
    attackMs.store(attack);
    releaseMs.store(release);
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
