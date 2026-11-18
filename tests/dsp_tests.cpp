// Standalone DSP tests for Arroyo EQ (no JUCE needed).
// Build: g++ -O2 -std=c++17 -I../Source tests/dsp_tests.cpp -o dsp_tests
#include "dsp/Equalizer.h"
#include "dsp/Compressor.h"
#include <cstdio>
#include <vector>
#include <random>

using namespace arroyo;
static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); } } while (0)

static const char* typeName (BandType t)
{
    static const char* n[] = { "Bell", "LowShelf", "HighShelf", "LowCut", "HighCut", "Notch", "BandPass" };
    return n[(int) t];
}

// Measured magnitude (dB) of a band, from its impulse response, at f.
static double measuredDb (const std::vector<double>& ir, double fs, double f)
{
    std::complex<double> acc = 0.0;
    const double w = 2.0 * kPi * f / fs;
    for (size_t n = 0; n < ir.size(); ++n)
        acc += ir[n] * std::exp (std::complex<double> (0.0, -w * (double) n));
    return 20.0 * std::log10 (std::abs (acc) + 1e-30);
}

static std::vector<double> impulseResponse (const BandSpec& spec, double fs, int len)
{
    Equalizer eq;
    eq.prepare (fs);
    for (int b = 0; b < kNumBands; ++b)
        eq.setBand (b, BandSpec {});
    eq.setBand (5, spec);
    std::vector<double> buf ((size_t) len, 0.0);
    buf[0] = 1.0;
    double* ch[1] = { buf.data() };
    eq.process (ch, 1, len);
    return buf;
}

static void testResponses()
{
    std::mt19937 rng (42);
    std::uniform_real_distribution<double> u (0.0, 1.0);
    double worst = 0.0;
    int cases = 0;
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int t = 0; t < kNumBandTypes; ++t)
            for (int trial = 0; trial < 6; ++trial)
            {
                BandSpec s;
                s.on = true;
                s.type = (BandType) t;
                s.freq = 30.0 * std::pow (600.0, u (rng));            // 30 Hz .. 18 kHz
                s.gainDb = -24.0 + 48.0 * u (rng);
                s.q = 0.3 * std::pow (20.0, u (rng));                 // 0.3 .. 6
                s.slopeIndex = trial % 4;
                const int len = 1 << 16;
                auto ir = impulseResponse (s, fs, len);
                // the tail must have decayed, otherwise the measurement is invalid
                double tail = 0;
                for (int i = len - 256; i < len; ++i) tail = std::max (tail, std::abs (ir[(size_t) i]));
                CHECK (tail < 1e-7, "IR not decayed (%s f=%.1f q=%.2f) tail=%g", typeName (s.type), s.freq, s.q, tail);
                for (int k = 0; k < 60; ++k)
                {
                    const double f = 20.0 * std::pow (1000.0, k / 59.0);
                    if (f > 0.49 * fs) break;
                    const double theory = 20.0 * std::log10 (std::abs (bandResponse (s, fs, f)) + 1e-30);
                    if (theory < -80.0) continue;
                    const double meas = measuredDb (ir, fs, f);
                    const double err = std::abs (theory - meas);
                    worst = std::max (worst, err);
                    CHECK (err < 0.01, "%s fs=%.0f f0=%.1f g=%.1f q=%.2f slope=%d at %.1f Hz: display %.3f dB, measured %.3f dB",
                           typeName (s.type), fs, s.freq, s.gainDb, s.q, s.slopeIndex, f, theory, meas);
                }
                ++cases;
            }
    std::printf ("Response match: %d random bands x 60 freqs, worst display-vs-measured error %.5f dB\n", cases, worst);
}

