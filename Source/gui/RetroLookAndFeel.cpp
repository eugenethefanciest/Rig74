#include "RetroLookAndFeel.h"
#include <Rig74BinaryData.h>

namespace rig::gui
{
using namespace juce;

//==============================================================================
static Typeface::Ptr scriptTypeface()
{
    static Typeface::Ptr tf = Typeface::createSystemTypefaceFor (Rig74BinaryData::ShrikhandRegular_ttf,
                                                                 (size_t) Rig74BinaryData::ShrikhandRegular_ttfSize);
    return tf;
}

Typeface::Ptr Fonts::labelTypeface()
{
    static Typeface::Ptr tf = Typeface::createSystemTypefaceFor (Rig74BinaryData::BebasNeueRegular_ttf,
                                                                 (size_t) Rig74BinaryData::BebasNeueRegular_ttfSize);
    return tf;
}

Font Fonts::script (float h) { return Font (FontOptions (scriptTypeface()).withHeight (h)); }
Font Fonts::label (float h)  { return Font (FontOptions (labelTypeface()).withHeight (h).withKerningFactor (0.06f)); }

//==============================================================================
namespace
{
inline uint8 u8 (float v) { return (uint8) jlimit (0, 255, roundToInt (v)); }

Image makeImage (int w, int h, std::function<Colour (int, int)> fn, bool alpha = false)
{
    Image img (alpha ? Image::ARGB : Image::RGB, w, h, true);
    Image::BitmapData bd (img, Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            bd.setPixelColour (x, y, fn (x, y));
    return img;
}

/** Smooth value noise on a grid (bilinear), tileable. */
struct ValueNoise
{
    int gw, gh; std::vector<float> v;
    ValueNoise (int gridW, int gridH, int seed) : gw (gridW), gh (gridH), v ((size_t) (gridW * gridH))
    {
        Random r (seed);
        for (auto& x : v) x = r.nextFloat();
    }
    float at (float x, float y) const
    {
        const int x0 = (int) std::floor (x), y0 = (int) std::floor (y);
        const float fx = x - (float) x0, fy = y - (float) y0;
        auto g = [this] (int i, int j) { i = ((i % gw) + gw) % gw; j = ((j % gh) + gh) % gh; return v[(size_t) (j * gw + i)]; };
        const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
        const float a = g (x0, y0) + sx * (g (x0 + 1, y0) - g (x0, y0));
        const float b = g (x0, y0 + 1) + sx * (g (x0 + 1, y0 + 1) - g (x0, y0 + 1));
        return a + sy * (b - a);
    }
};

Image makeWeave (int size, int thread, Colour base, Colour fleck, Colour light, int fleckEvery, int seed)
{
    Random rnd (seed);
    std::vector<int> colHash ((size_t) (size / thread + 1)), rowHash ((size_t) (size / thread + 1));
    for (auto& c : colHash) c = rnd.nextInt (100);
    for (auto& r : rowHash) r = rnd.nextInt (100);

    return makeImage (size, size, [&] (int x, int y)
    {
        const int cx = x / thread, cy = y / thread;
        const bool vertOnTop = ((cx + cy) & 1) == 0;
        const float px = (float) (x % thread) + 0.5f, py = (float) (y % thread) + 0.5f;
        const float profile = std::sin (MathConstants<float>::pi * (vertOnTop ? px : py) / (float) thread);
        const int hash = vertOnTop ? colHash[(size_t) cx] : rowHash[(size_t) cy];
        Colour c = (hash % fleckEvery == 0) ? fleck : ((hash % 7 == 0) ? light : base);
        const float shade = 0.45f + 0.55f * profile + (rnd.nextFloat() - 0.5f) * 0.12f;
        return Colour::fromFloatRGBA (c.getFloatRed() * shade, c.getFloatGreen() * shade, c.getFloatBlue() * shade, 1.0f);
    });
}
} // namespace

const Image& Textures::tolex()
{
    static Image img = []
    {
        ValueNoise n (128, 128, 74);
        ValueNoise fine (256, 256, 75);
        Random r (11);
        return makeImage (384, 384, [&] (int x, int y)
        {
            const float s = 3.0f;
            const float v = n.at (x / s, y / s), v2 = n.at ((x - 1) / s, (y - 1) / s);
            const float emboss = (v - v2) * 2.2f;
            const float grain = fine.at (x / 1.3f, y / 1.3f) * 0.5f;
            const float b = 0.10f + emboss * 0.10f + grain * 0.035f + (r.nextFloat() - 0.5f) * 0.015f;
            return Colour::fromFloatRGBA (b * 1.12f, b * 0.92f, b * 0.74f, 1.0f);
        });
    }();
    return img;
}

const Image& Textures::wood()
{
    static Image img = []
    {
        ValueNoise n (32, 256, 1974);
        Random r (5);
        return makeImage (96, 1024, [&] (int x, int y)
        {
            const float warp = 3.0f * n.at (x * 0.05f, y * 0.02f) + 1.2f * std::sin (y * 0.0045f + x * 0.03f);
            const float t = x * 0.42f + warp * 3.5f;
            float g = 0.5f + 0.5f * std::sin (t);
            g = std::pow (g, 4.0f);
            const float ring = 0.5f + 0.5f * std::sin (t * 0.31f + 1.7f);
            const float k = 0.55f * g + 0.25f * ring + (r.nextFloat() - 0.5f) * 0.05f;
            const Colour light (0xff8a5532), dark (0xff2f170b);
            return light.interpolatedWith (dark, jlimit (0.0f, 1.0f, k));
        });
    }();
    return img;
}

const Image& Textures::grille()
{
    static Image img = makeWeave (240, 3, Colour (0xff5b2119), Colour (0xffb08a52), Colour (0xff7a3324), 9, 68);
    return img;
}

const Image& Textures::basketWeave()
{
    static Image img = makeWeave (240, 4, Colour (0xff9d7f55), Colour (0xff4a3320), Colour (0xffc7ab7c), 5, 72);
    return img;
}

const Image& Textures::brushed()
{
    static Image img = []
    {
        Random r (3);
        std::vector<float> rows (256);
        float walk = 0.0f;
        for (auto& v : rows) { walk = walk * 0.85f + (r.nextFloat() - 0.5f) * 0.6f; v = walk; }
        return makeImage (512, 256, [&] (int x, int y)
        {
            const float streak = std::sin (x * 0.013f + rows[(size_t) y] * 3.0f) * 0.02f;
            const float b = 0.80f + rows[(size_t) y] * 0.05f + streak + (r.nextFloat() - 0.5f) * 0.05f;
            return Colour::fromFloatRGBA (b, b, b * 0.98f, 1.0f);
        });
    }();
    return img;
}

const Image& Textures::speckle()
{
    static Image img = []
    {
        Random r (9);
        return makeImage (256, 256, [&] (int, int)
        {
            const float n = r.nextFloat();
            if (n > 0.985f) return Colour (0x40ffffff);
            if (n < 0.02f)  return Colour (0x30000000);
            return Colour (0x00000000);
        }, true);
    }();
    return img;
}

void fillWithTexture (Graphics& g, const Image& img, Rectangle<float> area, float corner, float opacity)
{
    Graphics::ScopedSaveState s (g);
    Path p;
    if (corner > 0.0f) p.addRoundedRectangle (area, corner); else p.addRectangle (area);
    g.reduceClipRegion (p);
    g.setTiledImageFill (img, (int) area.getX(), (int) area.getY(), opacity);
    g.fillRect (area);
}

void drawScrew (Graphics& g, Point<float> c, float r, float angle)
{
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillEllipse (c.x - r + 0.6f, c.y - r + 1.0f, r * 2, r * 2);
    ColourGradient cg (Colour (0xfff2f2f2), c.x - r * 0.6f, c.y - r * 0.6f, Colour (0xff6c6c6c), c.x + r, c.y + r, true);
    g.setGradientFill (cg);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (Colour (0xff3a3a3a));
    const Point<float> d (std::cos (angle) * r * 0.75f, std::sin (angle) * r * 0.75f);
    g.drawLine (Line<float> (c - d, c + d), r * 0.32f);
}

static inline Point<float> polar (Point<float> c, float r, float a) { return { c.x + std::sin (a) * r, c.y - std::cos (a) * r }; }

//==============================================================================
RetroLookAndFeel::RetroLookAndFeel()
{
    setDefaultSansSerifTypeface (Fonts::labelTypeface());

    setColour (Slider::rotarySliderOutlineColourId, colours::cream);
    setColour (PopupMenu::backgroundColourId, Colour (0xff2a1a10));
    setColour (PopupMenu::textColourId, colours::cream);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::orange);
    setColour (PopupMenu::highlightedTextColourId, colours::brown);
    setColour (ComboBox::textColourId, colours::amber);
    setColour (Label::textColourId, colours::cream);
    setColour (TextButton::textColourOffId, colours::brown);
    setColour (TextButton::textColourOnId, colours::brown);
    setColour (ToggleButton::textColourId, colours::cream);
    setColour (TooltipWindow::textColourId, colours::brown);
    setColour (BubbleComponent::backgroundColourId, colours::cream);
}

//==============================================================================
void RetroLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float startA, float endA, Slider& s)
{
    const auto style = s.getProperties()["style"].toString();
    const auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float angle = startA + pos * (endA - startA);

    if (style == "selector")      drawSelector (g, area, pos, startA, endA, s);
    else if (style == "amp")
    {
        const float r = jmin (area.getWidth(), area.getHeight()) * 0.5f - 2.0f;
        drawChickenHead (g, area.getCentre(), r, angle, true, startA, endA, s.findColour (Slider::rotarySliderOutlineColourId));
    }
    else if (style == "rack")     drawRackKnob (g, area, angle, s);
    else                          drawPedalKnob (g, area, angle, s);
}

