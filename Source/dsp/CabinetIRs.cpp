#include "Cabinet.h"

namespace rig
{
namespace
{
struct Band { enum Kind { HP, LP, Peak, LowShelf, HighShelf } kind; double f, q, db; };

struct CabDesign
{
    std::vector<Band> bands;
    double reflectionMs;   // early reflection from an open back / room
    double reflectionGain;
    double coneBreakupHz;  // small comb of cone resonances
};

CabDesign design (int type)
{
    switch (type)
    {
        case 0: // 1x12 open back: bright, airy, loose low end
            return { { { Band::HP, 85, 0.9, 0 }, { Band::Peak, 115, 1.2, 3.0 }, { Band::Peak, 700, 1.0, -3.0 },
                       { Band::Peak, 2400, 1.6, 4.5 }, { Band::Peak, 4200, 2.5, 2.0 },
                       { Band::LP, 5600, 0.75, 0 }, { Band::LP, 6800, 0.6, 0 } }, 1.35, 0.22, 3100 };
        case 1: // 2x12 combo
            return { { { Band::HP, 78, 1.0, 0 }, { Band::Peak, 125, 1.1, 4.0 }, { Band::Peak, 480, 1.1, -2.5 },
                       { Band::Peak, 1800, 1.4, 3.0 }, { Band::Peak, 3400, 2.0, 3.5 },
                       { Band::LP, 5000, 0.8, 0 }, { Band::LP, 6000, 0.6, 0 } }, 1.05, 0.14, 2600 };
        case 2: // 4x12 closed back, vintage speakers: tight thump, strong upper mids
            return { { { Band::HP, 72, 1.2, 0 }, { Band::Peak, 105, 1.4, 6.0 }, { Band::Peak, 420, 1.0, -4.0 },
                       { Band::Peak, 1250, 1.2, 2.0 }, { Band::Peak, 2800, 1.8, 5.0 }, { Band::Peak, 4700, 3.0, -3.0 },
                       { Band::LP, 4600, 0.85, 0 }, { Band::LP, 5600, 0.6, 0 } }, 0.0, 0.0, 2300 };
        case 3: // 4x10 tweed: round, scooped highs, glassy
            return { { { Band::HP, 62, 0.9, 0 }, { Band::Peak, 150, 1.0, 3.0 }, { Band::Peak, 1000, 1.0, 2.0 },
                       { Band::Peak, 3000, 1.5, 3.0 }, { Band::LP, 6000, 0.8, 0 }, { Band::LP, 7400, 0.6, 0 } }, 1.6, 0.18, 3600 };
        default: // 1x15 jazz: warm and dark
            return { { { Band::HP, 55, 1.0, 0 }, { Band::Peak, 90, 1.2, 5.0 }, { Band::Peak, 1500, 1.0, -2.0 },
                       { Band::Peak, 2600, 2.0, 1.5 }, { Band::LP, 3400, 0.8, 0 }, { Band::LP, 4300, 0.6, 0 } }, 1.9, 0.12, 1900 };
    }
}
} // namespace

juce::AudioBuffer<float> makeCabinetIR (int type, double sr)
{
    const int len = juce::nextPowerOfTwo ((int) (sr * 0.045));
    std::vector<double> ir ((size_t) len, 0.0);
    ir[0] = 1.0;

    const auto d = design (type);

    // early reflection (delayed, darker copy)
    if (d.reflectionGain > 0.0)
    {
        const int refl = (int) (d.reflectionMs * 0.001 * sr);
        if (refl > 0 && refl < len) ir[(size_t) refl] += d.reflectionGain;
    }

    // run the impulse through the speaker-shaping filter cascade
    for (const auto& b : d.bands)
    {
        Biquad f;
        switch (b.kind)
        {
            case Band::HP:        f.makeHighPass (sr, b.f, b.q); break;
            case Band::LP:        f.makeLowPass (sr, b.f, b.q); break;
            case Band::Peak:      f.makePeak (sr, b.f, b.q, b.db); break;
            case Band::LowShelf:  f.makeLowShelf (sr, b.f, b.db); break;
            case Band::HighShelf: f.makeHighShelf (sr, b.f, b.db); break;
        }
        for (auto& s : ir) s = f.process ((float) s);
    }

    // cone break-up: a few narrow resonances/notches above the break-up frequency
    juce::Random rnd (1974 + type);
    for (int k = 0; k < 6; ++k)
    {
        Biquad f;
        const double freq = d.coneBreakupHz * (1.0 + 0.37 * k + 0.1 * rnd.nextDouble());
        f.makePeak (sr, std::min (freq, sr * 0.45), 6.0 + 4.0 * rnd.nextDouble(), (k % 2 == 0 ? -1.0 : 1.0) * (2.0 + 3.0 * rnd.nextDouble()));
        for (auto& s : ir) s = f.process ((float) s);
    }

    // fade-out window over the last 40 %
    const int fadeStart = (int) (len * 0.6);
    for (int i = fadeStart; i < len; ++i)
    {
        const double t = (double) (i - fadeStart) / (double) (len - fadeStart);
        ir[(size_t) i] *= 0.5 * (1.0 + std::cos (kPi * t));
    }

    juce::AudioBuffer<float> out (1, len);
    for (int i = 0; i < len; ++i) out.setSample (0, i, (float) ir[(size_t) i]);
    return out;
}
} // namespace rig
