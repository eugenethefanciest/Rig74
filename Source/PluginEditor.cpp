#include "PluginEditor.h"
#include "dsp/Tuner.h"

using namespace juce;
using namespace rig::gui;
namespace C = rig::gui::colours;

namespace
{
/** Delay: tempo-sync menu + ping-pong toggle stacked in one slot. */
struct SyncBox : public Component
{
    SyncBox (AudioProcessorValueTreeState& st)
    {
        sync.addItemList (rig::params::delaySyncs(), 1);
        sync.setTooltip ("Sync the delay time to the host tempo");
        addAndMakeVisible (sync);
        syncAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (st, "dly_sync", sync);
        pingPong.getProperties().set ("style", "bat");
        pingPong.setColour (ToggleButton::textColourId, C::brown);
        addAndMakeVisible (pingPong);
        ppAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (st, "dly_pingpong", pingPong);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const int h = jmin (26, r.getHeight() / 2 - 2);
        auto top = r.removeFromTop (r.getHeight() / 2);
        sync.setBounds (top.withSizeKeepingCentre (r.getWidth(), h));
        pingPong.setBounds (r.withSizeKeepingCentre (r.getWidth(), h));
    }
    ComboBox sync;
    ToggleButton pingPong { "PING-PONG" };
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> syncAtt;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> ppAtt;
};

void forEachSlider (Component& root, const std::function<void (Slider&)>& fn)
{
    for (auto* c : root.getChildren())
    {
        if (auto* s = dynamic_cast<Slider*> (c)) fn (*s);
        forEachSlider (*c, fn);
    }
}
} // namespace