static void drawTicks (Graphics& g, Point<float> c, float r, float startA, float endA, Colour col, int n = 11)
{
    g.setColour (col);
    for (int i = 0; i < n; ++i)
    {
        const float a = startA + (float) i / (float) (n - 1) * (endA - startA);
        const bool major = (i % 5) == 0;
        g.drawLine (Line<float> (polar (c, r * (major ? 0.84f : 0.88f), a), polar (c, r, a)), major ? 1.8f : 1.1f);
    }
}

void RetroLookAndFeel::drawPedalKnob (Graphics& g, Rectangle<float> area, float angle, Slider& s)
{
    const float r = jmin (area.getWidth(), area.getHeight()) * 0.5f - 2.0f;
    const auto c = area.getCentre();
    const auto params = s.getRotaryParameters();
    drawTicks (g, c, r, params.startAngleRadians, params.endAngleRadians, s.findColour (Slider::rotarySliderOutlineColourId).withAlpha (0.85f));

    const float kr = r * 0.76f;
    // shadow
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (c.x - kr * 1.02f, c.y - kr * 0.92f, kr * 2.04f, kr * 2.1f);

    // knurled black skirt
    ColourGradient skirt (Colour (0xff3b3b3b), c.x, c.y - kr, Colour (0xff0b0b0b), c.x, c.y + kr, false);
    g.setGradientFill (skirt);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    for (int i = 0; i < 48; ++i)
    {
        const float a = (float) i / 48.0f * MathConstants<float>::twoPi + angle;
        g.setColour (Colours::white.withAlpha ((i & 1) ? 0.10f : 0.03f));
        g.drawLine (Line<float> (polar (c, kr * 0.80f, a), polar (c, kr * 0.98f, a)), 1.0f);
    }

    // chrome cap
    const float cr = kr * 0.62f;
    ColourGradient cap (Colour (0xfffafafa), c.x - cr * 0.5f, c.y - cr * 0.7f, Colour (0xff7d7d7d), c.x + cr * 0.6f, c.y + cr, true);
    cap.addColour (0.55, Colour (0xffc9c9c9));
    g.setGradientFill (cap);
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2, 1.0f);

    // pointer
    g.setColour (Colour (0xff1a1a1a));
    g.drawLine (Line<float> (polar (c, cr * 0.15f, angle), polar (c, cr * 0.9f, angle)), jmax (1.5f, kr * 0.09f));
    g.setColour (colours::cream);
    g.drawLine (Line<float> (polar (c, kr * 0.68f, angle), polar (c, kr * 0.98f, angle)), jmax (1.8f, kr * 0.11f));
}

