#include "Widgets.h"
#include "../Parameters.h"

namespace rig::gui
{
using namespace juce;

static inline Point<float> polarPoint (Point<float> c, float r, float a) { return { c.x + std::sin (a) * r, c.y - std::cos (a) * r }; }

static void setupRotary (Slider& s, APVTS& st, const String& id)
{
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (MathConstants<float>::pi * 1.22f, MathConstants<float>::pi * 2.78f, true);
    s.setMouseDragSensitivity (220);
    if (auto* p = st.getParameter (id))
        s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

//==============================================================================
Knob::Knob (APVTS& st, const String& id, const String& label, const String& style, Colour textColour)
    : text (label), colour (textColour)
{
    setupRotary (slider, st, id);
    slider.getProperties().set ("style", style);
    slider.setColour (Slider::rotarySliderOutlineColourId, textColour);
    slider.setPopupDisplayEnabled (true, false, nullptr);
    if (auto* p = st.getParameter (id)) slider.setTooltip (p->getName (64));
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (st, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    const int labelH = jlimit (12, 20, getHeight() / 5);
    r.removeFromBottom (labelH);
    const int side = jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (side, side));
}

void Knob::paint (Graphics& g)
{
    const int labelH = jlimit (12, 20, getHeight() / 5);
    g.setColour (colour);
    g.setFont (Fonts::label ((float) labelH * 0.95f));
    g.drawText (text, getLocalBounds().removeFromBottom (labelH + 1), Justification::centred, false);
}

//==============================================================================
Selector::Selector (APVTS& st, const String& id, const String& t, const StringArray& labels, Colour textColour, float halfSpan, float labelFactor)
    : title (t), colour (textColour)
{
    setupRotary (slider, st, id);
    const float top = MathConstants<float>::twoPi;
    slider.setRotaryParameters (top - halfSpan, top + halfSpan, true);
    slider.setMouseDragSensitivity (90);
    slider.setDoubleClickReturnValue (false, 0.0);   // clicks step through the positions instead
    slider.getProperties().set ("style", "selector");
    slider.getProperties().set ("labels", labels.joinIntoString ("|"));
    slider.getProperties().set ("labelFactor", labelFactor);
    slider.setColour (Slider::rotarySliderOutlineColourId, textColour);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (st, id, slider);
}

void Selector::resized()
{
    auto r = getLocalBounds();
    if (title.isNotEmpty()) r.removeFromBottom (jlimit (12, 20, getHeight() / 5));
    slider.setBounds (r);
}

void Selector::paint (Graphics& g)
{
    if (title.isEmpty()) return;
    const int labelH = jlimit (12, 20, getHeight() / 5);
    g.setColour (colour);
    g.setFont (Fonts::label ((float) labelH * 0.95f));
    g.drawText (title, getLocalBounds().removeFromBottom (labelH + 1), Justification::centred, false);
}

//==============================================================================
FxPanel::FxPanel (APVTS& st, const String& bypassId, const String& t, const String& cap, Colour b, Style s)
    : state (st), title (t), caption (cap), body (b), style (s)
{
    bypass.getProperties().set ("style", "foot");
    bypass.setTooltip ("On / Off");
    addAndMakeVisible (bypass);
    bypassAttachment = std::make_unique<APVTS::ButtonAttachment> (st, bypassId, bypass);
    ledOn = bypass.getToggleState();
}

Colour FxPanel::textColour() const
{
    if (style == Style::Echo) return colours::brown;
    return body.getPerceivedBrightness() > 0.58f ? colours::brown : colours::cream;
}

Knob& FxPanel::knob (const String& id, const String& label, const String& knobStyle)
{
    return own (std::make_unique<Knob> (state, id, label, knobStyle, textColour()));
}

Selector& FxPanel::selector (const String& id, const String& label, const StringArray& labels, float halfSpan)
{
    return own (std::make_unique<Selector> (state, id, label, labels, textColour(), halfSpan));
}

void FxPanel::refreshLed()
{
    if (bypass.getToggleState() != ledOn)
    {
        ledOn = bypass.getToggleState();
        repaint (ledArea.expanded (10).getSmallestIntegerContainer());
    }
}

void FxPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    const int h = getHeight();
    const int footH = jlimit (44, 64, (int) (h * 0.2f));
    const int titleH = jlimit (34, 52, (int) (h * 0.16f));

    auto foot = r.removeFromBottom (footH);
    bypass.setBounds (foot.withSizeKeepingCentre (footH - 4, footH - 4));
    titleArea = r.removeFromBottom (titleH).toFloat();

    ledArea = Rectangle<float> (10.0f, 10.0f).withCentre ({ (float) getWidth() * 0.5f, 15.0f });
    r.removeFromTop (14);

    if (rows.empty()) return;
    const int rowH = r.getHeight() / (int) rows.size();
    for (auto& row : rows)
    {
        auto rr = r.removeFromTop (rowH);
        float total = 0.0f;
        for (auto& [c, w] : row) total += w;
        const float unit = (float) rr.getWidth() / jmax (1.0f, total);
        float x = (float) rr.getX();
        for (auto& [c, w] : row)
        {
            const int cw = (int) (unit * w);
            if (c != nullptr) c->setBounds (Rectangle<int> ((int) x, rr.getY(), cw, rr.getHeight()).reduced (2, 1));
            x += unit * w;
        }
    }
}

void FxPanel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const float corner = 12.0f;

