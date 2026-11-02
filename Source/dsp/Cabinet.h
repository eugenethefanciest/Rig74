#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "DspUtils.h"

namespace rig
{
/** Builds the built-in speaker-cabinet impulse responses for a given sample rate. */
juce::AudioBuffer<float> makeCabinetIR (int type, double sampleRate);

/** Scales an IR so its loudest point between 100 Hz and 5 kHz sits at 0 dB.
    (Energy normalisation would make the level depend on the sample rate.) */
inline void normaliseCabIR (juce::AudioBuffer<float>& ir, double sr)
{
    const int n = ir.getNumSamples();
    const float* x = ir.getReadPointer (0);
    double maxMag = 0.0;
    for (int k = 0; k < 48; ++k)
    {
        const double f = 100.0 * std::pow (50.0, k / 47.0);
        const double w = kTwoPi * f / sr;
        double re = 0.0, im = 0.0;
        for (int i = 0; i < n; ++i) { re += x[i] * std::cos (w * i); im -= x[i] * std::sin (w * i); }
        maxMag = std::max (maxMag, std::sqrt (re * re + im * im));
    }
    if (maxMag > 1.0e-9) ir.applyGain ((float) (1.0 / maxMag));
}

constexpr int kNumBuiltInCabs = 5;   // index 5 = user IR

/** Speaker cabinet: convolution with built-in or user-loaded IRs + low/high cut. */
class Cabinet
{
public:
    Cabinet()
    {
        for (int i = 0; i < kNumBuiltInCabs + 1; ++i)
            convs.push_back (std::make_unique<juce::dsp::Convolution> (queue));
    }

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        const juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 1 };
        for (auto& c : convs) c->prepare (spec);

        for (int i = 0; i < kNumBuiltInCabs; ++i)
        {
            auto ir = makeCabinetIR (i, sr);
            normaliseCabIR (ir, sr);
            convs[(size_t) i]->loadImpulseResponse (std::move (ir), sr,
                                                    juce::dsp::Convolution::Stereo::no,
                                                    juce::dsp::Convolution::Trim::no,
                                                    juce::dsp::Convolution::Normalise::no);
        }
        if (userIR.getNumSamples() > 0)
            loadUserIRBuffer();

        lowCut.reset(); highCut1.reset(); highCut2.reset();
        current = -1;
    }

    /** Message thread: load a .wav/.aiff impulse response. Returns false on failure. */
    bool loadUserIR (const juce::File& file)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples <= 0)
            return false;

        const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 1.0));
        juce::AudioBuffer<float> buf (1, len);
        reader->read (&buf, 0, len, 0, true, false);  // left (or mono) channel
        normaliseCabIR (buf, reader->sampleRate);
        userIR = std::move (buf);
        userIRRate = reader->sampleRate;
        userIRFile = file;
        loadUserIRBuffer();
        return true;
    }

    const juce::File& getUserIRFile() const noexcept { return userIRFile; }
    bool hasUserIR() const noexcept { return userIR.getNumSamples() > 0; }

    void setParams (int type, float lowCutHz, float highCutHz, float levelDb)
    {
        if (type != current)
        {
            current = type;
            convs[(size_t) juce::jlimit (0, kNumBuiltInCabs, type)]->reset();
        }
        if (lowCutHz != lastLow)   { lastLow = lowCutHz;  lowCut.makeHighPass (sr, lowCutHz, 0.707); }
        if (highCutHz != lastHigh) { lastHigh = highCutHz; highCut1.makeLowPass (sr, highCutHz, 0.54); highCut2.makeLowPass (sr, highCutHz, 1.31); }
        level = dbToGain (levelDb);
    }

    /** In-place mono processing. */
    void process (float* data, int numSamples)
    {
        const bool useUser = current == kNumBuiltInCabs;
        if (! useUser || hasUserIR())
        {
            float* chans[] = { data };
            juce::dsp::AudioBlock<float> block (chans, 1, (size_t) numSamples);
            convs[(size_t) juce::jlimit (0, kNumBuiltInCabs, current)]->process (juce::dsp::ProcessContextReplacing<float> (block));
        }
        for (int i = 0; i < numSamples; ++i)
            data[i] = highCut2.process (highCut1.process (lowCut.process (data[i]))) * level;
    }

private:
    void loadUserIRBuffer()
    {
        juce::AudioBuffer<float> copy (userIR);
        convs[(size_t) kNumBuiltInCabs]->loadImpulseResponse (std::move (copy), userIRRate,
                                                               juce::dsp::Convolution::Stereo::no,
                                                               juce::dsp::Convolution::Trim::yes,
                                                               juce::dsp::Convolution::Normalise::no);
    }

    juce::dsp::ConvolutionMessageQueue queue;
    std::vector<std::unique_ptr<juce::dsp::Convolution>> convs;
    juce::AudioBuffer<float> userIR;
    double userIRRate = 48000.0;
    juce::File userIRFile;

    double sr = 44100.0;
    int current = -1;
    float lastLow = -1.0f, lastHigh = -1.0f, level = 1.0f;
    Biquad lowCut, highCut1, highCut2;
};
} // namespace rig