void RetroLookAndFeel::drawChickenHead (Graphics& g, Point<float> c, float r, float angle, bool numbers, float startA, float endA, Colour mark)
{
    float kr = r;
    if (numbers)
    {
        g.setColour (mark);
        g.setFont (Fonts::label (jmax (9.0f, r * 0.26f)));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startA + (float) i / 10.0f * (endA - startA);
            const auto p = polar (c, r * 0.86f, a);
            g.drawText (String (i), Rectangle<float> (r * 0.4f, r * 0.3f).withCentre (p), Justification::centred);
        }
        kr = r * 0.68f;
    }

    // base disc
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - kr * 0.46f, c.y - kr * 0.40f, kr * 0.96f, kr * 0.96f);
    ColourGradient base (Colour (0xff3a3532), c.x, c.y - kr * 0.45f, Colour (0xff0e0c0b), c.x, c.y + kr * 0.45f, false);
    g.setGradientFill (base);
    g.fillEllipse (c.x - kr * 0.45f, c.y - kr * 0.45f, kr * 0.9f, kr * 0.9f);

    // pointer body
    Path p;
    p.startNewSubPath (0, -kr * 0.98f);
    p.lineTo (kr * 0.15f, -kr * 0.32f);
    p.quadraticTo (kr * 0.34f, kr * 0.08f, kr * 0.18f, kr * 0.32f);
    p.quadraticTo (0, kr * 0.46f, -kr * 0.18f, kr * 0.32f);
    p.quadraticTo (-kr * 0.34f, kr * 0.08f, -kr * 0.15f, -kr * 0.32f);
    p.closeSubPath();
    const auto t = AffineTransform::rotation (angle).translated (c);

    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillPath (p, t.translated (1.5f, 2.5f));
    ColourGradient body (Colour (0xff45403b), c.x - kr * 0.3f, c.y - kr * 0.6f, Colour (0xff0a0908), c.x + kr * 0.3f, c.y + kr * 0.5f, false);
    g.setGradientFill (body);
    g.fillPath (p, t);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.strokePath (p, PathStrokeType (1.0f), t);

    g.setColour (colours::cream);
    g.drawLine (Line<float> (polar (c, kr * 0.30f, angle), polar (c, kr * 0.90f, angle)), jmax (1.5f, kr * 0.07f));

    g.setColour (Colour (0xff1a1715));
    g.fillEllipse (c.x - kr * 0.11f, c.y - kr * 0.11f, kr * 0.22f, kr * 0.22f);
    g.setColour (Colours::white.withAlpha (0.25f));
    g.drawEllipse (c.x - kr * 0.11f, c.y - kr * 0.11f, kr * 0.22f, kr * 0.22f, 0.8f);
}