//==============================================================================
MainPanel::MainPanel (Rig74Processor& p) : proc (p)
{
    setLookAndFeel (&lnf);
    auto& st = proc.apvts;
    using Style = FxPanel::Style;

    // ---------------- header ----------------
    refreshPresetBox();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0 && id - 1 != proc.getCurrentProgram()) { userPresetName.clear(); proc.setCurrentProgram (id - 1); }
        else if (id > 0 && userPresetName.isNotEmpty()) { userPresetName.clear(); proc.setCurrentProgram (id - 1); }
    };
    presetBox.setTooltip ("Factory presets");
    addAndMakeVisible (presetBox);

    prevBtn.onClick = [this] { const int n = proc.getNumPrograms(); userPresetName.clear(); proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n); };
    nextBtn.onClick = [this] { const int n = proc.getNumPrograms(); userPresetName.clear(); proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % n); };
    saveBtn.onClick = [this] { savePreset(); };
    loadBtn.onClick = [this] { loadPreset(); };
    saveBtn.setTooltip ("Save the current sound to a preset file");
    loadBtn.setTooltip ("Load a preset file");
    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &loadBtn }) addAndMakeVisible (b);

    tunerBtn.getProperties().set ("style", "led");
    tunerBtn.setTooltip ("Tuner (mutes the output)");
    addAndMakeVisible (tunerBtn);
    tunerAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (st, "tuner", tunerBtn);

    addAndMakeVisible (vuIn);
    addAndMakeVisible (vuOut);
    inKnob = std::make_unique<Knob> (st, "in_gain", "INPUT", "rack", C::brown);
    outKnob = std::make_unique<Knob> (st, "out_gain", "OUTPUT", "rack", C::brown);
    addAndMakeVisible (*inKnob);
    addAndMakeVisible (*outKnob);

    // ---------------- row 1: stompboxes ----------------
    gate = std::make_unique<FxPanel> (st, "gate_on", "Gate", "NOISE SUPPRESSOR", C::chocolate);
    gate->addRow ({ { &gate->knob ("gate_thresh", "THRESHOLD"), 1.0f }, { &gate->knob ("gate_release", "RELEASE"), 1.0f },
                    { &gate->selector ("hum", "HUM FILTER", { "OFF", "50", "60" }, 0.9f), 1.25f } });

    comp = std::make_unique<FxPanel> (st, "comp_on", "Squeeze", "COMPRESSOR / SUSTAINER", C::mustard);
    comp->addRow ({ { &comp->knob ("comp_sustain", "SUSTAIN"), 1.0f }, { &comp->knob ("comp_attack", "ATTACK"), 1.0f },
                    { &comp->knob ("comp_level", "LEVEL"), 1.0f } });

    wah = std::make_unique<FxPanel> (st, "wah_on", "Wah-Wah", "PEDAL / AUTO / LFO", Colour (0xff2b2724));
    wah->addRow ({ { &wah->knob ("wah_pedal", "PEDAL"), 1.0f }, { &wah->selector ("wah_mode", "MODE", { "PEDAL", "AUTO", "LFO" }, 0.9f), 1.5f } });
    wah->addRow ({ { &wah->knob ("wah_sens", "SENSE"), 1.0f }, { &wah->knob ("wah_rate", "RATE"), 1.0f }, { &wah->knob ("wah_q", "RESO"), 1.0f } });

    oct = std::make_unique<FxPanel> (st, "oct_on", "Octavius", "OCTAVE DIVIDER", C::plum);
    oct->addRow ({ { &oct->knob ("oct_down", "OCT DOWN"), 1.0f }, { &oct->knob ("oct_up", "OCT UP"), 1.0f }, { &oct->knob ("oct_dry", "DIRECT"), 1.0f } });

    drive = std::make_unique<FxPanel> (st, "drv_on", "Drive", "OVERDRIVE / DISTORTION / FUZZ", C::red);
    drive->addRow ({ { nullptr, 0.5f }, { &drive->selector ("drv_type", "", { "OD", "DIST", "FUZZ" }, 0.9f), 2.0f }, { nullptr, 0.5f } });
    drive->addRow ({ { &drive->knob ("drv_gain", "GAIN"), 1.0f }, { &drive->knob ("drv_tone", "TONE"), 1.0f }, { &drive->knob ("drv_level", "LEVEL"), 1.0f } });

    // ---------------- row 2: amp + cab ----------------
    amp = std::make_unique<AmpPanel> (st);
    cab = std::make_unique<CabPanel> (st, [this] { chooseIR(); });

    // ---------------- row 3: rack EQ + modulation + time ----------------
    eq = std::make_unique<EqPanel> (st);

    chorus = std::make_unique<FxPanel> (st, "cho_on", "Chorus", "ANALOG ENSEMBLE", C::avocado);
    chorus->addRow ({ { &chorus->knob ("cho_rate", "RATE"), 1.0f }, { &chorus->knob ("cho_depth", "DEPTH"), 1.0f } });
    chorus->addRow ({ { nullptr, 0.5f }, { &chorus->knob ("cho_mix", "MIX"), 1.0f }, { nullptr, 0.5f } });

    flanger = std::make_unique<FxPanel> (st, "fla_on", "Flanger", "JET SWEEP", C::rust);
    flanger->addRow ({ { &flanger->knob ("fla_rate", "RATE"), 1.0f }, { &flanger->knob ("fla_depth", "DEPTH"), 1.0f } });
    flanger->addRow ({ { &flanger->knob ("fla_regen", "REGEN"), 1.0f }, { &flanger->knob ("fla_mix", "MIX"), 1.0f } });

    phaser = std::make_unique<FxPanel> (st, "pha_on", "Phaser", "FOUR STAGE", C::orange);
    phaser->addRow ({ { &phaser->knob ("pha_rate", "SPEED"), 1.0f }, { &phaser->knob ("pha_depth", "DEPTH"), 1.0f } });
    phaser->addRow ({ { &phaser->knob ("pha_feedback", "FEEDBACK"), 1.0f }, { &phaser->knob ("pha_mix", "MIX"), 1.0f } });

    tremolo = std::make_unique<FxPanel> (st, "tre_on", "Tremolo", "OPTICAL STYLE", C::teal);
    tremolo->addRow ({ { &tremolo->knob ("tre_rate", "SPEED"), 1.0f }, { &tremolo->knob ("tre_depth", "INTENSITY"), 1.0f } });
    tremolo->addRow ({ { nullptr, 0.5f }, { &tremolo->knob ("tre_shape", "SHAPE"), 1.0f }, { nullptr, 0.5f } });

    delay = std::make_unique<FxPanel> (st, "dly_on", "Echo", "TAPE / BUCKET BRIGADE / DIGITAL", C::cream, Style::Echo);
    auto& syncBox = delay->own (std::make_unique<SyncBox> (st));
    delay->addRow ({ { &delay->knob ("dly_time", "TIME", "rack"), 1.0f }, { &delay->knob ("dly_feedback", "REPEATS", "rack"), 1.0f },
                     { &delay->knob ("dly_mix", "MIX", "rack"), 1.0f }, { &delay->knob ("dly_tone", "TONE", "rack"), 1.0f } });
    delay->addRow ({ { &delay->knob ("dly_mod", "WOW", "rack"), 1.0f }, { &delay->selector ("dly_mode", "", { "TAPE", "BBD", "DIGI" }, 0.9f), 1.5f },
                     { &syncBox, 1.6f } });

    reverb = std::make_unique<FxPanel> (st, "rev_on", "Reverb", "SPRING / PLATE / HALL", C::denim);
    reverb->addRow ({ { &reverb->knob ("rev_decay", "DECAY"), 1.0f }, { &reverb->knob ("rev_tone", "TONE"), 1.0f }, { &reverb->knob ("rev_mix", "MIX"), 1.0f } });
    reverb->addRow ({ { &reverb->knob ("rev_predelay", "PRE-DLY"), 1.0f }, { &reverb->selector ("rev_type", "", { "SPRING", "PLATE", "HALL" }, 0.9f), 2.0f } });

    for (auto* c : std::initializer_list<Component*> { gate.get(), comp.get(), wah.get(), oct.get(), drive.get(), amp.get(), cab.get(), eq.get(),
                                                       chorus.get(), flanger.get(), phaser.get(), tremolo.get(), delay.get(), reverb.get() })
        addAndMakeVisible (c);

    // value bubbles appear inside the plugin window (works with every host)
    forEachSlider (*this, [this] (Slider& s) { s.setPopupDisplayEnabled (true, true, this); });

    proc.onPresetLoaded = [safe = SafePointer<MainPanel> (this)]
    {
        MessageManager::callAsync ([safe] { if (safe != nullptr) safe->refreshPresetBox(); });
    };

    setSize (designWidth, designHeight);
    startTimerHz (30);
}

