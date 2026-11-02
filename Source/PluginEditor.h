#pragma once

#include "PluginProcessor.h"
#include "gui/Widgets.h"

/** The whole pedalboard / amp / rack front panel, laid out at a fixed 1600 x 900 design size. */
class MainPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int designWidth = 1600, designHeight = 900;

    explicit MainPanel (Rig74Processor&);
    ~MainPanel() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshPresetBox();
    void chooseIR();
    void savePreset();
    void loadPreset();
    void updateTuner();
    static juce::File presetFolder();

    Rig74Processor& proc;
    rig::gui::RetroLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, loadBtn { "LOAD" };
    juce::ToggleButton tunerBtn { "TUNER" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tunerAttachment;
    rig::gui::VuMeter vuIn { "INPUT" }, vuOut { "OUTPUT" };
    std::unique_ptr<rig::gui::Knob> inKnob, outKnob;

    std::unique_ptr<rig::gui::FxPanel> gate, comp, wah, oct, drive, chorus, flanger, phaser, tremolo, delay, reverb;
    std::unique_ptr<rig::gui::AmpPanel> amp;
    std::unique_ptr<rig::gui::CabPanel> cab;
    std::unique_ptr<rig::gui::EqPanel> eq;
    std::vector<std::unique_ptr<juce::Component>> extras;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String userPresetName;

    // tuner analysis
    std::vector<float> tunerHistory, tunerScratch, tunerLinear;
    int tunerWrite = 0, tunerNew = 0, tunerSilentTicks = 0, decimCount = 0;
    float decimAcc = 0.0f, smoothedCents = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};

class Rig74Editor : public juce::AudioProcessorEditor
{
public:
    explicit Rig74Editor (Rig74Processor&);
    void resized() override;

private:
    MainPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Rig74Editor)
};
