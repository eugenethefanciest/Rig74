#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig::gui
{
namespace colours
{
    const juce::Colour cream      { 0xfff1e4c3 };
    const juce::Colour creamDark  { 0xffd8c597 };
    const juce::Colour brown      { 0xff3b2416 };
    const juce::Colour chocolate  { 0xff5a3420 };
    const juce::Colour orange     { 0xffd9661f };
    const juce::Colour mustard    { 0xffdea52b };
    const juce::Colour avocado    { 0xff7d8c3c };
    const juce::Colour rust       { 0xffb4461d };
    const juce::Colour red        { 0xffb3261e };
    const juce::Colour teal       { 0xff2f7477 };
    const juce::Colour plum       { 0xff7a3e62 };
    const juce::Colour denim      { 0xff3e5c76 };
    const juce::Colour gold       { 0xffcda75a };
    const juce::Colour tolex      { 0xff1d1611 };
    const juce::Colour amber      { 0xffffb53a };
    const juce::Colour ledRed     { 0xffff3b2a };
}

/** Embedded fonts: Shrikhand (70s display script) and Bebas Neue (labels). */
struct Fonts
{
    static juce::Font script (float height);
    static juce::Font label (float height);
    static juce::Typeface::Ptr labelTypeface();
};

/** Procedural textures (generated once, cached). */
struct Textures
{
    static const juce::Image& tolex();
    static const juce::Image& wood();
    static const juce::Image& grille();
    static const juce::Image& brushed();
    static const juce::Image& speckle();
    static const juce::Image& basketWeave();
};

/** Draws an image texture tiled/stretched to fill a (possibly rounded) area. */
void fillWithTexture (juce::Graphics& g, const juce::Image& img, juce::Rectangle<float> area, float cornerRadius = 0.0f, float opacity = 1.0f);
void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle = 0.6f);

/** Retro 70s look: knurled pedal knobs, chicken-head amp knobs, stompswitches, VU-style faces. */
class RetroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RetroLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;

    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip, const juce::Rectangle<float>& body) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
    int getSliderPopupPlacement (juce::Slider&) override;

    juce::Font getLabelFont (juce::Label&) override;

private:
    void drawPedalKnob (juce::Graphics&, juce::Rectangle<float>, float angle, juce::Slider&);
    void drawChickenHead (juce::Graphics&, juce::Point<float> c, float r, float angle, bool numbers, float startA, float endA, juce::Colour markColour);
    void drawRackKnob (juce::Graphics&, juce::Rectangle<float>, float angle, juce::Slider&);
    void drawSelector (juce::Graphics&, juce::Rectangle<float>, float pos, float startA, float endA, juce::Slider&);
};
} // namespace rig::gui