    // drop shadow
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (2.0f, 4.0f), corner);

    if (style == Style::Echo)
    {
        ColourGradient cg (colours::cream.brighter (0.05f), r.getX(), r.getY(), colours::creamDark, r.getX(), r.getBottom(), false);
        g.setGradientFill (cg);
        g.fillRoundedRectangle (r, corner);
        // brown & orange 70s stripes either side of the stompswitch
        const auto fs = bypass.getBounds().toFloat();
        const float y = fs.getCentreY() - 6.0f;
        for (auto seg : { Rectangle<float> (r.getX() + 16.0f, y, fs.getX() - 12.0f - (r.getX() + 16.0f), 12.0f),
                          Rectangle<float> (fs.getRight() + 12.0f, y, r.getRight() - 16.0f - (fs.getRight() + 12.0f), 12.0f) })
        {
            g.setColour (colours::chocolate); g.fillRect (seg.withHeight (4.0f));
            g.setColour (colours::orange);    g.fillRect (seg.withY (y + 5.0f).withHeight (3.0f));
            g.setColour (colours::mustard);   g.fillRect (seg.withY (y + 9.0f).withHeight (3.0f));
        }
    }
    else
    {
        ColourGradient cg (body.brighter (0.18f), r.getX(), r.getY(), body.darker (0.25f), r.getX(), r.getBottom(), false);
        g.setGradientFill (cg);
        g.fillRoundedRectangle (r, corner);
        fillWithTexture (g, Textures::speckle(), r, corner, 0.6f);
    }

    // bevel
    g.setColour (Colours::white.withAlpha (0.22f));
    g.drawRoundedRectangle (r.reduced (1.0f), corner - 1.0f, 1.2f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r, corner, 1.2f);

    // screws
    const float sr = 4.2f, inset = 9.0f;
    drawScrew (g, { r.getX() + inset, r.getY() + inset }, sr, 0.3f);
    drawScrew (g, { r.getRight() - inset, r.getY() + inset }, sr, 1.1f);
    drawScrew (g, { r.getX() + inset, r.getBottom() - inset }, sr, 2.0f);
    drawScrew (g, { r.getRight() - inset, r.getBottom() - inset }, sr, 0.8f);

    // LED
    const auto c = ledArea.getCentre();
    if (ledOn)
    {
        ColourGradient glow (colours::ledRed.withAlpha (0.6f), c.x, c.y, colours::ledRed.withAlpha (0.0f), c.x + 12.0f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (Rectangle<float> (24.0f, 24.0f).withCentre (c));
    }
    g.setColour (Colours::black.withAlpha (0.7f));
    g.fillEllipse (ledArea.expanded (1.5f));
    ColourGradient led (ledOn ? Colour (0xffffb0a0) : Colour (0xff5a1510), c.x - 2, c.y - 2,
                        ledOn ? colours::ledRed : Colour (0xff2a0805), c.x + 4, c.y + 4, true);
    g.setGradientFill (led);
    g.fillEllipse (ledArea);

    // title + caption
    auto t = titleArea;
    const float capH = caption.isNotEmpty() ? jmin (15.0f, t.getHeight() * 0.3f) : 0.0f;
    auto capArea = t.removeFromBottom (capH);
    const auto tc = textColour();
    g.setFont (Fonts::script (jmin (t.getHeight() * 0.95f, 40.0f)));
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawText (title, t.translated (1.5f, 2.0f), Justification::centred, false);
    g.setColour (style == Style::Echo ? colours::chocolate : tc);
    g.drawText (title, t, Justification::centred, false);
    if (capH > 0.0f)
    {
        g.setFont (Fonts::label (capH));
        g.setColour (tc.withAlpha (0.8f));
        g.drawText (caption, capArea, Justification::centred, false);
    }
}

