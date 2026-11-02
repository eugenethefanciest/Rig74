#pragma once

#include <juce_dsp/juce_dsp.h>
#include "DspUtils.h"

namespace rig
{
//==============================================================================
/** Stereo echo: tape / analog (BBD) / digital voicings, wow & flutter, ping-pong. */
class EchoDelay
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& d : lines) d.prepare ((int) (sr * 2.1));
        for (auto& f : lp) f.prepare (sr);
        for (auto& f : hp) { f.prepare (sr); f.setCutoff (90.0f); }
        wow.prepare (sr); flutter.prepare (sr, 0.3);
        wow.setRate (0.55f); flutter.setRate (6.7f);
        curDelay = -1.0f;
        smoothCoef = timeCoef (60.0f, sr);
        fbL = fbR = 0.0f;
    }

    void reset() { for (auto& d : lines) d.reset(); fbL = fbR = 0.0f; }

    void setParams (int newMode, float timeMs, float feedbackAmt, float mixAmt, float tone, float mod, bool pingPong)
    {
        mode = newMode; pp = pingPong; mix = mixAmt;
        targetDelay = (float) (timeMs * 0.001 * sr);
        if (curDelay < 0.0f) curDelay = targetDelay;
        feedback = feedbackAmt * (mode == 2 ? 0.95f : 1.02f);   // analog/tape can run away a little (saturated)

        float lo = 1500.0f, hi = 9000.0f;
        if (mode == 1) { lo = 700.0f; hi = 4500.0f; }
        if (mode == 2) { lo = 2000.0f; hi = 19000.0f; }
        const float cutoff = lo * std::pow (hi / lo, tone);
        for (auto& f : lp) f.setCutoff (cutoff);

        const float modScale = mode == 0 ? 1.0f : (mode == 1 ? 0.45f : 0.12f);
        wowDepth = mod * modScale * (float) (sr * 0.0028);
        flutterDepth = mod * modScale * (float) (sr * 0.00028);
        satDrive = mode == 0 ? 1.4f : (mode == 1 ? 1.9f : 0.0f);
    }

    inline void process (float& l, float& r) noexcept
    {
        curDelay = targetDelay + smoothCoef * (curDelay - targetDelay);   // tape-like glide when time changes
        const float m = wowDepth * Lfo::sine (wow.tick()) + flutterDepth * Lfo::sine (flutter.tick());
        const float d = std::max (2.0f, curDelay + m);

        const float wl = lines[0].read (d);
        const float wr = lines[1].read (d);

        const float mono = 0.5f * (l + r);
        float inL, inR;
        if (pp) { inL = mono + wr * feedback; inR = wl * feedback; }
        else    { inL = l + wl * feedback;    inR = r + wr * feedback; }

        lines[0].push (shape (hp[0].hp (lp[0].lp (inL))));
        lines[1].push (shape (hp[1].hp (lp[1].lp (inR))));

        const float dry = 1.0f - 0.25f * mix;
        l = l * dry + wl * mix * 1.1f;
        r = r * dry + wr * mix * 1.1f;
    }

private:
    inline float shape (float x) const noexcept
    {
        if (satDrive <= 0.0f) return juce::jlimit (-1.5f, 1.5f, x);
        return std::tanh (x * satDrive) / satDrive;
    }

    double sr = 44100.0;
    DelayLine lines[2];
    OnePole lp[2], hp[2];
    Lfo wow, flutter;
    int mode = 0;
    bool pp = false;
    float targetDelay = 1000.0f, curDelay = -1.0f, smoothCoef = 0.0f;
    float feedback = 0.3f, mix = 0.3f, wowDepth = 0.0f, flutterDepth = 0.0f, satDrive = 1.0f, fbL = 0.0f, fbR = 0.0f;
};

//==============================================================================
/** Spring tank model: dispersive all-pass chain inside a feedback loop
    (gives the "boing" chirp), three springs per side. */