MainPanel::~MainPanel()
{
    stopTimer();
    proc.onPresetLoaded = nullptr;
    setLookAndFeel (nullptr);
}

//==============================================================================
void MainPanel::refreshPresetBox()
{
    presetBox.clear (dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    if (userPresetName.isNotEmpty()) presetBox.setText (userPresetName, dontSendNotification);
    else presetBox.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
}

File MainPanel::presetFolder()
{
    auto dir = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Rig74 Presets");
    dir.createDirectory();
    return dir;
}

void MainPanel::chooseIR()
{
    chooser = std::make_unique<FileChooser> ("Load a cabinet impulse response", File::getSpecialLocation (File::userDocumentsDirectory), "*.wav;*.aif;*.aiff");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe = SafePointer<MainPanel> (this)] (const FileChooser& fc)
                          {
                              if (safe == nullptr) return;
                              const auto f = fc.getResult();
                              if (f.existsAsFile() && ! safe->proc.loadUserIR (f))
                                  AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Rig 74", "Could not read this impulse response file.");
                          });
}

void MainPanel::savePreset()
{
    chooser = std::make_unique<FileChooser> ("Save preset", presetFolder().getChildFile ("My Sound.rig74"), "*.rig74");
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                          [safe = SafePointer<MainPanel> (this)] (const FileChooser& fc)
                          {
                              if (safe == nullptr) return;
                              auto f = fc.getResult();
                              if (f == File()) return;
                              f = f.withFileExtension ("rig74");
                              if (safe->proc.savePresetToFile (f))
                              {
                                  safe->userPresetName = f.getFileNameWithoutExtension();
                                  safe->refreshPresetBox();
                              }
                          });
}

