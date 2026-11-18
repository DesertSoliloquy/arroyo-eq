#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace arroyo;

const juce::StringArray& arroyo::bandTypeNames()
{
    static const juce::StringArray names { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass" };
    return names;
}

const juce::StringArray& arroyo::slopeNames()
{
    static const juce::StringArray names { "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };
    return names;
}

namespace
{
juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float s, float e, float v) { return s * std::pow (e / s, v); },
             [] (float s, float e, float x) { return std::log (x / s) / std::log (e / s); },
             [] (float s, float e, float x) { return juce::jlimit (s, e, x); } };
}

juce::String hzText (float v, int)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                        : juce::String (v, v < 100.0f ? 1 : 0) + " Hz";
}

juce::String dbText (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; }
juce::String msText (float v, int) { return juce::String (v, v < 10.0f ? 1 : 0) + " ms"; }

const char* kCompIds[] = { "comp_on", "comp_thresh", "comp_ratio", "comp_knee", "comp_attack", "comp_release",
                           "comp_makeup", "comp_auto", "comp_mix", "comp_schpf", "comp_detector", "comp_position", "comp_link" };

// On/off switches are two-step choice parameters ("Off"/"On"): unlike
// juce::AudioParameterBool they snap a host's in-between value to a step, so a
// restored state always comes back exactly as it was saved.
std::unique_ptr<juce::AudioParameterChoice> onOff (const juce::String& id, const juce::String& name, bool on)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, juce::StringArray { "Off", "On" }, on ? 1 : 0);
}

float defaultBandFreq (int b)
{
    // spread across the spectrum: 30 Hz .. 16 kHz
    return (float) (30.0 * std::pow (16000.0 / 30.0, b / 15.0));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout ArroyoProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (onOff ("eq_on", "EQ On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "out_gain", 1 }, "Output Gain",
        NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    for (int b = 0; b < kNumBands; ++b)
    {
        const String n = "Band " + String (b + 1) + " ";
        const int defType = b == 0 ? (int) BandType::LowCut : (b == kNumBands - 1 ? (int) BandType::HighCut : (int) BandType::Bell);
        auto group = std::make_unique<AudioProcessorParameterGroup> ("band" + String (b + 1), "Band " + String (b + 1), "|");
        group->addChild (onOff (ids::band (b, "on"), n + "On", false));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ids::band (b, "type"), 1 }, n + "Type", bandTypeNames(), defType));
        group->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::band (b, "freq"), 1 }, n + "Frequency",
            logRange (10.0f, 22000.0f), defaultBandFreq (b), AudioParameterFloatAttributes().withStringFromValueFunction (hzText)));
        group->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::band (b, "gain"), 1 }, n + "Gain",
            NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
        group->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::band (b, "q"), 1 }, n + "Q",
            logRange (0.1f, 18.0f), 0.71f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); })));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ids::band (b, "slope"), 1 }, n + "Slope", slopeNames(), 1));
        layout.add (std::move (group));
    }

    auto comp = std::make_unique<AudioProcessorParameterGroup> ("comp", "Compressor", "|");
    comp->addChild (onOff ("comp_on", "Comp On", false));
    comp->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "comp_thresh", 1 }, "Threshold",
        NormalisableRange<float> (-60.0f, 0.0f, 0.01f), -18.0f, AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
    comp->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "comp_ratio", 1 }, "Ratio",
        NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.4f), 4.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, v < 10 ? 1 : 0) + ":1"; })));
    comp->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "comp_knee", 1 }, "Knee",
        NormalisableRange<float> (0.0f, 24.0f, 0.01f), 6.0f, AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
    comp->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "comp_attack", 1 }, "Attack",
        logRange (0.1f, 200.0f), 10.0f, AudioParameterFloatAttributes().withStringFromValueFunction (msText)));
    comp->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "comp_release", 1 }, "Release",
        logRange (5.0f, 2000.0f), 120.0f, AudioParameterFloatAttributes().withStringFromValueFunction (msText)));
    comp->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "comp_makeup", 1 }, "Makeup",
        NormalisableRange<float> (0.0f, 24.0f, 0.01f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
    comp->addChild (onOff ("comp_auto", "Auto Makeup", false));
    comp->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "comp_mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0) + " %"; })));
    comp->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "comp_schpf", 1 }, "Sidechain HPF",
        logRange (20.0f, 500.0f), 20.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v <= 20.5f ? juce::String ("Off") : hzText (v, 0); })));
    comp->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "comp_detector", 1 }, "Detector", StringArray { "Peak", "RMS" }, 0));
    comp->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "comp_position", 1 }, "Comp Position", StringArray { "After EQ", "Before EQ" }, 0));
    comp->addChild (onOff ("comp_link", "Stereo Link", true));
    layout.add (std::move (comp));
    return layout;
}