//==============================================================================
VuMeter::VuMeter (const String& l) : label (l) { setInterceptsMouseClicks (false, false); }

float VuMeter::levelToPos (float dbfs)
{
    const float vu = jlimit (-20.0f, 3.0f, dbfs + 18.0f);   // 0 VU = -18 dBFS
    const float lo = std::pow (10.0f, -20.0f / 20.0f), hi = std::pow (10.0f, 3.0f / 20.0f);
    return (std::pow (10.0f, vu / 20.0f) - lo) / (hi - lo);
}

void VuMeter::setTuner (const String& n, float cents, bool valid)
{
    tunerValid = valid;
    note = n;
    centsValue = cents;
    targetPos = valid ? jlimit (0.0f, 1.0f, (cents + 50.0f) / 100.0f) : 0.5f;
}

void VuMeter::tick()
{
    const float coef = targetPos > pos ? 0.35f : 0.18f;
    const float next = pos + (targetPos - pos) * coef;
    if (std::abs (next - pos) > 0.0005f || tunerMode) { pos = next; repaint(); }
}

void VuMeter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // bezel
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (1.5f, 3.0f), 8.0f);
    ColourGradient bez (Colour (0xff3a2a1e), r.getX(), r.getY(), Colour (0xff0e0907), r.getX(), r.getBottom(), false);
    g.setGradientFill (bez);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Colour (0xffb9b0a0));
    g.drawRoundedRectangle (r.reduced (3.0f), 6.0f, 1.0f);

    auto face = r.reduced (7.0f);
    ColourGradient glow (Colour (0xfffff3cc), face.getCentreX(), face.getY() + face.getHeight() * 0.65f,
                         Colour (0xffd9bd78), face.getX(), face.getY(), true);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (face, 4.0f);

    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (face.toNearestInt());

    const Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.38f);
    const float R = face.getHeight() * 1.02f;
    const float span = 0.70f;
    auto angleFor = [span] (float p) { return -span + 2.0f * span * p; };

    const Colour ink (0xff2a1c12);
    g.setFont (Fonts::label (jmax (10.0f, face.getHeight() * 0.15f)));

    if (! tunerMode)
    {
        // black arc, red arc
        Path arcBlack, arcRed;
        const float p0 = levelToPos (-18.0f);
        arcBlack.addCentredArc (pivot.x, pivot.y, R, R, 0.0f, angleFor (0.0f), angleFor (p0), true);
        arcRed.addCentredArc (pivot.x, pivot.y, R, R, 0.0f, angleFor (p0), angleFor (1.0f), true);
        g.setColour (ink);
        g.strokePath (arcBlack, PathStrokeType (1.4f));
        g.setColour (Colour (0xffc0261b));
        g.strokePath (arcRed, PathStrokeType (4.0f));

        const float marks[] = { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
        for (float m : marks)
        {
            const float a = angleFor (levelToPos (m - 18.0f));
            const bool red = m >= 0;
            g.setColour (red ? Colour (0xffc0261b) : ink);
            g.drawLine (Line<float> (polarPoint (pivot, R, a), polarPoint (pivot, R + face.getHeight() * 0.09f, a)), 1.3f);
            if (m == -20 || m == -10 || m == -7 || m == -5 || m == -3 || m == 0 || m == 3)
            {
                const auto p = polarPoint (pivot, R + face.getHeight() * 0.2f, a);
                g.drawText (String ((int) std::abs (m)), Rectangle<float> (24.0f, 14.0f).withCentre (p), Justification::centred, false);
            }
        }
        g.setColour (ink);
        g.setFont (Fonts::label (face.getHeight() * 0.26f));
        g.drawText ("VU", Rectangle<float> (60.0f, face.getHeight() * 0.3f).withCentre ({ face.getCentreX(), face.getY() + face.getHeight() * 0.68f }),
                    Justification::centred, false);
    }
    else
    {
        for (int c = -50; c <= 50; c += 10)
        {
            const float a = angleFor ((float) (c + 50) / 100.0f);
            g.setColour (c == 0 ? Colour (0xff2d7a2d) : ink);
            const float len = c == 0 ? 0.16f : 0.09f;
            g.drawLine (Line<float> (polarPoint (pivot, R, a), polarPoint (pivot, R + face.getHeight() * len, a)), c == 0 ? 2.2f : 1.2f);
        }
        Path ok;
        ok.addCentredArc (pivot.x, pivot.y, R, R, 0.0f, angleFor (0.45f), angleFor (0.55f), true);
        g.setColour (Colour (0xff2d9a2d));
        g.strokePath (ok, PathStrokeType (4.0f));

        const bool inTune = tunerValid && std::abs (centsValue) < 5.0f;
        g.setColour (inTune ? Colour (0xff1e7a1e) : ink);
        g.setFont (Fonts::script (face.getHeight() * 0.36f));
        g.drawText (tunerValid ? note : String ("-"),
                    Rectangle<float> (face.getWidth(), face.getHeight() * 0.42f).withCentre ({ face.getCentreX(), face.getY() + face.getHeight() * 0.58f }),
                    Justification::centred, false);
        g.setFont (Fonts::label (face.getHeight() * 0.14f));
        g.setColour (ink.withAlpha (0.8f));
        g.drawText ("FLAT", face.reduced (6.0f, 4.0f), Justification::bottomLeft, false);
        g.drawText ("SHARP", face.reduced (6.0f, 4.0f), Justification::bottomRight, false);
    }

    // label
    g.setColour (ink.withAlpha (0.75f));
    g.setFont (Fonts::label (jmax (10.0f, face.getHeight() * 0.14f)));
    if (tunerMode) g.drawText ("TUNER", face.reduced (6.0f, 4.0f), Justification::topLeft, false);

    // needle
    const float a = angleFor (pos);
    g.setColour (Colours::black.withAlpha (0.18f));
    g.drawLine (Line<float> (pivot.translated (3.0f, 2.0f), polarPoint (pivot, R + face.getHeight() * 0.12f, a).translated (3.0f, 2.0f)), 1.6f);
    g.setColour (tunerMode ? Colour (0xffc0261b) : Colour (0xff120c08));
    g.drawLine (Line<float> (pivot, polarPoint (pivot, R + face.getHeight() * 0.12f, a)), 1.5f);

    // glass reflection
    ColourGradient glass (Colours::white.withAlpha (0.28f), face.getX(), face.getY(), Colours::white.withAlpha (0.0f), face.getX(), face.getCentreY(), false);
    g.setGradientFill (glass);
    g.fillRect (face.withHeight (face.getHeight() * 0.45f));
}