void RetroLookAndFeel::drawRackKnob (Graphics& g, Rectangle<float> area, float angle, Slider& s)
{
    const float r = jmin (area.getWidth(), area.getHeight()) * 0.5f - 2.0f;
    const auto c = area.getCentre();
    const auto params = s.getRotaryParameters();
    drawTicks (g, c, r, params.startAngleRadians, params.endAngleRadians, s.findColour (Slider::rotarySliderOutlineColourId));

    const float kr = r * 0.76f;
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillEllipse (c.x - kr, c.y - kr * 0.9f, kr * 2, kr * 2.05f);

    ColourGradient skirt (Colour (0xffefefef), c.x - kr * 0.4f, c.y - kr, Colour (0xff8e8e8e), c.x + kr * 0.4f, c.y + kr, false);
    g.setGradientFill (skirt);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    for (int i = 1; i < 5; ++i)
    {
        const float rr = kr * (0.65f + 0.08f * (float) i);
        g.setColour (Colours::black.withAlpha (0.07f));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.7f);
    }

    const float cr = kr * 0.6f;
    ColourGradient cap (Colour (0xff4a4a4a), c.x, c.y - cr, Colour (0xff121212), c.x, c.y + cr, false);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);

    g.setColour (Colour (0xff111111));
    g.drawLine (Line<float> (polar (c, kr * 0.66f, angle), polar (c, kr * 0.98f, angle)), jmax (1.5f, kr * 0.09f));
    g.setColour (colours::orange);
    g.drawLine (Line<float> (polar (c, cr * 0.2f, angle), polar (c, cr * 0.92f, angle)), jmax (1.5f, kr * 0.09f));
}

