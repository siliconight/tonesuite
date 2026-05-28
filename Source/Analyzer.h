#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>

//==============================================================================
//  Single-producer / single-consumer FFT analyzer.
//
//  Audio thread:  pushBuffer() copies a mono-summed block into a ring buffer.
//  UI thread:     computeMagnitudes() reads the latest fftSize samples,
//                 windows, transforms, and updates a smoothed dB magnitude
//                 array with fast-rise / slow-fall ballistics.
//
//  Magnitudes are queried by frequency (linear interpolation between bins).
//==============================================================================
class SpectrumAnalyzer
{
public:
    static constexpr int fftOrder = 11;            // 2048-point FFT
    static constexpr int fftSize  = 1 << fftOrder;
    static constexpr int numBins  = fftSize / 2;
    static constexpr int ringSize = fftSize * 2;   // must be power of 2

    SpectrumAnalyzer()
        : fft (fftOrder),
          window (fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        magnitudesDb.fill (-100.0f);
    }

    void prepare (double sr, int /*blockSize*/) { sampleRate = sr; }

    // --- audio thread ---
    void pushBuffer (const juce::AudioBuffer<float>& buffer) noexcept
    {
        const int n  = buffer.getNumSamples();
        const int nc = juce::jmax (1, buffer.getNumChannels());
        const float invNc = 1.0f / (float) nc;

        int wp = writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < nc; ++ch)
                sum += buffer.getReadPointer (ch)[i];
            ring[(size_t) wp] = sum * invNc;
            wp = (wp + 1) & (ringSize - 1);
        }
        writePos.store (wp, std::memory_order_release);
    }

    // --- UI thread ---
    void computeMagnitudes() noexcept
    {
        const int wp    = writePos.load (std::memory_order_acquire);
        const int start = (wp - fftSize + ringSize) & (ringSize - 1);

        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = ring[(size_t) ((start + i) & (ringSize - 1))];
        for (int i = fftSize; i < fftSize * 2; ++i)
            fftData[(size_t) i] = 0.0f;

        window.multiplyWithWindowingTable (fftData.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const float norm = 2.0f / (float) fftSize;
        for (int i = 0; i < numBins; ++i)
        {
            const float mag = fftData[(size_t) i] * norm;
            const float db  = juce::Decibels::gainToDecibels (mag + 1.0e-9f);

            // Fast rise, slow fall.
            if (db > magnitudesDb[(size_t) i])
                magnitudesDb[(size_t) i] = db;
            else
                magnitudesDb[(size_t) i] += (db - magnitudesDb[(size_t) i]) * 0.15f;
        }
    }

    float getMagnitudeAtFreq (float freq) const noexcept
    {
        const float bin = freq * (float) fftSize / (float) sampleRate;
        const int   i0  = juce::jlimit (0, numBins - 1, (int) bin);
        const int   i1  = juce::jmin (i0 + 1, numBins - 1);
        const float t   = juce::jlimit (0.0f, 1.0f, bin - (float) i0);
        return juce::jmap (t, magnitudesDb[(size_t) i0], magnitudesDb[(size_t) i1]);
    }

    double getSampleRate() const noexcept { return sampleRate; }

private:
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, ringSize>   ring     {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, numBins>     magnitudesDb {};

    std::atomic<int> writePos { 0 };
    double sampleRate = 44100.0;
};
