#pragma once

#include "PluginProcessor.h"

//==============================================================================
//  The interactive frequency display. This component IS the EQ UI:
//
//   - Log-frequency horizontal axis (20 Hz .. 20 kHz)
//   - dB vertical axis (-30 .. +30 for the curve; spectrum mapped -90 .. 0)
//   - Live FFT spectrum of the plugin output behind everything
//   - Cascaded EQ response curve (static passive program EQ + active dynamic bands)
//   - Draggable handles for each band:
//       * passive program EQ handles snap horizontally to the hardware's stepped freqs
//       * Mouse wheel adjusts Q where it applies (high boost bandwidth, dyn band Q)
//       * Dynamic-band handles get a pulsing halo whose alpha tracks their
//         real-time activity (0..1) coming from the audio thread
//
//  All param writes go through APVTS so host automation and undo work.
//==============================================================================
class SpectrumView : public juce::Component,
                     private juce::Timer
{
public:
    explicit SpectrumView (ToneSuiteAudioProcessor&);
    ~SpectrumView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove        (const juce::MouseEvent&) override;
    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDrag        (const juce::MouseEvent&) override;
    void mouseUp          (const juce::MouseEvent&) override;
    void mouseWheelMove   (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseExit        (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    //==============================================================================
    struct Handle
    {
        juce::String name;
        juce::Colour color;
        std::function<float()> getFreq;
        std::function<float()> getGainDb;
        std::function<float()> getQ;            // 0 if not applicable
        std::function<float()> getActivity;     // 0..1, mostly for dyn bands
        std::function<bool()>  isEnabled;
        std::function<void(float)> setFreq;     // hz; snaps internally if stepped
        std::function<void(float)> setGainDb;   // clamped/mapped internally
        std::function<void(float)> setQ;        // no-op if no Q
        bool isDynamic = false;
        bool isCut     = false;                 // visual differentiation
    };

    //==============================================================================
    void buildHandles();
    void refreshCoefficients();

    void drawGrid     (juce::Graphics&, juce::Rectangle<int> plot);
    void drawSpectrum (juce::Graphics&, juce::Rectangle<int> plot);
    void drawEQCurve  (juce::Graphics&, juce::Rectangle<int> plot);
    void drawHandles  (juce::Graphics&, juce::Rectangle<int> plot);

    float computeEQDbAt (float freq) const;
    int   findHandleAt  (juce::Point<float> p, juce::Rectangle<int> plot) const;

    juce::Rectangle<int> plotArea() const;
    float freqToX (float freq,  juce::Rectangle<int> plot) const;
    float xToFreq (float x,     juce::Rectangle<int> plot) const;
    float dbToY   (float db,    juce::Rectangle<int> plot) const;
    float yToDb   (float y,     juce::Rectangle<int> plot) const;
    float spectrumDbToY (float dbFs, juce::Rectangle<int> plot) const;

    void setParamNormalised (const juce::String& id, float value0to1) const;
    void setParamValue       (const juce::String& id, float value) const;

    //==============================================================================
    ToneSuiteAudioProcessor& proc;
    std::vector<Handle> handles;

    int hoveredIdx  = -1;
    int draggingIdx = -1;

    using CoeffsPtr = juce::dsp::IIR::Coefficients<float>::Ptr;
    CoeffsPtr cLowBoost, cLowCut, cHighBoost, cHighCut;
    std::array<CoeffsPtr, 3> cDynPeak;

    bool eqBypassed = false, dynBypassed = false;

    static constexpr float kFreqMin       = 20.0f;
    static constexpr float kFreqMax       = 20000.0f;
    static constexpr float kDbMin         = -30.0f;
    static constexpr float kDbMax         =  30.0f;
    static constexpr float kSpectrumDbMin = -90.0f;
    static constexpr float kSpectrumDbMax =   0.0f;
    static constexpr float kHandleRadius  = 9.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
};
