#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace rig;

//==============================================================================
struct Rig74Processor::P
{
    explicit P (juce::AudioProcessorValueTreeState& s) : st (s) {}
    juce::AudioProcessorValueTreeState& st;
    std::atomic<float>* get (const char* id) { auto* v = st.getRawParameterValue (id); jassert (v != nullptr); return v; }

    std::atomic<float>
        *inGain = get ("in_gain"), *outGain = get ("out_gain"), *tuner = get ("tuner"),
        *gateOn = get ("gate_on"), *gateThresh = get ("gate_thresh"), *gateRelease = get ("gate_release"), *hum = get ("hum"),
        *compOn = get ("comp_on"), *compSustain = get ("comp_sustain"), *compAttack = get ("comp_attack"), *compLevel = get ("comp_level"),
        *wahOn = get ("wah_on"), *wahMode = get ("wah_mode"), *wahPedal = get ("wah_pedal"), *wahSens = get ("wah_sens"),
        *wahRate = get ("wah_rate"), *wahQ = get ("wah_q"),
        *octOn = get ("oct_on"), *octDown = get ("oct_down"), *octUp = get ("oct_up"), *octDry = get ("oct_dry"),
        *drvOn = get ("drv_on"), *drvType = get ("drv_type"), *drvGain = get ("drv_gain"), *drvTone = get ("drv_tone"), *drvLevel = get ("drv_level"),
        *ampOn = get ("amp_on"), *ampModel = get ("amp_model"), *ampGain = get ("amp_gain"), *ampBass = get ("amp_bass"),
        *ampMid = get ("amp_mid"), *ampTreble = get ("amp_treble"), *ampPresence = get ("amp_presence"), *ampMaster = get ("amp_master"),
        *cabOn = get ("cab_on"), *cabType = get ("cab_type"), *cabLow = get ("cab_lowcut"), *cabHigh = get ("cab_highcut"), *cabLevel = get ("cab_level"),
        *eqOn = get ("eq_on"), *eqLevel = get ("eq_level"),
        *choOn = get ("cho_on"), *choRate = get ("cho_rate"), *choDepth = get ("cho_depth"), *choMix = get ("cho_mix"),
        *flaOn = get ("fla_on"), *flaRate = get ("fla_rate"), *flaDepth = get ("fla_depth"), *flaRegen = get ("fla_regen"), *flaMix = get ("fla_mix"),
        *phaOn = get ("pha_on"), *phaRate = get ("pha_rate"), *phaDepth = get ("pha_depth"), *phaFeedback = get ("pha_feedback"), *phaMix = get ("pha_mix"),
        *treOn = get ("tre_on"), *treRate = get ("tre_rate"), *treDepth = get ("tre_depth"), *treShape = get ("tre_shape"),
        *dlyOn = get ("dly_on"), *dlyMode = get ("dly_mode"), *dlyTime = get ("dly_time"), *dlySync = get ("dly_sync"),
        *dlyFeedback = get ("dly_feedback"), *dlyMix = get ("dly_mix"), *dlyTone = get ("dly_tone"), *dlyMod = get ("dly_mod"),
        *dlyPingPong = get ("dly_pingpong"),
        *revOn = get ("rev_on"), *revType = get ("rev_type"), *revDecay = get ("rev_decay"), *revTone = get ("rev_tone"),
        *revPredelay = get ("rev_predelay"), *revMix = get ("rev_mix");

    std::atomic<float>* eqBands[params::kNumEqBands] {};
};

static inline bool on (std::atomic<float>* v)   { return v->load() > 0.5f; }
static inline int  idx (std::atomic<float>* v)  { return juce::roundToInt (v->load()); }
static inline float val (std::atomic<float>* v) { return v->load(); }

//==============================================================================
Rig74Processor::Rig74Processor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RIG74", params::createLayout())
{
    p = std::make_unique<P> (apvts);
    for (int i = 0; i < params::kNumEqBands; ++i)
        p->eqBands[i] = apvts.getRawParameterValue (params::eqIds[i]);
}

