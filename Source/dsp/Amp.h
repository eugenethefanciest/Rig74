#pragma once

#include "DspUtils.h"

namespace rig
{
//==============================================================================
/** Passive three-knob tone stack (bass / middle / treble) as found in classic
    tube amps, discretised from its analog transfer function (Yeh & Smith, 2006). */
class ToneStack
{
public:
    struct Components { double R1, R2, R3, R4, C1, C2, C3; };

    static Components american() { return { 250e3, 1e6, 25e3, 56e3, 250e-12, 20e-9, 20e-9 }; }
    static Components british()  { return { 220e3, 1e6, 22e3, 33e3, 470e-12, 22e-9, 22e-9 }; }

    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() { z[0] = z[1] = z[2] = 0.0; }

    /** bass, mid, treble in 0..1 */
    void set (const Components& comp, double bass, double mid, double treble)
    {
        // log-taper pots like the real thing
        const double l = std::pow (juce::jlimit (0.0, 1.0, bass), 2.0) * 0.999 + 0.0005;
        const double m = juce::jlimit (0.0005, 0.9995, mid);
        const double t = juce::jlimit (0.0005, 0.9995, treble);

        const double R1 = comp.R1, R2 = comp.R2, R3 = comp.R3, R4 = comp.R4, C1 = comp.C1, C2 = comp.C2, C3 = comp.C3;

        const double b1 = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
        const double b2 = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4)
                        - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
                        + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                        + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
        const double b3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                        - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + t * C1 * C2 * C3 * R1 * R3 * R4 - t * m * C1 * C2 * C3 * R1 * R3 * R4
                        + t * l * C1 * C2 * C3 * R1 * R2 * R4;
        const double a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
        const double a2 = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                        - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
                        + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
        const double a3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                        - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4)
                        + l * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;

        const double c  = 2.0 * fs, c2 = c * c, c3 = c2 * c;
        const double B0 = -b1 * c - b2 * c2 - b3 * c3;
        const double B1 = -b1 * c + b2 * c2 + 3 * b3 * c3;
        const double B2 =  b1 * c + b2 * c2 - 3 * b3 * c3;
        const double B3 =  b1 * c - b2 * c2 + b3 * c3;
        const double A0 = -1 - a1 * c - a2 * c2 - a3 * c3;
        const double A1 = -3 - a1 * c + a2 * c2 + 3 * a3 * c3;
        const double A2 = -3 + a1 * c + a2 * c2 - 3 * a3 * c3;
        const double A3 = -1 + a1 * c - a2 * c2 + a3 * c3;

        b[0] = B0 / A0; b[1] = B1 / A0; b[2] = B2 / A0; b[3] = B3 / A0;
        a[1] = A1 / A0; a[2] = A2 / A0; a[3] = A3 / A0;
    }

    inline float process (float xf) noexcept
    {
        const double x = xf;
        const double y = b[0] * x + z[0];
        z[0] = b[1] * x - a[1] * y + z[1];
        z[1] = b[2] * x - a[2] * y + z[2];
        z[2] = b[3] * x - a[3] * y;
        for (auto& s : z) if (std::abs (s) < 1e-20) s = 0;
        return (float) y;
    }

private:
    double fs = 44100.0;
    double b[4] { 1, 0, 0, 0 }, a[4] { 1, 0, 0, 0 }, z[3] { 0, 0, 0 };
};

//==============================================================================
/** Tube amp head: preamp stages → tone stack → push-pull power amp with sag & presence.
    Runs at the oversampled rate. */