class SpringTank
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        static const float lengthsMs[2][3] = { { 33.0f, 39.5f, 46.1f }, { 35.7f, 42.3f, 49.4f } };
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < 3; ++s)
            {
                auto& sp = springs[c][s];
                sp.line.prepare ((int) (sr * 0.08));
                sp.lengthSamples = (float) (lengthsMs[c][s] * 0.001 * sr);
                sp.lp.prepare (sr); sp.hp.prepare (sr); sp.hp.setCutoff (120.0f);
                for (auto& a : sp.ap) a = 0.0f;
                sp.fb = 0.0f;
                sp.lfo.prepare (sr, 0.17 * (c * 3 + s));
                sp.lfo.setRate (0.31f + 0.07f * (float) (c * 3 + s));
            }
        // all-pass coefficient for the dispersion (scaled for sample rate)
        apCoef = (float) juce::jlimit (-0.8, 0.8, -0.62 * std::pow (44100.0 / sr, 0.25));
    }

    void reset()
    {
        for (auto& ch : springs) for (auto& sp : ch) { sp.line.reset(); for (auto& a : sp.ap) a = 0.0f; sp.fb = 0.0f; }
    }

    void setParams (float decay, float tone)
    {
        gain = 0.55f + 0.38f * decay;
        for (auto& ch : springs) for (auto& sp : ch) sp.lp.setCutoff (2200.0f + 4000.0f * tone);
    }

    inline void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        const float in[2] = { inL, inR };
        float out[2] = { 0.0f, 0.0f };
        for (int c = 0; c < 2; ++c)
            for (auto& sp : springs[c])
            {
                float x = in[c] * 0.6f + sp.fb * gain;
                // dispersion: cascade of first-order all-passes
                for (auto& s : sp.ap)
                {
                    const float y = apCoef * x + s;
                    s = undenormal (x - apCoef * y);
                    x = y;
                }
                x = sp.hp.hp (sp.lp.lp (x));
                sp.line.push (x);
                const float d = sp.lengthSamples * (1.0f + 0.004f * Lfo::sine (sp.lfo.tick()));
                sp.fb = sp.line.read (d);
                out[c] += x;
            }
        outL = out[0] * 0.33f;
        outR = out[1] * 0.33f;
    }

private:
    struct Spring
    {
        DelayLine line;
        float lengthSamples = 1000.0f;
        float ap[14] {};
        float fb = 0.0f;
        OnePole lp, hp;
        Lfo lfo;
    };

    double sr = 44100.0;
    Spring springs[2][3];
    float apCoef = -0.6f, gain = 0.8f;
};

//==============================================================================
/** Reverb section: spring, plate or hall, with pre-delay and tone. */
class ReverbUnit
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        spring.prepare (sr);
        room.setSampleRate (sr);
        for (auto& d : pre) d.prepare ((int) (sr * 0.25));
        for (auto& f : inHp) { f.prepare (sr); f.setCutoff (110.0f); }
        for (auto& f : outLp) f.prepare (sr);
        wetL.assign ((size_t) maxBlock, 0.0f);
        wetR.assign ((size_t) maxBlock, 0.0f);
        type = -1;
    }

    void reset() { spring.reset(); room.reset(); for (auto& d : pre) d.reset(); }

    void setParams (int newType, float decay, float tone, float preDelayMs, float mixAmt)
    {
        if (newType != type) { type = newType; reset(); }
        mix = mixAmt;
        preSamples = std::max (1, (int) (preDelayMs * 0.001 * sr));
        spring.setParams (decay, tone);

        juce::Reverb::Parameters p;
        if (type == 1) { p.roomSize = 0.55f + 0.42f * decay; p.damping = 0.15f + 0.5f * (1.0f - tone); p.width = 1.0f; }
        else           { p.roomSize = 0.70f + 0.29f * decay; p.damping = 0.30f + 0.5f * (1.0f - tone); p.width = 1.0f; }
        p.wetLevel = 1.0f; p.dryLevel = 0.0f; p.freezeMode = 0.0f;
        room.setParameters (p);

        for (auto& f : outLp) f.setCutoff (2500.0f * std::pow (6.0f, tone));
    }

    void process (float* l, float* r, int n)
    {
        n = std::min (n, (int) wetL.size());
        for (int i = 0; i < n; ++i)
        {
            pre[0].push (inHp[0].hp (l[i]));
            pre[1].push (inHp[1].hp (r[i]));
            wetL[(size_t) i] = pre[0].readInt (preSamples);
            wetR[(size_t) i] = pre[1].readInt (preSamples);
        }

        if (type == 0)
        {
            for (int i = 0; i < n; ++i)
            {
                float a, b;
                spring.process (wetL[(size_t) i], wetR[(size_t) i], a, b);
                wetL[(size_t) i] = a; wetR[(size_t) i] = b;
            }
        }
        else
        {
            room.processStereo (wetL.data(), wetR.data(), n);
        }

        const float wetGain = (type == 0 ? 1.6f : 0.9f) * mix;
        const float dry = 1.0f - 0.35f * mix;
        for (int i = 0; i < n; ++i)
        {
            l[i] = l[i] * dry + outLp[0].lp (wetL[(size_t) i]) * wetGain;
            r[i] = r[i] * dry + outLp[1].lp (wetR[(size_t) i]) * wetGain;
        }
    }

private:
    double sr = 44100.0;
    int type = -1, preSamples = 1;
    float mix = 0.25f;
    SpringTank spring;
    juce::Reverb room;
    DelayLine pre[2];
    OnePole inHp[2], outLp[2];
    std::vector<float> wetL, wetR;
};
} // namespace rig