Rig74Processor::~Rig74Processor() = default;

bool Rig74Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono()) return false;
    return true;
}

//==============================================================================
void Rig74Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = std::max (16, samplesPerBlock);
    const int n = currentBlockSize;

    mono.setSize (1, n);
    gateGains.setSize (1, n);
    dryMono.setSize (1, n);
    stereo.setSize (2, n);
    dryStereo.setSize (2, n);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing ((size_t) n);
    const double osRate = sampleRate * 4.0;
    setLatencySamples (juce::roundToInt (oversampler->getLatencyInSamples()));

    hum.prepare (sampleRate);
    gate.prepare (sampleRate);
    comp.prepare (sampleRate);
    wah.prepare (sampleRate);
    octaver.prepare (sampleRate);
    drive.prepare (osRate);
    amp.prepare (osRate);
    cabinet.prepare (sampleRate, n);
    eq.prepare (sampleRate);
    chorus.prepare (sampleRate);
    flanger.prepare (sampleRate);
    phaser.prepare (sampleRate);
    tremolo.prepare (sampleRate);
    echo.prepare (sampleRate);
    reverb.prepare (sampleRate, n);

    swGate.prepare (sampleRate, on (p->gateOn));   swComp.prepare (sampleRate, on (p->compOn));
    swWah.prepare (sampleRate, on (p->wahOn));     swOct.prepare (sampleRate, on (p->octOn));
    swDrive.prepare (osRate, on (p->drvOn));       swAmp.prepare (osRate, on (p->ampOn));
    swCab.prepare (sampleRate, on (p->cabOn));     swEq.prepare (sampleRate, on (p->eqOn));
    swCho.prepare (sampleRate, on (p->choOn));     swFla.prepare (sampleRate, on (p->flaOn));
    swPha.prepare (sampleRate, on (p->phaOn));     swTre.prepare (sampleRate, on (p->treOn));
    swDly.prepare (sampleRate, on (p->dlyOn));     swRev.prepare (sampleRate, on (p->revOn));

    inGain.reset (sampleRate, 0.03);  inGain.setCurrentAndTargetValue (dbToGain (val (p->inGain)));
    outGain.reset (sampleRate, 0.03); outGain.setCurrentAndTargetValue (dbToGain (val (p->outGain)));

    tunerFifo.reset();
}

