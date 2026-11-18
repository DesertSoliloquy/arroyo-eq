// Development tool: renders the editor to a PNG with a typical setting and
// pink noise running through the processor, so the analyzer is populated.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <random>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double fs = 48000.0;
    const int block = 1600;
    ArroyoProcessor p;
    p.setPlayConfigDetails (2, 2, fs, block);
    p.prepareToPlay (fs, block);

    auto set = [&] (const juce::String& id, float v)
    {
        auto* prm = p.apvts.getParameter (id);
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    const bool demo = argc < 3 || juce::String (argv[2]) != "default";
    if (demo)
    {
        set ("b1_on", 1); set ("b1_freq", 38); set ("b1_slope", 2);
        set ("b3_on", 1); set ("b3_type", 1); set ("b3_freq", 120); set ("b3_gain", 3.5f); set ("b3_q", 0.7f);
        set ("b6_on", 1); set ("b6_freq", 320); set ("b6_gain", -4.5f); set ("b6_q", 1.6f);
        set ("b9_on", 1); set ("b9_freq", 2400); set ("b9_gain", 3.0f); set ("b9_q", 1.1f);
        set ("b11_on", 1); set ("b11_type", 5); set ("b11_freq", 5200); set ("b11_q", 6.0f);
        set ("b14_on", 1); set ("b14_type", 2); set ("b14_freq", 10000); set ("b14_gain", 2.5f); set ("b14_q", 0.7f);
        set ("comp_on", 1); set ("comp_thresh", -24); set ("comp_ratio", 3);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto* editor = dynamic_cast<ArroyoEditor*> (ed.get());
    if (demo)
        editor->selectBand (8);

    // pink noise (Paul Kellet's filter) through the processor
    std::mt19937 rng (3);
    std::normal_distribution<float> n (0.0f, 0.06f);
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    for (int frame = 0; frame < 60; ++frame)
    {
        for (int i = 0; i < block; ++i)
        {
            const float w = n (rng);
            b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f;
            b2 = 0.96900f * b2 + w * 0.1538520f; b3 = 0.86650f * b3 + w * 0.3104856f;
            b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
            const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
            b6 = w * 0.115926f;
            buf.setSample (0, i, pink);
            buf.setSample (1, i, pink * 0.9f);
        }
        p.processBlock (buf, midi);
        editor->tick();
    }

    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (argc > 1 ? argv[1] : "snapshot.png");
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
