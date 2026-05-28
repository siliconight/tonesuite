#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

//==============================================================================
//  Dynamic resonance suppressor (dynamic resonance-suppression style).
//
//  24 narrow bandpass filters log-spaced 200 Hz .. 14 kHz cover the spectrum.
//  Each band's envelope is compared against the **spatial average of its four
//  nearest neighbors**: a band that sticks up above the neighborhood by more
//  than `threshold` dB is treated as a resonance and attenuated by up to
//  `depth` dB, using the same parallel BP-and-add topology as the dynamic EQ
//  (with a negative gain offset). Spatial - not temporal - comparison means
//  this catches sustained nasal peaks as well as transient harshness.
//
//  Mix blends the correction signal: 0% = dry, 100% = full suppression.
//==============================================================================
class ResonanceSuppressor
{
public:
    static constexpr int   kNumBands  = 24;
    static constexpr float kFreqLow   = 200.0f;
    static constexpr float kFreqHigh  = 14000.0f;
    static constexpr float kBandQ     = 6.0f;
    static constexpr float kRangeDb   = 6.0f;     // dB above thresh = full activation

    void prepare (double sr, int /*blockSize*/, int /*numChannels*/)
    {
        sampleRate = sr;
        for (auto& b : bands)
        {
            for (auto& s : b.states) s = {};
            b.envFast = 0.0f;
            b.smoothActive = 0.0f;
        }
        envCoef = coefFromMs (8.0f); // band envelope tracking window
        updateAllBPs();
    }

    void setParams (bool on_, float depthDb, float threshDb_,
                    float atkMs, float relMs, float mix01) noexcept
    {
        on        = on_;
        linDepth  = juce::Decibels::decibelsToGain (depthDb);   // <= 1
        threshDb  = threshDb_;
        attackCoef  = coefFromMs (atkMs);
        releaseCoef = coefFromMs (relMs);
        mix       = juce::jlimit (0.0f, 1.0f, mix01);
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        if (! on)
        {
            for (auto& b : bands) b.smoothActive *= 0.9f;
            totalActivity.store (0.0f);
            return;
        }

        const int numCh = juce::jmin (buffer.getNumChannels(), 2);
        const int n     = buffer.getNumSamples();
        const float gOff = linDepth - 1.0f;   // negative because linDepth <= 1

        float maxActivity = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            // Pass 1: BP-filter each band per channel and update its envelope.
            for (size_t b = 0; b < kNumBands; ++b)
            {
                auto& band = bands[b];
                float maxBp = 0.0f;
                for (int ch = 0; ch < numCh; ++ch)
                {
                    const float x = buffer.getReadPointer (ch)[i];
                    band.bpOut[(size_t) ch] = processBP (x, band, (size_t) ch);
                    maxBp = juce::jmax (maxBp, std::abs (band.bpOut[(size_t) ch]));
                }
                band.envFast += (maxBp - band.envFast) * envCoef;
            }

            // Pass 2: for each band, compute spatial reference (mean of +/-2
            // neighbors), the excess in dB, and the smoothed active amount.
            for (size_t b = 0; b < kNumBands; ++b)
            {
                float refSum = 0.0f;
                int   refCnt = 0;
                for (int k = -2; k <= 2; ++k)
                {
                    if (k == 0) continue;
                    const int idx = (int) b + k;
                    if (idx >= 0 && idx < (int) kNumBands)
                    { refSum += bands[(size_t) idx].envFast; ++refCnt; }
                }
                const float ref = (refCnt > 0) ? refSum / (float) refCnt : 1.0e-9f;
                const float excessDb = juce::Decibels::gainToDecibels (
                                          (bands[b].envFast + 1.0e-9f) / (ref + 1.0e-9f));
                const float target = juce::jlimit (0.0f, 1.0f,
                                          (excessDb - threshDb) / kRangeDb);

                auto& sa = bands[b].smoothActive;
                const float coef = (target > sa) ? attackCoef : releaseCoef;
                sa += (target - sa) * coef;
                maxActivity = juce::jmax (maxActivity, sa);
            }

            // Pass 3: write the correction back into the buffer (scaled by mix).
            for (int ch = 0; ch < numCh; ++ch)
            {
                float correction = 0.0f;
                for (size_t b = 0; b < kNumBands; ++b)
                    correction += bands[b].bpOut[(size_t) ch] * gOff * bands[b].smoothActive;

                buffer.getWritePointer (ch)[i] += correction * mix;
            }
        }

        totalActivity.store (maxActivity);
    }

    float getActivity() const noexcept { return totalActivity.load(); }

private:
    //==============================================================================
    struct Band
    {
        // RBJ bandpass coefficients (constant 0 dB peak gain).
        float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

        struct State { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
        std::array<State, 2> states {};
        std::array<float, 2> bpOut  {};

        float envFast      = 0.0f;
        float smoothActive = 0.0f;
    };

    float coefFromMs (float ms) const noexcept
    {
        const float tau = juce::jmax (ms, 0.01f) * 0.001f;
        return 1.0f - std::exp (-1.0f / (tau * (float) sampleRate));
    }

    void updateAllBPs()
    {
        for (size_t b = 0; b < kNumBands; ++b)
        {
            const float t    = (float) b / (float) (kNumBands - 1);
            const float freq = kFreqLow * std::pow (kFreqHigh / kFreqLow, t);
            const float w0    = juce::MathConstants<float>::twoPi * freq / (float) sampleRate;
            const float cosw0 = std::cos (w0);
            const float sinw0 = std::sin (w0);
            const float alpha = sinw0 / (2.0f * kBandQ);
            const float a0    = 1.0f + alpha;

            auto& bd = bands[b];
            bd.b0 =  alpha       / a0;
            bd.b1 =  0.0f;
            bd.b2 = -alpha       / a0;
            bd.a1 = -2.0f * cosw0 / a0;
            bd.a2 = (1.0f - alpha) / a0;
        }
    }

    inline float processBP (float x, Band& bd, size_t ch) noexcept
    {
        auto& s = bd.states[ch];
        const float y = bd.b0 * x + bd.b1 * s.x1 + bd.b2 * s.x2
                                  - bd.a1 * s.y1 - bd.a2 * s.y2;
        s.x2 = s.x1; s.x1 = x;
        s.y2 = s.y1; s.y1 = y;
        return y;
    }

    //==============================================================================
    std::array<Band, kNumBands> bands;
    double sampleRate = 44100.0;
    bool   on         = false;
    float  linDepth   = juce::Decibels::decibelsToGain (-6.0f);
    float  threshDb   = 6.0f;
    float  mix        = 1.0f;
    float  envCoef    = 0.05f;
    float  attackCoef = 0.05f;
    float  releaseCoef = 0.005f;

    std::atomic<float> totalActivity { 0.0f };
};