//==============================================================================
void Rig74Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn = std::min (getTotalNumInputChannels(), buffer.getNumChannels());
    const int numOut = std::min (getTotalNumOutputChannels(), buffer.getNumChannels());
    if (numSamples == 0 || numOut == 0) return;

    // ---- decide where the guitar is (stereo track with the DI on one side only?) ----
    if (numIn >= 2)
    {
        const float l = buffer.getRMSLevel (0, 0, numSamples), r = buffer.getRMSLevel (1, 0, numSamples);
        if (l > 1.0e-4f || r > 1.0e-4f)
            monoSource = r < l * 0.05f ? 1 : (l < r * 0.05f ? 2 : 0);
    }
    else monoSource = 1;

    // ---- parameters (once per block) ----
    inGain.setTargetValue (dbToGain (val (p->inGain)));
    outGain.setTargetValue (dbToGain (val (p->outGain)));

    hum.setMode (idx (p->hum));
    gate.setParams (val (p->gateThresh), val (p->gateRelease));
    comp.setParams (val (p->compSustain), val (p->compAttack), val (p->compLevel));
    wah.setParams (idx (p->wahMode), val (p->wahPedal), val (p->wahSens), val (p->wahRate), val (p->wahQ));
    octaver.setParams (val (p->octDown), val (p->octUp), val (p->octDry));
    drive.setParams (idx (p->drvType), val (p->drvGain), val (p->drvTone), val (p->drvLevel));
    amp.setParams (idx (p->ampModel), val (p->ampGain), val (p->ampBass), val (p->ampMid), val (p->ampTreble), val (p->ampPresence), val (p->ampMaster));
    cabinet.setParams (idx (p->cabType), val (p->cabLow), val (p->cabHigh), val (p->cabLevel));

    float eqGains[params::kNumEqBands];
    for (int i = 0; i < params::kNumEqBands; ++i) eqGains[i] = val (p->eqBands[i]);
    eq.setParams (eqGains, params::eqFreqs, params::kNumEqBands, val (p->eqLevel));

    chorus.setParams (val (p->choRate), val (p->choDepth), val (p->choMix));
    flanger.setParams (val (p->flaRate), val (p->flaDepth), val (p->flaRegen), val (p->flaMix));
    phaser.setParams (val (p->phaRate), val (p->phaDepth), val (p->phaFeedback), val (p->phaMix));
    tremolo.setParams (val (p->treRate), val (p->treDepth), val (p->treShape));

    float delayMs = val (p->dlyTime);
    if (const int sync = idx (p->dlySync); sync > 0)
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm()) bpm = *b;
        delayMs = (float) juce::jlimit (20.0, 2000.0, 60000.0 / std::max (20.0, bpm) * params::delaySyncBeats (sync));
    }
    echo.setParams (idx (p->dlyMode), delayMs, val (p->dlyFeedback), val (p->dlyMix), val (p->dlyTone), val (p->dlyMod), on (p->dlyPingPong));
    reverb.setParams (idx (p->revType), val (p->revDecay), val (p->revTone), val (p->revPredelay), val (p->revMix));

    swGate.set (on (p->gateOn)); swComp.set (on (p->compOn)); swWah.set (on (p->wahOn)); swOct.set (on (p->octOn));
    swDrive.set (on (p->drvOn)); swAmp.set (on (p->ampOn)); swCab.set (on (p->cabOn)); swEq.set (on (p->eqOn));
    swCho.set (on (p->choOn)); swFla.set (on (p->flaOn)); swPha.set (on (p->phaOn)); swTre.set (on (p->treOn));
    swDly.set (on (p->dlyOn)); swRev.set (on (p->revOn));
    if (swCho.justEnabled) chorus.reset();
    if (swFla.justEnabled) flanger.reset();
    if (swPha.justEnabled) phaser.reset();
    if (swDly.justEnabled) echo.reset();
    if (swRev.justEnabled) reverb.reset();

    const bool tunerOn = on (p->tuner);

    float inPk = 0.0f, outPk = 0.0f;

    // ---- process in chunks no larger than the prepared block size ----
    for (int start = 0; start < numSamples; start += currentBlockSize)
    {
        const int len = std::min (currentBlockSize, numSamples - start);
        float* m = mono.getWritePointer (0);
        float* gg = gateGains.getWritePointer (0);

        // input → mono
        for (int i = 0; i < len; ++i)
        {
            float x = 0.0f;
            if (numIn >= 2)
            {
                const float l = buffer.getSample (0, start + i), r = buffer.getSample (1, start + i);
                x = monoSource == 1 ? l : (monoSource == 2 ? r : 0.5f * (l + r));
            }
            else if (numIn == 1) x = buffer.getSample (0, start + i);
            x *= inGain.getNextValue();
            inPk = std::max (inPk, std::abs (x));
            m[i] = x;
        }

        if (tunerOn)
        {
            int s1, n1, s2, n2;
            tunerFifo.prepareToWrite (len, s1, n1, s2, n2);
            if (n1 > 0) std::copy (m, m + n1, tunerBuffer.begin() + s1);
            if (n2 > 0) std::copy (m + n1, m + n1 + n2, tunerBuffer.begin() + s2);
            tunerFifo.finishedWrite (n1 + n2);

            for (int c = 0; c < numOut; ++c) buffer.clear (c, start, len);   // tuner mutes the output
            continue;
        }

        // ---- mono pre-chain: hum, gate key, comp, wah, octave ----
        for (int i = 0; i < len; ++i)
        {
            float x = hum.process (m[i]);
            const float gk = gate.computeGain (x);
            const float gs = swGate.next();
            gg[i] = 1.0f + gs * (gk - 1.0f);

            if (swComp.isActive()) { const float f = swComp.next(); x += f * (comp.process (x) - x); }
            if (swWah.isActive())  { const float f = swWah.next();  x += f * (wah.process (x) - x); }
            if (swOct.isActive())  { const float f = swOct.next();  x += f * (octaver.process (x) - x); }
            m[i] = x;
        }
        gateLevel.store (gate.getCurrentGain());

        // ---- 4x oversampled: drive + amp ----
        {
            float* chans[] = { m };
            juce::dsp::AudioBlock<float> block (chans, 1, (size_t) len);
            auto up = oversampler->processSamplesUp (block);
            float* u = up.getChannelPointer (0);
            const int un = (int) up.getNumSamples();
            for (int j = 0; j < un; ++j)
            {
                float x = u[j];
                if (swDrive.isActive()) { const float f = swDrive.next(); x += f * (drive.process (x) - x); }
                if (swAmp.isActive())   { const float f = swAmp.next();   x += f * (amp.process (x) - x); }
                u[j] = x;
            }
            oversampler->processSamplesDown (block);
        }

        // ---- cabinet ----
        if (swCab.isActive())
        {
            float* d = dryMono.getWritePointer (0);
            std::copy (m, m + len, d);
            cabinet.process (m, len);
            for (int i = 0; i < len; ++i) { const float f = swCab.next(); m[i] = d[i] + f * (m[i] - d[i]); }
        }

        // ---- noise gate gain (after amp & cab, where the hiss lives), EQ ----
        for (int i = 0; i < len; ++i)
        {
            float x = m[i] * gg[i];
            if (swEq.isActive()) { const float f = swEq.next(); x += f * (eq.process (x) - x); }
            m[i] = x;
        }

        // ---- stereo post-chain ----
        float* L = stereo.getWritePointer (0);
        float* R = stereo.getWritePointer (1);
        std::copy (m, m + len, L);
        std::copy (m, m + len, R);

        for (int i = 0; i < len; ++i)
        {
            float l = L[i], r = R[i];
            if (swCho.isActive()) { const float f = swCho.next(); float a = l, b = r; chorus.process (a, b);  l += f * (a - l); r += f * (b - r); }
            if (swFla.isActive()) { const float f = swFla.next(); float a = l, b = r; flanger.process (a, b); l += f * (a - l); r += f * (b - r); }
            if (swPha.isActive()) { const float f = swPha.next(); float a = l, b = r; phaser.process (a, b);  l += f * (a - l); r += f * (b - r); }
            if (swTre.isActive()) { const float f = swTre.next(); float a = l, b = r; tremolo.process (a, b); l += f * (a - l); r += f * (b - r); }
            if (swDly.isActive()) { const float f = swDly.next(); float a = l, b = r; echo.process (a, b);    l += f * (a - l); r += f * (b - r); }
            L[i] = l; R[i] = r;
        }

        if (swRev.isActive())
        {
            float* dl = dryStereo.getWritePointer (0);
            float* dr = dryStereo.getWritePointer (1);
            std::copy (L, L + len, dl);
            std::copy (R, R + len, dr);
            reverb.process (L, R, len);
            for (int i = 0; i < len; ++i)
            {
                const float f = swRev.next();
                L[i] = dl[i] + f * (L[i] - dl[i]);
                R[i] = dr[i] + f * (R[i] - dr[i]);
            }
        }

        // ---- output gain + safety soft clipper ----
        auto safety = [] (float x) noexcept
        {
            const float a = std::abs (x);
            if (a < 0.9f) return x;
            return std::copysign (0.9f + 0.1f * std::tanh ((a - 0.9f) * 10.0f), x);
        };

        for (int i = 0; i < len; ++i)
        {
            const float g = outGain.getNextValue() * 0.56f;   // internal headroom trim
            const float l = safety (L[i] * g), r = safety (R[i] * g);
            outPk = std::max (outPk, std::max (std::abs (l), std::abs (r)));
            if (numOut >= 2)
            {
                buffer.setSample (0, start + i, l);
                buffer.setSample (1, start + i, r);
            }
            else buffer.setSample (0, start + i, 0.5f * (l + r));
        }
    }

    for (int c = 2; c < numOut; ++c) buffer.clear (c, 0, numSamples);

    inputPeak.store (std::max (inputPeak.load(), inPk));
    outputPeak.store (std::max (outputPeak.load(), outPk));
}

