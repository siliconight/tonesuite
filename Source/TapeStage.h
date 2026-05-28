#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

//==============================================================================
//  Tape emulation. Signal flow:
//
//    drive -> [up-sample 2x]
//          -> asymmetric tanh saturation (bias offsets the curve)
//          -> [down-sample]
//          -> LF head bump (low shelf)
//          -> HF rolloff (first-order LP)
//          -> output trim
//
//  The 2x FIR halfband oversampler runs ALWAYS, so reported latency is
//  constant whether the stage is on or off (host PDC stays correct).
//==============================================================================
class TapeStage
{
public:
    TapeStage()
        : oversampler (2, 1,
                       juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                       true /*maxQuality*/,
                       true /*useIntegerLatency*/)
    {}

    void prepare (double sr, int blockSize, int numChannels)
    {
        sampleRate = sr;
        juce::dsp::ProcessSpec spec {
            sr,
            (juce::uint32) juce::jmax (1, blockSize),
            (juce::uint32) juce::jmax (1, numChannels)
        };
        bump.prepare (spec); bump.reset();
        lpf.prepare  (spec); lpf.reset();
        oversampler.initProcessing ((size_t) juce::jmax (1, blockSize));
        oversampler.reset();
        outputGain.reset (sr, 0.02);
        updateFilters();
    }

    void setParams (bool on_, float driveDb, float bias_, int speedIdx, float outDb)
    {
        on       = on_;
        drive    = juce::Decibels::decibelsToGain (driveDb);
        invDrive = 1.0f / juce::jmax (drive, 1.0e-6f);
        bias     = juce::jlimit (-1.0f, 1.0f, bias_);
        outputGain.setTargetValue (juce::Decibels::decibelsToGain (outDb));

        if (speedIdx != speed)
        {
            speed = juce::jlimit (0, 2, speedIdx);
            updateFilters();
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block (buffer);

        // Always upsample so latency is constant whether `on` is true or false.
        auto upBlock = oversampler.processSamplesUp (block);

        if (on)
        {
            const float biasOffset = bias * 0.15f;
            const float biasComp   = std::tanh (biasOffset);

            for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch)
            {
                auto* d = upBlock.getChannelPointer (ch);
                const size_t n = upBlock.getNumSamples();
                for (size_t i = 0; i < n; ++i)
                {
                    const float driven = d[i] * drive + biasOffset;
                    d[i] = (std::tanh (driven) - biasComp) * invDrive;
                }
            }
        }

        oversampler.processSamplesDown (block);

        if (! on) return;

        // Linear filters at native rate.
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        bump.process (ctx);
        lpf.process  (ctx);

        // Smoothed output trim.
        const int n = buffer.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            const float g = outputGain.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }

    int getLatencyInSamples() const { return (int) oversampler.getLatencyInSamples(); }

private:
    void updateFilters()
    {
        struct Preset { float bumpFreq, bumpDb, lpFreq; };
        static const Preset presets[3] =
        {
            { 100.0f, 2.5f, 10000.0f },
            {  65.0f, 1.5f, 16000.0f },
            {  50.0f, 0.8f, 22000.0f },
        };
        const auto& p = presets[(size_t) juce::jlimit (0, 2, speed)];
        const float nyq = (float) sampleRate * 0.45f;

        *bump.state = *Coeffs::makeLowShelf (sampleRate, p.bumpFreq, 0.7f,
                                             juce::Decibels::decibelsToGain (p.bumpDb));
        *lpf.state  = *Coeffs::makeFirstOrderLowPass (sampleRate, juce::jmin (p.lpFreq, nyq));
    }

    using Filter   = juce::dsp::IIR::Filter<float>;
    using Coeffs   = juce::dsp::IIR::Coefficients<float>;
    using StereoF  = juce::dsp::ProcessorDuplicator<Filter, Coeffs>;

    StereoF bump, lpf;
    juce::dsp::Oversampling<float> oversampler;

    double sampleRate = 44100.0;
    bool   on        = false;
    float  drive     = 1.0f, invDrive = 1.0f;
    float  bias      = 0.0f;
    int    speed     = 1;
    juce::SmoothedValue<float> outputGain { 1.0f };
};
