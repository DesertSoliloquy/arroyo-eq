#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

class CurveDisplay : public juce::Component
{
public:
    CurveDisplay (ArroyoProcessor&, std::function<void (int)> onBandSelected);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void setSelected (int b) { selected = b; repaint(); }
    int getSelected() const { return selected; }

    // Called ~30x per second by the editor.
    void updateAnalyzer();

private:
    struct Spectrum
    {
        std::vector<float> history, db;
        bool active = false;
    };

    ArroyoProcessor& proc;
    std::function<void (int)> onSelect;
    int selected = 0, hovered = -1, dragging = -1;
    juce::Point<float> dragStart;
    float dragStartFreq = 1000.0f, dragStartGain = 0.0f, dragStartQ = 1.0f;

    static constexpr int fftOrder = 13, fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false };
    std::vector<float> fftData, pullBuf;
    Spectrum pre, post;
    float windowGain = 1.0f;

    juce::Rectangle<float> plot() const;
    float freqToX (double f) const;
    double xToFreq (float x) const;
    float dbToY (double db) const;
    double yToDb (float y) const;
    float specDbToY (double db) const;
    double eqRange() const;
    juce::Point<float> nodePos (int b, const arroyo::BandSpec&) const;
    int hitNode (juce::Point<float>) const;

    void processTap (arroyo::AnalyzerTap&, Spectrum&, bool wanted);
    void drawSpectrum (juce::Graphics&, const Spectrum&, juce::Colour) const;
    void drawGrid (juce::Graphics&) const;
    void showBandMenu (int b);

    juce::RangedAudioParameter* param (int b, const char* what) const;
    void setParam (int b, const char* what, float value);
    void gesture (int b, bool begin);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveDisplay)
};
