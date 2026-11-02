// Offline test harness: renders guitar-like test audio through every preset,
// fuzzes all parameters for stability, checks the tuner, and saves GUI snapshots.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/dsp/Tuner.h"

using namespace juce;

static AudioBuffer<float> makeGuitar (double sr, double seconds)
{
    // Karplus-Strong plucks: E2 A2 D3 G3 B3 E4, a strum, then silence for the gate
    AudioBuffer<float> out (2, (int) (sr * seconds));
    out.clear();
    Random rnd (42);
    const float notes[] = { 82.41f, 110.0f, 146.83f, 196.0f, 246.94f, 329.63f };
    auto pluck = [&] (float freq, int start, float amp, int len)
    {
        const int period = (int) (sr / freq);
        std::vector<float> ring ((size_t) period);
        for (auto& v : ring) v = rnd.nextFloat() * 2.0f - 1.0f;
        int idx = 0;
        for (int i = 0; i < len && start + i < out.getNumSamples(); ++i)
        {
            const int nxt = (idx + 1) % period;
            const float v = 0.4985f * (ring[(size_t) idx] + ring[(size_t) nxt]);
            const float s = ring[(size_t) idx];
            ring[(size_t) idx] = v;
            idx = nxt;
            out.addSample (0, start + i, s * amp);
        }
    };
    for (int n = 0; n < 6; ++n) pluck (notes[n], (int) (sr * 0.35 * n), 0.32f, (int) (sr * 1.2));
    for (int n = 0; n < 6; ++n) pluck (notes[n], (int) (sr * (2.3 + 0.012 * n)), 0.22f, (int) (sr * 1.6));
    // a little hiss + 50 Hz hum like a real pickup
    for (int i = 0; i < out.getNumSamples(); ++i)
        out.addSample (0, i, (rnd.nextFloat() - 0.5f) * 0.0006f + 0.0008f * std::sin (2.0 * MathConstants<double>::pi * 50.0 * i / sr));
    out.copyFrom (1, 0, out, 0, 0, out.getNumSamples());
    return out;
}

struct Stats { float peak = 0, rms = 0, tailRms = 0; bool finite = true; };

static Stats render (Rig74Processor& p, const AudioBuffer<float>& in, AudioBuffer<float>* keep, int block = 512)
{
    AudioBuffer<float> buf (in);
    MidiBuffer midi;
    for (int s = 0; s < buf.getNumSamples(); s += block)
    {
        const int n = std::min (block, buf.getNumSamples() - s);
        AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, s, n);
        p.processBlock (view, midi);
    }
    Stats st;
    double sum = 0, tail = 0;
    const int tailStart = (int) (buf.getNumSamples() * 0.9);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const float v = buf.getSample (c, i);
            if (! std::isfinite (v)) st.finite = false;
            st.peak = std::max (st.peak, std::abs (v));
            sum += (double) v * v;
            if (i >= tailStart) tail += (double) v * v;
        }
    st.rms = (float) std::sqrt (sum / (2.0 * buf.getNumSamples()));
    st.tailRms = (float) std::sqrt (tail / (2.0 * (buf.getNumSamples() - tailStart)));
    if (keep) *keep = buf;
    return st;
}