class TubeAmp
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        inHp.prepare (sr);
        for (auto& s : stages) { s.dc.prepare (sr); s.lp.prepare (sr); s.dc.setCutoff (25.0f); }
        tone.prepare (sr);
        bright.reset(); presence.reset(); depth.reset();
        sagEnv.set (5.0f, 180.0f, sr); sagEnv.reset();
        outDc.prepare (sr); outDc.setCutoff (12.0f);
        model = -1;
    }

    /** All knobs 0..10 like on the amp face. */
    void setParams (int newModel, float gain, float bass, float mid, float treble, float pres, float master)
    {
        const bool modelChanged = newModel != model;
        model = newModel;
        const float g = gain / 10.0f;

        struct Model { int stages; float maxGain; float inHpHz; float stageLpHz; float bias; bool british; float level; float brightDb; };
        static const Model models[] = {
            { 1,   9.0f,  35.0f, 9000.0f, 0.10f, false, 1.05f, 4.0f },   // CLEAN '65
            { 2,  36.0f,  70.0f, 7500.0f, 0.18f, true,  0.80f, 3.0f },   // CRUNCH '68
            { 3,  70.0f, 110.0f, 6500.0f, 0.22f, true,  0.60f, 2.0f },   // LEAD '72
            { 4, 120.0f, 160.0f, 6000.0f, 0.26f, true,  0.52f, 1.0f } }; // HOT ROD '79
        const auto& m = models[juce::jlimit (0, 3, model)];

        numStages = m.stages;
        bias = m.bias;
        preGain = 1.0f + m.maxGain * g * g;
        inHp.setCutoff (m.inHpHz);
        for (int i = 0; i < 4; ++i)
        {
            stages[i].lp.setCutoff (m.stageLpHz + 1500.0f * (float) (3 - i));
            stages[i].dc.setCutoff (i == 0 ? 20.0f : m.inHpHz * 0.6f);
            stages[i].gain = i == 0 ? 1.0f : 2.2f + 1.5f * g;
        }
        if (modelChanged) for (auto& s : stages) { s.dc.reset(); s.lp.reset(); }

        // bright cap: strongest at low gain settings
        bright.makeHighShelf (sr, 2200.0, m.brightDb * (1.0f - g), 0.7);

        tone.set (m.british ? ToneStack::british() : ToneStack::american(), bass / 10.0, mid / 10.0, treble / 10.0);

        presence.makeHighShelf (sr, 3200.0, (pres / 10.0f) * 12.0f - 4.0f, 0.8);
        depth.makePeak (sr, 95.0, 0.9, m.british ? 2.5 : 1.0);

        const float mst = master / 10.0f;
        powerDrive = 0.6f + 5.0f * mst * mst;
        outLevel = m.level * (0.35f + 0.9f * mst) / std::tanh (powerDrive * 0.5f + 0.1f);
    }

    inline float process (float x) noexcept
    {
        float v = bright.process (inHp.hp (x)) * preGain;

        for (int i = 0; i < numStages; ++i)
        {
            auto& s = stages[i];
            v *= s.gain;
            v = triode (v);
            v = s.lp.lp (s.dc.hp (v));
        }

        v = tone.process (v) * 3.2f;   // compensate the passive stack's insertion loss

        // power amp with supply sag
        const float sag = sagEnv.process (v);
        const float drive = powerDrive / (1.0f + 0.35f * sag);
        v = std::tanh (v * drive);
        v = depth.process (presence.process (v));

        return outDc.hp (v) * outLevel;
    }

private:
    inline float triode (float x) const noexcept
    {
        // asymmetric soft clip: grid conduction rounds off positive peaks earlier
        // (slopes match at zero, so there is no kink in the transfer curve)
        const float tb = std::tanh (bias);
        return x > 0.0f ? std::tanh (x + bias) - tb
                        : x / (1.0f - x * 0.85f) * (1.0f - tb * tb);
    }

    struct Stage { OnePole dc, lp; float gain = 1.0f; };

    double sr = 44100.0;
    int model = -1, numStages = 2;
    float preGain = 1.0f, bias = 0.2f, powerDrive = 1.0f, outLevel = 1.0f;
    OnePole inHp, outDc;
    Stage stages[4];
    ToneStack tone;
    Biquad bright, presence, depth;
    EnvFollower sagEnv;
};
} // namespace rig
