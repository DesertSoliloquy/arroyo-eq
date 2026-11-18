#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace arroyo::theme
{
const juce::Colour bg      { 0xff14161b };
const juce::Colour panel   { 0xff1b1e25 };
const juce::Colour panel2  { 0xff232730 };
const juce::Colour grid    { 0xff2a2f38 };
const juce::Colour text    { 0xffd8dce3 };
const juce::Colour dim     { 0xff8b929d };
const juce::Colour accent  { 0xffe8873a }; // sunset orange
const juce::Colour accent2 { 0xff3fb7a8 }; // turquoise
const juce::Colour curve   { 0xfff3e9d8 };

inline juce::Colour band (int b)
{
    return juce::Colour::fromHSV (std::fmod (0.02f + b / 16.0f * 0.92f, 1.0f), 0.62f, 0.96f, 1.0f);
}

inline juce::Font font (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace arroyo::theme
