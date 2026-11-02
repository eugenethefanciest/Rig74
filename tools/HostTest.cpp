// Loads the built .vst3 like a DAW would: scan, instantiate, process, save/restore state, open the editor.
#include <juce_audio_utils/juce_audio_utils.h>

using namespace juce;

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::printf ("usage: HostTest <plugin.vst3>\n"); return 2; }

    AudioPluginFormatManager fm;
    fm.addFormat (new VST3PluginFormat());
    OwnedArray<PluginDescription> found;
    VST3PluginFormat vst3;
    vst3.findAllTypesForFile (found, argv[1]);
    if (found.isEmpty()) { std::printf ("FAIL: no plugin found in bundle\n"); return 1; }
    const auto& d = *found[0];
    std::printf ("Found: %s by %s, %s, in %d / out %d\n", d.name.toRawUTF8(), d.manufacturerName.toRawUTF8(), d.category.toRawUTF8(),
                 d.numInputChannels, d.numOutputChannels);

    String err;
    auto inst = fm.createPluginInstance (d, 48000.0, 256, err);
    if (inst == nullptr) { std::printf ("FAIL: %s\n", err.toRawUTF8()); return 1; }
    inst->enableAllBuses();
    inst->prepareToPlay (48000.0, 256);
    std::printf ("Parameters: %d, programs: %d, latency: %d\n", inst->getParameters().size(), inst->getNumPrograms(), inst->getLatencySamples());

    int bad = 0;
    AudioBuffer<float> buf (2, 256);
    MidiBuffer midi;
    for (int prog = 0; prog < inst->getNumPrograms(); ++prog)
    {
        inst->setCurrentProgram (prog);
        float peak = 0.0f;
        for (int b = 0; b < 400; ++b)
        {
            for (int i = 0; i < 256; ++i)
            {
                const float v = 0.25f * (float) std::sin (2.0 * MathConstants<double>::pi * 110.0 * (b * 256 + i) / 48000.0) * (b < 200 ? 1.0f : 0.0f);
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            inst->processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                {
                    const float v = buf.getSample (c, i);
                    if (! std::isfinite (v)) ++bad;
                    peak = std::max (peak, std::abs (v));
                }
        }
        std::printf ("  %-20s peak %.3f\n", inst->getProgramName (prog).toRawUTF8(), peak);
    }

    // state round trip
    inst->setCurrentProgram (3);
    MemoryBlock state;
    inst->getStateInformation (state);
    const float before = inst->getParameters()[20]->getValue();
    inst->setCurrentProgram (0);
    inst->setStateInformation (state.getData(), (int) state.getSize());
    const float after = inst->getParameters()[20]->getValue();
    std::printf ("State: %d bytes, round-trip %s\n", (int) state.getSize(), std::abs (before - after) < 1e-6f ? "OK" : "MISMATCH");
    if (std::abs (before - after) >= 1e-6f) ++bad;

    // editor
    if (auto* ed = inst->createEditorIfNeeded())
    {
        std::printf ("Editor: %d x %d\n", ed->getWidth(), ed->getHeight());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        File out = File::getCurrentWorkingDirectory().getChildFile ("test_out/host_editor.png");
        out.deleteFile();
        FileOutputStream os (out);
        PNGImageFormat().writeImageToStream (img, os);
        delete ed;
    }
    else { std::printf ("FAIL: no editor\n"); ++bad; }

    inst->releaseResources();
    inst.reset();
    std::printf ("%s\n", bad == 0 ? "HOST TEST PASSED" : "HOST TEST FAILED");
    return bad == 0 ? 0 : 1;
}
