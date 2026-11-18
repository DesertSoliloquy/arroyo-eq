// Arroyo EQ - 16-band equalizer with click-free parameter changes.
#pragma once
#include "Svf.h"
#include <array>

namespace arroyo
{
constexpr int kNumBands = 16;
constexpr int kMaxChannels = 2;
constexpr int kSubBlock = 16; // coefficients are refreshed every 16 samples

class EqBand
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        smoothCoef = std::exp (-double (kSubBlock) / (0.02 * fs)); // ~20 ms glide
        fadeStep = double (kSubBlock) / (0.015 * fs);              // ~15 ms fades
        snap = true;
        reset();
    }

    void reset()
    {
        for (auto& ch : state)
            for (auto& s : ch)
                s.reset();
    }

    void setTarget (const BandSpec& spec) { target = spec; }

    bool isActive() const { return amount > 0.0 || target.on; }

    // Call once per sub-block before processing samples.
    void advance()
    {
        const bool structural = target.type != running.type
                             || (typeHasSlope (target.type) && target.slopeIndex != running.slopeIndex);

        if (snap)
        {
            running = target;
            curLogF = std::log (target.freq);
            curGain = target.gainDb;
            curLogQ = std::log (target.q);
            amount = target.on ? 1.0 : 0.0;
            snap = false;
            rebuild();
            return;
        }

        // Fade out, switch filter structure, fade back in.
        double wantAmount = target.on ? 1.0 : 0.0;
        if (structural)
        {
            if (amount <= 0.0)
            {
                running.type = target.type;
                running.slopeIndex = target.slopeIndex;
                curLogF = std::log (target.freq);
                curGain = target.gainDb;
                curLogQ = std::log (target.q);
                reset();
                needRebuild = true;
            }
            else
                wantAmount = 0.0;
        }

        if (amount < wantAmount) amount = std::min (wantAmount, amount + fadeStep);
        else if (amount > wantAmount) amount = std::max (wantAmount, amount - fadeStep);

        const double tf = std::log (target.freq), tq = std::log (target.q), tg = target.gainDb;
        const bool moving = std::abs (tf - curLogF) > 1e-6 || std::abs (tq - curLogQ) > 1e-6
                         || std::abs (tg - curGain) > 1e-6;
        if (moving)
        {
            curLogF = tf + (curLogF - tf) * smoothCoef;
            curLogQ = tq + (curLogQ - tq) * smoothCoef;
            curGain = tg + (curGain - tg) * smoothCoef;
        }
        if (moving || needRebuild)
            rebuild();

        if (amount <= 0.0 && ! target.on && ! structural)
            reset(); // idle band starts clean next time
    }

    inline double process (int ch, double x)
    {
        double y = x;
        for (int i = 0; i < numSections; ++i)
            y = svfTick (coeffs[i], state[ch][i], y);
        return amount >= 1.0 ? y : x + amount * (y - x);
    }

    bool processing() const { return amount > 0.0; }

private:
    void rebuild()
    {
        BandSpec s = running;
        s.freq = std::exp (curLogF);
        s.q = std::exp (curLogQ);
        s.gainDb = curGain;
        const int prev = numSections;
        numSections = makeBand (s, fs, coeffs);
        if (numSections > prev)
            for (auto& ch : state)
                for (int i = prev; i < numSections; ++i)
                    ch[i].reset();
        needRebuild = false;
    }

    double fs = 48000.0, smoothCoef = 0.0, fadeStep = 0.01;
    BandSpec target, running;
    double curLogF = std::log (1000.0), curGain = 0.0, curLogQ = std::log (0.707), amount = 0.0;
    bool snap = true, needRebuild = true;
    SvfCoeffs coeffs[kMaxSections];
    int numSections = 1;
    SvfState state[kMaxChannels][kMaxSections];
};

class Equalizer
{
public:
    void prepare (double sampleRate)
    {
        for (auto& b : bands)
            b.prepare (sampleRate);
    }

    void reset()
    {
        for (auto& b : bands)
            b.reset();
    }

    void setBand (int i, const BandSpec& s) { bands[(size_t) i].setTarget (s); }

    // channels: array of pointers, numChannels 1 or 2
    template <typename Sample>
    void process (Sample* const* channels, int numChannels, int numSamples)
    {
        numChannels = std::min (numChannels, kMaxChannels);
        for (int start = 0; start < numSamples; start += kSubBlock)
        {
            const int n = std::min (kSubBlock, numSamples - start);
            int active[kNumBands];
            int numActive = 0;
            for (int b = 0; b < kNumBands; ++b)
            {
                bands[(size_t) b].advance();
                if (bands[(size_t) b].processing())
                    active[numActive++] = b;
            }
            if (numActive == 0)
                continue;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                Sample* d = channels[ch] + start;
                for (int i = 0; i < n; ++i)
                {
                    double x = (double) d[i];
                    for (int k = 0; k < numActive; ++k)
                        x = bands[(size_t) active[k]].process (ch, x);
                    d[i] = (Sample) x;
                }
            }
        }
    }

private:
    std::array<EqBand, kNumBands> bands;
};
} // namespace arroyo
