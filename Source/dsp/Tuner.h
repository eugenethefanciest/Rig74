#pragma once

#include <vector>
#include <cmath>

namespace rig
{
/** YIN pitch detector, run on the message thread from the editor's timer. */
class PitchDetector
{
public:
    /** Returns the detected frequency in Hz, or 0 if no clear pitch. */
    static float detect (const float* x, int n, double sr)
    {
        const int w = n / 2;
        const int tauMin = (int) (sr / 1400.0);
        const int tauMax = std::min (w - 2, (int) (sr / 55.0));
        if (tauMax <= tauMin + 2) return 0.0f;

        double energy = 0.0;
        for (int i = 0; i < n; ++i) energy += (double) x[i] * x[i];
        if (energy / n < 1.0e-7) return 0.0f;   // too quiet

        static thread_local std::vector<float> d;
        d.assign ((size_t) tauMax + 2, 0.0f);

        for (int tau = 1; tau <= tauMax + 1; ++tau)
        {
            double sum = 0.0;
            for (int i = 0; i < w; ++i)
            {
                const double diff = (double) x[i] - x[i + tau];
                sum += diff * diff;
            }
            d[(size_t) tau] = (float) sum;
        }

        // cumulative mean normalised difference
        double running = 0.0;
        d[0] = 1.0f;
        for (int tau = 1; tau <= tauMax + 1; ++tau)
        {
            running += d[(size_t) tau];
            d[(size_t) tau] = running > 0.0 ? (float) (d[(size_t) tau] * tau / running) : 1.0f;
        }

        int best = -1;
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (d[(size_t) tau] < 0.15f)
            {
                while (tau + 1 <= tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0) return 0.0f;

        // parabolic interpolation around the minimum
        const float s0 = d[(size_t) best - 1], s1 = d[(size_t) best], s2 = d[(size_t) best + 1];
        const float denom = 2.0f * (2.0f * s1 - s2 - s0);
        const float shift = std::abs (denom) > 1.0e-9f ? (s2 - s0) / denom : 0.0f;
        return (float) (sr / ((double) best + shift));
    }
};
} // namespace rig
