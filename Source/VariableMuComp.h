#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

//==============================================================================
//  variable-mu compressor.
//
//  Character notes that drive the model:
//    * Variable-mu => ratio RISES with level (soft onset, grabby when pushed),
//      so there is no fixed ratio knob. We approximate with ratio = 1 + over*slope.
//    * Six TIME CONSTANT positions set attack/release. Positions 5 & 6 are
//      "program dependent": a fast release component plus a long tail that only
//      engages after sustained gain reduction.
//    * Input Gain drives BOTH the signal and the detector (you push into it,
//      then make up on output) - that's how the hardware is used.
//    * Tube harmonics increase with gain reduction.
//
//  Stereo-linked detection (max of |L|,|R|).
//==============================================================================
class VariableMuCompressor
{
public:
    void prepare (double sr, int /*blockSize*/, int /*numChannels*/)
    {
        sampleRate = sr;
        reset();
        setTimeConstant (currentTC);
    }

    void reset() noexcept
    {
        gain      = 1.0f;
        holdEnv   = 0.0f;
        meterGRdb = 0.0f;
    }

    void setParams (float inputGainDb, float thresholdKnob /*0..10*/, int timeConstant /*1..6*/,
                    float makeupDb, float mix01) noexcept
    {
        inputGain   = juce::Decibels::decibelsToGain (inputGainDb);
        makeup      = juce::Decibels::decibelsToGain (makeupDb);
        mix         = juce::jlimit (0.0f, 1.0f, mix01);

        // Knob up => lower effective threshold => more compression.
        thresholdDb = juce::jmap (juce::jlimit (0.0f, 10.0f, thresholdKnob), 0.0f, 10.0f, 0.0f, -40.0f);

        if (timeConstant != currentTC)
            setTimeConstant (timeConstant);
    }

    float getGainReductionDb() const noexcept { return meterGRdb; } // <= 0

    //==============================================================================
    void process (float* L, float* R, int numSamples) noexcept
    {
        float minGain = 1.0f;

        for (int n = 0; n < numSamples; ++n)
        {
            const float dryL = L[n];
            const float dryR = (R != nullptr ? R[n] : L[n]);

            const float l = dryL * inputGain;
            const float r = dryR * inputGain;

            // ---- detector (stereo-linked) ----
            const float sc   = juce::jmax (std::abs (l), std::abs (r));
            const float scDb = juce::Decibels::gainToDecibels (sc + 1.0e-9f);

            // ---- static variable-mu curve ----
            const float over = scDb - thresholdDb;
            float grDb = 0.0f;
            if (over > 0.0f)
            {
                const float ratio = 1.0f + over * kRatioSlope; // ratio grows with level
                grDb = over - over / ratio;
            }
            const float targetGain = juce::Decibels::decibelsToGain (-grDb);

            // ---- attack / release smoothing on the gain ----
            float coef;
            if (targetGain < gain)
            {
                coef = attackCoef;                       // pulling gain down = attack
            }
            else if (dualRelease)
            {
                // program-dependent: blend fast->slow by how long we've been compressing
                const float t = juce::jlimit (0.0f, 1.0f, holdEnv * 3.0f);
                coef = juce::jmap (t, releaseFastCoef, releaseSlowCoef);
            }
            else
            {
                coef = releaseCoef;
            }

            gain += (targetGain - gain) * coef;
            minGain = juce::jmin (minGain, gain);

            // ---- "how long have we been compressing" envelope (for dual release) ----
            const float grAmt    = 1.0f - gain;          // ~0 (none) .. ~0.7 (heavy)
            const float holdCoef = (grAmt > holdEnv) ? holdAttackCoef : holdReleaseCoef;
            holdEnv += (grAmt - holdEnv) * holdCoef;

            // ---- apply gain, tube color (scales with GR), makeup ----
            float outL = l * gain;
            float outR = r * gain;

            const float color = grAmt;
            if (color > 1.0e-4f)
            {
                outL = tubeColor (outL, color);
                outR = tubeColor (outR, color);
            }

            outL *= makeup;
            outR *= makeup;

            // ---- wet/dry mix (dry is the un-driven input) ----
            L[n] = dryL * (1.0f - mix) + outL * mix;
            if (R != nullptr)
                R[n] = dryR * (1.0f - mix) + outR * mix;
        }

        meterGRdb = juce::Decibels::gainToDecibels (minGain); // most GR over the block
    }

private:
    //==============================================================================
    float coefFromMs (float ms) const noexcept
    {
        const float tau = juce::jmax (ms, 0.01f) * 0.001f;        // seconds
        return 1.0f - std::exp (-1.0f / (tau * (float) sampleRate));
    }

    void setTimeConstant (int tc) noexcept
    {
        currentTC = juce::jlimit (1, 6, tc);

        struct TC { float atkMs, relMs, relSlowMs; bool dual; };
        static const TC table[6] =
        {
            { 0.2f,  300.0f,     0.0f, false }, // 1
            { 0.2f,  800.0f,     0.0f, false }, // 2
            { 0.4f, 2000.0f,     0.0f, false }, // 3
            { 0.8f, 5000.0f,     0.0f, false }, // 4
            { 0.2f,  300.0f, 10000.0f, true  }, // 5  program-dependent
            { 0.4f,  300.0f, 25000.0f, true  }, // 6  program-dependent
        };

        const auto& t = table[currentTC - 1];
        attackCoef      = coefFromMs (t.atkMs);
        releaseCoef     = coefFromMs (t.relMs);
        releaseFastCoef = coefFromMs (t.relMs);
        releaseSlowCoef = coefFromMs (t.relSlowMs > 0.0f ? t.relSlowMs : t.relMs);
        dualRelease     = t.dual;

        holdAttackCoef  = coefFromMs (150.0f);
        holdReleaseCoef = coefFromMs (1500.0f);
    }

    static float tubeColor (float x, float amt) noexcept
    {
        const float g    = 1.0f + amt * 0.5f;
        const float bias = 0.05f * amt;                 // asymmetry -> even harmonics
        return (std::tanh (x * g + bias) - std::tanh (bias)) / g;
    }

    //==============================================================================
    static constexpr float kRatioSlope = 0.25f;         // higher = grabbier when pushed

    double sampleRate = 44100.0;

    float inputGain   = 1.0f;
    float makeup      = 1.0f;
    float mix         = 1.0f;
    float thresholdDb = 0.0f;

    float gain    = 1.0f;   // current applied gain (linked)
    float holdEnv = 0.0f;   // sustained-GR tracker for program-dependent release
    float meterGRdb = 0.0f;

    int   currentTC = 2;
    bool  dualRelease = false;
    float attackCoef = 0.1f, releaseCoef = 0.001f;
    float releaseFastCoef = 0.001f, releaseSlowCoef = 0.0001f;
    float holdAttackCoef = 0.01f, holdReleaseCoef = 0.001f;
};
