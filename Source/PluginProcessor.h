#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "Presets.h"
#include "dsp/Dynamics.h"
#include "dsp/PreFx.h"
#include "dsp/Drive.h"
#include "dsp/Amp.h"
#include "dsp/Cabinet.h"
#include "dsp/ModFx.h"
#include "dsp/TimeFx.h"

/** Smooth on/off for each effect so switching never clicks. */
struct FxSwitch
{
    float v = 0.0f, coef = 0.999f;
    bool target = false, justEnabled = false;

    void prepare (double sr, bool on) { coef = rig::timeCoef (8.0f, sr); target = on; v = on ? 1.0f : 0.0f; }
    void set (bool on) { justEnabled = on && ! target && v <= 0.0f; target = on; }
    inline float next() noexcept
    {
        const float t = target ? 1.0f : 0.0f;
        v = t + coef * (v - t);
        if (! target && v < 1.0e-4f) v = 0.0f;
        if (target && v > 0.9999f) v = 1.0f;
        return v;
    }
    bool isActive() const noexcept { return target || v > 0.0f; }
};

class Rig74Processor : public juce::AudioProcessor
{
public:
    Rig74Processor();
    ~Rig74Processor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Rig 74"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    bool loadUserIR (const juce::File& f);
    juce::File getUserIRFile() const { return cabinet.getUserIRFile(); }

    /** Meters (read & cleared by the editor). */
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f }, gateLevel { 1.0f };

    /** Tuner: editor pulls recent input samples from here. */
    int readTunerSamples (float* dest, int maxSamples);
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }

    /** Saves / loads a single preset file (.rig74). */
    bool savePresetToFile (const juce::File& f);
    bool loadPresetFromFile (const juce::File& f);

    std::function<void()> onPresetLoaded;   // message thread callback for the GUI

private:
    struct P;
    std::unique_ptr<P> p;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int currentProgram = 0;
    int monoSource = 0;   // 0 = average, 1 = left only, 2 = right only

    juce::AudioBuffer<float> mono, gateGains, dryMono, stereo, dryStereo;
    juce::SmoothedValue<float> inGain, outGain;

    rig::HumFilter hum;
    rig::NoiseGate gate;
    rig::Compressor comp;
    rig::Wah wah;
    rig::Octaver octaver;
    rig::Drive drive;
    rig::TubeAmp amp;
    rig::Cabinet cabinet;
    rig::GraphicEq eq;
    rig::Chorus chorus;
    rig::Flanger flanger;
    rig::Phaser phaser;
    rig::Tremolo tremolo;
    rig::EchoDelay echo;
    rig::ReverbUnit reverb;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    FxSwitch swGate, swComp, swWah, swOct, swDrive, swAmp, swCab, swEq, swCho, swFla, swPha, swTre, swDly, swRev;

    juce::AbstractFifo tunerFifo { 16384 };
    std::vector<float> tunerBuffer = std::vector<float> (16384, 0.0f);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Rig74Processor)
};
