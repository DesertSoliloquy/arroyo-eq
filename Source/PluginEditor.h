#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "gui/CurveDisplay.h"

class ArroyoLook : public juce::LookAndFeel_V4
{
public:
    ArroyoLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// A knob with a caption above and its value below.
struct Knob : public juce::Component
{
    Knob (const juce::String& caption, juce::Colour c = juce::Colour());
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
    juce::String caption;
    juce::Colour colour;
};

class LevelMeter : public juce::Component
{
public:
    LevelMeter (const juce::String& name, bool gainReduction = false) : label (name), isGr (gainReduction) {}
    void setLevel (float v) { level = v; repaint(); }
    void paint (juce::Graphics&) override;
private:
    juce::String label;
    bool isGr;
    float level = -100.0f;
};

class ArroyoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ArroyoEditor (ArroyoProcessor&);
    ~ArroyoEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void selectBand (int b);
    void tick() { timerCallback(); } // used by the snapshot tool

private:
    void timerCallback() override;
    void attachBand (int b);

    ArroyoProcessor& proc;
    ArroyoLook look;
    CurveDisplay curve;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // top bar
    juce::ToggleButton eqOn { "EQ" };
    juce::ComboBox analyzerBox, rangeBox, tiltBox;
    juce::Label analyzerLbl, rangeLbl, tiltLbl;
    std::unique_ptr<BA> eqOnAtt;

    // band panel
    int selectedBand = 0;
    juce::OwnedArray<juce::TextButton> bandButtons;
    juce::ToggleButton bandOn { "On" };
    juce::ComboBox typeBox, slopeBox;
    Knob freqKnob { "FREQUENCY" }, gainKnob { "GAIN" }, qKnob { "Q" };
    std::unique_ptr<SA> freqAtt, gainAtt, qAtt;
    std::unique_ptr<BA> bandOnAtt;
    std::unique_ptr<CA> typeAtt, slopeAtt;

    // compressor
    juce::ToggleButton compOn { "On" }, autoMakeup { "Auto makeup" }, link { "Stereo link" };
    juce::ComboBox detectorBox, positionBox;
    Knob thresh { "THRESHOLD" }, ratio { "RATIO" }, knee { "KNEE" }, attack { "ATTACK" },
         release { "RELEASE" }, makeup { "MAKEUP" }, mix { "MIX" }, scHpf { "SC HPF" };
    LevelMeter grMeter { "GR", true };
    std::vector<std::unique_ptr<SA>> compSliderAtts;
    std::vector<std::unique_ptr<BA>> compButtonAtts;
    std::vector<std::unique_ptr<CA>> compComboAtts;

    // output
    Knob outKnob { "OUTPUT" };
    std::unique_ptr<SA> outAtt;
    LevelMeter inMeter { "IN" }, outMeter { "OUT" };
    float inLevel = -100.0f, outLevel = -100.0f, grLevel = 0.0f;

    juce::Rectangle<int> topArea, bandArea, compArea, outArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArroyoEditor)
};
