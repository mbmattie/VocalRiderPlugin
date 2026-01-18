/*
  ==============================================================================

    WaveformDisplay.h
    Created: 2026
    Author:  MBM Audio

    FabFilter Pro-C inspired waveform display with:
    - Half-waveform with logarithmic scale
    - Ghost waveform showing original before gain
    - Separate boost/cut range handles
    - Clipping indicator
    - Fading gain curve

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <atomic>
#include <functional>

class WaveformDisplay : public juce::Component,
                        public juce::Timer
{
public:
    WaveformDisplay();
    ~WaveformDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    
    // Mouse interaction for handles
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;

    //==============================================================================
    // Audio data
    void pushSamples(const float* inputSamples, const float* outputSamples, 
                     const float* gainValues, int numSamples);
    void clear();

    //==============================================================================
    // Target and Range control (separate boost/cut)
    void setTargetLevel(float targetDb);
    float getTargetLevel() const { return targetLevelDb.load(); }
    
    void setBoostRange(float db);
    float getBoostRange() const { return boostRangeDb.load(); }
    
    void setCutRange(float db);
    float getCutRange() const { return cutRangeDb.load(); }
    
    // Combined range setter (for backwards compat)
    void setRange(float rangeDb);
    float getRange() const { return (boostRangeDb.load() + cutRangeDb.load()) / 2.0f; }

    // Callbacks for when user drags the handles
    std::function<void(float)> onTargetChanged;
    std::function<void(float)> onBoostRangeChanged;
    std::function<void(float)> onCutRangeChanged;
    std::function<void(float)> onRangeChanged;  // Legacy

    //==============================================================================
    // I/O Levels
    void setInputLevel(float db) { inputLevelDb.store(db); }
    void setOutputLevel(float db) { outputLevelDb.store(db); }

    //==============================================================================
    // Statistics
    float getAvgGainDb() const { return avgGainDb.load(); }
    float getMinGainDb() const { return minGainDb.load(); }
    float getMaxGainDb() const { return maxGainDb.load(); }
    void setGainStats(float avg, float minG, float maxG);
    void resetStats();

private:
    //==============================================================================
    struct SampleData
    {
        float inputPeak = 0.0f;     // Peak level (0-1)
        float outputPeak = 0.0f;    // Output peak for ghost comparison
        float gainDb = 0.0f;        // Gain adjustment in dB
    };

    // Coordinate conversions (logarithmic scale)
    float linearToLogY(float linear) const;  // 0-1 linear to Y with log scale
    float dbToY(float db) const;
    float yToDb(float y) const;
    float gainDbToY(float gainDb) const;
    
    // Hit testing for draggable handles
    enum class DragTarget { None, TargetHandle, BoostRangeHandle, CutRangeHandle };
    DragTarget hitTestHandle(const juce::Point<float>& pos) const;
    
    // Drawing helpers
    void drawBackground(juce::Graphics& g);
    void drawGridLines(juce::Graphics& g);
    void drawWaveform(juce::Graphics& g);
    void drawGhostWaveform(juce::Graphics& g);
    void drawGainCurve(juce::Graphics& g);
    void drawTargetAndRangeLines(juce::Graphics& g);
    void drawHandles(juce::Graphics& g);
    void drawIOMeters(juce::Graphics& g);
    void drawClippingIndicator(juce::Graphics& g);

    //==============================================================================
    // Display buffer (ring buffer)
    std::vector<SampleData> displayBuffer;
    std::atomic<int> writeIndex { 0 };
    int displayWidth = 500;
    
    // Areas
    juce::Rectangle<float> waveformArea;
    juce::Rectangle<float> handleArea;
    static constexpr int handleAreaWidth = 70;
    static constexpr int ioMeterWidth = 20;
    
    // Downsampling
    float samplesPerPixel = 128.0f;
    int sampleCounter = 0;
    float currentInputMax = 0.0f;
    float currentOutputMax = 0.0f;
    float currentGainSum = 0.0f;
    int gainSampleCount = 0;

    // Parameters (separate boost and cut)
    std::atomic<float> targetLevelDb { -18.0f };
    std::atomic<float> boostRangeDb { 12.0f };
    std::atomic<float> cutRangeDb { 12.0f };
    
    // I/O levels
    std::atomic<float> inputLevelDb { -100.0f };
    std::atomic<float> outputLevelDb { -100.0f };

    // Drag state
    DragTarget currentDragTarget = DragTarget::None;
    DragTarget hoverTarget = DragTarget::None;
    float dragStartValue = 0.0f;
    float dragStartY = 0.0f;

    // Statistics
    std::atomic<float> avgGainDb { 0.0f };
    std::atomic<float> minGainDb { 0.0f };
    std::atomic<float> maxGainDb { 0.0f };
    float gainAccumulator = 0.0f;
    float gainMinTrack = 100.0f;
    float gainMaxTrack = -100.0f;
    int statsSampleCount = 0;
    
    // Gain curve visibility (fades in when audio plays)
    float gainCurveOpacity = 0.0f;
    bool hasActiveAudio = false;
    int silenceSampleCount = 0;
    
    // Clipping detection
    bool isClipping = false;
    int clippingSampleCount = 0;

    // Visual settings
    static constexpr float handleHitDistance = 12.0f;

    juce::SpinLock bufferLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
