#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "UI/Theme.h"

// Procedural materials for PRISM's hardware look. One light source, top-left, everywhere.
namespace prism::ui::mat
{
inline juce::uint32 hash (juce::uint32 x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
inline float rnd01 (juce::uint32 seed) { return (float) (hash (seed) & 0xFFFFFF) / 16777215.0f; }

inline juce::uint8 clampByte (float v) { return (juce::uint8) juce::jlimit (0, 255, (int) v); }

// Brushed aluminium: per-row streak luminance (random walk + long streaks) plus fine pixel grain,
// with a soft horizontal specular band.
inline juce::Image brushed (int w, int h, juce::Colour base, juce::uint32 seed, float streak = 9.0f)
{
    juce::Image img (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), false);
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    std::vector<float> row ((size_t) w);
    float walk = 0;
    for (int y = 0; y < h; ++y)
    {
        walk = walk * 0.85f + (rnd01 (seed + (juce::uint32) y * 7919u) - 0.5f) * 4.0f;
        const float rowLum = walk + (rnd01 (seed ^ (juce::uint32) (y * 104729)) - 0.5f) * streak;
        // occasional long, slightly brighter streaks
        float s = 0;
        for (int x = 0; x < w; ++x)
        {
            if ((hash ((juce::uint32) (x * 31 + y * 8191) ^ seed) & 1023) == 0) s = (rnd01 ((juce::uint32) (x + y * w) ^ seed) - 0.4f) * 10.0f;
            s *= 0.985f;
            row[(size_t) x] = rowLum + s + (rnd01 ((juce::uint32) (x * 2654435761u) ^ (juce::uint32) (y * 40503u) ^ seed) - 0.5f) * 5.0f;
        }
        const float band = 10.0f * std::exp (-std::pow (((float) y / (float) h - 0.28f) * 3.2f, 2.0f)) - 6.0f * (float) y / (float) h;
        for (int x = 0; x < w; ++x)
        {
            const float l = row[(size_t) x] + band;
            bd.setPixelColour (x, y, juce::Colour (clampByte (base.getRed() + l), clampByte (base.getGreen() + l), clampByte (base.getBlue() + l * 1.05f)));
        }
    }
    return img;
}

// Powder-coat / bead-blasted finish: isotropic fine grain with faint mottling
inline juce::Image grain (int w, int h, juce::Colour base, juce::uint32 seed, float amount = 7.0f)
{
    juce::Image img (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), false);
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const float mottle = (rnd01 ((juce::uint32) ((x / 9) * 92821 + (y / 9) * 68917) ^ seed) - 0.5f) * amount * 0.5f;
            const float n = (rnd01 ((juce::uint32) (x * 73856093) ^ (juce::uint32) (y * 19349663) ^ seed) - 0.5f) * amount;
            const float shade = -8.0f * (float) y / (float) h;
            const float l = n + mottle + shade;
            bd.setPixelColour (x, y, juce::Colour (clampByte (base.getRed() + l), clampByte (base.getGreen() + l), clampByte (base.getBlue() + l)));
        }
    return img;
}

// Raised bevel: highlight on the top-left edge, shade on the bottom-right
inline void bevel (juce::Graphics& g, juce::Rectangle<float> r, float radius, float width = 2.0f, float strength = 1.0f)
{
    juce::Path p; p.addRoundedRectangle (r.reduced (width * 0.5f), radius);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.9f * strength), r.getX(), r.getY(),
                                             juce::Colours::black.withAlpha (0.35f * strength), r.getRight(), r.getBottom(), false));
    g.strokePath (p, juce::PathStrokeType (width));
}

// Recessed edge: the reverse of a bevel (shadow top-left, highlight bottom-right) plus an inner shadow
inline void innerShadow (juce::Graphics& g, juce::Rectangle<float> r, float radius, float depth = 8.0f, float alpha = 0.35f)
{
    juce::Path hole; hole.addRoundedRectangle (r, radius);
    juce::Path ring; ring.addRectangle (r.expanded (40)); ring.addRoundedRectangle (r, radius); ring.setUsingNonZeroWinding (false);
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (hole);
    juce::DropShadow (juce::Colours::black.withAlpha (alpha), (int) depth, { 0, (int) (depth * 0.35f) }).drawForPath (g, ring);
    juce::Path edge; edge.addRoundedRectangle (r.reduced (0.5f), radius);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.35f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.7f), r.getRight(), r.getBottom(), false));
    g.strokePath (edge, juce::PathStrokeType (1.2f));
}