// Compare the bell with the RBJ Audio EQ Cookbook peaking filter (sanity check
// that the curve is the standard one).
static void testAgainstCookbook()
{
    double worst = 0.0;
    for (double f0 : { 50.0, 400.0, 3000.0, 12000.0 })
        for (double g : { -12.0, 6.0, 18.0 })
            for (double q : { 0.5, 1.0, 4.0 })
            {
                const double fs = 48000.0, A = std::pow (10.0, g / 40.0), w0 = 2 * kPi * f0 / fs;
                const double alpha = std::sin (w0) / (2 * q);
                const double b0 = 1 + alpha * A, b1 = -2 * std::cos (w0), b2 = 1 - alpha * A;
                const double a0 = 1 + alpha / A, a1 = -2 * std::cos (w0), a2 = 1 - alpha / A;
                BandSpec s; s.on = true; s.type = BandType::Bell; s.freq = f0; s.gainDb = g; s.q = q;
                for (double f = 20; f < 20000; f *= 1.1)
                {
                    const auto z = std::exp (std::complex<double> (0, -2 * kPi * f / fs));
                    const auto h = (b0 + b1 * z + b2 * z * z) / (a0 + a1 * z + a2 * z * z);
                    const double err = std::abs (20 * std::log10 (std::abs (h)) - 20 * std::log10 (std::abs (bandResponse (s, fs, f))));
                    worst = std::max (worst, err);
                }
            }
    CHECK (worst < 1e-6, "Bell differs from RBJ cookbook by %g dB", worst);
    std::printf ("Bell vs RBJ cookbook peaking EQ: worst difference %.2e dB\n", worst);
}

static void testSlopes()
{
    const double fs = 96000.0;
    for (int s = 0; s < 4; ++s)
    {
        BandSpec b; b.on = true; b.type = BandType::LowCut; b.freq = 1000.0; b.q = 0.70710678; b.slopeIndex = s;
        const int N = 2 * (s + 1);
        const double at = 20 * std::log10 (std::abs (bandResponse (b, fs, 250.0)));  // two octaves below
        const double ideal = -10 * std::log10 (1 + std::pow (4.0, 2 * N));
        const double atFc = 20 * std::log10 (std::abs (bandResponse (b, fs, 1000.0)));
        CHECK (std::abs (at - ideal) < 0.2, "slope %d dB/oct: %.2f dB vs ideal %.2f", 6 * N, at, ideal);
        CHECK (std::abs (atFc + 3.01) < 0.05, "slope %d: -3 dB point off (%.2f)", 6 * N, atFc);
        std::printf ("Low cut %2d dB/oct: %.1f dB two octaves below (Butterworth ideal %.1f), %.2f dB at cutoff\n",
                     6 * N, at, ideal, atFc);
    }
}

static double runSine (Compressor& c, double fs, double ampDb, double seconds, double freq = 1000.0)
{
    const int n = (int) (seconds * fs);
    std::vector<double> l ((size_t) n), r ((size_t) n);
    const double a = std::pow (10.0, ampDb / 20.0);
    for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = a * std::sin (2 * kPi * freq * i / fs);
    double* ch[2] = { l.data(), r.data() };
    c.process (ch, 2, n);
    double peak = 0;
    for (int i = n - (int) (0.1 * fs); i < n; ++i) peak = std::max (peak, std::abs (l[(size_t) i]));
    return 20 * std::log10 (peak);
}

static void testCompressorStatic()
{
    const double fs = 48000.0;
    // Long release so the peak detector holds steady on a sine (with a fast
    // release the detector ripples between peaks and GR is a little lower,
    // exactly as on a hardware peak compressor).
    CompSettings s; s.on = true; s.thresholdDb = -20; s.ratio = 4; s.kneeDb = 6; s.attackMs = 1; s.releaseMs = 2000;
    double worst = 0;
    for (double in = -40; in <= 0; in += 4)
    {
        Compressor c; c.prepare (fs); c.setSettings (s);
        const double out = runSine (c, fs, in, 3.0);
        const double expect = Compressor::gainComputer (in, s.thresholdDb, s.ratio, s.kneeDb);
        // a peak detector on a sine ripples slightly; allow 0.3 dB
        worst = std::max (worst, std::abs (out - expect));
        CHECK (std::abs (out - expect) < 0.05, "static curve at %.0f dBFS: got %.2f expected %.2f", in, out, expect);
    }
    std::printf ("Compressor static curve (T=-20, 4:1, 6 dB knee): worst error %.3f dB\n", worst);
}

