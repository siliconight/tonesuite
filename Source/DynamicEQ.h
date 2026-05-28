#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

//==============================================================================
//  Single dynamic EQ band, "parallel BP + scaled add" topology.
//
//  output = input + (G - 1) * activeAmount * BP(input)
//
//  - BP is a constant-0dB peak bandpass at (freq, Q). Its envelope is the
//    band's sidechain - it only "hears" energy in its own frequency range.
//  - activeAmount is a smoothed 0..1 driven by (envDb - threshDb) / kRange,
//    with per-band attack and release.
//  - G is the target linear gain. Positive gain dB = upward dynamic
//    (the band boosts when it's loud); negative = downward (the band cuts
//    when it's loud). Inactive (below threshold) -> 0 dB, signal passes
//    through unchanged.
//
//  Because the gain modulator is just a per-sample scalar, dynamic response
//  is at audio rate without recomputing biquad coefficients.
//==============================================================================
class DynamicBand
{
public:
    void prepare (double sr, int /*blockSize*/, int /*numChannels*/)
    {
        sampleRate = sr;
        for (auto& s : bpStates) s = {};
        smoothActive = 0.0f;
        updateBP();
    }

    void setParams (bool on_, float freq_, float q_, float gainDb,
                    float threshDb_, float atkMs, float relMs) noexcept
    {
        const bool needBP = (freq_ != freq) || (q_ != q);
        freq      = freq_;
        q         = juce::jlimit (0.3f, 8.0f, q_);
        linGain   = juce::Decibels::decibelsToGain (gainDb);
        threshDb  = threshDb_;
        on        = on_;
        attackCoef  = coefFromMs (atkMs);
        releaseCoef = coefFromMs (relMs);
        if (needBP) updateBP();
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        if (! on)
        {
            // Decay activity to zero so a re-enable doesn't pop.
            smoothActive *= 0.9f;
            return;
        }

        const int numCh   = juce::jmin (buffer.getNumChannels(), 2);
        const int n       = buffer.getNumSamples();
        const float gOff  = linGain - 1.0f;

        for (int i = 0; i < n; ++i)
        {
            std::array<float, 2> bp { 0.0f, 0.0f };
            float maxBp = 0.0f;

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float x = buffer.getReadPointer (ch)[i];
                bp[(size_t) ch] = processBP (x, bpStates[(size_t) ch]);
                maxBp = juce::jmax (maxBp, std::abs (bp[(size_t) ch]));
            }

            const float scDb = juce::Decibels::gainToDecibels (maxBp + 1.0e-9f);
            const float target = juce::jlimit (0.0f, 1.0f, (scDb - threshDb) / kRange);

            const float coef = (target > smoothActive) ? attackCoef : releaseCoef;
            smoothActive += (target - smoothActive) * coef;

            const float scaledOffset = gOff * smoothActive;
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] += bp[(size_t) ch] * scaledOffset;
        }
    }

    float getActiveAmount() const noexcept { return smoothActive; }

private:
    //==============================================================================
    struct BPState { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };

    float coefFromMs (float ms) const noexcept
    {
        const float tau = juce::jmax (ms, 0.01f) * 0.001f;
        return 1.0f - std::exp (-1.0f / (tau * (float) sampleRate));
    }

    // RBJ Cookbook bandpass, constant 0 dB peak gain.
    void updateBP() noexcept
    {
        const float w0    = juce::MathConstants<float>::twoPi * freq / (float) sampleRate;
        const float cosw0 = std::cos (w0);
        const float sinw0 = std::sin (w0);
        const float alpha = sinw0 / (2.0f * q);

        const float a0 = 1.0f + alpha;
        b0 =  alpha       / a0;
        b1 =  0.0f;
        b2 = -alpha       / a0;
        a1 = -2.0f * cosw0 / a0;
        a2 = (1.0f - alpha) / a0;
    }

    inline float processBP (float x, BPState& s) noexcept
    {
        const float y = b0 * x + b1 * s.x1 + b2 * s.x2 - a1 * s.y1 - a2 * s.y2;
        s.x2 = s.x1; s.x1 = x;
        s.y2 = s.y1; s.y1 = y;
        return y;
    }

    //==============================================================================
    // 12 dB above threshold = full activation. Hard-coded; expose as a "Range"
    // param later if you want finer control.
    static constexpr float kRange = 12.0f;

    double sampleRate = 44100.0;

    float freq = 1000.0f, q = 1.0f;
    float linGain  = 1.0f;
    float threshDb = -20.0f;
    bool  on       = false;
    float attackCoef = 0.05f, releaseCoef = 0.005f;

    float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    std::array<BPState, 2> bpStates {};

    float smoothActive = 0.0f;
};