void RetroLookAndFeel::drawSelector (Graphics& g, Rectangle<float> area, float pos, float startA, float endA, Slider& s)
{
    const auto labels = StringArray::fromTokens (s.getProperties()["labels"].toString(), "|", "");
    const int n = jmax (1, labels.size());
    const bool fixedSize = s.getProperties().contains ("knobRadius");
    float labelFactor = s.getProperties().contains ("labelFactor") ? (float) s.getProperties()["labelFactor"] : 1.55f;
    float kr;
    Point<float> c;
    if (fixedSize)
    {
        kr = (float) s.getProperties()["knobRadius"];
        c = { area.getCentreX(), area.getCentreY() + kr * 0.25f };
    }
    else
    {
        // fit knob + the label arc above it inside the slot
        labelFactor = jmin (labelFactor, 1.45f);
        kr = jmin (area.getWidth() * 0.23f, (area.getHeight() - 8.0f) / (labelFactor + 1.12f));
        c = { area.getCentreX(), area.getBottom() - kr * 1.1f };
    }
    const float lr = kr * labelFactor;
    const int current = jlimit (0, n - 1, roundToInt (pos * (float) (n - 1)));
    const auto mark = s.findColour (Slider::rotarySliderOutlineColourId);

    g.setFont (Fonts::label (jlimit (13.0f, 18.0f, kr * 0.62f)));
    for (int i = 0; i < n; ++i)
    {
        const float a = startA + (n > 1 ? (float) i / (float) (n - 1) : 0.5f) * (endA - startA);
        const auto dot = polar (c, kr * 1.2f, a);
        if (i == current)
        {
            g.setColour (colours::ledRed.withAlpha (0.35f));
            g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (dot));
            g.setColour (colours::ledRed);
            g.fillEllipse (Rectangle<float> (4.5f, 4.5f).withCentre (dot));
        }
        else
        {
            g.setColour (mark.withAlpha (0.6f));
            g.fillEllipse (Rectangle<float> (3.5f, 3.5f).withCentre (dot));
        }

        const auto p = polar (c, lr, a);
        const auto text = labels[i];
        const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 4.0f;
        const float th = g.getCurrentFont().getHeight();
        const float sa = std::sin (a);
        Rectangle<float> box;
        Justification just = Justification::centred;
        if (sa < -0.3f)     { box = { p.x - tw, p.y - th * 0.5f, tw, th }; just = Justification::centredRight; }
        else if (sa > 0.3f) { box = { p.x, p.y - th * 0.5f, tw, th };      just = Justification::centredLeft; }
        else                box = Rectangle<float> (tw, th).withCentre ({ p.x, p.y - th * 0.15f });

        g.setColour (i == current ? colours::amber.brighter (0.2f) : mark.withAlpha (0.8f));
        if (i == current && mark.getBrightness() < 0.4f) g.setColour (colours::rust);
        g.drawText (text, box, just, false);
    }

    const float angle = startA + pos * (endA - startA);
    drawChickenHead (g, c, kr, angle, false, startA, endA, mark);
}