//==============================================================================
AmpPanel::AmpPanel (APVTS& st)
{
    const auto ink = colours::brown;
    model = std::make_unique<Selector> (st, "amp_model", "", params::ampModels(), ink, 1.95f, 1.6f);
    addAndMakeVisible (*model);
    gain     = std::make_unique<Knob> (st, "amp_gain", "GAIN", "amp", ink);
    bass     = std::make_unique<Knob> (st, "amp_bass", "BASS", "amp", ink);
    mid      = std::make_unique<Knob> (st, "amp_mid", "MIDDLE", "amp", ink);
    treble   = std::make_unique<Knob> (st, "amp_treble", "TREBLE", "amp", ink);
    presence = std::make_unique<Knob> (st, "amp_presence", "PRESENCE", "amp", ink);
    master   = std::make_unique<Knob> (st, "amp_master", "MASTER", "amp", ink);
    for (auto* k : { gain.get(), bass.get(), mid.get(), treble.get(), presence.get(), master.get() })
        addAndMakeVisible (*k);

    power.getProperties().set ("style", "jewel");
    power.setTooltip ("Amp on / off");
    addAndMakeVisible (power);
    powerAttachment = std::make_unique<APVTS::ButtonAttachment> (st, "amp_on", power);
}

void AmpPanel::resized()
{
    auto r = getLocalBounds().toFloat().reduced (14.0f, 12.0f);
    grilleArea = r.removeFromTop (r.getHeight() * 0.36f);
    panelArea = r;

    auto p = panelArea.toNearestInt().reduced (16, 6);
    auto jewel = p.removeFromRight (70);
    power.setBounds (jewel.withSizeKeepingCentre (56, 56).translated (0, -6));
    model->setBounds (p.removeFromLeft (250));
    model->setKnobRadius ((float) jmin (32, p.getHeight() / 3));
    const int kw = p.getWidth() / 6;
    for (auto* k : { gain.get(), bass.get(), mid.get(), treble.get(), presence.get(), master.get() })
        k->setBounds (p.removeFromLeft (kw).reduced (2, 0));
}

