// Gain-staging & aliasing report: how loud does the plugin get with a hot guitar,
// how often the output safety stage works, and how much inharmonic (aliasing) junk distortion makes.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"

using namespace juce;

static AudioBuffer<float> guitar (double sr, float peakDb)
{
    AudioBuffer<float> out (2, (int) (sr * 4.0));
    out.clear();
    Random rnd (42);
    const float notes[] = { 82.41f, 110.0f, 146.83f, 196.0f, 246.94f, 329.63f };
    auto pluck = [&] (float f, int start, int len)
    {
        const int period = (int) (sr / f);
        std::vector<float> ring ((size_t) period);
        for (auto& v : ring) v = rnd.nextFloat() * 2.0f - 1.0f;
        int idx = 0;
        for (int i = 0; i < len && start + i < out.getNumSamples(); ++i)
        {
            const int nxt = (idx + 1) % period;
            const float s = ring[(size_t) idx];
            ring[(size_t) idx] = 0.4985f * (ring[(size_t) idx] + ring[(size_t) nxt]);
            idx = nxt;
            out.addSample (0, start + i, s);
        }
    };
    for (int n = 0; n < 6; ++n) pluck (notes[n], (int) (sr * 0.3 * n), (int) (sr * 1.0));
    for (int n = 0; n < 6; ++n) pluck (notes[n], (int) (sr * (2.0 + 0.01 * n)), (int) (sr * 1.8));
    out.applyGain (0, 0, out.getNumSamples(), rig::dbToGain (peakDb) / out.getMagnitude (0, 0, out.getNumSamples()));
    out.copyFrom (1, 0, out, 0, 0, out.getNumSamples());
    return out;
}

struct R { float peakDb, rmsDb, hotPct; };

static R run (Rig74Processor& p, const AudioBuffer<float>& in)
{
    AudioBuffer<float> b (in);
    MidiBuffer m;
    for (int pass = 0; pass < 2; ++pass)
    {
        b.makeCopyOf (in);
        for (int s = 0; s < b.getNumSamples(); s += 512)
        {
            AudioBuffer<float> v (b.getArrayOfWritePointers(), 2, s, std::min (512, b.getNumSamples() - s));
            p.processBlock (v, m);
        }
    }
    double sum = 0; float pk = 0; int hot = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float v = b.getSample (0, i);
        sum += v * v; pk = std::max (pk, std::abs (v));
        if (std::abs (v) > 0.89f) ++hot;
    }
    return { rig::gainToDb (pk), rig::gainToDb ((float) std::sqrt (sum / b.getNumSamples())), 100.0f * hot / b.getNumSamples() };
}

static void set (Rig74Processor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

/** Ratio of energy NOT on harmonics of f0 (aliasing / intermod) to energy on harmonics, in dB. */
static float aliasing (Rig74Processor& p, double sr, float f0)
{
    const int n = 1 << 15;
    AudioBuffer<float> b (2, n * 2);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float v = 0.4f * (float) std::sin (2.0 * MathConstants<double>::pi * f0 * i / sr);
        b.setSample (0, i, v); b.setSample (1, i, v);
    }
    MidiBuffer m;
    for (int s = 0; s < b.getNumSamples(); s += 512)
    {
        AudioBuffer<float> v (b.getArrayOfWritePointers(), 2, s, 512);
        p.processBlock (v, m);
    }
    dsp::FFT fft (15);
    std::vector<float> data ((size_t) n * 2, 0.0f);
    for (int i = 0; i < n; ++i)
        data[(size_t) i] = b.getSample (0, n + i) * (0.5f - 0.5f * std::cos (2.0f * MathConstants<float>::pi * i / (n - 1)));
    fft.performFrequencyOnlyForwardTransform (data.data());
    double harm = 0, junk = 0;
    for (int k = 4; k < n / 2; ++k)
    {
        const double f = k * sr / n;
        const double ratio = f / f0;
        const double e = (double) data[(size_t) k] * data[(size_t) k];
        if (std::abs (ratio - std::round (ratio)) * f0 < 6.0 * sr / n) harm += e; else if (f < 10000.0) junk += e;   // audible band only
    }
    return (float) (10.0 * std::log10 (junk / std::max (harm, 1e-20)));
}

