/*
  ==============================================================================

    PluginProcessor.h
    Created: 2026
    Author:  MBM Audio

    Advanced Vocal Rider with spectral focus, breath detection, LUFS,
    transient preservation, and automation write modes.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/RMSDetector.h"
#include "DSP/GainSmoother.h"
#include "DSP/PeakDetector.h"
#include "UI/WaveformDisplay.h"

//==============================================================================
class VocalRiderAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    // Automation modes
    // Off: Plugin calculates gain internally, no automation I/O
    // Read: Plugin reads automation from DAW and applies that gain (no internal calculation)
    // Touch/Latch/Write: Plugin calculates gain and writes to DAW automation
    enum class AutomationMode { Off = 0, Read, Touch, Latch, Write };
    
    //==============================================================================
    VocalRiderAudioProcessor();
    ~VocalRiderAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter access
    juce::AudioProcessorValueTreeState& getApvts() { return apvts; }

    // Metering access (thread-safe)
    float getInputLevelDb() const { return inputLevelDb.load(); }
    float getOutputLevelDb() const { return outputLevelDb.load(); }
    float getGainReductionDb() const { return currentGainDb.load(); }
    float getCurrentGainDb() const { return currentGainDb.load(); }
    float getInputLufs() const { return inputLufs.load(); }

    // Waveform display
    void setWaveformDisplay(WaveformDisplay* display) { waveformDisplay.store(display); }

    //==============================================================================
    // Auto-calibrate
    void startAutoCalibrate();
    void stopAutoCalibrate();
    bool isAutoCalibrating() const { return autoCalibrating.load(); }
    float getAutoCalibrateProgress() const;

    //==============================================================================
    // Advanced settings
    
    // Look-ahead: 0=Off, 1=10ms, 2=20ms, 3=30ms
    void setLookAheadMode(int mode);
    int getLookAheadMode() const { return lookAheadMode.load(); }
    int getLookAheadLatency() const;
    bool isLookAheadEnabled() const { return lookAheadMode.load() > 0; }
    
    // Natural Mode (phrase-based processing)
    void setNaturalModeEnabled(bool enabled);
    bool isNaturalModeEnabled() const { return naturalModeEnabled.load(); }
    bool isInPhrase() const { return inPhrase.load(); }  // For visual feedback (atomic for thread safety)
    
    // Smart Silence (silence reduction)
    void setSmartSilenceEnabled(bool enabled) { smartSilenceEnabled.store(enabled); }
    bool isSmartSilenceEnabled() const { return smartSilenceEnabled.load(); }
    
    // Learning mode (auto-analyze voice)
    void startLearning() { isLearning.store(true); learningSamples = 0; }
    void stopLearning() { isLearning.store(false); }
    bool getIsLearning() const { return isLearning.load(); }
    
    // LUFS mode (vs RMS)
    void setUseLufs(bool useLufs);
    bool getUseLufs() const { return useLufsMode.load(); }
    
    // Breath reduction
    void setBreathReduction(float reductionDb);
    float getBreathReduction() const { return breathReductionDb.load(); }
    
    // Transient preservation
    void setTransientPreservation(float amount);  // 0.0 = off, 1.0 = full
    float getTransientPreservation() const { return transientPreservation.load(); }
    
    // Output trim (makeup gain)
    void setOutputTrim(float trimDb);
    float getOutputTrim() const { return outputTrimDb.load(); }
    
    // Noise floor threshold
    void setNoiseFloor(float thresholdDb);
    float getNoiseFloor() const { return noiseFloorDb.load(); }
    
    // Sidechain control (reference track matching)
    void setSidechainEnabled(bool enabled) { sidechainEnabled.store(enabled); }
    bool isSidechainEnabled() const { return sidechainEnabled.load(); }
    void setSidechainAmount(float amount) { sidechainAmount.store(juce::jlimit(0.0f, 100.0f, amount)); }
    float getSidechainAmount() const { return sidechainAmount.load(); }
    bool hasSidechainInput() const;  // Returns true if sidechain bus is connected
    float getSidechainLevelDb() const { return sidechainLevelDb.load(); }
    
    // Vocal focus filter (frequency-weighted detection)
    void setVocalFocusEnabled(bool enabled) { vocalFocusEnabled.store(enabled); }
    bool isVocalFocusEnabled() const { return vocalFocusEnabled.load(); }
    
    // Range lock state (linked boost/cut)
    void setRangeLocked(bool locked) { rangeLocked.store(locked); }
    bool isRangeLocked() const { return rangeLocked.load(); }
    
    // Scroll speed for waveform (saved in state)
    void setScrollSpeed(float speed) { scrollSpeedSetting.store(speed); }
    float getScrollSpeed() const { return scrollSpeedSetting.load(); }
    
    // Currently selected preset index (saved in state)
    void setCurrentPresetIndex(int index) { currentPresetIndex.store(index); }
    int getCurrentPresetIndex() const { return currentPresetIndex.load(); }
    
    // Window size (saved in state: 0=Small, 1=Medium, 2=Large)
    void setWindowSizeIndex(int index) { windowSizeIndex.store(index); }
    int getWindowSizeIndex() const { return windowSizeIndex.load(); }
    
    // Automation write mode
    void setAutomationMode(AutomationMode mode);
    AutomationMode getAutomationMode() const { return automationMode.load(); }
    bool isAutomationWriting() const { 
        auto mode = automationMode.load();
        return mode == AutomationMode::Touch || mode == AutomationMode::Latch || mode == AutomationMode::Write;
    }
    bool isAutomationReading() const { return automationMode.load() == AutomationMode::Read; }
    float getGainOutputForAutomation() const { return gainOutputParam.load(); }

    void setAttackMs(float ms);
    void setReleaseMs(float ms);
    void setHoldMs(float ms);
    float getAttackMs() const { return attackMs.load(); }
    float getReleaseMs() const { return releaseMs.load(); }
    float getHoldMs() const { return holdMs.load(); }

    // Update attack/release from speed (for linking)
    // updateUI: if true, also updates APVTS parameters (for UI slider movement)
    void updateAttackReleaseFromSpeed(float speed, bool updateUI = false);

    //==============================================================================
    // Presets
    struct Preset
    {
        juce::String category;  // Category for grouping presets
        juce::String name;
        float targetLevel;
        float speed;
        float range;
        float attackMs;
        float releaseMs;
        float holdMs;
        bool naturalMode;
        bool smartSilence;
        bool useLufs;
        float breathReduction;      // 0-12 dB
        float transientPreservation; // 0-100%
        float noiseFloor;           // -100 = off, -60 to -20 dB
        // Extended fields for user presets (factory presets use defaults)
        int lookAheadMode = 0;      // 0=Off, 1=10ms, 2=20ms, 3=30ms
        float outputTrim = 0.0f;    // -12 to +12 dB
    };
    
    static const std::vector<Preset>& getFactoryPresets();
    static std::vector<juce::String> getPresetCategories();
    void loadPreset(int index);
    void loadPresetFromData(const Preset& preset);
    void resetToDefaults();
    
    // User presets (saved to disk)
    static juce::File getUserPresetsFolder();
    static std::vector<Preset> loadUserPresets();
    bool saveUserPreset(const juce::String& name);
    bool deleteUserPreset(const juce::String& name);
    Preset getCurrentSettingsAsPreset(const juce::String& name) const;

    //==============================================================================
    // Parameter IDs
    static const juce::String targetLevelParamId;
    static const juce::String speedParamId;
    static const juce::String rangeParamId;
    static const juce::String boostRangeParamId;
    static const juce::String cutRangeParamId;
    static const juce::String gainOutputParamId;
    
    // Advanced parameter IDs
    static const juce::String attackParamId;
    static const juce::String releaseParamId;
    static const juce::String holdParamId;
    static const juce::String breathReductionParamId;
    static const juce::String transientPreservationParamId;
    static const juce::String naturalModeParamId;
    static const juce::String smartSilenceParamId;
    static const juce::String outputTrimParamId;
    static const juce::String noiseFloorParamId;

    //==============================================================================
    // Audio file playback (for standalone testing)
    #if JucePlugin_Build_Standalone
    bool loadAudioFile(const juce::File& file);
    void startPlayback();
    void stopPlayback();
    void togglePlayback();
    void rewindPlayback();
    bool isPlaying() const { return transportPlaying.load(); }
    bool hasPlaybackFinished() const;
    double getPlaybackPosition() const;
    double getPlaybackLength() const;
    juce::String getLoadedFileName() const { return loadedFileName; }
    bool hasFileLoaded() const { return fileLoaded.load(); }
    #endif

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float softClip(float sample);
    float calculateLufs(const float* samples, int numSamples);
    bool detectBreath(float spectralFlatness, float zeroCrossRate);
    float calculateSpectralFlatness(const float* samples, int numSamples);
    float calculateZeroCrossingRate(const float* samples, int numSamples);
    void separateTransientSustain(float sample, float& transient, float& sustain);

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    // DSP components
    RMSDetector rmsDetector;
    GainSmoother gainSmoother;
    PeakDetector peakDetector;
    
    // Sidechain filters for spectral focus (200Hz - 4kHz vocal range)
    juce::dsp::StateVariableTPTFilter<float> sidechainHPF;  // High-pass at 200Hz
    juce::dsp::StateVariableTPTFilter<float> sidechainLPF;  // Low-pass at 4kHz
    
    // Transient detection filter
    juce::dsp::StateVariableTPTFilter<float> transientHPF;  // For transient detection
    float transientEnvelope = 0.0f;
    float sustainEnvelope = 0.0f;
    
    // Noise gate parameters
    static constexpr float gateThresholdDb = -45.0f;
    static constexpr float gateHysteresisDb = 3.0f;
    static constexpr float silenceGainDbReduction = -6.0f;  // When smart silence is ON
    bool gateOpen = false;
    float gateSmoothedLevel = -100.0f;
    
    // Smart Silence helper
    float getSilenceGainDb() const { return smartSilenceEnabled.load() ? silenceGainDbReduction : 0.0f; }
    
    // Soft knee parameters
    static constexpr float kneeWidthDb = 6.0f;
    
    //==============================================================================
    // Phrase-based processing (Natural Mode)
    std::atomic<bool> naturalModeEnabled { true };  // Default ON
    
    // Smart Silence
    std::atomic<bool> smartSilenceEnabled { false };
    
    // UI state to persist
    std::atomic<float> scrollSpeedSetting { 0.5f };  // Default to medium (50%)
    std::atomic<int> currentPresetIndex { 0 };  // 0 = no preset selected
    std::atomic<int> windowSizeIndex { 1 };  // 0=Small, 1=Medium, 2=Large (default Medium)
    
    // Learning mode
    std::atomic<bool> isLearning { false };
    int learningSamples = 0;
    float learningPeakSum = 0.0f;
    
    std::atomic<bool> inPhrase { false };
    std::atomic<bool> phraseStateNeedsReset { false };  // Flag for thread-safe reset
    float phraseAccumulator = 0.0f;
    int phraseSampleCount = 0;
    float currentPhraseGainDb = 0.0f;
    float lastPhraseGainDb = 0.0f;
    int silenceSampleCount = 0;
    int phraseMinSamples = 0;
    int silenceMinSamples = 0;
    float phraseGainSmoother = 0.0f;
    static constexpr float phraseGainSmoothCoeff = 0.9995f;
    int processorSilenceBlockCount = 0;  // Counter for silent processBlock calls
    
    // Intelligent phrase boundary detection
    float phraseLastLevelDb = -100.0f;  // For energy delta tracking
    int phraseStartSample = 0;           // Sample where current phrase started
    int phraseEndSample = 0;             // Sample where last phrase ended
    
    // Phrase lookahead buffer
    std::vector<float> phraseLookaheadBuffer;
    int phraseLookaheadWritePos = 0;
    int phraseLookaheadSamples = 0;
    
    //==============================================================================
    // LUFS measurement
    std::atomic<bool> useLufsMode { false };
    std::atomic<bool> lufsNeedsReset { false };  // Signal audio thread to reset LUFS state
    std::atomic<float> inputLufs { -100.0f };
    float lufsIntegrator = 0.0f;
    int lufsSampleCount = 0;
    juce::dsp::StateVariableTPTFilter<float> lufsPreFilter;  // K-weighting pre-filter
    juce::dsp::StateVariableTPTFilter<float> lufsHighShelf;  // K-weighting high shelf
    
    //==============================================================================
    // Breath detection
    std::atomic<float> breathReductionDb { 0.0f };  // 0 = no reduction
    bool isBreath = false;
    float breathEnvelope = 0.0f;
    
    //==============================================================================
    // Transient preservation
    std::atomic<float> transientPreservation { 0.0f };  // 0.0 = off, 1.0 = full
    
    //==============================================================================
    // Output trim (makeup gain)
    std::atomic<float> outputTrimDb { 0.0f };  // -12 to +12 dB
    
    //==============================================================================
    // Noise floor threshold (signals below this are ignored)
    std::atomic<float> noiseFloorDb { -100.0f };  // -100 = off, -60 to -20 dB active range
    
    //==============================================================================
    // Sidechain (reference track matching)
    std::atomic<bool> sidechainEnabled { false };
    std::atomic<float> sidechainAmount { 50.0f };  // 0-100% blend with sidechain level
    std::atomic<float> sidechainLevelDb { -100.0f };  // Current sidechain level for metering
    RMSDetector sidechainRmsDetector;
    
    //==============================================================================
    // Vocal focus filter (frequency-weighted detection)
    std::atomic<bool> vocalFocusEnabled { true };  // Default ON for better vocal detection
    juce::dsp::StateVariableTPTFilter<float> vocalFocusHighPass;  // Cut below ~200Hz
    juce::dsp::StateVariableTPTFilter<float> vocalFocusLowPass;   // Cut above ~4kHz
    juce::dsp::StateVariableTPTFilter<float> vocalFocusBandBoost; // Boost 1-3kHz presence
    
    //==============================================================================
    // Automation write
    std::atomic<AutomationMode> automationMode { AutomationMode::Off };  // Default to Off
    std::atomic<float> gainOutputParam { 0.0f };
    std::atomic<float>* gainOutputParamPtr = nullptr;
    std::atomic<bool> automationWriteActive { false };
    std::atomic<bool> automationGestureActive { false };
    std::atomic<bool> automationGestureNeedsEnd { false };  // Signal audio thread to end gesture

    // Cached parameters
    std::atomic<float>* targetLevelParam = nullptr;
    std::atomic<float>* speedParam = nullptr;
    std::atomic<float>* rangeParam = nullptr;
    std::atomic<float>* boostRangeParam = nullptr;
    std::atomic<float>* cutRangeParam = nullptr;
    
    // Advanced cached parameters
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* holdParam = nullptr;
    std::atomic<float>* breathReductionParam = nullptr;
    std::atomic<float>* transientPreservationParam = nullptr;
    std::atomic<float>* naturalModeParam = nullptr;
    std::atomic<float>* smartSilenceParam = nullptr;
    std::atomic<float>* outputTrimParam = nullptr;
    std::atomic<float>* noiseFloorParam = nullptr;
    
    // Smoothed parameters (to prevent clicks/pops on rapid changes)
    float smoothedTargetLevel = -18.0f;
    float smoothedBoostRange = 6.0f;
    float smoothedCutRange = 6.0f;
    float paramSmoothingCoeff = 0.85f;  // Computed from sample rate in prepareToPlay (~3ms)

    // Metering values (for UI)
    std::atomic<float> inputLevelDb { -100.0f };
    std::atomic<float> outputLevelDb { -100.0f };
    std::atomic<float> currentGainDb { 0.0f };

    // Speed / parameter tracking (avoid redundant coefficient updates)
    float lastSpeed = -1.0f;
    float lastAttackMs = -1.0f;
    float lastReleaseMs = -1.0f;
    float lastHoldMs = -1.0f;

    // Waveform display (non-owning pointer, set by editor) - atomic for thread safety.
    // IMPORTANT: Editor MUST call setWaveformDisplay(nullptr) in its destructor BEFORE
    // the WaveformDisplay is destroyed, to prevent use-after-free on the audio thread.
    std::atomic<WaveformDisplay*> waveformDisplay { nullptr };

    // Range lock state
    std::atomic<bool> rangeLocked { true };
    
    // Soft limiter
    static constexpr float ceilingDb = -0.3f;
    // Derived: 10^(ceilingDb/20) -- keep in sync automatically
    const float ceilingLinear = std::pow(10.0f, ceilingDb / 20.0f);  // ~0.966

    //==============================================================================
    // Look-ahead buffer
    std::atomic<int> lookAheadMode { 0 };
    std::atomic<int> lookAheadSamples { 0 };
    int maxLookAheadSamples = 0;
    juce::AudioBuffer<float> lookAheadDelayBuffer;
    std::vector<float> lookAheadGainBuffer;
    int lookAheadWritePos = 0;
    bool lookAheadBufferFilled = false;
    std::atomic<bool> lookAheadNeedsClear { false };  // Set by processBlockBypassed
    
    void updateLookAheadSamples();

    //==============================================================================
    // Advanced parameters
    std::atomic<float> attackMs { 50.0f };
    std::atomic<float> releaseMs { 200.0f };
    std::atomic<float> holdMs { 0.0f };

    //==============================================================================
    // Auto-calibrate
    std::atomic<bool> autoCalibrating { false };
    std::atomic<bool> autoCalibrateNeedsReset { false };  // Signal audio thread to reset
    float autoCalibrateAccumulator = 0.0f;
    int autoCalibrateSampleCount = 0;
    static constexpr float autoCalibrateSeconds = 2.5f;
    double currentSampleRate = 44100.0;
    

    //==============================================================================
    // Pre-allocated scratch buffers for processBlock (avoid heap allocs on audio thread)
    juce::AudioBuffer<float> scratchMonoBuffer;
    juce::AudioBuffer<float> scratchFilteredBuffer;
    juce::AudioBuffer<float> scratchSidechainBuffer;
    std::vector<float> scratchInputSamples;
    std::vector<float> scratchGainSamples;
    std::vector<float> scratchPeakAheadLevels;
    std::vector<float> scratchPrecomputedGains;
    std::vector<float> scratchOutputSamples;
    int preparedBlockSize = 0;  // Track allocated size for overflow guard

    //==============================================================================
    #if JucePlugin_Build_Standalone
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    std::atomic<bool> transportPlaying { false };
    std::atomic<bool> fileLoaded { false };
    juce::String loadedFileName;
    int currentBlockSize = 512;
    #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalRiderAudioProcessor)
};
