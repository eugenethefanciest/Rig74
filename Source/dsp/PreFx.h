#pragma once

#include "DspUtils.h"

namespace rig
{
//==============================================================================
/** Wah: resonant band-pass sweeping 350 Hz – 2.2 kHz like a classic inductor wah.
    Modes: 0 = pedal (automatable), 1 = auto (envelope), 2 = LFO. */
class Wah
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        svf.prepare (sr);
        env.set (4.0f, 120.0f, sr);
        env.reset();
        lfo.prepare (sr);
        posSmooth = 0.5f;
        smoothCoef = timeCoef (8.0f, sr);
    }

    void setParams (int newMode, float pedal, float sens, float rateHz, float q)
    {
        mode = newMode; pedalPos = pedal; sensitivity = sens; resonance = q;
        lfo.setRate (rateHz);
    }

    inline float process (float x) noexcept
    {
        float target = pedalPos;
        if (mode == 1)
        {
            const float e = env.process (x);
            target = clamp01 (e * (2.0f + 30.0f * sensitivity * sensitivity));
        }
        else if (mode == 2)
        {
            target = 0.5f + 0.5f * Lfo::sine (lfo.tick());
        }

        posSmooth = target + smoothCoef * (posSmooth - target);

        if (++counter >= 8)   // update the filter every 8 samples (cheap & still smooth)
        {
            counter = 0;
            const float f = 350.0f * std::pow (2200.0f / 350.0f, posSmooth);
            svf.set (f, resonance);
        }

        svf.process (x);
        // normalised band-pass (unity at the peak) + a touch of low-pass for body
        const float band = svf.bpOut * svf.k;
        return 1.15f * band * (0.6f + 0.08f * resonance) + 0.2f * svf.lpOut;
    }

private:
    double sr = 44100.0;
    Svf svf;
    EnvFollower env;
    Lfo lfo;
    int mode = 0, counter = 0;
    float pedalPos = 0.5f, sensitivity = 0.5f, resonance = 5.0f, posSmooth = 0.5f, smoothCoef = 0.0f;
};

//==============================================================================
/** Analog-style octaver: flip-flop sub octave + rectifier octave up. */
class Octaver
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        trackLp1.prepare (sr); trackLp1.setCutoff (900.0f);
        trackLp2.prepare (sr); trackLp2.setCutoff (900.0f);
        subLp.prepare (sr);    subLp.setCutoff (700.0f);
        upHp.prepare (sr);     upHp.setCutoff (60.0f);
        upLp.prepare (sr);     upLp.setCutoff (5000.0f);
        env.set (2.0f, 60.0f, sr); env.reset();
        flip = 1.0f; lastSign = false;
    }

    void setParams (float down, float up, float dry) { downLevel = down; upLevel = up; dryLevel = dry; }

    inline float process (float x) noexcept
    {
        const float tracked = trackLp2.lp (trackLp1.lp (x));
        const float e = env.process (tracked);

        // flip-flop toggles on every rising zero crossing → square at f/2
        const bool sign = tracked > 0.0005f * (lastSign ? -1.0f : 1.0f);
        if (sign && ! lastSign) flip = -flip;
        lastSign = sign;
        const float sub = subLp.lp (flip * e * 1.4f);

        // full-wave rectifier doubles the frequency
        const float up = upLp.lp (upHp.hp (std::abs (x) * 2.2f));

        return dryLevel * x + downLevel * sub * 1.6f + upLevel * up;
    }

private:
    double sr = 44100.0;
    OnePole trackLp1, trackLp2, subLp, upHp, upLp;
    EnvFollower env;
    float flip = 1.0f, downLevel = 0.5f, upLevel = 0.0f, dryLevel = 0.8f;
    bool lastSign = false;
};
} // namespace rig
