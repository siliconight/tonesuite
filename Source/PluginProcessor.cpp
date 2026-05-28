#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
//  Frequency selector tables (Hz) - the classic passive program EQ stepped values.
//==============================================================================
const std::array<float, 4> ToneSuiteAudioProcessor::kLowFreqs       { 20.f, 30.f, 60.f, 100.f };
const std::array<float, 7> ToneSuiteAudioProcessor::kHighBoostFreqs  { 3000.f, 4000.f, 5000.f, 8000.f, 10000.f, 12000.f, 16000.f };
const std::array<float, 3> ToneSuiteAudioProcessor::kHighAttenFreqs  { 5000.f, 10000.f, 20000.f };

//==============================================================================
//  Voicing constants now live as static constexpr members of the processor
//  class so the editor can use the exact same values to draw its EQ curve.
//==============================================================================

//==============================================================================
ToneSuiteAudioProcessor::ToneSuiteAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
ToneSuiteAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto dial = NormalisableRange<float> (0.0f, 10.0f, 0.01f);

    // ---- LOW band ----
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "lowBoost", 1 }, "Low Boost", dial, 0.0f));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "lowAtten", 1 }, "Low Atten", dial, 0.0f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "lowFreq", 1 },  "Low Freq",
                                                        StringArray { "20", "30", "60", "100" }, 1));

    // ---- HIGH boost band ----
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "highBoost", 1 }, "High Boost", dial, 0.0f));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "highBW", 1 },    "Bandwidth",
                                                        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 3.0f)); // 0 broad .. 10 sharp
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "highFreq", 1 },  "High Freq",
                                                        StringArray { "3k", "4k", "5k", "8k", "10k", "12k", "16k" }, 2));

    // ---- HIGH atten band ----
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "highAtten", 1 },     "High Atten", dial, 0.0f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "highAttenFreq", 1 }, "Atten Freq",
                                                        StringArray { "5k", "10k", "20k" }, 1));

    // ---- Output stage ----
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "drive", 1 },  "Drive",
                                                        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "output", 1 }, "Output",
                                                        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "bypass", 1 }, "Bypass", false));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "eqBypass", 1 },    "EQ Bypass", false));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "dynEqBypass", 1 }, "Dyn EQ Bypass", false));

    // ---- Compressor (variable-mu-style) ----
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "compBypass", 1 }, "Comp Bypass", false));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "compInput", 1 },  "Comp Input",
                                                        NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "compThresh", 1 }, "Comp Threshold",
                                                        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "compTime", 1 },   "Time Constant",
                                                        StringArray { "1", "2", "3", "4", "5", "6" }, 1));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "compMakeup", 1 }, "Comp Makeup",
                                                        NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "compMix", 1 },    "Comp Mix",
                                                        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    // ---- Routing ----
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "routing", 1 }, "Routing",
                                                        StringArray { "EQ -> Comp", "Comp -> EQ" }, 0));

    // ---- Dynamic EQ bands (3) ----
    struct BandDefaults { float freq, q, gainDb, threshDb, atkMs, relMs; };
    static const BandDefaults bandDefs[3] = {
        {  200.0f, 0.8f, -4.0f, -18.0f, 5.0f, 100.0f },
        { 1200.0f, 1.5f, -3.0f, -20.0f, 3.0f,  70.0f },
        { 6000.0f, 0.7f, +2.0f, -22.0f, 2.0f,  50.0f },
    };

    auto freqRange = NormalisableRange<float> (30.0f, 18000.0f, 0.0f, 0.3f); // log skew
    auto qRange    = NormalisableRange<float> (0.3f, 8.0f, 0.0f, 0.4f);
    auto timeRange = NormalisableRange<float> (0.5f, 500.0f, 0.0f, 0.3f);

    for (int b = 1; b <= 3; ++b)
    {
        const auto& d = bandDefs[b - 1];
        const String pfx  = "dyn" + String (b);
        const String name = "Dyn " + String (b);

        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { pfx + "On", 1 },     name + " On", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pfx + "Freq", 1 },   name + " Freq",   freqRange, d.freq));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pfx + "Q", 1 },      name + " Q",      qRange,    d.q));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pfx + "Gain", 1 },   name + " Gain",
                                                           NormalisableRange<float> (-18.0f, 18.0f, 0.1f), d.gainDb));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pfx + "Thresh", 1 }, name + " Thresh",
                                                           NormalisableRange<float> (-60.0f, 0.0f, 0.1f), d.threshDb));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pfx + "Atk", 1 },    name + " Attack",  timeRange, d.atkMs));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pfx + "Rel", 1 },    name + " Release", timeRange, d.relMs));
    }

    // ---- Tape ----
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "tapeOn", 1 },    "Tape On", false));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "tapeDrive", 1 }, "Tape Drive",
                                                        NormalisableRange<float> (-12.0f, 18.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "tapeBias", 1 },  "Tape Bias",
                                                        NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "tapeSpeed", 1 }, "Tape Speed",
                                                        StringArray { "7.5 IPS", "15 IPS", "30 IPS" }, 1));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "tapeOut", 1 },   "Tape Out",
                                                        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    // ---- Resonance Suppressor (dynamic resonance-suppression style, end of chain) ----
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "resBypass", 1 }, "Resonance Bypass", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "resDepth", 1 },  "Resonance Depth",
                                                       NormalisableRange<float> (-18.0f, 0.0f, 0.1f), -6.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "resThresh", 1 }, "Resonance Threshold",
                                                       NormalisableRange<float> (0.0f, 18.0f, 0.1f), 6.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "resAtk", 1 },    "Resonance Attack",
                                                       NormalisableRange<float> (0.5f, 50.0f, 0.0f, 0.3f), 5.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "resRel", 1 },    "Resonance Release",
                                                       NormalisableRange<float> (5.0f, 500.0f, 0.0f, 0.3f), 100.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "resMix", 1 },    "Resonance Mix",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    return layout;
}

