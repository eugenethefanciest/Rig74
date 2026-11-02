#pragma once

#include "DspUtils.h"

namespace rig
{
/** Stompbox drive. Runs at the oversampled rate.
    Types: 0 = overdrive (mid-hump soft clip), 1 = distortion (hard diode clip), 2 = fuzz (biased germanium-ish). */
class Drive
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto* f : { &pre1, &pre2, &toneLp, &toneLp2, &dcBlock, &bodyLp })
            f->prepare (sr);
        dcBlock.setCutoff (15.0f);
        type = -1;
    }

    void setParams (int newType, float gain, float tone, float level)
    {
        if (newType != type) { type = newType; pre1.reset(); pre2.reset(); toneLp.reset(); toneLp2.reset(); bodyLp.reset(); }

        switch (type)
        {
            case 0: // overdrive
                drive = 1.0f + 140.0f * gain * gain;
                pre1.setCutoff (720.0f);                       // clipping path high-pass → "mid hump"
                bodyLp.setCutoff (4000.0f);
                toneLp.setCutoff (600.0f * std::pow (12.0f, tone));
                outGain = 1.3f;
                break;
            case 1: // distortion
                drive = 1.0f + 1600.0f * gain * gain;
                pre1.setCutoff (90.0f);
                pre2.setCutoff (5500.0f);
                toneLp.setCutoff (450.0f * std::pow (30.0f, tone));
                toneLp2.setCutoff (450.0f * std::pow (30.0f, tone));
                outGain = 0.55f;
                break;
            default: // fuzz
                drive = 2.0f + 500.0f * gain * gain;
                pre1.setCutoff (60.0f);
                toneLp.setCutoff (700.0f * std::pow (12.0f, tone));
                outGain = 0.3f;
                break;
        }
        level = std::max (0.0f, level);
        outLevel = outGain * 2.0f * level * level;
    }

    inline float process (float x) noexcept
    {
        float y = 0.0f;
        switch (type)
        {
            case 0:
            {
                const float clipPath = pre1.hp (x) * drive;
                const float clipped = std::tanh (clipPath + 0.08f) - 0.0798f;     // slight asymmetry
                y = bodyLp.lp (x + 0.55f * clipped);
                y = toneLp.lp (y);
                break;
            }
            case 1:
            {
                float v = pre2.lp (pre1.hp (x)) * drive;
                v = v / std::pow (1.0f + std::pow (std::abs (v), 2.5f), 0.4f);      // hard-ish diode knee
                v = juce::jlimit (-1.0f, 1.0f, v);
                y = toneLp2.lp (toneLp.lp (v));
                break;
            }
            default:
            {
                const float v = pre1.hp (x) * drive;
                // asymmetric transistor clipping with a bias shift
                const float pos = std::tanh (v * 0.9f + 0.25f) - 0.2449f;
                const float clipped = pos > 0.0f ? pos : std::max (-0.62f, pos * 1.25f);
                y = toneLp.lp (clipped);
                break;
            }
        }
        return dcBlock.hp (y) * outLevel;
    }

private:
    double sr = 44100.0;
    int type = 0;
    float drive = 1.0f, outGain = 1.0f, outLevel = 0.5f;
    OnePole pre1, pre2, toneLp, toneLp2, dcBlock, bodyLp;
};
} // namespace rig
