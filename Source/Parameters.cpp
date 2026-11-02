#include "Parameters.h"

namespace rig::params
{
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using Attr = juce::AudioParameterFloatAttributes;

const float eqFreqs[kNumEqBands] = { 100.0f, 200.0f, 400.0f, 800.0f, 1600.0f, 3200.0f, 6400.0f };
const char* const eqIds[kNumEqBands] = { "eq_100", "eq_200", "eq_400", "eq_800", "eq_1k6", "eq_3k2", "eq_6k4" };
const char* const eqLabels[kNumEqBands] = { "100", "200", "400", "800", "1.6K", "3.2K", "6.4K" };

const juce::StringArray& humChoices()  { static juce::StringArray s { "OFF", "50 HZ", "60 HZ" }; return s; }
const juce::StringArray& wahModes()    { static juce::StringArray s { "PEDAL", "AUTO", "LFO" }; return s; }
const juce::StringArray& driveTypes()  { static juce::StringArray s { "OVERDRIVE", "DISTORTION", "FUZZ" }; return s; }
const juce::StringArray& ampModels()   { static juce::StringArray s { "CLEAN '65", "CRUNCH '68", "LEAD '72", "HOT ROD '79" }; return s; }
const juce::StringArray& cabTypes()    { static juce::StringArray s { "1x12 OPEN", "2x12 COMBO", "4x12 VINTAGE", "4x10 TWEED", "1x15 JAZZ", "USER IR" }; return s; }
const juce::StringArray& delayModes()  { static juce::StringArray s { "TAPE", "ANALOG", "DIGITAL" }; return s; }
const juce::StringArray& delaySyncs()  { static juce::StringArray s { "FREE", "1/2", "1/4", "1/4 DOT", "1/4 TRIP", "1/8", "1/8 DOT", "1/8 TRIP", "1/16" }; return s; }
const juce::StringArray& reverbTypes() { static juce::StringArray s { "SPRING", "PLATE", "HALL" }; return s; }

double delaySyncBeats (int index)
{
    static const double beats[] = { 0.0, 2.0, 1.0, 1.5, 2.0 / 3.0, 0.5, 0.75, 1.0 / 3.0, 0.25 };
    return beats[juce::jlimit (0, 8, index)];
}

namespace
{
juce::ParameterID pid (const char* id) { return { id, 1 }; }

juce::NormalisableRange<float> skewed (float lo, float hi, float centre, float step = 0.0f)
{
    juce::NormalisableRange<float> r (lo, hi, step);
    r.setSkewForCentre (centre);
    return r;
}

Attr dbAttr()
{
    return Attr().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; });
}
Attr msAttr()
{
    return Attr().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; });
}
Attr hzAttr()
{
    return Attr().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (v, v < 10.0f ? 2 : 0) + " Hz";
    });
}
Attr pctAttr()
{
    return Attr().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; });
}
Attr tenAttr()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); });
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto group = [] (const char* id, const char* name) { return std::make_unique<juce::AudioProcessorParameterGroup> (id, name, "|"); };
    auto pct = [] (const char* id, const char* name, float def) { return std::make_unique<APF> (pid (id), name, juce::NormalisableRange<float> (0.0f, 1.0f), def, pctAttr()); };
    auto ten = [] (const char* id, const char* name, float def) { return std::make_unique<APF> (pid (id), name, juce::NormalisableRange<float> (0.0f, 10.0f), def, tenAttr()); };
    auto db  = [] (const char* id, const char* name, float lo, float hi, float def) { return std::make_unique<APF> (pid (id), name, juce::NormalisableRange<float> (lo, hi), def, dbAttr()); };
    auto onOff = [] (const char* id, const char* name, bool def) { return std::make_unique<APB> (pid (id), name, def); };

    {   auto g = group ("global", "Global");
        g->addChild (db ("in_gain", "Input", -24.0f, 24.0f, 0.0f));
        g->addChild (db ("out_gain", "Output", -24.0f, 12.0f, 0.0f));
        g->addChild (onOff ("tuner", "Tuner (mute)", false));
        layout.add (std::move (g)); }

    {   auto g = group ("gate", "Noise Gate");
        g->addChild (onOff ("gate_on", "Gate On", true));
        g->addChild (db ("gate_thresh", "Gate Threshold", -90.0f, -20.0f, -58.0f));
        g->addChild (std::make_unique<APF> (pid ("gate_release"), "Gate Release", skewed (5.0f, 500.0f, 80.0f), 80.0f, msAttr()));
        g->addChild (std::make_unique<APC> (pid ("hum"), "Hum Filter", humChoices(), 0));
        layout.add (std::move (g)); }

    {   auto g = group ("comp", "Compressor");
        g->addChild (onOff ("comp_on", "Comp On", false));
        g->addChild (pct ("comp_sustain", "Comp Sustain", 0.5f));
        g->addChild (std::make_unique<APF> (pid ("comp_attack"), "Comp Attack", skewed (1.0f, 50.0f, 10.0f), 8.0f, msAttr()));
        g->addChild (db ("comp_level", "Comp Level", -12.0f, 12.0f, 0.0f));
        layout.add (std::move (g)); }

    {   auto g = group ("wah", "Wah");
        g->addChild (onOff ("wah_on", "Wah On", false));
        g->addChild (std::make_unique<APC> (pid ("wah_mode"), "Wah Mode", wahModes(), 0));
        g->addChild (pct ("wah_pedal", "Wah Pedal", 0.5f));
        g->addChild (pct ("wah_sens", "Wah Sensitivity", 0.5f));
        g->addChild (std::make_unique<APF> (pid ("wah_rate"), "Wah LFO Rate", skewed (0.1f, 8.0f, 1.5f), 1.5f, hzAttr()));
        g->addChild (std::make_unique<APF> (pid ("wah_q"), "Wah Resonance", juce::NormalisableRange<float> (1.0f, 10.0f), 5.0f, tenAttr()));
        layout.add (std::move (g)); }

    {   auto g = group ("oct", "Octave");
        g->addChild (onOff ("oct_on", "Octave On", false));
        g->addChild (pct ("oct_down", "Octave Down", 0.5f));
        g->addChild (pct ("oct_up", "Octave Up", 0.0f));
        g->addChild (pct ("oct_dry", "Octave Dry", 0.8f));
        layout.add (std::move (g)); }

    {   auto g = group ("drive", "Drive");
        g->addChild (onOff ("drv_on", "Drive On", false));
        g->addChild (std::make_unique<APC> (pid ("drv_type"), "Drive Type", driveTypes(), 0));
        g->addChild (pct ("drv_gain", "Drive Gain", 0.5f));
        g->addChild (pct ("drv_tone", "Drive Tone", 0.5f));
        g->addChild (pct ("drv_level", "Drive Level", 0.5f));
        layout.add (std::move (g)); }

    {   auto g = group ("amp", "Amp");
        g->addChild (onOff ("amp_on", "Amp On", true));
        g->addChild (std::make_unique<APC> (pid ("amp_model"), "Amp Model", ampModels(), 1));
        g->addChild (ten ("amp_gain", "Amp Gain", 5.0f));
        g->addChild (ten ("amp_bass", "Amp Bass", 5.0f));
        g->addChild (ten ("amp_mid", "Amp Middle", 5.0f));
        g->addChild (ten ("amp_treble", "Amp Treble", 5.0f));
        g->addChild (ten ("amp_presence", "Amp Presence", 5.0f));
        g->addChild (ten ("amp_master", "Amp Master", 5.0f));
        layout.add (std::move (g)); }

    {   auto g = group ("cab", "Cabinet");
        g->addChild (onOff ("cab_on", "Cab On", true));
        g->addChild (std::make_unique<APC> (pid ("cab_type"), "Cabinet", cabTypes(), 2));
        g->addChild (std::make_unique<APF> (pid ("cab_lowcut"), "Cab Low Cut", skewed (20.0f, 400.0f, 80.0f), 60.0f, hzAttr()));
        g->addChild (std::make_unique<APF> (pid ("cab_highcut"), "Cab High Cut", skewed (2000.0f, 20000.0f, 7000.0f), 9000.0f, hzAttr()));
        g->addChild (db ("cab_level", "Cab Level", -12.0f, 12.0f, 0.0f));
        layout.add (std::move (g)); }

    {   auto g = group ("eq", "Graphic EQ");
        g->addChild (onOff ("eq_on", "EQ On", false));
        for (int i = 0; i < kNumEqBands; ++i)
            g->addChild (db (eqIds[i], (juce::String ("EQ ") + eqLabels[i]).toRawUTF8(), -12.0f, 12.0f, 0.0f));
        g->addChild (db ("eq_level", "EQ Level", -12.0f, 12.0f, 0.0f));
        layout.add (std::move (g)); }

    {   auto g = group ("chorus", "Chorus");
        g->addChild (onOff ("cho_on", "Chorus On", false));
        g->addChild (std::make_unique<APF> (pid ("cho_rate"), "Chorus Rate", skewed (0.05f, 5.0f, 0.8f), 0.8f, hzAttr()));
        g->addChild (pct ("cho_depth", "Chorus Depth", 0.5f));
        g->addChild (pct ("cho_mix", "Chorus Mix", 0.5f));
        layout.add (std::move (g)); }

    {   auto g = group ("flanger", "Flanger");
        g->addChild (onOff ("fla_on", "Flanger On", false));
        g->addChild (std::make_unique<APF> (pid ("fla_rate"), "Flanger Rate", skewed (0.05f, 5.0f, 0.4f), 0.3f, hzAttr()));
        g->addChild (pct ("fla_depth", "Flanger Depth", 0.6f));
        g->addChild (std::make_unique<APF> (pid ("fla_regen"), "Flanger Regen", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.5f, pctAttr()));
        g->addChild (pct ("fla_mix", "Flanger Mix", 0.5f));
        layout.add (std::move (g)); }

    {   auto g = group ("phaser", "Phaser");
        g->addChild (onOff ("pha_on", "Phaser On", false));
        g->addChild (std::make_unique<APF> (pid ("pha_rate"), "Phaser Rate", skewed (0.05f, 8.0f, 0.6f), 0.5f, hzAttr()));
        g->addChild (pct ("pha_depth", "Phaser Depth", 0.7f));
        g->addChild (pct ("pha_feedback", "Phaser Feedback", 0.3f));
        g->addChild (pct ("pha_mix", "Phaser Mix", 0.5f));
        layout.add (std::move (g)); }

    {   auto g = group ("tremolo", "Tremolo");
        g->addChild (onOff ("tre_on", "Tremolo On", false));
        g->addChild (std::make_unique<APF> (pid ("tre_rate"), "Tremolo Rate", skewed (0.5f, 15.0f, 4.0f), 5.0f, hzAttr()));
        g->addChild (pct ("tre_depth", "Tremolo Depth", 0.6f));
        g->addChild (pct ("tre_shape", "Tremolo Shape", 0.2f));
        layout.add (std::move (g)); }

    {   auto g = group ("delay", "Delay");
        g->addChild (onOff ("dly_on", "Delay On", false));
        g->addChild (std::make_unique<APC> (pid ("dly_mode"), "Delay Mode", delayModes(), 0));
        g->addChild (std::make_unique<APF> (pid ("dly_time"), "Delay Time", skewed (20.0f, 1500.0f, 350.0f), 380.0f, msAttr()));
        g->addChild (std::make_unique<APC> (pid ("dly_sync"), "Delay Sync", delaySyncs(), 0));
        g->addChild (pct ("dly_feedback", "Delay Feedback", 0.35f));
        g->addChild (pct ("dly_mix", "Delay Mix", 0.3f));
        g->addChild (pct ("dly_tone", "Delay Tone", 0.5f));
        g->addChild (pct ("dly_mod", "Delay Wow/Flutter", 0.2f));
        g->addChild (onOff ("dly_pingpong", "Delay Ping-Pong", false));
        layout.add (std::move (g)); }

    {   auto g = group ("reverb", "Reverb");
        g->addChild (onOff ("rev_on", "Reverb On", false));
        g->addChild (std::make_unique<APC> (pid ("rev_type"), "Reverb Type", reverbTypes(), 0));
        g->addChild (pct ("rev_decay", "Reverb Decay", 0.5f));
        g->addChild (pct ("rev_tone", "Reverb Tone", 0.5f));
        g->addChild (std::make_unique<APF> (pid ("rev_predelay"), "Reverb Pre-Delay", juce::NormalisableRange<float> (0.0f, 200.0f), 10.0f, msAttr()));
        g->addChild (pct ("rev_mix", "Reverb Mix", 0.25f));
        layout.add (std::move (g)); }

    return layout;
}
} // namespace rig::params