void MainPanel::loadPreset()
{
    chooser = std::make_unique<FileChooser> ("Load preset", presetFolder(), "*.rig74");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe = SafePointer<MainPanel> (this)] (const FileChooser& fc)
                          {
                              if (safe == nullptr) return;
                              const auto f = fc.getResult();
                              if (! f.existsAsFile()) return;
                              safe->userPresetName = f.getFileNameWithoutExtension();
                              if (! safe->proc.loadPresetFromFile (f))
                              {
                                  safe->userPresetName.clear();
                                  AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Rig 74", "This is not a Rig 74 preset file.");
                              }
                          });
}

//==============================================================================
void MainPanel::timerCallback()
{
    const bool tunerOn = tunerBtn.getToggleState();
    vuIn.setTunerMode (tunerOn);
    if (tunerOn) updateTuner();
    else vuIn.setLevelDb (rig::gainToDb (proc.inputPeak.exchange (0.0f)));
    vuOut.setLevelDb (rig::gainToDb (proc.outputPeak.exchange (0.0f)));
    vuIn.tick();
    vuOut.tick();

    for (auto* fx : { gate.get(), comp.get(), wah.get(), oct.get(), drive.get(), chorus.get(), flanger.get(), phaser.get(),
                      tremolo.get(), delay.get(), reverb.get() })
        fx->refreshLed();
    eq->refreshLed();

    const auto ir = proc.getUserIRFile();
    cab->setIRName (ir.existsAsFile() ? ir.getFileNameWithoutExtension().toUpperCase() : String ("NO IR LOADED"));
}

void MainPanel::updateTuner()
{
    const double sr = proc.getCurrentSampleRate();
    const int factor = sr > 100000.0 ? 4 : (sr > 50000.0 ? 2 : 1);
    const double rate = sr / factor;
    constexpr int histSize = 4096;
    if ((int) tunerHistory.size() != histSize) { tunerHistory.assign (histSize, 0.0f); tunerLinear.assign (histSize, 0.0f); }
    if (tunerScratch.size() < 16384) tunerScratch.resize (16384);

    const int n = proc.readTunerSamples (tunerScratch.data(), (int) tunerScratch.size());
    for (int i = 0; i < n; ++i)
    {
        decimAcc += tunerScratch[(size_t) i];
        if (++decimCount >= factor)
        {
            tunerHistory[(size_t) tunerWrite] = decimAcc / (float) factor;
            tunerWrite = (tunerWrite + 1) % histSize;
            decimAcc = 0.0f; decimCount = 0; ++tunerNew;
        }
    }
    if (tunerNew < 512) return;
    tunerNew = 0;

    for (int i = 0; i < histSize; ++i) tunerLinear[(size_t) i] = tunerHistory[(size_t) ((tunerWrite + i) % histSize)];
    const float f = rig::PitchDetector::detect (tunerLinear.data(), histSize, rate);

    if (f > 30.0f && f < 1500.0f)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
        const int nearest = roundToInt (midi);
        const float cents = (midi - (float) nearest) * 100.0f;
        smoothedCents = tunerSilentTicks > 0 ? cents : smoothedCents + 0.5f * (cents - smoothedCents);
        tunerSilentTicks = 0;
        vuIn.setTuner (String (names[((nearest % 12) + 12) % 12]) + String (nearest / 12 - 1), smoothedCents, true);
    }
    else if (++tunerSilentTicks > 8)
        vuIn.setTuner ({}, 0.0f, false);
}