//==============================================================================
void ToneSuiteAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 2;

    for (auto* f : { &lowBoost, &lowCut, &highBoost, &highCut })
    {
        f->prepare (spec);
        f->reset();
    }

    comp.prepare (sr, samplesPerBlock, 2);
    for (auto& band : dynBands)
        band.prepare (sr, samplesPerBlock, 2);
    tape.prepare (sr, samplesPerBlock, 2);
    resSup.prepare (sr, samplesPerBlock, 2);
    analyzer.prepare (sr, samplesPerBlock);

    eqOversampler.initProcessing ((size_t) juce::jmax (1, samplesPerBlock));
    eqOversampler.reset();

    // Report total latency: EQ tube + Tape saturation oversamplers.
    setLatencySamples ((int) eqOversampler.getLatencyInSamples()
                     + tape.getLatencyInSamples());

    outputGain.reset (sr, 0.02);
    updateFilters();
}

bool ToneSuiteAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& main = layouts.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::mono() && main != juce::AudioChannelSet::stereo())
        return false;

    return main == layouts.getMainInputChannelSet();
}

//==============================================================================
void ToneSuiteAudioProcessor::updateFilters()
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    // --- LOW band ---
    const int   lowIdx   = (int) get ("lowFreq");
    const float lowF     = kLowFreqs[(size_t) juce::jlimit (0, 3, lowIdx)];
    const float boostDb  =  (get ("lowBoost") / 10.0f) * kLowBoostMaxDb;
    const float attenDb  = -(get ("lowAtten") / 10.0f) * kLowAttenMaxDb;

    *lowBoost.state = *Coeffs::makeLowShelf (sampleRate, lowF, kLowShelfQ,
                                             juce::Decibels::decibelsToGain (boostDb));
    *lowCut.state   = *Coeffs::makeLowShelf (sampleRate,
                                             juce::jmin (lowF * kLowCutRatio, (float) sampleRate * 0.45f),
                                             kLowShelfQ,
                                             juce::Decibels::decibelsToGain (attenDb));

    // --- HIGH boost (peaking, variable bandwidth) ---
    const int   hIdx     = (int) get ("highFreq");
    const float highF    = kHighBoostFreqs[(size_t) juce::jlimit (0, 6, hIdx)];
    const float hBoostDb = (get ("highBoost") / 10.0f) * kHighBoostMaxDb;
    const float bw       = get ("highBW");                    // 0 broad .. 10 sharp
    const float q        = juce::jmap (bw, 0.0f, 10.0f, 0.6f, 3.5f);

    *highBoost.state = *Coeffs::makePeakFilter (sampleRate,
                                                juce::jmin (highF, (float) sampleRate * 0.45f),
                                                q,
                                                juce::Decibels::decibelsToGain (hBoostDb));

    // --- HIGH atten (shelf cut) ---
    const int   haIdx    = (int) get ("highAttenFreq");
    const float haF      = kHighAttenFreqs[(size_t) juce::jlimit (0, 2, haIdx)];
    const float hAttenDb = -(get ("highAtten") / 10.0f) * kHighAttenMaxDb;

    *highCut.state = *Coeffs::makeHighShelf (sampleRate,
                                             juce::jmin (haF, (float) sampleRate * 0.45f),
                                             0.7f,
                                             juce::Decibels::decibelsToGain (hAttenDb));

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (get ("output")));
}

//==============================================================================
float ToneSuiteAudioProcessor::tubeStage (float x, float drive) noexcept
{
    if (drive <= 0.0f)
        return x;

    const float g    = 1.0f + drive * 0.4f;      // input gain into the nonlinearity
    const float bias = 0.15f * (drive / 10.0f);  // asymmetry -> even harmonics
    const float driven = x * g + bias;
    const float shaped = std::tanh (driven) - std::tanh (bias);
    return shaped / g;                           // restore unity-ish level
}