void AmpPanel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (2.0f, 5.0f), 14.0f);

    // tolex box with cream piping
    g.setColour (Colour (0xff141010));
    g.fillRoundedRectangle (r, 14.0f);
    fillWithTexture (g, Textures::tolex(), r, 14.0f, 1.0f);
    g.setColour (colours::cream.withAlpha (0.85f));
    g.drawRoundedRectangle (r.reduced (6.0f), 10.0f, 2.0f);

    // grille cloth
    fillWithTexture (g, Textures::grille(), grilleArea, 6.0f);
    ColourGradient shade (Colours::black.withAlpha (0.0f), grilleArea.getX(), grilleArea.getY(), Colours::black.withAlpha (0.4f), grilleArea.getX(), grilleArea.getBottom(), false);
    g.setGradientFill (shade);
    g.fillRoundedRectangle (grilleArea, 6.0f);

    // logo badge (script, gold)
    auto badge = Rectangle<float> (230.0f, grilleArea.getHeight() * 0.86f).withCentre (grilleArea.getCentre());
    g.setFont (Fonts::script (badge.getHeight() * 0.92f));
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawText ("Rig '74", badge.translated (2.0f, 3.0f), Justification::centred, false);
    ColourGradient goldText (Colour (0xfffff0b8), badge.getX(), badge.getY(), Colour (0xffa87c2e), badge.getX(), badge.getBottom(), false);
    g.setGradientFill (goldText);
    g.drawText ("Rig '74", badge, Justification::centred, false);

    // gold control panel
    auto pa = panelArea.reduced (8.0f, 4.0f);
    fillWithTexture (g, Textures::brushed(), pa, 5.0f);
    g.setColour (colours::gold.withAlpha (0.62f));
    g.fillRoundedRectangle (pa, 5.0f);
    ColourGradient sheen (Colours::white.withAlpha (0.25f), pa.getX(), pa.getY(), Colours::black.withAlpha (0.15f), pa.getX(), pa.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (pa, 5.0f);
    g.setColour (Colour (0xff6b4c1c));
    g.drawRoundedRectangle (pa, 5.0f, 1.5f);

    // section label
    g.setColour (colours::brown);
    g.setFont (Fonts::label (14.0f));
    g.drawText ("CHANNEL", Rectangle<float> (250.0f, 16.0f).withPosition (pa.getX() + 8.0f, pa.getBottom() - 20.0f), Justification::centred, false);
    g.drawText ("POWER", Rectangle<float> (70.0f, 16.0f).withPosition (pa.getRight() - 78.0f, pa.getBottom() - 20.0f), Justification::centred, false);
}

//==============================================================================
CabPanel::CabPanel (APVTS& st, std::function<void()> onLoadIR)
{
    speaker.addItemList (params::cabTypes(), 1);
    addAndMakeVisible (speaker);
    speakerAttachment = std::make_unique<APVTS::ComboBoxAttachment> (st, "cab_type", speaker);

    loadIR.onClick = std::move (onLoadIR);
    loadIR.setTooltip ("Load your own cabinet impulse response (.wav / .aiff)");
    addAndMakeVisible (loadIR);

    irName.setFont (Fonts::label (14.0f));
    irName.setColour (Label::textColourId, colours::brown);
    irName.setJustificationType (Justification::centred);
    irName.setText ("NO IR LOADED", dontSendNotification);
    addAndMakeVisible (irName);

    power.getProperties().set ("style", "bat");
    power.setColour (ToggleButton::textColourId, colours::brown);
    addAndMakeVisible (power);
    powerAttachment = std::make_unique<APVTS::ButtonAttachment> (st, "cab_on", power);

    lowCut  = std::make_unique<Knob> (st, "cab_lowcut", "LOW CUT", "rack", colours::brown);
    highCut = std::make_unique<Knob> (st, "cab_highcut", "HIGH CUT", "rack", colours::brown);
    level   = std::make_unique<Knob> (st, "cab_level", "LEVEL", "rack", colours::brown);
    for (auto* k : { lowCut.get(), highCut.get(), level.get() }) addAndMakeVisible (*k);
}