int main()
{
    ScopedJuceInitialiser_GUI gui;
    const double sr = 48000.0;
    Rig74Processor p;
    p.setPlayConfigDetails (2, 2, sr, 512);
    p.prepareToPlay (sr, 512);

    std::printf ("== Presets vs guitar input level (peak dBFS / rms dBFS / %% samples > -1 dBFS) ==\n");
    std::printf ("%-20s %22s %22s %22s\n", "", "in -12 dBFS", "in -6 dBFS", "in -1 dBFS");
    const auto g12 = guitar (sr, -12.0f), g6 = guitar (sr, -6.0f), g1 = guitar (sr, -1.0f);
    for (int i = 0; i < p.getNumPrograms(); ++i)
    {
        p.setCurrentProgram (i);
        const auto a = run (p, g12), b = run (p, g6), c = run (p, g1);
        std::printf ("%-20s %6.1f %6.1f %6.1f%%  %6.1f %6.1f %6.1f%%  %6.1f %6.1f %6.1f%%\n", p.getProgramName (i).toRawUTF8(),
                     a.peakDb, a.rmsDb, a.hotPct, b.peakDb, b.rmsDb, b.hotPct, c.peakDb, c.rmsDb, c.hotPct);
    }

    std::printf ("\n== Drive x Amp matrix at -6 dBFS input (rms / %%hot) ==\n");
    const char* types[] = { "OD", "DIST", "FUZZ" };
    for (int model = 0; model < 4; ++model)
    {
        std::printf ("amp %d: ", model);
        for (int t = -1; t < 3; ++t)
            for (float dg : { 0.5f, 1.0f })
                for (float ag : { 5.0f, 10.0f })
                {
                    if (t < 0 && dg > 0.6f) continue;
                    p.setCurrentProgram (0);
                    set (p, "amp_model", (float) model); set (p, "amp_gain", ag);
                    if (t >= 0) { set (p, "drv_on", 1); set (p, "drv_type", (float) t); set (p, "drv_gain", dg); }
                    const auto r = run (p, g6);
                    std::printf (" [%s%s g%.0f] %5.1f/%4.1f%%", t < 0 ? "--" : types[t], t < 0 ? "" : (dg > 0.6f ? "max" : "mid"), ag, r.rmsDb, r.hotPct);
                }
        std::printf ("\n");
    }

    {
        // CPU: worst case (Hot Rod + drive + everything on), 60 s of audio
        p.setCurrentProgram (11);
        for (const char* id : { "drv_on", "cho_on", "pha_on", "dly_on", "rev_on" }) set (p, id, 1);
        AudioBuffer<float> b (2, 512);
        MidiBuffer m;
        const int blocks = (int) (60.0 * sr / 512);
        const auto t0 = Time::getMillisecondCounterHiRes();
        for (int i = 0; i < blocks; ++i)
        {
            for (int k = 0; k < 512; ++k) { const float v = 0.2f * (float) std::sin (i * 512 + k); b.setSample (0, k, v); b.setSample (1, k, v); }
            p.processBlock (b, m);
        }
        const double ms = Time::getMillisecondCounterHiRes() - t0;
        std::printf ("\nCPU: 60 s of audio processed in %.2f s  ->  %.1f %% of one core\n", ms / 1000.0, ms / 600.0);
    }

    std::printf ("\n== Diagnostic: which block makes inharmonic junk (f0 659 Hz) ==\n");
    struct Case { const char* name; int amp; int drv; float ag; };
    for (auto c : { Case { "all off", 0, -1, 0 }, Case { "amp lead g8", 1, -1, 8 }, Case { "amp lead g2", 1, -1, 2 },
                    Case { "OD only", 0, 0, 0 }, Case { "DIST only", 0, 1, 0 }, Case { "amp clean g5", 2, -1, 5 } })
    {
        p.setCurrentProgram (0);
        set (p, "gate_on", 0); set (p, "cab_on", 0);
        set (p, "amp_on", c.amp > 0 ? 1.0f : 0.0f);
        set (p, "amp_model", c.amp == 2 ? 0.0f : 2.0f); set (p, "amp_gain", c.ag);
        if (c.drv >= 0) { set (p, "drv_on", 1); set (p, "drv_type", (float) c.drv); set (p, "drv_gain", 0.9f); }
        std::printf ("%-14s %6.1f dB\n", c.name, aliasing (p, sr, 659.25f));
    }

    std::printf ("\n== Aliasing (inharmonic energy vs harmonic, dB; lower = cleaner) ==\n");
    for (float f0 : { 659.25f, 1318.5f })
        for (int t = 0; t < 3; ++t)
        {
            p.setCurrentProgram (0);
            set (p, "gate_on", 0); set (p, "cab_on", 0);
            set (p, "amp_model", 2); set (p, "amp_gain", 8.0f);
            set (p, "drv_on", 1); set (p, "drv_type", (float) t); set (p, "drv_gain", 0.9f);
            std::printf ("f0 %7.1f Hz %-5s : %6.1f dB\n", f0, types[t], aliasing (p, sr, f0));
        }
    return 0;
}
