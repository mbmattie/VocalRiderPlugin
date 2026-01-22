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
    // Automation write modes
    enum class AutomationMode { Read = 0, Touch, Latch, Write };
    
    //==============================================================================
    VocalRiderAudioProcessor();
    ~VocalRiderAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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
    void setOutputTrim(float trimDb) { outputTrimDb.store(juce::jlimit(-12.0f, 12.0f, trimDb)); }
    float getOutputTrim() const { return outputTrimDb.load(); }
    
    // Scroll speed for waveform (saved in state)
    void setScrollSpeed(float speed) { scrollSpeedSetting.store(speed); }
    float getScrollSpeed() const { return scrollSpeedSetting.load(); }
    
    // Currently selected preset index (saved in state)
    void setCurrentPresetIndex(int index) { currentPresetIndex.store(index); }
    int getCurrentPresetIndex() const { return currentPresetIndex.load(); }
    
    // Automation write mode
    void setAutomationMode(AutomationMode mode);
    AutomationMode getAutomationMode() const { return automationMode.load(); }
    bool isAutomationWriting() const { return automationMode.load() != AutomationMode::Read; }
    float getGainOutputForAutomation() const { return gainOutputParam.load(); }

    void setAttackMs(float ms);
    void setReleaseMs(float ms);
    void setHoldMs(float ms);
    float getAttackMs() const { return attackMs.load(); }
    float getReleaseMs() const { return releaseMs.load(); }
    float getHoldMs() const { return holdMs.load(); }

    // Update attack/release from speed (for linking)
    void updateAttackReleaseFromSpeed(float speed);

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
    };
    
    static const std::vector<Preset>& getFactoryPresets();
    static std::vector<juce::String> getPresetCategories();
    void loadPreset(int index);
    void resetToDefaults();

    //==============================================================================
    // Parameter IDs
    static const juce::String targetLevelParamId;
    static const juce::String speedParamId;
    static const juce::String rangeParamId;
    static const juce::String gainOutputParamId;

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
    std::atomic<bool> naturalModeEnabled { false };
    
    // Smart Silence
    std::atomic<bool> smartSilenceEnabled { false };
    
    // UI state to persist
    std::atomic<float> scrollSpeedSetting { 0.33f };  // Default to medium (15s)
    std::atomic<int> currentPresetIndex { 0 };  // 0 = no preset selected
    
    // Learning mode
    std::atomic<bool> isLearning { false };
    int learningSamples = 0;
    float learningPeakSum = 0.0f;
    
    bool inPhrase = false;
    float phraseAccumulator = 0.0f;
    int phraseSampleCount = 0;
    float currentPhraseGainDb = 0.0f;
    float lastPhraseGainDb = 0.0f;
    int silenceSampleCount = 0;
    int phraseMinSamples = 0;
    int silenceMinSamples = 0;
    float phraseGainSmoother = 0.0f;
    static constexpr float phraseGainSmoothCoeff = 0.9995f;
    
    // Phrase lookahead buffer
    std::vector<float> phraseLookaheadBuffer;
    int phraseLookaheadWritePos = 0;
    int phraseLookaheadSamples = 0;
    
    //==============================================================================
    // LUFS measurement
    std::atomic<bool> useLufsMode { false };
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
    // Automation write
    std::atomic<AutomationMode> automationMode { AutomationMode::Read };
    std::atomic<float> gainOutputParam { 0.0f };
    std::atomic<float>* gainOutputParamPtr = nullptr;
    bool automationWriteActive = false;

    // Cached parameters
    std::atomic<float>* targetLevelParam = nullptr;
    std::atomic<float>* speedParam = nullptr;
    std::atomic<float>* rangeParam = nullptr;
    
    // Smoothed parameters (to prevent clicks/pops on rapid changes)
    float smoothedTargetLevel = -18.0f;
    float smoothedRange = 12.0f;
    static constexpr float paramSmoothingCoeff = 0.85f;  // ~3ms at 44.1kHz - very fast response

    // Metering values (for UI)
    std::atomic<float> inputLevelDb { -100.0f };
    std::atomic<float> outputLevelDb { -100.0f };
    std::atomic<float> currentGainDb { 0.0f };

    // Speed tracking
    float lastSpeed = -1.0f;

    // Waveform display (non-owning pointer, set by editor) - atomic for thread safety
    std::atomic<WaveformDisplay*> waveformDisplay { nullptr };

    // Soft limiter
    static constexpr float ceilingDb = -0.3f;
    float ceilingLinear = 0.966f;

    //==============================================================================
    // Look-ahead buffer
    std::atomic<int> lookAheadMode { 0 };
    int lookAheadSamples = 0;
    int maxLookAheadSamples = 0;
    juce::AudioBuffer<float> lookAheadDelayBuffer;
    std::vector<float> lookAheadGainBuffer;
    int lookAheadWritePos = 0;
    bool lookAheadBufferFilled = false;
    
    void updateLookAheadSamples();

    //==============================================================================
    // Advanced parameters
    std::atomic<float> attackMs { 50.0f };
    std::atomic<float> releaseMs { 200.0f };
    std::atomic<float> holdMs { 0.0f };

    //==============================================================================
    // Auto-calibrate
    std::atomic<bool> autoCalibrating { false };
    float autoCalibrateAccumulator = 0.0f;
    int autoCalibrateSampleCount = 0;
    static constexpr float autoCalibrateSeconds = 2.5f;
    double currentSampleRate = 44100.0;

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
