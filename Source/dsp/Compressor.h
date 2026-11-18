// Arroyo EQ - feed-forward compressor.
// Log-domain gain computer with soft knee and branching attack/release
// smoothing of the gain reduction (Giannoulis, Massberg & Reiss, JAES 2012).
#pragma once
#include "Svf.h"
#include <atomic>

namespace arroyo
{
struct CompSettings
{
    bool on = false;
    double thresholdDb = -18.0;
    double ratio = 4.0;
    double kneeDb = 6.0;
    double attackMs = 10.0;
    double releaseMs = 120.0;
    double makeupDb = 0.0;
    bool autoMakeup = false;
    double mix = 1.0;         // 0..1, parallel (dry/wet)
    double scHighPassHz = 20.0; // 20 Hz = effectively off
    bool rms = false;
    bool link = true;
};

class Compressor
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        rmsCoef = std::exp (-1.0 / (0.010 * fs));
        fadeCoef = std::exp (-1.0 / (0.010 * fs));
        reset();
    }

    void reset()
    {
        for (auto& s : sc)
            s.reset();
        env[0] = env[1] = 0.0;
        ms[0] = ms[1] = 0.0;
    }

    void setSettings (const CompSettings& s) { set = s; }

    // Static curve: output level (dB) for an input level (dB), before makeup.
    static double gainComputer (double x, double T, double R, double W)
    {
        const double over = x - T;
        if (2.0 * over < -W)
            return x;
        if (W > 0.0 && 2.0 * std::abs (over) <= W)
        {
            const double t = over + 0.5 * W;
            return x + (1.0 / R - 1.0) * t * t / (2.0 * W);
        }
        return T + over / R;
    }

    static double autoMakeupDb (const CompSettings& s)
    {
        return -0.5 * s.thresholdDb * (1.0 - 1.0 / s.ratio);
    }

    template <typename Sample>
    void process (Sample* const* channels, int numChannels, int numSamples)
    {
        numChannels = std::min (numChannels, 2);
        if (! set.on && wet <= 0.0)
        {
            reset();
            gainReductionDb.store (0.0f);
            return;
        }
        const double aA = std::exp (-1.0 / (std::max (set.attackMs, 0.01) * 0.001 * fs));
        const double aR = std::exp (-1.0 / (std::max (set.releaseMs, 0.1) * 0.001 * fs));
        const SvfCoeffs hp = makeSection (BandType::LowCut, fs, set.scHighPassHz, 0.0, 0.70710678);
        const bool useHp = set.scHighPassHz > 21.0;
        const double makeup = set.makeupDb + (set.autoMakeup ? autoMakeupDb (set) : 0.0);
        const double targetWet = set.on ? 1.0 : 0.0;
        double blockMaxGr = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            double x[2] = { 0.0, 0.0 }, lvl[2] = { 0.0, 0.0 };
            for (int ch = 0; ch < numChannels; ++ch)
            {
                x[ch] = (double) channels[ch][i];
                double d = useHp ? svfTick (hp, sc[ch], x[ch]) : x[ch];
                if (set.rms)
                {
                    ms[ch] = rmsCoef * ms[ch] + (1.0 - rmsCoef) * d * d;
                    lvl[ch] = std::sqrt (ms[ch]);
                }
                else
                    lvl[ch] = std::abs (d);
            }

            const int numDet = set.link ? 1 : numChannels;
            if (set.link && numChannels == 2)
                lvl[0] = set.rms ? std::sqrt (0.5 * (lvl[0] * lvl[0] + lvl[1] * lvl[1]))
                                 : std::max (lvl[0], lvl[1]);

            double gain[2];
            for (int d = 0; d < numDet; ++d)
            {
                const double xdb = 20.0 * std::log10 (lvl[d] + 1e-12);
                const double gr = xdb - gainComputer (xdb, set.thresholdDb, set.ratio, set.kneeDb);
                env[d] = gr > env[d] ? aA * env[d] + (1.0 - aA) * gr
                                     : aR * env[d] + (1.0 - aR) * gr;
                if (env[d] < 1e-9) env[d] = 0.0;
                gain[d] = std::pow (10.0, (makeup - env[d]) / 20.0);
                blockMaxGr = std::max (blockMaxGr, env[d] * wet);
            }
            if (numDet == 1)
                gain[1] = gain[0];

            wet = targetWet + (wet - targetWet) * fadeCoef;
            if (std::abs (wet - targetWet) < 1e-6) wet = targetWet;
            const double m = set.mix * wet;
            for (int ch = 0; ch < numChannels; ++ch)
                channels[ch][i] = (Sample) (x[ch] + m * (x[ch] * gain[ch] - x[ch]));
        }
        gainReductionDb.store ((float) blockMaxGr);
    }

    std::atomic<float> gainReductionDb { 0.0f };

private:
    double fs = 48000.0, rmsCoef = 0.0, fadeCoef = 0.0, wet = 0.0;
    CompSettings set;
    SvfState sc[2];
    double env[2] { 0.0, 0.0 }, ms[2] { 0.0, 0.0 };
};
} // namespace arroyo
