#pragma once

#include "DspUtils.h"

namespace rig
{
//==============================================================================
/** 7-band graphic EQ (mono, before the stereo effects). */
class GraphicEq
{
public:
    void prepare (double sampleRate) { sr = sampleRate; for (auto& b : bands) b.reset(); for (auto& g : last) g = 1000.0f; }

    void setParams (const float* gainsDb, const float* freqs, int n, float levelDb)
    {
        for (int i = 0; i < n && i < 7; ++i)
            if (gainsDb[i] != last[i]) { last[i] = gainsDb[i]; bands[i].makePeak (sr, freqs[i], 1.1, gainsDb[i]); }
        level = dbToGain (levelDb);
    }

    inline float process (float x) noexcept
    {
        for (auto& b : bands) x = b.process (x);
        return x * level;
    }

private:
    double sr = 44100.0;
    Biquad bands[7];
    float last[7] {};
    float level = 1.0f;
};

//==============================================================================
/** Bucket-brigade style stereo chorus. */
class Chorus
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& d : lines) d.prepare ((int) (sr * 0.05));
        lfo.prepare (sr);
        for (auto& f : darken) { f.prepare (sr); f.setCutoff (6500.0f); }
    }
    void reset() { for (auto& d : lines) d.reset(); }
    void setParams (float rate, float depth, float mixAmt) { lfo.setRate (rate); depthMs = 0.3f + 4.5f * depth; mix = mixAmt; }

    inline void process (float& l, float& r) noexcept
    {
        const double p = lfo.tick();
        const float mono = 0.5f * (l + r);
        lines[0].push (mono); lines[1].push (mono);
        const float msToS = (float) (sr * 0.001);
        const float dl = (7.0f + depthMs * (0.5f + 0.5f * Lfo::sine (p))) * msToS;
        const float dr = (7.0f + depthMs * (0.5f + 0.5f * Lfo::sine (p + 0.25))) * msToS;
        const float wl = darken[0].lp (lines[0].read (dl));
        const float wr = darken[1].lp (lines[1].read (dr));
        const float dry = 1.0f - 0.4f * mix;
        l = l * dry + wl * mix * 0.8f;
        r = r * dry + wr * mix * 0.8f;
    }

private:
    double sr = 44100.0;
    DelayLine lines[2];
    OnePole darken[2];
    Lfo lfo;
    float depthMs = 2.0f, mix = 0.5f;
};

//==============================================================================
/** Through-zero-less jet flanger with regeneration. */
class Flanger
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& d : lines) d.prepare ((int) (sr * 0.02));
        lfo.prepare (sr);
        fb[0] = fb[1] = 0.0f;
    }
    void reset() { for (auto& d : lines) d.reset(); fb[0] = fb[1] = 0.0f; }
    void setParams (float rate, float depth, float regen, float mixAmt) { lfo.setRate (rate); depthAmt = depth; feedback = regen * 0.92f; mix = mixAmt; }

    inline void process (float& l, float& r) noexcept
    {
        const double p = lfo.tick();
        const float msToS = (float) (sr * 0.001);
        float* io[2] = { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            const float tri = Lfo::tri (c == 0 ? p : std::fmod (p + 0.1, 1.0));
            const float d = (0.25f + depthAmt * 4.0f * (0.5f + 0.5f * tri)) * msToS;
            const float in = *io[c];
            lines[c].push (std::tanh (in + fb[c] * feedback));
            const float wet = lines[c].read (d);
            fb[c] = undenormal (wet);
            *io[c] = in * (1.0f - 0.5f * mix) + wet * mix * 0.7f;
        }
    }

private:
    double sr = 44100.0;
    DelayLine lines[2];
    Lfo lfo;
    float fb[2] {}, depthAmt = 0.6f, feedback = 0.4f, mix = 0.5f;
};

//==============================================================================
/** Four-stage phaser (orange-box style) with feedback, stereo LFO offset. */
class Phaser
{
public:
    void prepare (double sampleRate) { sr = sampleRate; lfo.prepare (sr); reset(); }
    void reset() { for (auto& ch : state) for (auto& s : ch) s = 0.0f; fb[0] = fb[1] = 0.0f; }
    void setParams (float rate, float depth, float feedbackAmt, float mixAmt) { lfo.setRate (rate); depthAmt = depth; feedback = feedbackAmt * 0.85f; mix = mixAmt; }

    inline void process (float& l, float& r) noexcept
    {
        const double p = lfo.tick();
        float* io[2] = { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            const float lfoVal = 0.5f + 0.5f * Lfo::sine (c == 0 ? p : p + 0.18);
            const float freq = 180.0f * std::pow (2.0f, depthAmt * 4.0f * lfoVal) + 60.0f;
            const float t = (float) std::tan (kPi * std::min (freq, (float) (sr * 0.45)) / sr);
            const float a = (t - 1.0f) / (t + 1.0f);

            float x = *io[c] + fb[c] * feedback;
            for (auto& s : state[c])
            {
                const float y = a * x + s;
                s = undenormal (x - a * y);
                x = y;
            }
            fb[c] = x;
            *io[c] = *io[c] * (1.0f - 0.5f * mix) + x * mix * 0.9f;
        }
    }

private:
    double sr = 44100.0;
    Lfo lfo;
    float state[2][4] {};
    float fb[2] {}, depthAmt = 0.7f, feedback = 0.3f, mix = 0.5f;
};

//==============================================================================
/** Amp-style tremolo; Shape morphs from smooth sine to choppy square. */
class Tremolo
{
public:
    void prepare (double sampleRate) { sr = sampleRate; lfo.prepare (sr); }
    void setParams (float rate, float depth, float shape) { lfo.setRate (rate); depthAmt = depth; shapeAmt = shape; }

    inline void process (float& l, float& r) noexcept
    {
        const float s = Lfo::sine (lfo.tick());
        const float k = 1.0f + 12.0f * shapeAmt * shapeAmt;
        const float shaped = std::tanh (s * k) / std::tanh (k);
        const float g = 1.0f - depthAmt * (0.5f + 0.5f * shaped);
        l *= g; r *= g;
    }

private:
    double sr = 44100.0;
    Lfo lfo;
    float depthAmt = 0.6f, shapeAmt = 0.2f;
};
} // namespace rig
