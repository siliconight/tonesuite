#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "VariableMuComp.h"
#include "DynamicEQ.h"
#include "TapeStage.h"
#include "Analyzer.h"
#include "ResonanceSuppressor.h"

//==============================================================================
//  passive program EQ.
//
//  Control set mirrors the classic passive program EQ:
//    LOW  : Boost, Atten (cut), and a shared frequency selector (20/30/60/100 Hz)
//    HIGH : Boost + Bandwidth + frequency selector (3..16 kHz)
//    HIGH ATTEN : cut amount + frequency selector (5/10/20 kHz)
//    Plus a Drive (tube) stage and Output trim.
//
//  The signature move - boosting AND cutting the LOW band at the same setting -
//  produces a bump in the deep lows with a scoop just above it. We recreate that
//  by cornering the cut shelf above the boost shelf (LOW_CUT_RATIO).
//==============================================================================
class ToneSuiteAudioProcessor : public juce::AudioProcessor
{
public:
    ToneSuiteAudioProcessor();
    ~ToneSuiteAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Frequency tables exposed so the editor can label the selectors.
    static const std::array<float, 4> kLowFreqs;
    static const std::array<float, 7> kHighBoostFreqs;
    static const std::array<float, 3> kHighAttenFreqs;

    // EQ voicing constants - shared with the editor so its response curve
    // matches the audio exactly.
    static constexpr float kLowBoostMaxDb  = 13.5f;
    static constexpr float kLowAttenMaxDb  = 17.5f;
    static constexpr float kHighBoostMaxDb = 18.0f;
    static constexpr float kHighAttenMaxDb = 16.0f;
    static constexpr float kLowCutRatio    = 2.2f;
    static constexpr float kLowShelfQ      = 0.55f;

    // Read by the editor's GR meter (<= 0 dB).
    std::atomic<float> compGainReductionDb { 0.0f };

    // Live activity (0..1) per dynamic EQ band, read by the editor.
    std::array<std::atomic<float>, 3> dynBandActivity { {} };

    // Live activity (0..1) of the resonance suppressor (peak band).
    std::atomic<float> resonanceActivity { 0.0f };

    // The FFT analyzer reads from the OUTPUT (post-chain) - shows you what
    // your processing actually produced.
    SpectrumAnalyzer analyzer;

private:
    //==============================================================================
    using Filter   = juce::dsp::IIR::Filter<float>;
    using Coeffs   = juce::dsp::IIR::Coefficients<float>;
    using StereoEQ = juce::dsp::ProcessorDuplicator<Filter, Coeffs>;

    StereoEQ lowBoost, lowCut, highBoost, highCut;

    VariableMuCompressor comp;
    std::array<DynamicBand, 3> dynBands;
    TapeStage tape;
    ResonanceSuppressor resSup;

    // 2x oversampler that wraps the EQ tube stage. Always-on so the reported
    // plugin latency stays constant.
    juce::dsp::Oversampling<float> eqOversampler {
        2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
        true /*maxQuality*/,
        true /*useIntegerLatency*/
    };

    void updateFilters();
    void applyEQ        (juce::AudioBuffer<float>&);
    void applyComp      (juce::AudioBuffer<float>&);
    void applyDynEQ     (juce::AudioBuffer<float>&);
    void applyTape      (juce::AudioBuffer<float>&);
    void applyResonance (juce::AudioBuffer<float>&);

    // Asymmetric soft-clip: emphasises even harmonics, tube-flavoured.
    static inline float tubeStage (float x, float drive) noexcept;

    double sampleRate = 44100.0;
    juce::dsp::ProcessSpec spec {};

    juce::SmoothedValue<float> outputGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneSuiteAudioProcessor)
};