// Hex-socket cap screw
inline void screw (juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillEllipse (c.x - r - 0.5f, c.y - r + 1.0f, r * 2 + 1, r * 2 + 1);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff7f7f6), c.x - r, c.y - r, juce::Colour (0xff8e8e8c), c.x + r, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb9b8b5), c.x - r, c.y - r, juce::Colour (0xffe9e8e6), c.x + r, c.y + r, false));
    g.fillEllipse (c.x - r * 0.78f, c.y - r * 0.78f, r * 1.56f, r * 1.56f);
    juce::Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float a = angle + juce::MathConstants<float>::twoPi * (float) i / 6.0f;
        const juce::Point<float> p (c.x + std::cos (a) * r * 0.42f, c.y + std::sin (a) * r * 0.42f);
        i == 0 ? hex.startNewSubPath (p) : hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), c.x - r * 0.4f, c.y - r * 0.4f, juce::Colour (0xff8a8a88), c.x + r * 0.4f, c.y + r * 0.4f, false));
    g.fillPath (hex);
}

// Engraved / screen-printed text: a 1px highlight below the glyphs sells the depth
inline void engraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Font f, juce::Colour ink,
                      juce::Justification j = juce::Justification::centred, bool onDark = false)
{
    g.setFont (f);
    g.setColour (onDark ? juce::Colours::black.withAlpha (0.45f) : juce::Colours::white.withAlpha (0.85f));
    g.drawText (text, r.translated (0, onDark ? -1.0f : 1.0f), j, false);
    g.setColour (ink);
    g.drawText (text, r, j, false);
}

// Glass dome LED
inline void led (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, bool on)
{
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.55f), c.x, c.y, colour.withAlpha (0.0f), c.x + r * 4.5f, c.y, true));
        g.fillEllipse (c.x - r * 4.5f, c.y - r * 4.5f, r * 9, r * 9);
    }
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillEllipse (c.x - r - 1, c.y - r - 1, r * 2 + 2, r * 2 + 2);
    const auto body = on ? colour.brighter (0.2f) : colour.withSaturation (0.35f).darker (1.4f);
    g.setGradientFill (juce::ColourGradient (body.brighter (0.6f), c.x - r * 0.5f, c.y - r * 0.6f, body.darker (0.4f), c.x + r, c.y + r, true));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.85f : 0.4f));
    g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.7f, r * 0.7f, r * 0.45f);
}

// Dark glass display: bezel recess, deep tinted glass, diagonal reflection
inline void glassBackground (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour tint)
{
    g.setColour (juce::Colours::black.withAlpha (0.18f)); g.fillRoundedRectangle (r.expanded (3), radius + 3);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1c22).interpolatedWith (tint, 0.06f), r.getX(), r.getY(),
                                             juce::Colour (0xff07080b), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setGradientFill (juce::ColourGradient (tint.withAlpha (0.18f), r.getCentreX(), r.getBottom(), tint.withAlpha (0.0f), r.getCentreX(), r.getCentreY(), false));
    g.fillRoundedRectangle (r, radius);
}
inline void glassOverlay (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    juce::Graphics::ScopedSaveState s (g);
    juce::Path clip; clip.addRoundedRectangle (r, radius);
    g.reduceClipRegion (clip);
    juce::Path sheen;
    sheen.startNewSubPath (r.getX(), r.getY());
    sheen.lineTo (r.getX() + r.getWidth() * 0.62f, r.getY());
    sheen.lineTo (r.getX() + r.getWidth() * 0.34f, r.getBottom());
    sheen.lineTo (r.getX(), r.getBottom());
    sheen.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.4f, r.getBottom(), false));
    g.fillPath (sheen);
    g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + radius, r.getRight() - radius);
    innerShadow (g, r, radius, 10.0f, 0.6f);
}
} // namespace prism::ui::mat
