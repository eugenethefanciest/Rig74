#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <utility>

namespace rig
{
struct Preset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;   // parameter id → real (unnormalised) value
};

const std::vector<Preset>& factoryPresets();

/** Resets every parameter to its default, then applies the preset's values. */
void applyPreset (juce::AudioProcessorValueTreeState& state, const Preset& p);
} // namespace rig
