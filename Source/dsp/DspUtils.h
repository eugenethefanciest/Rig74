#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include <juce_core/juce_core.h>

namespace rig
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline float dbToGain (float db) noexcept   { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept    { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float clamp01 (float x) noexcept     { return std::min (1.0f, std::max (0.0f, x)); }

/** Flush tiny values to zero (avoids denormal CPU spikes in feedback loops). */
inline float undenormal (float x) noexcept  { return std::abs (x) < 1.0e-15f ? 0.0f : x; }

/** Coefficient for a one-pole smoother with the given time constant. */
inline float timeCoef (float ms, double sr) noexcept
{
    if (ms <= 0.0f) return 0.0f;
    return (float) std::exp (-1.0 / (0.001 * ms * sr));
}

//==============================================================================
/** First-order low/high-pass (TPT form, stable under fast modulation). */
struct OnePole
{
    float g = 0.0f, s = 0.0f;
    double sr = 44100.0;

    void prepare (double sampleRate) { sr = sampleRate; s = 0.0f; }
    void reset() { s = 0.0f; }
    void setCutoff (float hz)
    {
        hz = std::min ((float) (sr * 0.49), std::max (1.0f, hz));
        const float wc = (float) std::tan (kPi * hz / sr);
        g = wc / (1.0f + wc);
    }
    inline float lp (float x) noexcept
    {
        const float v = (x - s) * g;
        const float y = v + s;
        s = undenormal (y + v);
        return y;
    }
    inline float hp (float x) noexcept { return x - lp (x); }
};

//==============================================================================
/** RBJ biquad, transposed direct form II, double precision state. */
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    inline float process (float xf) noexcept
    {
        const double x = xf;
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        if (std::abs (z1) < 1e-20) z1 = 0;
        if (std::abs (z2) < 1e-20) z2 = 0;
        return (float) y;
    }

    void setNormalised (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }

    void makeLowPass (double sr, double f, double q)
    {
        const double w = kTwoPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNormalised ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void makeHighPass (double sr, double f, double q)
    {
        const double w = kTwoPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNormalised ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void makePeak (double sr, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = kTwoPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNormalised (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void makeNotch (double sr, double f, double q)
    {
        const double w = kTwoPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNormalised (1, -2 * c, 1, 1 + al, -2 * c, 1 - al);
    }
    void makeLowShelf (double sr, double f, double gainDb, double slope = 1.0)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = kTwoPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / slope - 1) + 2);
        const double sa = 2 * std::sqrt (A) * al;
        setNormalised (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
                       (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    void makeHighShelf (double sr, double f, double gainDb, double slope = 1.0)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = kTwoPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / slope - 1) + 2);
        const double sa = 2 * std::sqrt (A) * al;
        setNormalised (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
                       (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
};

//==============================================================================
/** Cytomic / Zavalishin state-variable filter (TPT), safe to modulate per sample. */
struct Svf
{
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    float ic1 = 0, ic2 = 0;
    double sr = 44100.0;
    float lpOut = 0, bpOut = 0, hpOut = 0;

    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { ic1 = ic2 = 0; }
    void set (float hz, float q)
    {
        hz = std::min ((float) (sr * 0.45), std::max (10.0f, hz));
        g = (float) std::tan (kPi * hz / sr);
        k = 1.0f / std::max (0.1f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline void process (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = undenormal (2 * v1 - ic1);
        ic2 = undenormal (2 * v2 - ic2);
        lpOut = v2; bpOut = v1; hpOut = x - k * v1 - v2;
    }
};

//==============================================================================
/** Simple circular delay line with 4-point Hermite interpolation. */
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare (int maxSamples)
    {
        int size = 1;
        while (size < maxSamples + 4) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }

    /** Read 'delay' samples behind the last written sample (delay >= 1). */
    inline float read (float delay) const noexcept
    {
        delay = std::max (1.0f, std::min (delay, (float) (mask - 3)));
        const float pos = (float) w - delay;
        int i = (int) std::floor (pos);
        const float f = pos - (float) i;
        const float xm1 = buf[(size_t) ((i - 1) & mask)];
        const float x0  = buf[(size_t) (i & mask)];
        const float x1  = buf[(size_t) ((i + 1) & mask)];
        const float x2  = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    inline float readInt (int delay) const noexcept { return buf[(size_t) ((w - delay) & mask)]; }
};

//==============================================================================
struct Lfo
{
    double phase = 0.0, inc = 0.0, sr = 44100.0;
    void prepare (double sampleRate, double startPhase = 0.0) { sr = sampleRate; phase = startPhase; }
    void setRate (float hz) { inc = hz / sr; }
    /** Advances and returns the phase in [0,1). */
    inline double tick() noexcept { phase += inc; if (phase >= 1.0) phase -= 1.0; return phase; }
    static inline float sine (double p) noexcept { return (float) std::sin (kTwoPi * p); }
    static inline float tri (double p) noexcept { return (float) (p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p); }
};

//==============================================================================
/** Envelope follower with separate attack/release. */
struct EnvFollower
{
    float att = 0, rel = 0, env = 0;
    void set (float attackMs, float releaseMs, double sr) { att = timeCoef (attackMs, sr); rel = timeCoef (releaseMs, sr); }
    void reset() { env = 0; }
    inline float process (float x) noexcept
    {
        const float a = std::abs (x);
        env = a > env ? a + att * (env - a) : a + rel * (env - a);
        env = undenormal (env);
        return env;
    }
};
} // namespace rig