//==============================================================================
void MainPanel::resized()
{
    // header
    prevBtn.setBounds (410, 46, 40, 40);
    presetBox.setBounds (456, 46, 300, 40);
    nextBtn.setBounds (762, 46, 40, 40);
    saveBtn.setBounds (812, 46, 70, 40);
    loadBtn.setBounds (888, 46, 70, 40);
    tunerBtn.setBounds (966, 46, 82, 40);
    vuIn.setBounds (1058, 14, 152, 86);
    vuOut.setBounds (1216, 14, 152, 86);
    inKnob->setBounds (1376, 14, 86, 100);
    outKnob->setBounds (1466, 14, 86, 100);

    // row 1: five stompboxes
    {
        const int x0 = 44, y = 132, h = 252, gap = 16;
        const int w = (1512 - 4 * gap) / 5;
        int x = x0;
        for (auto* fx : { gate.get(), comp.get(), wah.get(), oct.get(), drive.get() })
        {
            fx->setBounds (x, y, w, h);
            x += w + gap;
        }
    }

    // row 2: amp + cab
    amp->setBounds (44, 394, 960, 224);
    cab->setBounds (1018, 394, 538, 224);

    // row 3
    {
        int x = 44;
        const int y = 628, h = 264, gap = 12;
        const std::pair<Component*, int> units[] = { { eq.get(), 300 }, { chorus.get(), 156 }, { flanger.get(), 170 }, { phaser.get(), 170 },
                                                     { tremolo.get(), 156 }, { delay.get(), 272 }, { reverb.get(), 216 } };
        for (auto& [c, w] : units) { c->setBounds (x, y, w, h); x += w + gap; }
    }
}

