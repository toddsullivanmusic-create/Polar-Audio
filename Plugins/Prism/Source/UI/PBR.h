#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <thread>
#include <map>
#include <mutex>

// A tiny physically-based software renderer for PRISM's hardware.
//
// Every surface is described as a height field + material. For each pixel we derive the normal from the
// height gradient, then light it with an analytic studio environment (gaussian "softbox" lobes in
// reflection space). Roughness — including anisotropic roughness for brushed and spun metal — is applied
// by widening each lobe with an oriented gaussian, which is exactly a pre-filtered environment lookup.
// Fresnel (Schlick), metallic/dielectric split, a clear-coat layer, ACES tone mapping, linear->sRGB.
// One shared camera means reflections sweep continuously across every panel, like a real photo.
namespace prism::ui::pbr
{
struct V3 { float x = 0, y = 0, z = 0; };
inline V3 operator+ (V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 operator- (V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V3 operator* (V3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
inline V3 operator* (V3 a, V3 b) { return { a.x * b.x, a.y * b.y, a.z * b.z }; }
inline float dot (V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 norm (V3 a) { const float l = std::sqrt (dot (a, a)); return l > 1e-9f ? a * (1.0f / l) : V3 { 0, 0, 1 }; }
inline V3 lerp (V3 a, V3 b, float t) { return a + (b - a) * t; }

inline float s2l (float c) { return c <= 0.04045f ? c / 12.92f : std::pow ((c + 0.055f) / 1.055f, 2.4f); }
inline float l2s (float c) { c = juce::jlimit (0.0f, 1.0f, c); return c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow (c, 1.0f / 2.4f) - 0.055f; }
inline V3 lin (juce::Colour c) { return { s2l (c.getFloatRed()), s2l (c.getFloatGreen()), s2l (c.getFloatBlue()) }; }
inline float aces1 (float x) { return juce::jlimit (0.0f, 1.0f, (x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f)); }
inline juce::PixelARGB toPixel (V3 c, float alpha = 1.0f)
{
    c = c * 1.32f;   // exposure
    const auto r = (juce::uint8) juce::roundToInt (l2s (aces1 (c.x)) * 255.0f), g = (juce::uint8) juce::roundToInt (l2s (aces1 (c.y)) * 255.0f),
               b = (juce::uint8) juce::roundToInt (l2s (aces1 (c.z)) * 255.0f), a = (juce::uint8) juce::roundToInt (juce::jlimit (0.0f, 1.0f, alpha) * 255.0f);
    juce::PixelARGB p (a, r, g, b); p.premultiply(); return p;
}

// ---------------------------------------------------------------------------------------------
// noise
// ---------------------------------------------------------------------------------------------
inline juce::uint32 hash2 (int x, int y, juce::uint32 s)
{
    juce::uint32 h = (juce::uint32) x * 374761393u + (juce::uint32) y * 668265263u + s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16);
}
inline float hf (int x, int y, juce::uint32 s) { return (float) (hash2 (x, y, s) & 0xFFFFFF) / 16777215.0f; }
inline float vnoise (float x, float y, juce::uint32 s)
{
    const int xi = (int) std::floor (x), yi = (int) std::floor (y);
    float fx = x - (float) xi, fy = y - (float) yi;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    const float a = hf (xi, yi, s), b = hf (xi + 1, yi, s), c = hf (xi, yi + 1, s), d = hf (xi + 1, yi + 1, s);
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}
inline float sdRoundRect (float px, float py, float cx, float cy, float hw, float hh, float r)
{
    const float qx = std::abs (px - cx) - hw + r, qy = std::abs (py - cy) - hh + r;
    const float ox = std::max (qx, 0.0f), oy = std::max (qy, 0.0f);
    return std::sqrt (ox * ox + oy * oy) + std::min (std::max (qx, qy), 0.0f) - r;
}
inline float smooth01 (float e0, float e1, float x) { const float t = juce::jlimit (0.0f, 1.0f, (x - e0) / (e1 - e0)); return t * t * (3 - 2 * t); }

// ---------------------------------------------------------------------------------------------
// studio environment
// ---------------------------------------------------------------------------------------------
struct Lobe { float cx, cy, sx, sy; V3 L; };
inline const std::array<Lobe, 6>& studio()
{
    // reflection-plane coordinates (x right, y DOWN); (0,0) is straight back at the camera
    static const std::array<Lobe, 6> l { {
        { -0.42f, -0.58f, 0.24f, 0.15f, { 3.2f, 3.05f, 2.85f } },   // key softbox, top-left, warm
        {  0.58f, -0.40f, 0.17f, 0.26f, { 1.05f, 1.12f, 1.25f } },  // fill, top-right, cool
        {  0.92f,  0.10f, 0.05f, 0.55f, { 1.7f, 1.7f, 1.75f } },    // vertical strip light, right
        {  0.00f, -0.98f, 0.70f, 0.06f, { 1.1f, 1.1f, 1.1f } },     // ceiling strip
        { -0.85f,  0.30f, 0.08f, 0.35f, { 0.55f, 0.55f, 0.6f } },   // small kicker, left
        {  0.00f,  0.95f, 0.90f, 0.22f, { 0.14f, 0.12f, 0.10f } },  // warm floor bounce
    } };
    return l;
}
// Environment radiance for reflection direction (rx, ry), pre-filtered by an oriented gaussian roughness
// kernel (ru along `ang`, rv across it).
inline V3 envEval (float rx, float ry, float ru, float rv, float ang)
{
    const float ca = std::cos (ang), sa = std::sin (ang);
    const float k11 = ca * ca * ru * ru + sa * sa * rv * rv, k22 = sa * sa * ru * ru + ca * ca * rv * rv, k12 = ca * sa * (ru * ru - rv * rv);
    V3 out { 0, 0, 0 };
    for (auto& lb : studio())
    {
        const float a = lb.sx * lb.sx + k11, d = lb.sy * lb.sy + k22, b = k12;
        const float det = a * d - b * b;
        const float dx = rx - lb.cx, dy = ry - lb.cy;
        const float q = (d * dx * dx - 2 * b * dx * dy + a * dy * dy) / det;
        out = out + lb.L * (lb.sx * lb.sy / std::sqrt (det) * std::exp (-0.5f * q));
    }
    // sky / floor gradient
    const float t = juce::jlimit (0.0f, 1.0f, 0.5f - ry * 0.5f);
    return out + lerp (V3 { 0.035f, 0.032f, 0.03f }, V3 { 0.30f, 0.31f, 0.33f }, t);
}

struct Material
{
    V3 base { 0.8f, 0.8f, 0.8f };
    float metal = 0, rough = 0.5f;
    float anisoU = 1, anisoV = 1, anisoAngle = 0;   // roughness multipliers along / across anisoAngle
    float coat = 0, coatRough = 0.12f;              // clear-coat layer
};

// Raking key light: a soft box low on the left. For world-positioned surfaces it is a point light (so the
// falloff sweeps across the faceplate); for small parts (knobs, keys...) a fixed direction.
struct KeyLight { V3 pos { -1400.0f, 250.0f, 1000.0f }; V3 colour { 3.6f, 3.5f, 3.35f }; };
inline V3 fixedLightDir() { return norm ({ -0.84f, -0.18f, 0.50f }); }

inline V3 shade (V3 n, V3 v, const Material& m, float ao, V3 L, float Li)
{
    const float ndv = juce::jmax (1.0e-3f, dot (n, v));
    const V3 r = n * (2.0f * dot (n, v)) - v;
    const V3 f0 = lerp (V3 { 0.04f, 0.04f, 0.04f }, m.base, m.metal);
    const float fw = std::pow (1.0f - ndv, 5.0f) * (1.0f - m.rough * 0.7f);
    const V3 F = f0 + (V3 { 1, 1, 1 } - f0) * fw;
    const V3 spec = F * envEval (r.x, r.y, 0.02f + m.rough * m.anisoU, 0.02f + m.rough * m.anisoV, m.anisoAngle);
    const V3 diff = m.base * (1.0f - m.metal) * envEval (n.x * 0.8f, n.y * 0.8f, 0.85f, 0.85f, 0) * 1.0f;
    V3 c = (diff + spec * 0.55f) * ao;
    // direct light: Lambert + GGX
    const float ndl = dot (n, L);
    if (ndl > 0)
    {
        const V3 h = norm (L + v);
        const float ndh = juce::jmax (0.0f, dot (n, h)), a = juce::jmax (0.02f, m.rough * m.rough), a2 = a * a;
        const float dd = ndh * ndh * (a2 - 1.0f) + 1.0f;
        const float D = a2 / (3.14159f * dd * dd);
        const float k = (m.rough + 1) * (m.rough + 1) / 8.0f;
        const float G = (ndl / (ndl * (1 - k) + k)) * (ndv / (ndv * (1 - k) + k));
        const float vdh = juce::jmax (0.0f, dot (v, h));
        const V3 Fh = f0 + (V3 { 1, 1, 1 } - f0) * std::pow (1.0f - vdh, 5.0f);
        const V3 specD = Fh * (D * G / juce::jmax (1.0e-3f, 4.0f * ndv * ndl));
        const V3 kd = (V3 { 1, 1, 1 } - Fh) * (1.0f - m.metal);
        c = c + (kd * m.base * (1.0f / 3.14159f) + specD) * (ndl * Li * 3.14159f * 0.32f) * (0.35f + 0.65f * ao);
    }
    if (m.coat > 0)
    {
        const float cf = 0.04f + 0.96f * std::pow (1.0f - ndv, 5.0f);
        c = c * (1.0f - cf * m.coat) + envEval (r.x, r.y, m.coatRough, m.coatRough, 0) * (cf * m.coat);
    }
    return c;
}

// rows in parallel
template <typename Fn> void parallelRows (int h, Fn&& fn)
{
    const int n = juce::jlimit (1, 16, (int) std::thread::hardware_concurrency());
    std::vector<std::thread> ts;
    for (int t = 0; t < n; ++t) ts.emplace_back ([&, t] { for (int y = t; y < h; y += n) fn (y); });
    for (auto& t : ts) t.join();
}

// One global camera over the 1320x880 canvas, so every panel shares a single perspective.
struct Camera { float x = 660, y = 360, z = 1450; };
inline V3 viewDir (float wx, float wy, const Camera& cam = {}) { return norm ({ cam.x - wx, cam.y - wy, cam.z }); }

// Generic renderer: heightFn (x, y) and materialFn (x, y, material&, ao&) in canvas units; returns ARGB image
// at `scale` pixels per unit. coverageFn returns alpha in [0,1].
template <typename H, typename M, typename A>
juce::Image renderSurface (float ox, float oy, float w, float h, float scale, H&& height, M&& material, A&& coverage, float bump = 1.0f, bool worldLight = true)
{
    const int W = juce::jmax (1, (int) std::ceil (w * scale)), Hh = juce::jmax (1, (int) std::ceil (h * scale));
    juce::Image img (juce::Image::ARGB, W, Hh, true);
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    const float e = 0.5f / scale;
    parallelRows (Hh, [&] (int py)
    {
        for (int px = 0; px < W; ++px)
        {
            const float x = ox + ((float) px + 0.5f) / scale, y = oy + ((float) py + 0.5f) / scale;
            const float a = coverage (x, y);
            if (a <= 0.0f) continue;
            const float dx = (height (x + e, y) - height (x - e, y)) / (2 * e);
            const float dy = (height (x, y + e) - height (x, y - e)) / (2 * e);
            const V3 n = norm ({ -dx * bump, -dy * bump, 1.0f });
            Material m; float ao = 1.0f;
            material (x, y, m, ao);
            V3 L = fixedLightDir(); float Li = 3.5f;
            if (worldLight)
            {
                const KeyLight kl;
                const V3 d = kl.pos - V3 { x, y, height (x, y) };
                const float dist = std::sqrt (dot (d, d));
                L = d * (1.0f / dist);
                Li = 6.9f / (1.0f + std::pow (dist / 3000.0f, 2.0f));
            }
            const V3 c = shade (n, viewDir (x, y), m, ao, L, Li);
            *reinterpret_cast<juce::PixelARGB*> (bd.getPixelPointer (px, py)) = toPixel (c, a);
        }
    });
    return img;
}

// ---------------------------------------------------------------------------------------------
// Materials used by PRISM
// ---------------------------------------------------------------------------------------------
inline Material brushedAluminium (float streak)
{
    Material m; m.base = V3 { 0.86f, 0.865f, 0.87f } * (0.92f + 0.14f * streak); m.metal = 1; m.rough = 0.28f;
    m.anisoU = 0.12f; m.anisoV = 1.6f; m.anisoAngle = 0;   // horizontal grain: highlights stretch vertically
    return m;
}
inline Material powderCoat()   // pure white matte paint over metal
{
    Material m; m.base = lin (juce::Colour (0xfff9f9f7)); m.metal = 0; m.rough = 0.80f; m.coat = 0.06f; m.coatRough = 0.4f; return m;
}
inline Material anodised (juce::Colour c, float)   // matte coloured paint for the header plates
{
    Material m; m.base = lin (c) * 0.42f; m.metal = 0; m.rough = 0.70f; m.coat = 0.08f; m.coatRough = 0.35f; return m;   // deep pigment so the raking light lands it at full saturation
}
inline float paintPeel (float x, float y, juce::uint32 s)   // orange-peel texture of sprayed paint
{
    return 0.6f * vnoise (x * 0.42f, y * 0.42f, s) + 0.4f * vnoise (x * 1.05f, y * 1.05f, s + 3);
}

// Holes / wells cut into a panel
struct Recess { juce::Rectangle<float> r; float radius = 6, depth = 3, wall = 2; };

inline float brushedHeight (float x, float y, juce::uint32 s)
{
    return 0.55f * vnoise (x * 0.012f, y * 1.1f, s) + 0.30f * vnoise (x * 0.05f, y * 2.3f, s + 1) + 0.15f * vnoise (x * 0.25f, y * 3.7f, s + 2);
}

// Faceplate of the whole unit: brushed aluminium with a chamfered edge and machined wells
inline juce::Image renderChassis (float w, float h, float scale, std::vector<Recess> wells)
{
    const juce::uint32 seed = 7u;
    auto height = [&] (float x, float y)
    {
        const float d = sdRoundRect (x, y, w * 0.5f, h * 0.5f, w * 0.5f, h * 0.5f, 12.0f);
        float z = 6.0f * std::sin (juce::jlimit (0.0f, 1.0f, -d / 7.0f) * 1.5708f);       // rounded chamfer
        z += 0.22f * paintPeel (x, y, seed);                                                 // sprayed paint texture
        for (auto& r : wells)
        {
            const float dr = sdRoundRect (x, y, r.r.getCentreX(), r.r.getCentreY(), r.r.getWidth() * 0.5f, r.r.getHeight() * 0.5f, r.radius);
            z -= r.depth * smooth01 (r.wall, -r.wall * 0.2f, dr);
        }
        return z;
    };
    auto material = [&] (float x, float y, Material& m, float& ao)
    {
        m = powderCoat();
        for (auto& r : wells)
        {
            const float dr = sdRoundRect (x, y, r.r.getCentreX(), r.r.getCentreY(), r.r.getWidth() * 0.5f, r.r.getHeight() * 0.5f, r.radius);
            if (dr < 0) ao *= 0.5f + 0.5f * smooth01 (0.0f, -r.depth * 2.5f, dr);
        }
        const float d = sdRoundRect (x, y, w * 0.5f, h * 0.5f, w * 0.5f, h * 0.5f, 12.0f);
        ao *= 0.75f + 0.25f * smooth01 (0.0f, -3.0f, d);
    };
    auto cover = [&] (float x, float y) { return juce::jlimit (0.0f, 1.0f, 0.5f - sdRoundRect (x, y, w * 0.5f, h * 0.5f, w * 0.5f, h * 0.5f, 12.0f) * scale); };
    return renderSurface (0, 0, w, h, scale, height, material, cover, 1.0f);
}

// A powder-coated module plate with an anodised header band and machined recesses for its controls.
// (ox, oy) is the plate's position on the canvas, so its reflections line up with the faceplate's.
inline juce::Image renderPlate (float ox, float oy, float w, float h, float scale, float bandH, juce::Colour bandColour,
                                std::vector<Recess> wells, float radius = 14.0f, juce::uint32 seed = 11u)
{
    auto local = [=] (float x, float y) { return juce::Point<float> (x - ox, y - oy); };
    auto height = [&] (float x, float y)
    {
        const auto p = local (x, y);
        const float d = sdRoundRect (p.x, p.y, w * 0.5f, h * 0.5f, w * 0.5f, h * 0.5f, radius);
        float z = 4.0f * std::sin (juce::jlimit (0.0f, 1.0f, -d / 4.5f) * 1.5708f);
        if (bandH > 0)
        {
            z += 0.22f * paintPeel (x, y, seed);
            z -= 1.2f * std::exp (-std::pow ((p.y - bandH) / 0.9f, 2.0f));        // seam between band and face
        }
        else z += 0.22f * paintPeel (x, y, seed);
        for (auto& r : wells)
        {
            const float dr = sdRoundRect (p.x, p.y, r.r.getCentreX(), r.r.getCentreY(), r.r.getWidth() * 0.5f, r.r.getHeight() * 0.5f, r.radius);
            z -= r.depth * smooth01 (r.wall, -r.wall * 0.2f, dr);
        }
        return z;
    };
    auto material = [&] (float x, float y, Material& m, float& ao)
    {
        const auto p = local (x, y);
        m = (bandH > 0 && p.y < bandH) ? anodised (bandColour, brushedHeight (x, y, seed)) : powderCoat();
        for (auto& r : wells)
        {
            const float dr = sdRoundRect (p.x, p.y, r.r.getCentreX(), r.r.getCentreY(), r.r.getWidth() * 0.5f, r.r.getHeight() * 0.5f, r.radius);
            if (dr < 0) ao *= 0.45f + 0.55f * smooth01 (0.0f, -r.depth * 2.5f, dr);   // occluded floor near the walls
        }
    };
    auto cover = [&] (float x, float y) { const auto p = local (x, y); return juce::jlimit (0.0f, 1.0f, 0.5f - sdRoundRect (p.x, p.y, w * 0.5f, h * 0.5f, w * 0.5f, h * 0.5f, radius) * scale); };
    return renderSurface (ox, oy, w, h, scale, height, material, cover, 1.0f);
}

// ---------------------------------------------------------------------------------------------
// Knob frames. Lighting is fixed; the knurl repeats every 2*pi/kKnurl, so kFrames frames over one knurl
// period cover every possible angle.
// ---------------------------------------------------------------------------------------------
constexpr int kKnurl = 36, kFrames = 16;

inline juce::Image renderKnobFrame (int px, float phase)
{
    const int full = (int) (px * 1.3f);
    juce::Image img (juce::Image::ARGB, full, full, true);
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    const float c = (float) full * 0.5f, R = (float) px * 0.5f;
    const float Rc = R * 0.70f, Hc = R * 0.62f, bw = R * 0.11f, Hs0 = R * 0.24f, Hs1 = R * 0.10f;
    auto height = [&] (float x, float y)
    {
        const float dx = x - c, dy = y - c, r = std::sqrt (dx * dx + dy * dy);
        if (r <= Rc)
        {
            float z;
            if (r < Rc - bw) z = Hc;
            else { const float t = r - (Rc - bw); z = Hc - bw + std::sqrt (juce::jmax (0.0f, bw * bw - t * t)); }
            if (r < Rc * 0.12f) z -= R * 0.03f * (1.0f - (r / (Rc * 0.12f)) * (r / (Rc * 0.12f)));   // centre dimple
            return z;
        }
        if (r <= R)
        {
            const float t = (r - Rc) / (R - Rc);
            float z = Hs0 + (Hs1 - Hs0) * t;
            if (t > 0.88f) { const float u = (t - 0.88f) / 0.12f; z *= std::sqrt (juce::jmax (0.0f, 1.0f - u * u)); }
            const float th = std::atan2 (dy, dx) - phase;
            if (t > 0.12f) z += R * 0.035f * std::abs (std::sin (th * (float) kKnurl * 0.5f)) * smooth01 (0.12f, 0.25f, t);
            return z;
        }
        return 0.0f;
    };
    const V3 v { 0, 0, 1 };
    const float e = 0.5f;
    for (int py = 0; py < full; ++py)
        for (int px2 = 0; px2 < full; ++px2)
        {
            const float x = (float) px2 + 0.5f, y = (float) py + 0.5f;
            const float dx = x - c, dy = y - c, r = std::sqrt (dx * dx + dy * dy);
            if (r > R + 1.0f)
            {
                // soft contact shadow, cast down-right
                const float sx = dx - R * 0.22f, sy = dy - R * 0.07f, sr = std::sqrt (sx * sx + sy * sy);
                const float sh = 0.62f * std::exp (-std::pow (juce::jmax (0.0f, sr - R * 0.92f) / (R * 0.16f), 2.0f));
                if (sh > 0.003f) *reinterpret_cast<juce::PixelARGB*> (bd.getPixelPointer (px2, py)) = juce::PixelARGB ((juce::uint8) (sh * 255), 0, 0, 0);
                continue;
            }
            const float gx = (height (x + e, y) - height (x - e, y)) / (2 * e), gy = (height (x, y + e) - height (x, y - e)) / (2 * e);
            const V3 n = norm ({ -gx, -gy, 1.0f });
            Material m;
            float ao = 1.0f;
            if (r <= Rc)
            {
                m.base = { 0.82f, 0.82f, 0.83f }; m.metal = 1; m.rough = 0.22f;
                m.anisoU = 2.0f; m.anisoV = 0.15f; m.anisoAngle = std::atan2 (dy, dx);   // spun: streaks run radially
            }
            else
            {
                m.base = { 0.035f, 0.036f, 0.04f }; m.metal = 1; m.rough = 0.30f; m.coat = 0.2f;
                ao = 0.55f + 0.45f * smooth01 (Rc, Rc + R * 0.08f, r);                     // AO where the cap meets the skirt
            }
            V3 col = shade (n, v, m, ao, fixedLightDir(), 3.5f);
            const float a = juce::jlimit (0.0f, 1.0f, R - r + 0.5f);
            juce::PixelARGB p = toPixel (col, 1.0f);
            if (a < 1.0f)
            {
                // blend over the shadow for an anti-aliased rim
                const float sx = dx - R * 0.22f, sy = dy - R * 0.07f, sr = std::sqrt (sx * sx + sy * sy);
                const float sh = 0.62f * std::exp (-std::pow (juce::jmax (0.0f, sr - R * 0.92f) / (R * 0.16f), 2.0f));
                p.unpremultiply();
                const float outA = a + sh * (1 - a);
                const float k = outA > 0 ? a / outA : 0;
                p = juce::PixelARGB ((juce::uint8) (outA * 255), (juce::uint8) (p.getRed() * k), (juce::uint8) (p.getGreen() * k), (juce::uint8) (p.getBlue() * k));
                p.premultiply();
            }
            *reinterpret_cast<juce::PixelARGB*> (bd.getPixelPointer (px2, py)) = p;
        }
    return img;
}

inline const std::vector<juce::Image>& knobFrames (int px)
{
    static std::mutex mtx;
    static std::map<int, std::vector<juce::Image>> cache;
    std::lock_guard<std::mutex> lock (mtx);
    auto& v = cache[px];
    if (v.empty())
    {
        v.resize (kFrames);
        std::vector<std::thread> ts;
        for (int f = 0; f < kFrames; ++f)
            ts.emplace_back ([&, f] { v[(size_t) f] = renderKnobFrame (px, juce::MathConstants<float>::twoPi / (float) kKnurl * (float) f / (float) kFrames); });
        for (auto& t : ts) t.join();
    }
    return v;
}

// ---------------------------------------------------------------------------------------------
// Satin hardware key (raised cushion; pressed = sunk and slightly dished)
// ---------------------------------------------------------------------------------------------
inline juce::Image renderKey (float bw, float bh, float scale, bool pressed)
{
    const float pad = 3.5f, W = bw, H = bh, w = bw - pad * 2, h = bh - pad * 2, rad = 5.0f;
    auto height = [&] (float x, float y)
    {
        const float d = sdRoundRect (x, y, W * 0.5f, H * 0.5f, w * 0.5f, h * 0.5f, rad);
        if (d > 0) return 0.0f;
        const float cushion = std::sin (juce::jlimit (0.0f, 1.0f, -d / 4.0f) * 1.5708f);
        const float ux = (x - W * 0.5f) / (w * 0.5f), uy = (y - H * 0.5f) / (h * 0.5f);
        const float dome = (1.0f - 0.5f * (ux * ux * 0.3f + uy * uy)) * 0.6f;
        return pressed ? 1.2f * cushion - dome * 0.5f : 3.0f * cushion + dome;
    };
    auto material = [&] (float, float, Material& m, float& ao)
    {
        m.base = lin (juce::Colour (pressed ? 0xffe2e1dc : 0xfff0efea)); m.metal = 0; m.rough = 0.42f; m.coat = 0.25f; m.coatRough = 0.25f;
        ao = pressed ? 0.88f : 1.0f;
    };
    auto cover = [&] (float x, float y) { return juce::jlimit (0.0f, 1.0f, 0.5f - sdRoundRect (x, y, W * 0.5f, H * 0.5f, w * 0.5f, h * 0.5f, rad) * scale); };
    auto img = renderSurface (0, 0, W, H, scale, height, material, cover, 1.0f, false);
    // shadow underneath
    juce::Image out (juce::Image::ARGB, img.getWidth(), img.getHeight(), true);
    {
        juce::Graphics g (out);
        juce::Path p; p.addRoundedRectangle (pad * scale, pad * scale, w * scale, h * scale, rad * scale);
        if (! pressed) juce::DropShadow (juce::Colours::black.withAlpha (0.5f), (int) (3 * scale), { (int) (2.0f * scale), (int) (0.8f * scale) }).drawForPath (g, p);
        else juce::DropShadow (juce::Colours::black.withAlpha (0.25f), (int) (2 * scale), { 0, (int) (1 * scale) }).drawForPath (g, p);
        g.setOpacity (1.0f); g.drawImageAt (img, 0, 0);
    }
    return out;
}

inline juce::Image cachedKey (int w, int h, bool pressed, float scale)
{
    static std::mutex mtx; static std::map<juce::int64, juce::Image> cache;
    const juce::int64 key = ((juce::int64) w << 32) | ((juce::int64) h << 8) | (pressed ? 1 : 0) | ((juce::int64) (scale * 4) << 2);
    std::lock_guard<std::mutex> lock (mtx);
    auto& img = cache[key];
    if (! img.isValid()) img = renderKey ((float) w, (float) h, scale, pressed);
    return img;
}

// ---------------------------------------------------------------------------------------------
// Chrome stomp switch on a chamfered hex nut
// ---------------------------------------------------------------------------------------------
inline juce::Image renderStomp (float size, float scale, bool pressed)
{
    const float pad = size * 0.2f, W = size + pad * 2, c = W * 0.5f, Rn = size * 0.5f, Rp = size * 0.34f;
    const float Hn = size * 0.12f, Hp = size * (pressed ? 0.26f : 0.32f);
    auto hexD = [&] (float x, float y)
    {
        float dx = std::abs (x - c), dy = std::abs (y - c);
        // flat-topped hexagon distance
        const float k = 0.8660254f;
        return std::max (dx * k + dy * 0.5f, dy) - Rn * k;
    };
    auto height = [&] (float x, float y)
    {
        const float dx = x - c, dy = y - c, r = std::sqrt (dx * dx + dy * dy);
        if (r < Rp)
        {
            const float u = r / Rp;
            float z = Hn + (Hp - Hn) * std::sqrt (juce::jmax (0.0f, 1.0f - std::pow (u, 2.6f)));
            return z;
        }
        const float d = hexD (x, y);
        if (d > 0) return 0.0f;
        return Hn * juce::jmin (1.0f, -d / (size * 0.07f));   // chamfered nut faces
    };
    auto material = [&] (float x, float y, Material& m, float& ao)
    {
        const float dx = x - c, dy = y - c, r = std::sqrt (dx * dx + dy * dy);
        if (r < Rp) { m.base = { 0.95f, 0.95f, 0.96f }; m.metal = 1; m.rough = 0.07f; m.anisoU = 1.8f; m.anisoV = 0.4f; m.anisoAngle = std::atan2 (dy, dx); ao = 0.75f + 0.25f * smooth01 (Rp, Rp * 0.8f, r); }
        else { m.base = { 0.80f, 0.80f, 0.81f }; m.metal = 1; m.rough = 0.26f; ao = 0.6f + 0.4f * smooth01 (Rp, Rp * 1.25f, r); }
    };
    auto cover = [&] (float x, float y)
    {
        const float dx = x - c, dy = y - c, r = std::sqrt (dx * dx + dy * dy);
        if (r < Rp) return 1.0f;
        return juce::jlimit (0.0f, 1.0f, 0.5f - hexD (x, y) * scale);
    };
    auto img = renderSurface (0, 0, W, W, scale, height, material, cover, 1.0f, false);
    juce::Image out (juce::Image::ARGB, img.getWidth(), img.getHeight(), true);
    {
        juce::Graphics g (out);
        juce::Path hex;
        for (int i = 0; i < 6; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 6.0f;
            const juce::Point<float> p ((c + std::cos (a) * Rn) * scale, (c + std::sin (a) * Rn) * scale);
            i == 0 ? hex.startNewSubPath (p) : hex.lineTo (p);
        }
        hex.closeSubPath();
        juce::DropShadow (juce::Colours::black.withAlpha (0.5f), (int) (size * 0.12f * scale), { (int) (size * 0.11f * scale), (int) (size * 0.04f * scale) }).drawForPath (g, hex);
        g.setOpacity (1.0f); g.drawImageAt (img, 0, 0);
    }
    return out;
}

inline juce::Image cachedStomp (int size, bool pressed, float scale)
{
    static std::mutex mtx; static std::map<int, juce::Image> cache;
    const int key = size * 4 + (pressed ? 1 : 0) + (int) (scale * 1000) * 1000;
    std::lock_guard<std::mutex> lock (mtx);
    auto& img = cache[key];
    if (! img.isValid()) img = renderStomp ((float) size, scale, pressed);
    return img;
}

// ---------------------------------------------------------------------------------------------
// Domed hex-socket screw
// ---------------------------------------------------------------------------------------------
inline juce::Image cachedScrew (int r, float scale)
{
    static std::mutex mtx; static std::map<int, juce::Image> cache;
    std::lock_guard<std::mutex> lock (mtx);
    auto& img = cache[r * 100 + (int) (scale * 10)];
    if (img.isValid()) return img;
    const float W = (float) r * 2.6f, c = W * 0.5f, R = (float) r;
    auto hexD = [&] (float x, float y) { float dx = std::abs (x - c), dy = std::abs (y - c); return std::max (dx * 0.8660254f + dy * 0.5f, dy) - R * 0.38f * 0.8660254f; };
    auto height = [&] (float x, float y)
    {
        const float dx = x - c, dy = y - c, rr = std::sqrt (dx * dx + dy * dy);
        if (rr > R) return 0.0f;
        float z = R * 0.45f * std::sqrt (juce::jmax (0.0f, 1.0f - (rr / R) * (rr / R)));
        if (hexD (x, y) < 0) z -= R * 0.35f;
        return z;
    };
    auto material = [&] (float x, float y, Material& m, float& ao)
    {
        m.base = { 0.75f, 0.75f, 0.76f }; m.metal = 1; m.rough = 0.25f;
        if (hexD (x, y) < 0.5f) ao = 0.25f;
    };
    auto cover = [&] (float x, float y) { const float dx = x - c, dy = y - c; return juce::jlimit (0.0f, 1.0f, (R - std::sqrt (dx * dx + dy * dy)) * scale + 0.5f); };
    auto s = renderSurface (0, 0, W, W, scale, height, material, cover, 1.0f, false);
    juce::Image out (juce::Image::ARGB, s.getWidth(), s.getHeight(), true);
    {
        juce::Graphics g (out);
        juce::Path p; p.addEllipse ((c - R) * scale, (c - R) * scale, R * 2 * scale, R * 2 * scale);
        juce::DropShadow (juce::Colours::black.withAlpha (0.5f), (int) (R * 0.5f * scale), { (int) (R * 0.30f * scale), (int) (R * 0.10f * scale) }).drawForPath (g, p);
        g.setOpacity (1.0f); g.drawImageAt (s, 0, 0);
    }
    img = out;
    return img;
}

inline void drawScrew (juce::Graphics& g, juce::Point<float> c, float r)
{
    const float scale = 3.0f;
    auto img = cachedScrew ((int) r, scale);
    const float W = (float) (int) r * 2.6f;
    g.setOpacity (1.0f);
    g.drawImage (img, juce::Rectangle<float> (W, W).withCentre (c), juce::RectanglePlacement::stretchToFit);
}
} // namespace prism::ui::pbr