void CabPanel::setIRName (const String& n)
{
    if (irName.getText() != n) irName.setText (n, dontSendNotification);
}

void CabPanel::resized()
{
    auto r = getLocalBounds().toFloat().reduced (14.0f, 12.0f);
    clothArea = r;
    plateArea = r.removeFromRight (r.getWidth() * 0.58f).reduced (8.0f, 6.0f);

    auto p = plateArea.toNearestInt().reduced (12, 10);
    auto top = p.removeFromTop (30);
    power.setBounds (top.removeFromLeft (78));
    speaker.setBounds (top.reduced (2, 0));
    p.removeFromTop (6);
    auto irRow = p.removeFromTop (26);
    loadIR.setBounds (irRow.removeFromLeft (86));
    irName.setBounds (irRow.reduced (4, 0));
    p.removeFromTop (4);
    const int kw = p.getWidth() / 3;
    for (auto* k : { lowCut.get(), highCut.get(), level.get() })
        k->setBounds (p.removeFromLeft (kw));
}

void CabPanel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (2.0f, 5.0f), 10.0f);
    g.setColour (Colour (0xff141010));
    g.fillRoundedRectangle (r, 10.0f);
    fillWithTexture (g, Textures::tolex(), r, 10.0f);
    g.setColour (colours::cream.withAlpha (0.85f));
    g.drawRoundedRectangle (r.reduced (6.0f), 7.0f, 2.0f);

    // basket-weave grille with speaker silhouettes
    fillWithTexture (g, Textures::basketWeave(), clothArea, 6.0f);
    const auto spk = clothArea.withWidth (clothArea.getWidth() * 0.42f);
    for (int i = 0; i < 2; ++i)
    {
        const float d = jmin (spk.getHeight() * 0.44f, spk.getWidth() * 0.86f);
        const auto c = Point<float> (spk.getCentreX(), spk.getY() + spk.getHeight() * (0.27f + 0.46f * (float) i));
        ColourGradient cone (Colours::black.withAlpha (0.38f), c.x, c.y, Colours::black.withAlpha (0.02f), c.x + d * 0.5f, c.y, true);
        g.setGradientFill (cone);
        g.fillEllipse (Rectangle<float> (d, d).withCentre (c));
        g.setColour (Colours::black.withAlpha (0.18f));
        g.drawEllipse (Rectangle<float> (d * 0.3f, d * 0.3f).withCentre (c), 1.2f);
    }
    ColourGradient shade (Colours::white.withAlpha (0.06f), clothArea.getX(), clothArea.getY(), Colours::black.withAlpha (0.35f), clothArea.getX(), clothArea.getBottom(), false);
    g.setGradientFill (shade);
    g.fillRoundedRectangle (clothArea, 6.0f);

    // aluminium control plate
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (plateArea.translated (2.0f, 3.0f), 6.0f);
    fillWithTexture (g, Textures::brushed(), plateArea, 6.0f);
    g.setColour (colours::cream.withAlpha (0.25f));
    g.fillRoundedRectangle (plateArea, 6.0f);
    g.setColour (Colour (0xff5a5048));
    g.drawRoundedRectangle (plateArea, 6.0f, 1.2f);
    drawScrew (g, plateArea.getTopLeft().translated (7.0f, 7.0f), 3.5f);
    drawScrew (g, plateArea.getTopRight().translated (-7.0f, 7.0f), 3.5f);
    drawScrew (g, plateArea.getBottomLeft().translated (7.0f, -7.0f), 3.5f);
    drawScrew (g, plateArea.getBottomRight().translated (-7.0f, -7.0f), 3.5f);

    g.setColour (colours::cream);
    g.setFont (Fonts::script (26.0f));
    g.setColour (Colours::black.withAlpha (0.4f));
    g.drawText ("Cabinet", spk.withHeight (34.0f).withY (clothArea.getBottom() - 40.0f).translated (1.5f, 2.0f), Justification::centred, false);
    g.setColour (colours::cream);
    g.drawText ("Cabinet", spk.withHeight (34.0f).withY (clothArea.getBottom() - 40.0f), Justification::centred, false);
}

