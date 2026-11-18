#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Equalizer.h"
#include "dsp/Compressor.h"

namespace arroyo
{
// Lock-free mono sample tap for the spectrum analyzer.
class AnalyzerTap
{
public:
    AnalyzerTap() : fifo (kSize) { buffer.resize (kSize); }

    void push (const float* l, const float* r, int n)
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (n, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i)
            buffer[(size_t) (s1 + i)] = r ? 0.5f * (l[i] + r[i]) : l[i];
        for (int i = 0; i < n2; ++i)
            buffer[(size_t) (s2 + i)] = r ? 0.5f * (l[n1 + i] + r[n1 + i]) : l[n1 + i];
        fifo.finishedWrite (n1 + n2);
    }

    int pull (float* dest, int maxN)
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (maxN, s1, n1, s2, n2);
        std::copy (buffer.begin() + s1, buffer.begin() + s1 + n1, dest);
        std::copy (buffer.begin() + s2, buffer.begin() + s2 + n2, dest + n1);
        fifo.finishedRead (n1 + n2);
        return n1 + n2;
    }

private:
    static constexpr int kSize = 1 << 15;
    juce::AbstractFifo fifo;
    std::vector<float> buffer;
};

namespace ids
{
inline juce::String band (int b, const char* what) { return "b" + juce::String (b + 1) + "_" + what; }
}

const juce::StringArray& bandTypeNames();
const juce::StringArray& slopeNames();
} // namespace arroyo

class ArroyoProcessor : public juce::AudioProcessor
{
public:
    ArroyoProcessor();
    ~ArroyoProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Arroyo EQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    arroyo::BandSpec getBandSpec (int band) const;
    double getCurrentSampleRate() const { return currentRate.load(); }

    juce::AudioProcessorValueTreeState apvts;
    arroyo::AnalyzerTap preTap, postTap;
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<float>& gainReduction() { return comp.gainReductionDb; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    template <typename Sample>
    void process (juce::AudioBuffer<Sample>&);
    void pullParameters();

    arroyo::Equalizer eq;
    arroyo::Compressor comp;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> outGain { 1.0 };
    std::atomic<double> currentRate { 48000.0 };

    struct BandParamPtrs
    {
        std::atomic<float>* on;
        std::atomic<float>* type;
        std::atomic<float>* freq;
        std::atomic<float>* gain;
        std::atomic<float>* q;
        std::atomic<float>* slope;
    };
    std::array<BandParamPtrs, arroyo::kNumBands> bandParams {};
    std::atomic<float>* eqOn = nullptr;
    std::atomic<float>* outGainDb = nullptr;
    std::atomic<float>* compParams[13] {};
    bool compPre = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArroyoProcessor)
};