//==============================================================================
//  EQ stage. Linear filters run at native rate; the tube saturation runs
//  oversampled. The oversampler ALWAYS executes one up/down cycle, so the
//  reported latency stays constant whether the stage is bypassed or driven.
//==============================================================================
void ToneSuiteAudioProcessor::applyEQ (juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block (buffer);

    const bool bypassed = apvts.getRawParameterValue ("eqBypass")->load() > 0.5f;
    const float drive   = apvts.getRawParameterValue ("drive")->load();

    if (! bypassed)
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        lowBoost.process  (ctx);
        lowCut.process    (ctx);
        highBoost.process (ctx);
        highCut.process   (ctx);
    }

    // ALWAYS run one oversampler cycle to keep latency constant.
    auto upBlock = eqOversampler.processSamplesUp (block);

    if (! bypassed && drive > 0.0f)
    {
        for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch)
        {
            auto* data = upBlock.getChannelPointer (ch);
            const size_t n = upBlock.getNumSamples();
            for (size_t i = 0; i < n; ++i)
                data[i] = tubeStage (data[i], drive);
        }
    }

    eqOversampler.processSamplesDown (block);
}

void ToneSuiteAudioProcessor::applyComp (juce::AudioBuffer<float>& buffer)
{
    if (apvts.getRawParameterValue ("compBypass")->load() > 0.5f)
    {
        compGainReductionDb.store (0.0f);
        return;
    }

    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    comp.setParams (get ("compInput"),
                    get ("compThresh"),
                    (int) get ("compTime") + 1,     // choice index 0..5 -> position 1..6
                    get ("compMakeup"),
                    get ("compMix") * 0.01f);

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    comp.process (L, R, buffer.getNumSamples());

    compGainReductionDb.store (comp.getGainReductionDb());
}

//==============================================================================
void ToneSuiteAudioProcessor::applyDynEQ (juce::AudioBuffer<float>& buffer)
{
    const bool bypassed = apvts.getRawParameterValue ("dynEqBypass")->load() > 0.5f;
    if (bypassed)
    {
        for (auto& a : dynBandActivity) a.store (0.0f);
        return;
    }

    const auto getF = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };

    for (int b = 0; b < (int) dynBands.size(); ++b)
    {
        const juce::String pfx = "dyn" + juce::String (b + 1);
        const bool on = getF (pfx + "On") > 0.5f;

        dynBands[(size_t) b].setParams (on,
                                        getF (pfx + "Freq"),
                                        getF (pfx + "Q"),
                                        getF (pfx + "Gain"),
                                        getF (pfx + "Thresh"),
                                        getF (pfx + "Atk"),
                                        getF (pfx + "Rel"));
        dynBands[(size_t) b].process (buffer);
        dynBandActivity[(size_t) b].store (dynBands[(size_t) b].getActiveAmount());
    }
}

void ToneSuiteAudioProcessor::applyTape (juce::AudioBuffer<float>& buffer)
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    tape.setParams (get ("tapeOn") > 0.5f,
                    get ("tapeDrive"),
                    get ("tapeBias"),
                    (int) get ("tapeSpeed"),
                    get ("tapeOut"));
    tape.process (buffer);
}

//==============================================================================
void ToneSuiteAudioProcessor::applyResonance (juce::AudioBuffer<float>& buffer)
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const bool bypassed = get ("resBypass") > 0.5f;
    resSup.setParams (! bypassed,
                      get ("resDepth"),
                      get ("resThresh"),
                      get ("resAtk"),
                      get ("resRel"),
                      get ("resMix") * 0.01f);
    resSup.process (buffer);
    resonanceActivity.store (resSup.getActivity());
}

//==============================================================================
void ToneSuiteAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (auto i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (apvts.getRawParameterValue ("bypass")->load() > 0.5f)
    {
        compGainReductionDb.store (0.0f);
        return;
    }

    updateFilters();

    const bool eqFirst = apvts.getRawParameterValue ("routing")->load() < 0.5f;
    if (eqFirst) { applyEQ (buffer);  applyComp (buffer); }
    else         { applyComp (buffer); applyEQ (buffer);  }

    applyDynEQ (buffer);
    applyTape  (buffer);
    applyResonance (buffer);

    // Smoothed global output trim (always the final stage).
    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        const float gain = outputGain.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer (ch)[n] *= gain;
    }

    // Feed the FFT analyzer the final output.
    analyzer.pushBuffer (buffer);
}

//==============================================================================
juce::AudioProcessorEditor* ToneSuiteAudioProcessor::createEditor()
{
    return new ToneSuiteAudioProcessorEditor (*this);
}

void ToneSuiteAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void ToneSuiteAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ToneSuiteAudioProcessor();
}
