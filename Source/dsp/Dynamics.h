#pragma once

#include "DspUtils.h"

namespace rig
{
//==============================================================================
/** Removes mains hum: notches at the fundamental and three harmonics. */
class HumFilter
{
public:
    void prepare (double sampleRate) { sr = sampleRate; mode = -1; for (auto& n : notches) n.reset(); }

    /** mode: 0 = off, 1 = 50 Hz, 2 = 60 Hz */
    void setMode (int newMode)
    {
        if (newMode == mode) return;
        mode = newMode;
        if (mode == 0) return;
        const double f0 = mode == 1 ? 50.0 : 60.0;
        for (int i = 0; i < 4; ++i)
        {
            notches[i].makeNotch (sr, f0 * (i + 1), 10.0 + 4.0 * i);
            notches[i].reset();
        }
    }

    inline float process (float x) noexcept
    {
        if (mode <= 0) return x;
        for (auto& n : notches) x = n.process (x);
        return x;
    }

private:
    double sr = 44100.0;
    int mode = -1;
    Biquad notches[4];
};

//==============================================================================
/** Noise suppressor in the classic "loop" style: the key signal is the clean
    guitar input, but the gain is applied after the amp & cab where the hiss is. */
class NoiseGate
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        detector.set (0.5f, 30.0f, sr);
        detector.reset();
        gain = 1.0f; open = true; holdCounter = 0;
    }

    void setParams (float thresholdDb, float releaseMs)
    {
        openThresh  = dbToGain (thresholdDb);
        closeThresh = dbToGain (thresholdDb - 6.0f);   // hysteresis: no chattering
        attackCoef  = timeCoef (0.4f, sr);
        releaseCoef = timeCoef (releaseMs, sr);
        holdSamples = (int) (0.025 * sr);
    }

    /** Returns the gate gain for this input sample. */
    inline float computeGain (float key) noexcept
    {
        const float env = detector.process (key);
        if (env > openThresh)            { open = true; holdCounter = holdSamples; }
        else if (env < closeThresh)      { if (holdCounter > 0) --holdCounter; else open = false; }

        const float target = open ? 1.0f : 0.0f;
        const float c = target > gain ? attackCoef : releaseCoef;
        gain = target + c * (gain - target);
        return gain;
    }

    float getCurrentGain() const noexcept { return gain; }

private:
    double sr = 44100.0;
    EnvFollower detector;
    float openThresh = 0.001f, closeThresh = 0.0005f, attackCoef = 0, releaseCoef = 0, gain = 1.0f;
    bool open = true;
    int holdSamples = 1000, holdCounter = 0;
};

//==============================================================================
/** 70s-style "squeezer" compressor with a single Sustain control. */
class Compressor
{
public:
    void prepare (double sampleRate) { sr = sampleRate; envDb = -120.0f; gainSmooth = 1.0f; }

    void setParams (float sustain, float attackMs, float levelDb)
    {
        threshDb = -8.0f - 34.0f * sustain;
        ratio    = 2.0f + 10.0f * sustain;
        attCoef  = timeCoef (attackMs, sr);
        relCoef  = timeCoef (120.0f + 200.0f * sustain, sr);
        makeup   = dbToGain (-threshDb * (1.0f - 1.0f / ratio) * 0.55f + levelDb);
    }

    inline float process (float x) noexcept
    {
        const float inDb = gainToDb (std::abs (x) + 1.0e-7f);
        envDb = inDb > envDb ? inDb + attCoef * (envDb - inDb) : inDb + relCoef * (envDb - inDb);

        // soft knee (8 dB)
        const float over = envDb - threshDb;
        constexpr float knee = 8.0f;
        float grDb = 0.0f;
        if (over > knee * 0.5f)        grDb = over * (1.0f / ratio - 1.0f);
        else if (over > -knee * 0.5f)  grDb = (1.0f / ratio - 1.0f) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);

        return x * dbToGain (grDb) * makeup;
    }

private:
    double sr = 44100.0;
    float threshDb = -20.0f, ratio = 4.0f, attCoef = 0, relCoef = 0, makeup = 1.0f, envDb = -120.0f, gainSmooth = 1.0f;
};
} // namespace rig