//==============================================================================
int Rig74Processor::readTunerSamples (float* dest, int maxSamples)
{
    int s1, n1, s2, n2;
    tunerFifo.prepareToRead (std::min (maxSamples, tunerFifo.getNumReady()), s1, n1, s2, n2);
    if (n1 > 0) std::copy (tunerBuffer.begin() + s1, tunerBuffer.begin() + s1 + n1, dest);
    if (n2 > 0) std::copy (tunerBuffer.begin() + s2, tunerBuffer.begin() + s2 + n2, dest + n1);
    tunerFifo.finishedRead (n1 + n2);
    return n1 + n2;
}

bool Rig74Processor::loadUserIR (const juce::File& f)
{
    const bool ok = cabinet.loadUserIR (f);
    if (ok)
        if (auto* cab = apvts.getParameter ("cab_type"))
            cab->setValueNotifyingHost (cab->convertTo0to1 ((float) kNumBuiltInCabs));
    return ok;
}

//==============================================================================
int Rig74Processor::getNumPrograms() { return (int) factoryPresets().size(); }

void Rig74Processor::setCurrentProgram (int index)
{
    const auto& ps = factoryPresets();
    if (index < 0 || index >= (int) ps.size()) return;
    currentProgram = index;
    applyPreset (apvts, ps[(size_t) index]);
    if (onPresetLoaded) onPresetLoaded();
}