//==============================================================================
EqPanel::EqPanel (APVTS& st)
{
    for (int i = 0; i <= params::kNumEqBands; ++i)
    {
        const bool isLevel = i == params::kNumEqBands;
        const String id = isLevel ? "eq_level" : params::eqIds[i];
        auto* s = faders.add (new Slider (Slider::LinearVertical, Slider::NoTextBox));
        s->setColour (Slider::rotarySliderOutlineColourId, colours::brown);
        s->setPopupDisplayEnabled (true, false, nullptr);
        s->setMouseDragSensitivity (160);
        if (auto* p = st.getParameter (id)) s->setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        addAndMakeVisible (s);
        attachments.add (new APVTS::SliderAttachment (st, id, *s));
        labels.add (isLevel ? "LEVEL" : params::eqLabels[i]);
    }
    power.getProperties().set ("style", "bat");
    power.setColour (ToggleButton::textColourId, colours::cream);
    addAndMakeVisible (power);
    powerAttachment = std::make_unique<APVTS::ButtonAttachment> (st, "eq_on", power);
    ledOn = power.getToggleState();
}

void EqPanel::refreshLed()
{
    if (power.getToggleState() != ledOn) { ledOn = power.getToggleState(); repaint(); }
}

void EqPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    auto header = r.removeFromTop (34);
    power.setBounds (header.removeFromRight (70).reduced (0, 3));
    ledArea = Rectangle<float> (9.0f, 9.0f).withCentre ({ (float) header.getRight() - 6.0f, (float) header.getCentreY() });
    r.removeFromTop (14);
    r.removeFromBottom (24);
    const int w = r.getWidth() / faders.size();
    for (int i = 0; i < faders.size(); ++i)
    {
        auto col = r.removeFromLeft (w);
        if (i == faders.size() - 1) col = col.translated (4, 0);
        faders[i]->setBounds (col);
    }
}

void EqPanel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (2.0f, 4.0f), 8.0f);
    fillWithTexture (g, Textures::brushed(), r, 8.0f);
    g.setColour (Colour (0xff4a4038));
    g.drawRoundedRectangle (r, 8.0f, 1.5f);

    // black header strip
    auto header = r.reduced (6.0f).removeFromTop (38.0f);
    g.setColour (Colour (0xff1b1410));
    g.fillRoundedRectangle (header, 5.0f);
    g.setColour (colours::orange);
    g.fillRect (header.withTop (header.getBottom() - 4.0f).reduced (4.0f, 0));
    g.setColour (colours::cream);
    g.setFont (Fonts::script (24.0f));
    g.drawText ("Graphic EQ", header.withTrimmedLeft (12.0f), Justification::centredLeft, false);

    // LED
    const auto c = ledArea.getCentre();
    if (ledOn)
    {
        ColourGradient glow (colours::ledRed.withAlpha (0.6f), c.x, c.y, colours::ledRed.withAlpha (0.0f), c.x + 11.0f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (Rectangle<float> (22.0f, 22.0f).withCentre (c));
    }
    g.setColour (ledOn ? colours::ledRed : Colour (0xff3a0a06));
    g.fillEllipse (ledArea);

    // dB legend
    if (! faders.isEmpty())
    {
        const auto fb = faders[0]->getBounds().toFloat();
        g.setColour (colours::brown);
        g.setFont (Fonts::label (12.0f));
        g.drawText ("+12", Rectangle<float> (24.0f, 12.0f).withCentre ({ fb.getX() + 2.0f, fb.getY() + 6.0f }), Justification::centred, false);
        g.drawText ("0", Rectangle<float> (24.0f, 12.0f).withCentre ({ fb.getX() + 2.0f, fb.getCentreY() }), Justification::centred, false);
        g.drawText ("-12", Rectangle<float> (24.0f, 12.0f).withCentre ({ fb.getX() + 2.0f, fb.getBottom() - 6.0f }), Justification::centred, false);
    }

    g.setFont (Fonts::label (14.0f));
    g.setColour (colours::brown);
    for (int i = 0; i < faders.size(); ++i)
    {
        const auto fb = faders[i]->getBounds();
        g.drawText (labels[i], Rectangle<int> (fb.getX() - 4, fb.getBottom() + 4, fb.getWidth() + 8, 18), Justification::centred, false);
    }
    // separator before LEVEL
    if (faders.size() > 1)
    {
        const float x = (float) faders.getLast()->getX() - 2.0f;
        g.setColour (Colours::black.withAlpha (0.3f));
        g.drawLine (x, (float) faders[0]->getY(), x, (float) faders[0]->getBottom(), 1.0f);
    }
}
} // namespace rig::gui
