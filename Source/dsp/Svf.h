// Arroyo EQ - state-variable filter building block.
// Trapezoidal-integrated SVF (A. Simper, Cytomic, "Solving the continuous SVF
// equations using trapezoidal integration"). Equivalent in steady state to the
// bilinear-transformed analog prototypes (i.e. the RBJ cookbook filters), but
// well behaved when its parameters change while audio is running.
#pragma once
#include <cmath>
#include <complex>
#include <algorithm>

namespace arroyo
{
constexpr double kPi = 3.14159265358979323846;

enum class BandType
{
    Bell = 0,
    LowShelf,
    HighShelf,
    LowCut,
    HighCut,
    Notch,
    BandPass
};
constexpr int kNumBandTypes = 7;

inline bool typeHasGain (BandType t)
{
    return t == BandType::Bell || t == BandType::LowShelf || t == BandType::HighShelf;
}

inline bool typeHasSlope (BandType t)
{
    return t == BandType::LowCut || t == BandType::HighCut;
}

struct SvfCoeffs
{
    double a1 = 1.0, a2 = 0.0, a3 = 0.0;
    double m0 = 1.0, m1 = 0.0, m2 = 0.0;
    double k = 1.0, g = 0.0;
};

struct SvfState
{
    double ic1 = 0.0, ic2 = 0.0;
    void reset() { ic1 = ic2 = 0.0; }
};

inline double clampFreq (double f, double fs)
{
    return std::clamp (f, 1.0, 0.4995 * fs);
}

// One second-order section.  q is the section's own Q.
inline SvfCoeffs makeSection (BandType t, double fs, double freq, double gainDb, double q)
{
    SvfCoeffs c;
    const double g0 = std::tan (kPi * clampFreq (freq, fs) / fs);
    const double A = std::pow (10.0, gainDb / 40.0);
    q = std::max (q, 0.025);

    switch (t)
    {
        case BandType::Bell:
            c.g = g0; c.k = 1.0 / (q * A);
            c.m0 = 1.0; c.m1 = c.k * (A * A - 1.0); c.m2 = 0.0;
            break;
        case BandType::LowShelf:
            c.g = g0 / std::sqrt (A); c.k = 1.0 / q;
            c.m0 = 1.0; c.m1 = c.k * (A - 1.0); c.m2 = A * A - 1.0;
            break;
        case BandType::HighShelf:
            c.g = g0 * std::sqrt (A); c.k = 1.0 / q;
            c.m0 = A * A; c.m1 = c.k * (1.0 - A) * A; c.m2 = 1.0 - A * A;
            break;
        case BandType::LowCut: // high-pass
            c.g = g0; c.k = 1.0 / q;
            c.m0 = 1.0; c.m1 = -c.k; c.m2 = -1.0;
            break;
        case BandType::HighCut: // low-pass
            c.g = g0; c.k = 1.0 / q;
            c.m0 = 0.0; c.m1 = 0.0; c.m2 = 1.0;
            break;
        case BandType::Notch:
            c.g = g0; c.k = 1.0 / q;
            c.m0 = 1.0; c.m1 = -c.k; c.m2 = 0.0;
            break;
        case BandType::BandPass: // unity gain at centre
            c.g = g0; c.k = 1.0 / q;
            c.m0 = 0.0; c.m1 = c.k; c.m2 = 0.0;
            break;
    }
    c.a1 = 1.0 / (1.0 + c.g * (c.g + c.k));
    c.a2 = c.g * c.a1;
    c.a3 = c.g * c.a2;
    return c;
}

inline double svfTick (const SvfCoeffs& c, SvfState& s, double v0)
{
    const double v3 = v0 - s.ic2;
    const double v1 = c.a1 * s.ic1 + c.a2 * v3;
    const double v2 = s.ic2 + c.a2 * s.ic1 + c.a3 * v3;
    s.ic1 = 2.0 * v1 - s.ic1;
    s.ic2 = 2.0 * v2 - s.ic2;
    return c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
}

// Exact frequency response of the digital section at frequency f (Hz).
inline std::complex<double> svfResponse (const SvfCoeffs& c, double fs, double f)
{
    const double w = 2.0 * kPi * clampFreq (f, fs) / fs;
    const std::complex<double> s (0.0, std::tan (0.5 * w) / c.g);
    const std::complex<double> den = s * s + c.k * s + 1.0;
    return c.m0 + c.m1 * s / den + c.m2 / den;
}

// ---------------------------------------------------------------------------
// A complete EQ band = 1..4 sections (cuts get Butterworth cascades).
// slopeIndex 0..3 -> 12, 24, 36, 48 dB/octave.
struct BandSpec
{
    bool on = false;
    BandType type = BandType::Bell;
    double freq = 1000.0;
    double gainDb = 0.0;
    double q = 0.707;
    int slopeIndex = 1;
};

constexpr int kMaxSections = 4;

inline int makeBand (const BandSpec& b, double fs, SvfCoeffs out[kMaxSections])
{
    if (! typeHasSlope (b.type))
    {
        out[0] = makeSection (b.type, fs, b.freq, b.gainDb, b.q);
        return 1;
    }
    const int order = 2 * (std::clamp (b.slopeIndex, 0, 3) + 1);
    const int n = order / 2;
    const double resonance = b.q / 0.70710678118654752;
    for (int i = 0; i < n; ++i)
    {
        double qi = 1.0 / (2.0 * std::cos ((2.0 * (i + 1) - 1.0) * kPi / (2.0 * order)));
        if (i == n - 1)
            qi *= resonance; // the highest-Q section carries the user's resonance
        out[i] = makeSection (b.type, fs, b.freq, 0.0, qi);
    }
    return n;
}

inline std::complex<double> bandResponse (const BandSpec& b, double fs, double f)
{
    SvfCoeffs c[kMaxSections];
    const int n = makeBand (b, fs, c);
    std::complex<double> h (1.0, 0.0);
    for (int i = 0; i < n; ++i)
        h *= svfResponse (c[i], fs, f);
    return h;
}
} // namespace arroyo