static void writeWav (const File& f, const AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static void snapshot (Rig74Processor& p, const File& f)
{
    std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1600, 900);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    f.deleteFile();
    FileOutputStream os (f);
    PNGImageFormat().writeImageToStream (img, os);
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir = File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "test_out");
    outDir.createDirectory();
    int failures = 0;

    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        Rig74Processor p;
        p.setPlayConfigDetails (2, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        const auto guitar = makeGuitar (sr, 5.0);

        std::printf ("\n=== %.0f Hz (latency %d samples) ===\n", sr, p.getLatencySamples());
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            render (p, guitar, nullptr);   // settle smoothing / IR loading
            AudioBuffer<float> out;
            const auto s = render (p, guitar, &out);
            const bool ok = s.finite && s.peak <= 1.0f && s.rms > 0.003f;
            if (! ok) ++failures;
            std::printf ("%-20s peak %6.2f dBFS  rms %6.1f dBFS  tail %6.1f dBFS  %s\n", p.getProgramName (i).toRawUTF8(),
                         rig::gainToDb (s.peak), rig::gainToDb (s.rms), rig::gainToDb (s.tailRms), ok ? "OK" : "<-- FAIL");
            if (sr == 48000.0)
                writeWav (outDir.getChildFile (String (i).paddedLeft ('0', 2) + "_" + p.getProgramName (i).replaceCharacters (" '", "__") + ".wav"), out, sr);
        }
    }

    // ---- parameter fuzzing: random settings, odd block sizes, everything on ----
    {
        Rig74Processor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 256);
        p.prepareToPlay (48000.0, 256);
        const auto guitar = makeGuitar (48000.0, 1.5);
        Random rnd (7);
        int bad = 0;
        float worst = 0.0f;
        for (int iter = 0; iter < 120; ++iter)
        {
            for (auto* prm : p.getParameters())
                if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
                    rp->setValueNotifyingHost (rp->getParameterID() == "tuner" ? 0.0f : rnd.nextFloat());
            const int block = 1 + rnd.nextInt (600);   // includes blocks larger than prepared
            const auto s = render (p, guitar, nullptr, block);
            worst = std::max (worst, s.peak);
            if (! s.finite || s.peak > 1.0001f) ++bad;
        }
        std::printf ("\nFuzz: 120 random patches, %d unstable, worst peak %.2f dBFS\n", bad, rig::gainToDb (worst));
        failures += bad;
    }

    // ---- extreme feedback: everything maxed for 20 seconds ----
    {
        Rig74Processor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        for (const char* id : { "drv_on", "amp_on", "cab_on", "eq_on", "cho_on", "fla_on", "pha_on", "tre_on", "dly_on", "rev_on", "comp_on", "wah_on", "oct_on" })
            p.apvts.getParameter (id)->setValueNotifyingHost (1.0f);
        for (const char* id : { "dly_feedback", "fla_regen", "pha_feedback", "rev_decay", "amp_gain", "drv_gain", "amp_master", "dly_mix", "rev_mix" })
            p.apvts.getParameter (id)->setValueNotifyingHost (1.0f);
        AudioBuffer<float> longIn (2, 48000 * 20);
        longIn.clear();
        const auto g = makeGuitar (48000.0, 2.0);
        longIn.copyFrom (0, 0, g, 0, 0, g.getNumSamples());
        longIn.copyFrom (1, 0, g, 1, 0, g.getNumSamples());
        const auto s = render (p, longIn, nullptr);
        std::printf ("Max-feedback 20 s: peak %.2f dBFS, finite %s\n", rig::gainToDb (s.peak), s.finite ? "yes" : "NO");
        if (! s.finite) ++failures;
    }

    // ---- tuner accuracy ----
    {
        const double sr = 48000.0;
        std::vector<float> x (4096);
        for (float f : { 82.41f, 110.0f, 146.83f, 196.0f, 246.94f, 329.63f, 440.0f })
        {
            for (int i = 0; i < 4096; ++i)
                x[(size_t) i] = 0.3f * (float) (std::sin (2 * MathConstants<double>::pi * f * i / sr) + 0.5 * std::sin (4 * MathConstants<double>::pi * f * i / sr));
            const float d = rig::PitchDetector::detect (x.data(), 4096, sr);
            const float cents = 1200.0f * std::log2 (d / f);
            std::printf ("Tuner %7.2f Hz -> %7.2f Hz (%+.2f cents)\n", f, d, cents);
            if (std::abs (cents) > 3.0f) ++failures;
        }
    }

    // ---- tone stack sanity: frequency response at noon / extremes ----
    {
        rig::ToneStack ts;
        ts.prepare (192000.0);
        for (auto [b, m, t] : { std::tuple { 0.5, 0.5, 0.5 }, std::tuple { 1.0, 0.0, 1.0 }, std::tuple { 0.0, 1.0, 0.0 } })
        {
            ts.set (rig::ToneStack::british(), b, m, t);
            std::printf ("ToneStack b%.1f m%.1f t%.1f :", b, m, t);
            for (double f : { 60.0, 250.0, 700.0, 2000.0, 6000.0 })
            {
                ts.reset();
                double peak = 0;
                for (int i = 0; i < 192000; ++i)
                {
                    const float y = ts.process ((float) std::sin (2 * MathConstants<double>::pi * f * i / 192000.0));
                    if (i > 96000) peak = std::max (peak, (double) std::abs (y));
                }
                std::printf ("  %5.0fHz %6.1fdB", f, 20 * std::log10 (peak));
            }
            std::printf ("\n");
        }
    }

    // ---- GUI snapshots ----
    {
        Rig74Processor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        p.setCurrentProgram (3);
        snapshot (p, outDir.getChildFile ("gui.png"));
        p.apvts.getParameter ("tuner")->setValueNotifyingHost (1.0f);
        snapshot (p, outDir.getChildFile ("gui_tuner.png"));
        std::printf ("GUI snapshots written\n");
    }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
