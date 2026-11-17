#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Polar design language: Apple-grade dark glass, one accent per product, generous spacing.
namespace polar
{
struct Theme
{
    juce::Colour accent        { 0xff4f8cff };   // per-product
    juce::Colour accentSoft    { 0x334f8cff };
    juce::Colour bg0           { 0xff0b0c10 };   // window
    juce::Colour bg1           { 0xff14161c };   // panels
    juce::Colour bg2           { 0xff1d2027 };   // controls
    juce::Colour line          { 0x14ffffff };
    juce::Colour text          { 0xfff2f3f5 };
    juce::Colour textDim       { 0x99f2f3f5 };
    juce::Colour textFaint     { 0x55f2f3f5 };
    float        radius        { 14.0f };

    static Theme withAccent (juce::Colour c)
    {
        Theme t; t.accent = c; t.accentSoft = c.withAlpha (0.2f); return t;
    }
};

inline juce::Font uiFont (float size, bool bold = false)
{
    // SF Pro on macOS, Segoe/Inter elsewhere; JUCE resolves the sans default per platform.
    auto f = juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
   #if JUCE_MAC
    f.setTypefaceName (".AppleSystemUIFont");
   #endif
    return f;
}
} // namespace polar