//==============================================================================
void RetroLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                                         Slider::SliderStyle style, Slider& s)
{
    if (style != Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, style, s);
        return;
    }

    const auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float cx = area.getCentreX();
    const auto mark = s.findColour (Slider::rotarySliderOutlineColourId);

    // scale
    g.setColour (mark.withAlpha (0.7f));
    for (int i = 0; i <= 12; ++i)
    {
        const float ty = area.getY() + area.getHeight() * (float) i / 12.0f;
        const float len = (i % 6 == 0) ? w * 0.42f : w * 0.26f;
        g.drawLine (cx - len, ty, cx - 6.0f, ty, i % 6 == 0 ? 1.4f : 0.8f);
        g.drawLine (cx + 6.0f, ty, cx + len, ty, i % 6 == 0 ? 1.4f : 0.8f);
    }

    // groove
    auto groove = Rectangle<float> (cx - 3.0f, area.getY() - 4.0f, 6.0f, area.getHeight() + 8.0f);
    g.setColour (Colour (0xff0d0a08));
    g.fillRoundedRectangle (groove, 3.0f);
    g.setColour (Colours::white.withAlpha (0.25f));
    g.drawRoundedRectangle (groove.translated (0, 1), 3.0f, 0.6f);

    // fader cap
    const float tw = jmin ((float) w * 0.82f, 30.0f), th = 16.0f;
    auto cap = Rectangle<float> (tw, th).withCentre ({ cx, sliderPos });
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (cap.translated (1.0f, 2.5f), 2.5f);
    ColourGradient cg (Colour (0xff4a4440), cap.getX(), cap.getY(), Colour (0xff100e0c), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (cg);
    g.fillRoundedRectangle (cap, 2.5f);
    g.setColour (Colours::white.withAlpha (0.2f));
    g.drawLine (cap.getX() + 2, cap.getY() + 1.5f, cap.getRight() - 2, cap.getY() + 1.5f, 1.0f);
    g.setColour (colours::cream);
    g.drawLine (cap.getX() + 3, cap.getCentreY(), cap.getRight() - 3, cap.getCentreY(), 2.0f);
}

