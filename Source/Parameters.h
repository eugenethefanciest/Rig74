#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace rig::params
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Choice lists (shared with the GUI so labels always match)
const juce::StringArray& humChoices();
const juce::StringArray& wahModes();
const juce::StringArray& driveTypes();
const juce::StringArray& ampModels();
const juce::StringArray& cabTypes();
const juce::StringArray& delayModes();
const juce::StringArray& delaySyncs();
const juce::StringArray& reverbTypes();

/** Beat multiples for the delay sync choices (index 0 = free time). */
double delaySyncBeats (int index);

constexpr int kNumEqBands = 7;
extern const float eqFreqs[kNumEqBands];
extern const char* const eqIds[kNumEqBands];
extern const char* const eqLabels[kNumEqBands];
} // namespace rig::params
