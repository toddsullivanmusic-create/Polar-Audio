#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

// Embedded typefaces (SIL Open Font License, see Plugins/Prism/Fonts):
//   display — Michroma: wide, machined-looking caps for the wordmark, module names and section labels
//   label   — Barlow Condensed SemiBold: classic hardware panel lettering for knobs and keys
//   mono    — Space Mono Bold: numeric readouts and the display screens
namespace prism::ui::fonts
{
inline juce::Typeface::Ptr load (const void* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); }
inline juce::Font display (float h) { static auto t = load (BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize); return juce::Font (juce::FontOptions (t).withHeight (h)); }
inline juce::Font label (float h)   { static auto t = load (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize); return juce::Font (juce::FontOptions (t).withHeight (h)); }
inline juce::Font mono (float h)    { static auto t = load (BinaryData::SpaceMonoBold_ttf, BinaryData::SpaceMonoBold_ttfSize); return juce::Font (juce::FontOptions (t).withHeight (h)); }
}