//==============================================================================
void RetroLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool down)
{
    const auto style = b.getProperties()["style"].toString();
    auto area = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();

    if (style == "foot")
    {
        const float R = jmin (area.getWidth(), area.getHeight()) * 0.48f;
        const auto c = area.getCentre();
        Path hex;
        for (int i = 0; i < 6; ++i)
        {
            const float a = MathConstants<float>::pi / 6.0f + (float) i * MathConstants<float>::pi / 3.0f;
            const Point<float> p (c.x + std::cos (a) * R, c.y + std::sin (a) * R);
            if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
        }
        hex.closeSubPath();
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillPath (hex, AffineTransform::translation (1.0f, 2.5f));
        ColourGradient nut (Colour (0xffe6e6e6), c.x - R, c.y - R, Colour (0xff5d5d5d), c.x + R, c.y + R, false);
        g.setGradientFill (nut);
        g.fillPath (hex);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.strokePath (hex, PathStrokeType (1.0f));

        const float cr = R * (down ? 0.58f : 0.62f);
        const auto cc = c.translated (0, down ? 1.0f : 0.0f);
        ColourGradient cap (Colour (0xffffffff), cc.x - cr * 0.5f, cc.y - cr * 0.6f, Colour (0xff767676), cc.x + cr * 0.7f, cc.y + cr, true);
        cap.addColour (0.6, Colour (0xffbdbdbd));
        g.setGradientFill (cap);
        g.fillEllipse (cc.x - cr, cc.y - cr, cr * 2, cr * 2);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawEllipse (cc.x - cr, cc.y - cr, cr * 2, cr * 2, 1.0f);
        if (highlighted)
        {
            g.setColour (Colours::white.withAlpha (0.12f));
            g.fillEllipse (cc.x - cr, cc.y - cr, cr * 2, cr * 2);
        }
        return;
    }

    if (style == "jewel")
    {
        const float R = jmin (area.getWidth(), area.getHeight()) * 0.5f;
        const auto c = area.getCentre();
        if (on)
        {
            ColourGradient glow (colours::ledRed.withAlpha (0.45f), c.x, c.y, colours::ledRed.withAlpha (0.0f), c.x + R, c.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
        }
        const float br = R * 0.62f;
        ColourGradient bez (Colour (0xfff0f0f0), c.x - br, c.y - br, Colour (0xff555555), c.x + br, c.y + br, false);
        g.setGradientFill (bez);
        g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
        const float jr = br * 0.74f;
        ColourGradient jewel (on ? Colour (0xffff8a6a) : Colour (0xff6a1b14), c.x - jr * 0.3f, c.y - jr * 0.3f,
                              on ? Colour (0xff9a1208) : Colour (0xff250705), c.x + jr, c.y + jr, true);
        g.setGradientFill (jewel);
        g.fillEllipse (c.x - jr, c.y - jr, jr * 2, jr * 2);
        g.setColour (Colours::white.withAlpha (on ? 0.25f : 0.1f));
        for (int i = 0; i < 8; ++i)
        {
            const float a = (float) i * MathConstants<float>::pi / 4.0f;
            g.drawLine (Line<float> (c, polar (c, jr * 0.95f, a)), 0.7f);
        }
        g.setColour (Colours::white.withAlpha (0.55f));
        g.fillEllipse (c.x - jr * 0.5f, c.y - jr * 0.55f, jr * 0.35f, jr * 0.25f);
        return;
    }

    if (style == "bat")
    {
        const float R = jmin (area.getHeight() * 0.5f, 18.0f);
        const auto c = Point<float> (area.getX() + R + 2.0f, area.getCentreY());
        ColourGradient nut (Colour (0xffeeeeee), c.x - R, c.y - R, Colour (0xff606060), c.x + R, c.y + R, false);
        g.setGradientFill (nut);
        g.fillEllipse (c.x - R * 0.55f, c.y - R * 0.55f, R * 1.1f, R * 1.1f);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillEllipse (c.x - R * 0.22f, c.y - R * 0.22f, R * 0.44f, R * 0.44f);

        const auto tip = Point<float> (c.x, c.y + (on ? -R * 0.95f : R * 0.95f));
        g.setColour (Colours::black.withAlpha (0.4f));
        g.drawLine (Line<float> (c.translated (1.5f, 1.5f), tip.translated (1.5f, 1.5f)), R * 0.28f);
        g.setColour (Colour (0xffd9d9d9));
        g.drawLine (Line<float> (c, tip), R * 0.26f);
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (R * 0.42f, R * 0.42f).withCentre (tip));

        g.setColour (b.findColour (ToggleButton::textColourId));
        g.setFont (Fonts::label (jmin (16.0f, area.getHeight() * 0.55f)));
        g.drawText (b.getButtonText(), area.withLeft (c.x + R + 4.0f), Justification::centredLeft, false);
        return;
    }

    if (style == "led")
    {
        auto r = area.reduced (1.0f);
        ColourGradient bg (Colour (0xff3a2a1e), r.getX(), r.getY(), Colour (0xff150d08), r.getX(), r.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (Colours::black);
        g.drawRoundedRectangle (r, 4.0f, 1.2f);
        const auto ledC = Point<float> (r.getX() + 12.0f, r.getCentreY());
        if (on)
        {
            g.setColour (colours::amber.withAlpha (0.35f));
            g.fillEllipse (Rectangle<float> (14.0f, 14.0f).withCentre (ledC));
        }
        g.setColour (on ? colours::amber : Colour (0xff4a3210));
        g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (ledC));
        g.setColour (on ? colours::amber : colours::creamDark);
        g.setFont (Fonts::label (jmin (17.0f, r.getHeight() * 0.62f)));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (18.0f), Justification::centred, false);
        if (highlighted) { g.setColour (Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (r, 4.0f); }
        return;
    }

    LookAndFeel_V4::drawToggleButton (g, b, highlighted, down);
}

void RetroLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0, 2.0f), 4.0f);
    if (down) r = r.translated (0, 1.0f);
    ColourGradient cg (Colour (0xfff6ecd3), r.getX(), r.getY(), Colour (0xffc9b385), r.getX(), r.getBottom(), false);
    g.setGradientFill (cg);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::brown.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 4.0f, 1.2f);
    if (highlighted) { g.setColour (Colours::white.withAlpha (0.15f)); g.fillRoundedRectangle (r, 4.0f); }
}

void RetroLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool down)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (colours::brown);
    g.drawText (b.getButtonText(), b.getLocalBounds().translated (0, down ? 2 : 1), Justification::centred, false);
}

Font RetroLookAndFeel::getTextButtonFont (TextButton&, int h) { return Fonts::label (jmin (19.0f, (float) h * 0.62f)); }

//==============================================================================
void RetroLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    ColourGradient bg (Colour (0xff1b100a), 0, 0, Colour (0xff2c1a0f), 0, (float) h, false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (r, 4.0f, 1.5f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawLine (r.getX() + 4, r.getBottom() - 0.5f, r.getRight() - 4, r.getBottom() - 0.5f, 1.0f);

    // amber glow behind the text
    ColourGradient glow (colours::amber.withAlpha (0.12f), r.getCentreX(), r.getCentreY(), colours::amber.withAlpha (0.0f), r.getRight(), r.getCentreY(), true);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (r, 4.0f);

    Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 5, ay - 3, ax + 5, ay - 3, ax, ay + 4);
    g.setColour (box.findColour (ComboBox::textColourId).withAlpha (box.isEnabled() ? 0.9f : 0.3f));
    g.fillPath (arrow);
}

Font RetroLookAndFeel::getComboBoxFont (ComboBox& b) { return Fonts::label (jmin (20.0f, (float) b.getHeight() * 0.66f)); }

void RetroLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (Justification::centred);
}

Font RetroLookAndFeel::getPopupMenuFont() { return Fonts::label (19.0f); }

void RetroLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (findColour (PopupMenu::backgroundColourId));
    g.setColour (colours::orange.withAlpha (0.6f));
    g.drawRect (0, 0, w, h, 1);
}

void RetroLookAndFeel::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                          bool isTicked, bool, const String& text, const String&, const Drawable*, const Colour*)
{
    if (isSeparator)
    {
        g.setColour (colours::creamDark.withAlpha (0.3f));
        g.fillRect (area.reduced (8, area.getHeight() / 2 - 1).withHeight (1));
        return;
    }
    auto r = area.reduced (2, 0);
    if (isHighlighted && isActive)
    {
        g.setColour (findColour (PopupMenu::highlightedBackgroundColourId));
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
    }
    g.setColour (isHighlighted ? findColour (PopupMenu::highlightedTextColourId)
                               : findColour (PopupMenu::textColourId).withAlpha (isActive ? 1.0f : 0.4f));
    g.setFont (getPopupMenuFont());
    g.drawText (text, r.withTrimmedLeft (22), Justification::centredLeft, true);
    if (isTicked)
    {
        g.setColour (isHighlighted ? colours::brown : colours::amber);
        g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre ({ (float) r.getX() + 11.0f, (float) r.getCentreY() }));
    }
}

//==============================================================================
void RetroLookAndFeel::drawBubble (Graphics& g, BubbleComponent&, const Point<float>&, const Rectangle<float>& body)
{
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (body.translated (1, 2), 4.0f);
    g.setColour (colours::cream);
    g.fillRoundedRectangle (body, 4.0f);
    g.setColour (colours::brown);
    g.drawRoundedRectangle (body, 4.0f, 1.2f);
}

Font RetroLookAndFeel::getSliderPopupFont (Slider&) { return Fonts::label (17.0f); }
int RetroLookAndFeel::getSliderPopupPlacement (Slider&) { return BubbleComponent::above; }
Font RetroLookAndFeel::getLabelFont (Label& l) { return Fonts::label (jmax (12.0f, l.getFont().getHeight())); }
} // namespace rig::gui