ArroyoProcessor::ArroyoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ARROYO", createLayout())
{
    for (int b = 0; b < kNumBands; ++b)
        bandParams[(size_t) b] = { apvts.getRawParameterValue (ids::band (b, "on")),
                                   apvts.getRawParameterValue (ids::band (b, "type")),
                                   apvts.getRawParameterValue (ids::band (b, "freq")),
                                   apvts.getRawParameterValue (ids::band (b, "gain")),
                                   apvts.getRawParameterValue (ids::band (b, "q")),
                                   apvts.getRawParameterValue (ids::band (b, "slope")) };
    eqOn = apvts.getRawParameterValue ("eq_on");
    outGainDb = apvts.getRawParameterValue ("out_gain");
    for (int i = 0; i < 13; ++i)
        compParams[i] = apvts.getRawParameterValue (kCompIds[i]);

    // GUI-only settings live in the state tree (not automatable)
    apvts.state.setProperty ("analyzer", 2, nullptr);  // 0 off, 1 pre, 2 post, 3 both
    apvts.state.setProperty ("range", 1, nullptr);     // 0 = 12 dB, 1 = 24 dB, 2 = 36 dB
    apvts.state.setProperty ("tilt", 2, nullptr);      // 0, 3, 4.5, 6 dB/oct
}

bool ArroyoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void ArroyoProcessor::prepareToPlay (double sampleRate, int)
{
    currentRate.store (sampleRate);
    eq.prepare (sampleRate);
    comp.prepare (sampleRate);
    outGain.reset (sampleRate, 0.03);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain ((double) outGainDb->load()));
    pullParameters();
}

BandSpec ArroyoProcessor::getBandSpec (int b) const
{
    const auto& p = bandParams[(size_t) b];
    BandSpec s;
    s.on = p.on->load() > 0.5f;
    s.type = (BandType) juce::jlimit (0, kNumBandTypes - 1, (int) p.type->load());
    s.freq = p.freq->load();
    s.gainDb = p.gain->load();
    s.q = p.q->load();
    s.slopeIndex = (int) p.slope->load();
    return s;
}

void ArroyoProcessor::pullParameters()
{
    const bool eqEnabled = eqOn->load() > 0.5f;
    for (int b = 0; b < kNumBands; ++b)
    {
        auto s = getBandSpec (b);
        s.on = s.on && eqEnabled;
        eq.setBand (b, s);
    }
    CompSettings c;
    c.on = compParams[0]->load() > 0.5f;
    c.thresholdDb = compParams[1]->load();
    c.ratio = compParams[2]->load();
    c.kneeDb = compParams[3]->load();
    c.attackMs = compParams[4]->load();
    c.releaseMs = compParams[5]->load();
    c.makeupDb = compParams[6]->load();
    c.autoMakeup = compParams[7]->load() > 0.5f;
    c.mix = compParams[8]->load() / 100.0;
    c.scHighPassHz = compParams[9]->load();
    c.rms = compParams[10]->load() > 0.5f;
    compPre = compParams[11]->load() > 0.5f;
    c.link = compParams[12]->load() > 0.5f;
    comp.setSettings (c);
    outGain.setTargetValue (juce::Decibels::decibelsToGain ((double) outGainDb->load()));
}

static float peakOf (const float* const* ch, int nch, int n)
{
    float p = 0.0f;
    for (int c = 0; c < nch; ++c)
        for (int i = 0; i < n; ++i)
            p = std::max (p, std::abs (ch[c][i]));
    return p;
}

template <typename Sample>
void ArroyoProcessor::process (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int nch = std::min (getTotalNumInputChannels(), 2);
    const int n = buffer.getNumSamples();
    for (int c = nch; c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (nch == 0 || n == 0)
        return;

    pullParameters();
    auto* const* ch = buffer.getArrayOfWritePointers();

    // analyzer taps and meters work in float
    // (meters hold the max since the GUI last read them; the GUI resets them)
    auto tap = [&] (AnalyzerTap& t, std::atomic<float>& meter)
    {
        if constexpr (std::is_same_v<Sample, float>)
        {
            t.push (ch[0], nch > 1 ? ch[1] : nullptr, n);
            meter.store (std::max (meter.load(), peakOf (ch, nch, n)));
        }
        else
        {
            constexpr int chunk = 256; // no allocation on the audio thread
            float tmp[2][chunk];
            float pk = 0.0f;
            for (int start = 0; start < n; start += chunk)
            {
                const int m = std::min (chunk, n - start);
                for (int c = 0; c < nch; ++c)
                    for (int i = 0; i < m; ++i)
                        tmp[c][i] = (float) ch[c][start + i];
                const float* p[2] = { tmp[0], nch > 1 ? tmp[1] : nullptr };
                t.push (p[0], p[1], m);
                pk = std::max (pk, peakOf (p, nch, m));
            }
            meter.store (std::max (meter.load(), pk));
        }
    };

    tap (preTap, inPeak);
    if (compPre)
        comp.process (ch, nch, n);
    eq.process (ch, nch, n);
    if (! compPre)
        comp.process (ch, nch, n);

    if (outGain.isSmoothing())
    {
        for (int i = 0; i < n; ++i)
        {
            const double g = outGain.getNextValue();
            for (int c = 0; c < nch; ++c)
                ch[c][i] = (Sample) (ch[c][i] * g);
        }
    }
    else if (std::abs (outGain.getTargetValue() - 1.0) > 1e-9)
        buffer.applyGain (0, n, (Sample) outGain.getTargetValue());

    tap (postTap, outPeak);
}

void ArroyoProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { process (b); }
void ArroyoProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { process (b); }

void ArroyoProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void ArroyoProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ArroyoProcessor::createEditor()
{
   #if ARROYO_GENERIC_EDITOR // diagnostic build only
    return new juce::GenericAudioProcessorEditor (*this);
   #else
    return new ArroyoEditor (*this);
   #endif
}

#if ! ARROYO_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ArroyoProcessor(); }
#endif
