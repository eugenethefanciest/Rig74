#include "Presets.h"

namespace rig
{
const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets
    {
        { "Init Rig", {} },

        { "Sunday Clean",
          { { "amp_model", 0 }, { "amp_gain", 3.5f }, { "amp_bass", 5.5f }, { "amp_mid", 4.5f }, { "amp_treble", 6.2f },
            { "amp_presence", 5.0f }, { "amp_master", 5.0f }, { "cab_type", 1 },
            { "comp_on", 1 }, { "comp_sustain", 0.45f },
            { "cho_on", 1 }, { "cho_rate", 0.7f }, { "cho_depth", 0.45f }, { "cho_mix", 0.45f },
            { "rev_on", 1 }, { "rev_type", 0 }, { "rev_decay", 0.45f }, { "rev_mix", 0.25f } } },

        { "Crunch '68",
          { { "amp_model", 1 }, { "amp_gain", 6.5f }, { "amp_bass", 5.0f }, { "amp_mid", 6.5f }, { "amp_treble", 6.0f },
            { "amp_presence", 6.0f }, { "amp_master", 6.0f }, { "cab_type", 2 },
            { "rev_on", 1 }, { "rev_type", 1 }, { "rev_decay", 0.35f }, { "rev_mix", 0.15f } } },

        { "Stadium Lead '77",
          { { "drv_on", 1 }, { "drv_type", 0 }, { "drv_gain", 0.25f }, { "drv_tone", 0.55f }, { "drv_level", 0.7f },
            { "amp_model", 2 }, { "amp_gain", 7.0f }, { "amp_mid", 7.0f }, { "amp_treble", 6.0f }, { "amp_presence", 6.0f },
            { "amp_master", 6.5f }, { "cab_type", 2 },
            { "dly_on", 1 }, { "dly_mode", 0 }, { "dly_time", 420.0f }, { "dly_feedback", 0.32f }, { "dly_mix", 0.25f },
            { "dly_tone", 0.45f }, { "dly_mod", 0.2f },
            { "rev_on", 1 }, { "rev_type", 2 }, { "rev_decay", 0.6f }, { "rev_mix", 0.2f } } },

        { "Fuzz Machine",
          { { "drv_on", 1 }, { "drv_type", 2 }, { "drv_gain", 0.8f }, { "drv_tone", 0.55f }, { "drv_level", 0.6f },
            { "amp_model", 0 }, { "amp_gain", 5.0f }, { "amp_bass", 6.0f }, { "amp_treble", 5.5f }, { "amp_master", 6.0f },
            { "cab_type", 3 }, { "gate_thresh", -55.0f },
            { "rev_on", 1 }, { "rev_type", 0 }, { "rev_decay", 0.3f }, { "rev_mix", 0.2f } } },

        { "Funky Wah",
          { { "wah_on", 1 }, { "wah_mode", 1 }, { "wah_sens", 0.55f }, { "wah_q", 6.0f },
            { "comp_on", 1 }, { "comp_sustain", 0.6f },
            { "amp_model", 0 }, { "amp_gain", 4.0f }, { "amp_treble", 6.5f }, { "cab_type", 0 },
            { "rev_on", 1 }, { "rev_type", 1 }, { "rev_mix", 0.12f } } },

        { "Space Echo Dream",
          { { "amp_model", 0 }, { "amp_gain", 4.0f }, { "cab_type", 1 },
            { "cho_on", 1 }, { "cho_mix", 0.3f },
            { "dly_on", 1 }, { "dly_mode", 0 }, { "dly_time", 520.0f }, { "dly_feedback", 0.55f }, { "dly_mix", 0.4f },
            { "dly_tone", 0.4f }, { "dly_mod", 0.55f }, { "dly_pingpong", 1 },
            { "rev_on", 1 }, { "rev_type", 0 }, { "rev_decay", 0.65f }, { "rev_mix", 0.3f } } },

        { "Phased Out",
          { { "amp_model", 1 }, { "amp_gain", 5.0f }, { "cab_type", 2 },
            { "pha_on", 1 }, { "pha_rate", 0.45f }, { "pha_depth", 0.75f }, { "pha_feedback", 0.35f }, { "pha_mix", 0.5f } } },

        { "Surf Tremolo",
          { { "amp_model", 0 }, { "amp_gain", 4.5f }, { "amp_bass", 4.5f }, { "amp_treble", 6.5f }, { "cab_type", 3 },
            { "tre_on", 1 }, { "tre_rate", 6.0f }, { "tre_depth", 0.7f }, { "tre_shape", 0.3f },
            { "rev_on", 1 }, { "rev_type", 0 }, { "rev_decay", 0.75f }, { "rev_tone", 0.6f }, { "rev_mix", 0.45f } } },

        { "Jet Flanger",
          { { "amp_model", 2 }, { "amp_gain", 6.0f }, { "cab_type", 2 },
            { "fla_on", 1 }, { "fla_rate", 0.15f }, { "fla_depth", 0.8f }, { "fla_regen", 0.7f }, { "fla_mix", 0.5f } } },

        { "Octave Freakout",
          { { "oct_on", 1 }, { "oct_down", 0.0f }, { "oct_up", 0.8f }, { "oct_dry", 0.5f },
            { "drv_on", 1 }, { "drv_type", 2 }, { "drv_gain", 0.75f }, { "gate_thresh", -50.0f },
            { "amp_model", 1 }, { "amp_gain", 5.0f }, { "cab_type", 2 } } },

        { "Hot Rod '79",
          { { "amp_model", 3 }, { "amp_gain", 7.0f }, { "amp_bass", 6.0f }, { "amp_mid", 5.0f }, { "amp_treble", 6.5f },
            { "amp_presence", 6.0f }, { "amp_master", 6.0f }, { "cab_type", 2 },
            { "gate_thresh", -55.0f }, { "gate_release", 60.0f },
            { "eq_on", 1 }, { "eq_100", 2.0f }, { "eq_800", -2.0f }, { "eq_3k2", 1.5f },
            { "dly_on", 1 }, { "dly_mode", 2 }, { "dly_time", 300.0f }, { "dly_feedback", 0.2f }, { "dly_mix", 0.12f } } },

        { "Jazz Box",
          { { "amp_model", 0 }, { "amp_gain", 3.0f }, { "amp_bass", 6.0f }, { "amp_treble", 3.5f }, { "cab_type", 4 },
            { "comp_on", 1 }, { "comp_sustain", 0.3f },
            { "rev_on", 1 }, { "rev_type", 1 }, { "rev_decay", 0.4f }, { "rev_mix", 0.18f } } },

        { "Doom Sub Octave",
          { { "oct_on", 1 }, { "oct_down", 0.7f }, { "oct_up", 0.0f }, { "oct_dry", 0.7f },
            { "drv_on", 1 }, { "drv_type", 1 }, { "drv_gain", 0.7f }, { "drv_tone", 0.35f },
            { "amp_model", 3 }, { "amp_gain", 6.0f }, { "amp_bass", 7.0f }, { "cab_type", 2 }, { "gate_thresh", -52.0f } } },
    };
    return presets;
}

void applyPreset (juce::AudioProcessorValueTreeState& state, const Preset& p)
{
    for (auto* param : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (rp->getParameterID() != "tuner")
                rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : p.values)
        if (auto* rp = state.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}
} // namespace rig
