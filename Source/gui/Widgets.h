#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "RetroLookAndFeel.h"

namespace rig::gui
{
using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
/** Rotary knob with its label underneath. style: "pedal", "amp", "rack". */
class Knob : public juce::Component
{
public:
    Knob (APVTS&, const juce::String& paramId, const juce::String& label, const juce::String& style, juce::Colour textColour);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider;

private:
    juce::String text;
    juce::Colour colour;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

//==============================================================================
/** Rotary switch for choice parameters; click to step, drag to turn. */
class Selector : public juce::Component
{
public:
    Selector (APVTS&, const juce::String& paramId, const juce::String& title, const juce::StringArray& labels,
              juce::Colour textColour, float halfSpanRadians, float labelFactor = 1.55f);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setKnobRadius (float r) { slider.getProperties().set ("knobRadius", r); }

    struct ClickSlider : public juce::Slider
    {
        void mouseDown (const juce::MouseEvent& e) override { downPos = e.position; juce::Slider::mouseDown (e); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            juce::Slider::mouseUp (e);
            if (e.position.getDistanceFrom (downPos) < 3.0f && ! e.mods.isPopupMenu())
                setValue (getValue() + 1.0 > getMaximum() ? getMinimum() : getValue() + 1.0, juce::sendNotificationSync);
        }
        juce::Point<float> downPos;
    };

    ClickSlider slider;

private:
    juce::String title;
    juce::Colour colour;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

//==============================================================================
/** Base for every effect unit: body, title, LED, stompswitch, rows of controls. */
class FxPanel : public juce::Component
{
public:
    enum class Style { Pedal, Echo };

    FxPanel (APVTS&, const juce::String& bypassId, const juce::String& title, const juce::String& caption, juce::Colour body, Style = Style::Pedal);

    juce::Colour textColour() const;

    Knob& knob (const juce::String& id, const juce::String& label, const juce::String& style = "pedal");
    Selector& selector (const juce::String& id, const juce::String& label, const juce::StringArray& labels, float halfSpan);
    template <typename T> T& own (std::unique_ptr<T> c) { auto& r = *c; addAndMakeVisible (r); owned.push_back (std::move (c)); return r; }

    void addRow (std::vector<std::pair<juce::Component*, float>> row) { rows.push_back (std::move (row)); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshLed();

    juce::ToggleButton bypass;

protected:
    APVTS& state;
    juce::String title, caption;
    juce::Colour body;
    Style style;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::vector<std::pair<juce::Component*, float>>> rows;
    std::unique_ptr<APVTS::ButtonAttachment> bypassAttachment;
    juce::Rectangle<float> ledArea, titleArea;
    bool ledOn = false;
};

//==============================================================================
/** Classic needle VU meter; doubles as the tuner display. */
class VuMeter : public juce::Component
{
public:
    explicit VuMeter (const juce::String& label);
    void paint (juce::Graphics&) override;

    void setLevelDb (float dbfs) { targetPos = levelToPos (dbfs); }
    void setTunerMode (bool on) { if (tunerMode != on) { tunerMode = on; repaint(); } }
    void setTuner (const juce::String& noteName, float cents, bool valid);
    void tick();   // ~30 Hz

private:
    static float levelToPos (float dbfs);
    juce::String label;
    float targetPos = 0.0f, pos = 0.0f;
    bool tunerMode = false, tunerValid = false;
    juce::String note;
    float centsValue = 0.0f;
};

//==============================================================================
class AmpPanel : public juce::Component
{
public:
    explicit AmpPanel (APVTS&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    std::unique_ptr<Selector> model;
    std::unique_ptr<Knob> gain, bass, mid, treble, presence, master;
    juce::ToggleButton power;
    std::unique_ptr<APVTS::ButtonAttachment> powerAttachment;
    juce::Rectangle<float> grilleArea, panelArea;
};

//==============================================================================
class CabPanel : public juce::Component
{
public:
    CabPanel (APVTS&, std::function<void()> onLoadIR);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setIRName (const juce::String& name);

private:
    juce::ComboBox speaker;
    juce::TextButton loadIR { "LOAD IR" };
    juce::Label irName;
    juce::ToggleButton power { "CAB" };
    std::unique_ptr<Knob> lowCut, highCut, level;
    std::unique_ptr<APVTS::ComboBoxAttachment> speakerAttachment;
    std::unique_ptr<APVTS::ButtonAttachment> powerAttachment;
    juce::Rectangle<float> clothArea, plateArea;
};

//==============================================================================
class EqPanel : public juce::Component
{
public:
    explicit EqPanel (APVTS&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshLed();

private:
    juce::OwnedArray<juce::Slider> faders;
    juce::OwnedArray<APVTS::SliderAttachment> attachments;
    juce::StringArray labels;
    juce::ToggleButton power { "ON" };
    std::unique_ptr<APVTS::ButtonAttachment> powerAttachment;
    juce::Rectangle<float> ledArea;
    bool ledOn = false;
};
} // namespace rig::gui