void MainPanel::paint (Graphics& g)
{
    const auto all = getLocalBounds().toFloat();
    g.fillAll (C::tolex);
    fillWithTexture (g, Textures::tolex(), all);

    // walnut side cheeks
    for (auto side : { all.withWidth (34.0f), all.withLeft (all.getRight() - 34.0f) })
    {
        fillWithTexture (g, Textures::wood(), side);
        ColourGradient edge (Colours::black.withAlpha (0.0f), side.getX(), 0, Colours::black.withAlpha (0.45f), side.getRight(), 0, false);
        if (side.getX() > 100.0f) edge = ColourGradient (Colours::black.withAlpha (0.45f), side.getX(), 0, Colours::black.withAlpha (0.0f), side.getRight(), 0, false);
        g.setGradientFill (edge);
        g.fillRect (side);
    }

    // header plate
    auto header = Rectangle<float> (44.0f, 8.0f, 1512.0f, 112.0f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (header.translated (2.0f, 4.0f), 8.0f);
    ColourGradient plate (C::cream.brighter (0.04f), header.getX(), header.getY(), C::creamDark, header.getX(), header.getBottom(), false);
    g.setGradientFill (plate);
    g.fillRoundedRectangle (header, 8.0f);
    fillWithTexture (g, Textures::speckle(), header, 8.0f, 0.35f);
    g.setColour (C::brown.withAlpha (0.7f));
    g.drawRoundedRectangle (header, 8.0f, 1.5f);

    // 70s racing stripes under the logo
    {
        Graphics::ScopedSaveState ss (g);
        Path clip; clip.addRoundedRectangle (header, 8.0f);
        g.reduceClipRegion (clip);
        const Colour stripes[] = { C::chocolate, C::rust, C::orange, C::mustard };
        for (int i = 0; i < 4; ++i)
        {
            Path s;
            const float y = header.getBottom() - 30.0f + (float) i * 7.0f;
            s.addRectangle (header.getX(), y, 330.0f, 6.0f);
            s.addTriangle (header.getX() + 330.0f, y, header.getX() + 352.0f, y, header.getX() + 330.0f, y + 6.0f);
            g.setColour (stripes[i]);
            g.fillPath (s);
        }
    }

    auto logo = Rectangle<float> (60.0f, 6.0f, 320.0f, 76.0f);
    g.setFont (Fonts::script (66.0f));
    g.setColour (C::cream.darker (0.2f));
    g.drawText ("Rig '74", logo.translated (3.0f, 3.0f), Justification::centredLeft, false);
    g.setColour (C::chocolate);
    g.drawText ("Rig '74", logo, Justification::centredLeft, false);
    g.setFont (Fonts::label (15.0f));
    g.setColour (C::brown.withAlpha (0.85f));
    g.drawText ("GUITAR TONE MACHINE", Rectangle<float> (232.0f, 30.0f, 170.0f, 18.0f), Justification::centredLeft, false);
    g.drawText ("STEREO  /  MODEL 74", Rectangle<float> (232.0f, 48.0f, 170.0f, 18.0f), Justification::centredLeft, false);

    g.setFont (Fonts::label (14.0f));
    g.setColour (C::brown.withAlpha (0.8f));
    g.drawText ("PRESET", Rectangle<float> (456.0f, 26.0f, 300.0f, 16.0f), Justification::centred, false);
    g.drawText ("FILE", Rectangle<float> (812.0f, 26.0f, 146.0f, 16.0f), Justification::centred, false);
    g.drawText ("INPUT", Rectangle<float> (1058.0f, 101.0f, 152.0f, 16.0f), Justification::centred, false);
    g.drawText ("OUTPUT", Rectangle<float> (1216.0f, 101.0f, 152.0f, 16.0f), Justification::centred, false);
}

void MainPanel::paintOverChildren (Graphics& g)
{
    // patch cables between neighbouring units
    auto cable = [&g] (Rectangle<int> a, Rectangle<int> b, float yOffset, Colour col)
    {
        const float y = (float) a.getY() + yOffset;
        const float x1 = (float) a.getRight() - 2.0f, x2 = (float) b.getX() + 2.0f;
        auto plug = [&g] (float x, float yy, bool left)
        {
            auto body = Rectangle<float> (left ? x - 1.0f : x - 7.0f, yy - 5.0f, 8.0f, 10.0f);
            ColourGradient cg (Colour (0xffe8e8e8), body.getX(), body.getY(), Colour (0xff6a6a6a), body.getX(), body.getBottom(), false);
            g.setGradientFill (cg);
            g.fillRoundedRectangle (body, 1.5f);
        };
        Path p;
        p.startNewSubPath (x1 + 6.0f, y);
        p.quadraticTo ((x1 + x2) * 0.5f, y + 9.0f, x2 - 6.0f, y);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.strokePath (p, PathStrokeType (5.0f), AffineTransform::translation (0.0f, 2.0f));
        g.setColour (col);
        g.strokePath (p, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
        plug (x1, y, true);
        plug (x2, y, false);
    };

    const Component* row1[] = { gate.get(), comp.get(), wah.get(), oct.get(), drive.get() };
    for (int i = 0; i < 4; ++i) cable (row1[i]->getBounds(), row1[i + 1]->getBounds(), 40.0f, Colour (0xff1a1410));
    const Component* row3[] = { eq.get(), chorus.get(), flanger.get(), phaser.get(), tremolo.get(), delay.get(), reverb.get() };
    for (int i = 0; i < 6; ++i) cable (row3[i]->getBounds(), row3[i + 1]->getBounds(), 40.0f, Colour (0xff1a1410));
}

//==============================================================================
Rig74Editor::Rig74Editor (Rig74Processor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (800, 450, 2240, 1260);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) MainPanel::designWidth / MainPanel::designHeight);
    setSize (1200, 675);
}

void Rig74Editor::resized()
{
    panel.setBounds (0, 0, MainPanel::designWidth, MainPanel::designHeight);
    panel.setTransform (AffineTransform::scale ((float) getWidth() / (float) MainPanel::designWidth));
}