static void testCompressorTiming()
{
    const double fs = 48000.0;
    for (double attack : { 5.0, 30.0 })
    {
        CompSettings s; s.on = true; s.thresholdDb = -30; s.ratio = 20; s.kneeDb = 0; s.attackMs = attack; s.releaseMs = 200;
        Compressor c; c.prepare (fs); c.setSettings (s);
        // DC step: -60 dBFS -> -10 dBFS. Final GR = 20 * (1 - 1/20) = 19 dB
        const int n = (int) (0.5 * fs);
        std::vector<double> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = (i < n / 10 ? 0.001 : 0.316227766);
        double* ch[2] = { l.data(), r.data() };
        c.process (ch, 2, n);
        const double finalGr = 19.0;
        int t63 = -1;
        for (int i = n / 10; i < n; ++i)
        {
            const double gr = -10.0 - 20 * std::log10 (std::abs (l[(size_t) i]));
            if (gr >= 0.632 * finalGr) { t63 = i - n / 10; break; }
        }
        const double ms = 1000.0 * t63 / fs;
        CHECK (std::abs (ms - attack) / attack < 0.05, "attack %.1f ms measured %.2f ms", attack, ms);
        std::printf ("Attack set %.0f ms -> measured time constant %.2f ms\n", attack, ms);
    }
}

static void testStability()
{
    const double fs = 44100.0;
    std::mt19937 rng (7);
    std::uniform_real_distribution<double> u (0.0, 1.0);
    std::normal_distribution<double> nrm (0.0, 0.25);
    Equalizer eq; eq.prepare (fs);
    Compressor comp; comp.prepare (fs);
    const int block = 512;
    std::vector<float> l (block), r (block);
    float* ch[2] = { l.data(), r.data() };
    double peak = 0;
    bool bad = false;
    const int blocks = (int) (120.0 * fs / block);
    for (int b = 0; b < blocks; ++b)
    {
        // random automation of every band, every ~20 ms
        if (b % 2 == 0)
            for (int k = 0; k < kNumBands; ++k)
            {
                BandSpec s;
                s.on = u (rng) < 0.7;
                s.type = (BandType) (int) (u (rng) * 6.999);
                s.freq = 10.0 * std::pow (2200.0, u (rng));
                s.gainDb = -24 + 48 * u (rng);
                s.q = 0.1 * std::pow (180.0, u (rng));
                s.slopeIndex = (int) (u (rng) * 3.999);
                eq.setBand (k, s);
            }
        CompSettings cs; cs.on = u (rng) < 0.8; cs.thresholdDb = -60 * u (rng); cs.ratio = 1 + 19 * u (rng);
        cs.attackMs = 0.1 + 200 * u (rng); cs.releaseMs = 5 + 2000 * u (rng); cs.kneeDb = 24 * u (rng);
        cs.rms = u (rng) < 0.5; cs.link = u (rng) < 0.5; cs.scHighPassHz = 20 + 480 * u (rng);
        comp.setSettings (cs);
        for (int i = 0; i < block; ++i) { l[(size_t) i] = (float) nrm (rng); r[(size_t) i] = (float) nrm (rng); }
        eq.process (ch, 2, block);
        comp.process (ch, 2, block);
        for (int i = 0; i < block; ++i)
        {
            if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) bad = true;
            peak = std::max (peak, (double) std::abs (l[(size_t) i]));
        }
    }
    // 16 bands at up to +24 dB each could legitimately reach very high gain; the
    // point here is that nothing blows up (NaN/Inf or runaway growth).
    CHECK (! bad, "NaN/Inf under random automation");
    CHECK (peak < 1e9, "runaway output %g", peak);
    std::printf ("Stability: 120 s of noise with random automation every ~23 ms: %s (peak %.1f dBFS)\n",
                 bad ? "NaN/Inf!" : "no NaN/Inf", 20 * std::log10 (peak));
}

static void testBypassTransparency()
{
    // All bands off => bit-transparent (no processing at all)
    Equalizer eq; eq.prepare (48000.0);
    for (int b = 0; b < kNumBands; ++b) eq.setBand (b, BandSpec {});
    std::vector<float> x (4096), y;
    std::mt19937 rng (1); std::normal_distribution<float> n (0, 0.3f);
    for (auto& v : x) v = n (rng);
    y = x;
    float* ch[1] = { y.data() };
    eq.process (ch, 1, 4096);
    CHECK (x == y, "flat EQ is not bit-transparent");
    std::printf ("All bands off: output is bit-identical to input\n");
}

int main()
{
    testResponses();
    testAgainstCookbook();
    testSlopes();
    testCompressorStatic();
    testCompressorTiming();
    testStability();
    testBypassTransparency();
    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
