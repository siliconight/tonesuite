#pragma once

#include "PluginProcessor.h"
#include "SpectrumView.h"

//==============================================================================
//  Modern dark look. Cream + brass accents on deep blue.
//==============================================================================
class ProLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ProLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;
};

//==============================================================================
//  A clickable status indicator for one stage in the signal chain.
//  Click to toggle that stage's bypass. Visually shows on/off state.
//==============================================================================
class ChainPill : public juce::Component
{
public:
    ChainPill (juce::AudioProcessorValueTreeState& apvts,
               const juce::String& paramID,
               const juce::String& displayName,
               bool invertActive = false);  // tapeOn: 1.0 means ON, not bypassed

    void paint (juce::Graphics&) override;
    void mouseDown  (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit  (const juce::MouseEvent&) override;

private:
    bool isActive() const noexcept;
    juce::ParameterAttachment attachment;
    juce::String name;
    bool invert;
    float paramValue = 0.0f;
    bool  hover      = false;
};

//==============================================================================
//  The routing arrow between the EQ and Comp pills. Click to flip direction.
//==============================================================================
class RoutingArrow : public juce::Component
{
public:
    explicit RoutingArrow (juce::AudioProcessorValueTreeState& apvts);

    void paint (juce::Graphics&) override;
    void mouseDown  (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit  (const juce::MouseEvent&) override;

private:
    juce::ParameterAttachment attachment;
    float value = 0.0f;
    bool  hover = false;
};

//==============================================================================
//  GR meter (lifted from previous editor, restyled).
//==============================================================================
class GainReductionMeter : public juce::Component, private juce::Timer
{
public:
    explicit GainReductionMeter (std::atomic<float>& src) : source (src) { startTimerHz (30); }
    ~GainReductionMeter() override { stopTimer(); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    std::atomic<float>& source;
    float displayed = 0.0f;
    static constexpr float kRange = 20.0f;
};

//==============================================================================
//  Compact module panels.
//==============================================================================
class CompModule : public juce::Component
{
public:
    explicit CompModule (ToneSuiteAudioProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BoxAtt    = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    ToneSuiteAudioProcessor& proc;
    juce::Slider   input, thresh, makeup, mix;
    juce::ComboBox time;
    juce::Label    inputL, threshL, timeL, makeupL, mixL, title;
    GainReductionMeter meter;

    std::unique_ptr<SliderAtt> a1, a2, a3, a4;
    std::unique_ptr<BoxAtt>    b1;
};

class TapeModule : public juce::Component
{
public:
    explicit TapeModule (ToneSuiteAudioProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BoxAtt    = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    ToneSuiteAudioProcessor& proc;
    juce::Slider   drive, bias, out;
    juce::ComboBox speed;
    juce::Label    driveL, biasL, speedL, outL, title;

    std::unique_ptr<SliderAtt> a1, a2, a3;
    std::unique_ptr<BoxAtt>    b1;
};

class DynamicsModule : public juce::Component
{
public:
    explicit DynamicsModule (ToneSuiteAudioProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Row
    {
        juce::Label   bandName;
        juce::Slider  thresh, atk, rel;
        juce::Label   threshL, atkL, relL;
        std::unique_ptr<SliderAtt> a1, a2, a3;
    };

    ToneSuiteAudioProcessor& proc;
    juce::Label title;
    std::array<Row, 3> rows;
};

//==============================================================================
//  Dynamic resonance suppressor module - the "surgical cleanup" controls.
//==============================================================================
class ResonanceModule : public juce::Component, private juce::Timer
{
public:
    explicit ResonanceModule (ToneSuiteAudioProcessor&);
    ~ResonanceModule() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;

    ToneSuiteAudioProcessor& proc;
    juce::Slider depth, thresh, atk, rel, mix;
    juce::Label  depthL, threshL, atkL, relL, mixL, title;

    std::unique_ptr<SliderAtt> a1, a2, a3, a4, a5;

    float activity = 0.0f;
};

//==============================================================================
//  Main editor.
//==============================================================================
class ToneSuiteAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ToneSuiteAudioProcessorEditor (ToneSuiteAudioProcessor&);
    ~ToneSuiteAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS     = juce::AudioProcessorValueTreeState;
    using SliderAtt = APVTS::SliderAttachment;
    using BtnAtt    = APVTS::ButtonAttachment;

    ToneSuiteAudioProcessor& proc;
    ProLookAndFeel lnf;

    SpectrumView    spectrum;
    DynamicsModule  dynMod;
    CompModule      compMod;
    TapeModule      tapeMod;
    ResonanceModule resMod;

    ChainPill      pillEQ, pillComp, pillDyn, pillTape, pillRes;
    RoutingArrow   routingArrow;

    juce::Slider       output;
    juce::Label        outputL;
    juce::ToggleButton bypass { "Bypass" };

    std::unique_ptr<SliderAtt> aOutput;
    std::unique_ptr<BtnAtt>    aBypass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneSuiteAudioProcessorEditor)
};
