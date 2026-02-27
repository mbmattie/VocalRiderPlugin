/*
  ==============================================================================

    WaveformDisplay.h
    Created: 2026
    Author:  MBM Audio

    FabFilter Pro-C inspired waveform display with:
    - Image-based scrolling for ultra-smooth animation
    - Half-waveform with logarithmic scale
    - Separate boost/cut range handles
    - Clipping indicator

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
    
    // Range lock state for drag behavior
    void setRangeLocked(bool locked) { rangeLocked.store(locked); }
    
    // Scroll speed is now fixed at 8 seconds
    
    // Current gain for external access
    void setCurrentGain(float gainDb) { currentGainDb = gainDb; }
    float getRange() const { return (boostRangeDb.load() + cutRangeDb.load()) / 2.0f; }

    // Callbacks for when user drags the handles
    std::function<void(float)> onTargetChanged;
    std::function<void(float)> onBoostRangeChanged;
    std::function<void(float)> onCutRangeChanged;
    std::function<void(float)> onRangeChanged;  // Legacy

    //==============================================================================
    // Natural Mode phrase state (for visual feedback)
    void setNaturalModeEnabled(bool enabled) 
    { 
        if (naturalModeActive != enabled) staticElementsChanged = true;
        naturalModeActive = enabled; 
    }
    void setInPhrase(bool inPhrase) 
    { 
        if (phraseActive != inPhrase) staticElementsChanged = true;
        phraseActive = inPhrase; 
    }
    
    //==============================================================================
    // Noise Floor visual indicator
    void setNoiseFloorDb(float db) 
    { 
        float prev = noiseFloorDb.exchange(db);
        if (std::abs(prev - db) > 0.1f) staticElementsChanged = true;
    }
    void setNoiseFloorActive(bool active) 
    { 
        if (noiseFloorActive != active) staticElementsChanged = true;
        noiseFloorActive = active; 
    }
    
    //==============================================================================
    // Zoom button callbacks (for status bar tooltips)
    void setZoomButtonCallbacks(std::function<void()> onEnter, std::function<void()> onExit)
    {
        zoomButton.onMouseEnterCb = std::move(onEnter);
        zoomButton.onMouseExitCb = std::move(onExit);
    }
    bool isZoomEnabled() const { return zoomButton.isOn(); }
    
    //==============================================================================
    // Sidechain display
    void setSidechainLevel(float db) { sidechainLevelDb.store(db); }
    void setSidechainActive(bool active) { sidechainActive.store(active); }
    
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
        float inputRms = 0.0f;      // Input RMS level (linear 0-1)
        float inputPeak = 0.0f;     // Input peak level for faint outline
        float outputRms = 0.0f;     // Output RMS level (linear 0-1)
        float gainDb = 0.0f;        // Average gain adjustment in dB
    };

    // Coordinate conversions (logarithmic scale)
    float linearToLogY(float linear, float areaHeight) const;
    float dbToY(float db) const;
    float yToDb(float y) const;
    float gainDbToY(float gainDb) const;
    float gainDbToImageY(float gainDb, float imageHeight) const;
    
    // Hit testing for draggable handles
    enum class DragTarget { None, TargetHandle, BoostRangeHandle, CutRangeHandle };
    DragTarget hitTestHandle(const juce::Point<float>& pos) const;
    
    // Drawing helpers (static elements drawn each frame)
    void drawBackground(juce::Graphics& g);
    void drawGridLines(juce::Graphics& g);
    void drawTargetAndRangeLines(juce::Graphics& g);
    void drawHandles(juce::Graphics& g);
    void drawIOMeters(juce::Graphics& g);
    void drawClippingIndicator(juce::Graphics& g);
    void drawPhraseIndicator(juce::Graphics& g);
    
    // Image-based waveform rendering
    void initializeWaveformImage();
    void shiftWaveformImage(int pixels);
    void storeColumnData(int x, const SampleData& data);  // Store position data only
    void drawWaveformPaths(juce::Graphics& g);  // Draw closed path waveforms (fill + stroke)
    void drawGainCurvePath(juce::Graphics& g);  // Draw smooth gain curve as path

    //==============================================================================
    // Cached background image (noise texture + vignette - only regenerated on resize)
    juce::Image cachedBackgroundImage;
    bool backgroundNeedsRedraw = true;
    void renderCachedBackground();
    
    // Cached static overlay (grid lines, target/range lines - regenerated when params change)
    juce::Image cachedStaticOverlay;
    bool staticOverlayNeedsRedraw = true;
    void renderCachedStaticOverlay();
    
    // Offscreen waveform image (scrolls continuously)
    juce::Image waveformImage;
    int imageWidth = 0;
    int imageHeight = 0;
    
    // Gain curve data buffer (for smooth path drawing)
    std::vector<float> gainCurveBuffer;  // Ring buffer of gain values
    int gainCurveWriteIndex = 0;
    
    // Waveform position buffers (for closed path rendering)
    std::vector<float> inputTopBuffer;    // Y positions of input max (top edge)
    std::vector<float> inputBottomBuffer; // Y positions of input min (bottom edge)
    std::vector<float> outputTopBuffer;   // Y positions of output max (for boost/cut)
    std::vector<float> outputBottomBuffer;// Y positions of output min
    
    // Time-based scrolling
    double scrollAccumulator = 0.0;  // Accumulates fractional pixels
    double lastFrameTime = 0.0;
    
    // Pending sample data (queue for new columns)
    std::vector<SampleData> pendingData;
    int pendingReadIndex = 0;     // Read cursor into pendingData (avoids O(n) erase)
    juce::SpinLock pendingLock;
    
    // Pre-allocated frame data buffer (avoids heap allocation every frame)
    std::vector<SampleData> frameDataBuffer;
    
    // Areas
    juce::Rectangle<float> waveformArea;
    static constexpr int handleAreaWidth = 0;
    static constexpr int ioMeterWidth = 36;  // Narrower to give more space to waveform
    
    // Scroll timing - fixed pixels per second (independent of display size)
    static constexpr float pixelsPerSecondFixed = 150.0f;  // Constant scroll rate
    
    // Data capture - samples per buffer entry (smaller = more resolution)
    static constexpr int samplesPerEntry = 256;  // Block size for visual sampling
    int sampleCounter = 0;
    float currentInputSumSq = 0.0f;    // Sum of squared input samples (for RMS)
    float currentInputPeak = 0.0f;     // Peak input level (for faint outline)
    float currentOutputSumSq = 0.0f;   // Sum of squared output samples (for RMS)
    float currentGainSum = 0.0f;
    int gainSampleCount = 0;
    
    // For waveform interpolation
    SampleData lastDrawnData;
    bool hasLastDrawnData = false;
    
    // Raw sample data history (one per column, for rebuilding on zoom changes)
    std::vector<SampleData> columnRawData;
    void rebuildWaveformFromRawData();

    // Parameters (separate boost and cut)
    std::atomic<float> targetLevelDb { -18.0f };
    std::atomic<float> boostRangeDb { 12.0f };
    std::atomic<float> cutRangeDb { 12.0f };
    
    // I/O levels
    std::atomic<float> inputLevelDb { -100.0f };
    std::atomic<float> outputLevelDb { -100.0f };
    
    // Peak hold for meter display
    float inputPeakHoldDb = -100.0f;
    float outputPeakHoldDb = -100.0f;
    int inputPeakHoldCounter = 0;
    int outputPeakHoldCounter = 0;
    static constexpr int peakHoldFrames = 60;
    
    // Natural mode phrase indicator (atomic: set from UI, read from paint/timer)
    std::atomic<bool> naturalModeActive { false };
    std::atomic<bool> phraseActive { false };
    
    // Noise floor indicator
    std::atomic<float> noiseFloorDb { -100.0f };
    std::atomic<bool> noiseFloorActive { false };
    
    // Range lock (when true, dragging one handle moves both)
    std::atomic<bool> rangeLocked { true };
    
    // Sidechain display
    std::atomic<float> sidechainLevelDb { -100.0f };
    std::atomic<bool> sidechainActive { false };
    std::vector<float> sidechainTraceBuffer;  // Ring of dB values for trace line
    void drawSidechainTrace(juce::Graphics& g);
    void drawSidechainIndicator(juce::Graphics& g);
    
    // Adaptive display zoom — centers view around the target level
    float displayFloor = -64.0f;       // Current bottom of display (dB)
    float displayCeiling = 6.0f;       // Current top of display (dB)
    float displayFloorTarget = -64.0f; // Smoothing target
    float displayCeilingTarget = 6.0f;
    static constexpr float adaptiveMargin = 20.0f;  // dB above/below target to show
    static constexpr float adaptiveFloorMin = -64.0f;
    static constexpr float adaptiveCeilingMax = 6.0f;
    static constexpr float adaptiveSmoothCoeff = 0.08f;   // ~1.5s settle
    bool adaptiveZoomEnabled = false;  // Default OFF — user toggles on
    bool adaptiveZoomLocked = false;   // Once settled, stop moving
    bool adaptiveZoomingOut = false;   // Smooth zoom-out in progress
    int adaptiveZoomSettleFrames = 0;
    static constexpr int adaptiveZoomSettleThreshold = 50; // ~1.7s at 30fps
    float zoomAnchorTarget = -22.0f;   // Frozen target level captured when zoom is enabled
    float zoomAnchorBoost = 6.0f;      // Frozen boost range at zoom enable
    float zoomAnchorCut = 6.0f;        // Frozen cut range at zoom enable
    void updateAdaptiveZoom();
    void resetAdaptiveZoom();
    
    // Zoom toggle button (drawn inside the waveform area)
    class ZoomToggleButton : public juce::Component
    {
    public:
        ZoomToggleButton();
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseEnter(const juce::MouseEvent& event) override;
        void mouseExit(const juce::MouseEvent& event) override;
        bool isOn() const { return toggled; }
        void setToggled(bool on) { toggled = on; repaint(); }
        std::function<void(bool)> onToggled;
        std::function<void()> onMouseEnterCb;
        std::function<void()> onMouseExitCb;
    private:
        bool toggled = true;
        bool hovering = false;
        std::unique_ptr<juce::Drawable> cachedIcon;
        juce::Colour cachedIconColour;
        void ensureIconCached(juce::Colour col);
    };
    ZoomToggleButton zoomButton;
    
    // Flag: static elements (target/range/noise floor) changed, force repaint even without audio
    std::atomic<bool> staticElementsChanged { false };  // Also triggers staticOverlayNeedsRedraw
    
    // RMS average for output meter
    float outputRmsDb = -100.0f;
    float outputDisplayDb = -100.0f;
    float rmsAccumulator = 0.0f;
    int rmsSampleCount = 0;
    
    // Clip indicator hold
    bool clipIndicatorActive = false;
    int clipHoldCounter = 0;
    static constexpr int clipHoldFrames = 180;
    
    // Current gain for gain meter
    std::atomic<float> currentGainDb { 0.0f };
    float gainPeakHoldDb = 0.0f;
    int gainPeakHoldCounter = 0;
    float smoothedReadoutGainDb = 0.0f;  // Smoothed value for readable text display
    float smoothedMeterGainDb = 0.0f;    // Smoothed value for meter bar (faster response)
    float smoothedPeakMeterDb = -100.0f; // Smoothed value for peak/level meter

    // Drag state
    DragTarget currentDragTarget = DragTarget::None;
    DragTarget hoverTarget = DragTarget::None;
    float dragStartValue = 0.0f;
    float dragStartY = 0.0f;

    // Statistics
    std::atomic<float> avgGainDb { 0.0f };
    std::atomic<float> minGainDb { 0.0f };
    std::atomic<float> maxGainDb { 0.0f };
    std::atomic<float> gainAccumulator { 0.0f };
    std::atomic<float> gainMinTrack { 100.0f };
    std::atomic<float> gainMaxTrack { -100.0f };
    std::atomic<int> statsSampleCount { 0 };
    
    // Gain curve visibility
    float gainCurveOpacity = 0.0f;
    std::atomic<bool> hasActiveAudio { false };
    std::atomic<int> silenceSampleCount { 0 };
    
    // Tail scrolling: keep scrolling after audio stops so old data scrolls off screen
    int tailScrollFrames = 0;
    static constexpr int maxTailScrollFrames = 300;  // ~10 seconds at 30Hz
    
    // RMS average bar for peak meter (FabFilter-style slower bar)
    float rmsPeakBarDb = -100.0f;  // Slow-moving RMS average for display
    
    // Clipping detection (atomic: written from audio thread pushSamples, read from UI paint)
    std::atomic<bool> isClipping { false };
    std::atomic<int> clippingSampleCount { 0 };

    // Visual settings
    static constexpr float handleHitDistance = 18.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