const juce::String Rig74Processor::getProgramName (int index)
{
    const auto& ps = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) ps.size()) ? ps[(size_t) index].name : juce::String();
}

void Rig74Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("userIR", cabinet.getUserIRFile().getFullPathName(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void Rig74Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (! xml->hasTagName (apvts.state.getType())) return;
        auto tree = juce::ValueTree::fromXml (*xml);
        currentProgram = tree.getProperty ("program", 0);
        const juce::File ir (tree.getProperty ("userIR").toString());
        apvts.replaceState (tree);
        if (ir.existsAsFile()) cabinet.loadUserIR (ir);
        if (onPresetLoaded) onPresetLoaded();
    }
}

bool Rig74Processor::savePresetToFile (const juce::File& f)
{
    auto state = apvts.copyState();
    state.setProperty ("userIR", cabinet.getUserIRFile().getFullPathName(), nullptr);
    if (auto xml = state.createXml())
        return xml->writeTo (f);
    return false;
}

bool Rig74Processor::loadPresetFromFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    auto tree = juce::ValueTree::fromXml (*xml);
    const juce::File ir (tree.getProperty ("userIR").toString());
    // keep the tuner state of the current session
    for (int i = 0; i < tree.getNumChildren(); ++i)
        if (tree.getChild (i).getProperty ("id").toString() == "tuner")
            tree.getChild (i).setProperty ("value", 0.0f, nullptr);
    apvts.replaceState (tree);
    if (ir.existsAsFile()) cabinet.loadUserIR (ir);
    if (onPresetLoaded) onPresetLoaded();
    return true;
}

//==============================================================================
juce::AudioProcessorEditor* Rig74Processor::createEditor() { return new Rig74Editor (*this); }

#if ! RIG74_TEST_BUILD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Rig74Processor(); }
#endif
