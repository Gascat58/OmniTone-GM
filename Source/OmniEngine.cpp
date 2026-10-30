#include "OmniEngine.h"

//==============================================================================
// EngineSet

EngineSet::~EngineSet()
{
    for (auto* e : engines)
        SoundFontCache::releaseEngine (e);
}

static int findMelodicPreset (tsf* f, int program)
{
    int idx = tsf_get_presetindex (f, 0, program);
    if (idx >= 0)
        return idx;

    // Not in bank 0: take the same program number from any melodic bank.
    const int count = tsf_get_presetcount (f);
    for (int i = 0; i < count; ++i)
        if (tsfx_preset_number (f, i) == program && tsfx_preset_bank (f, i) < 128)
            return i;

    // Still nothing: fall back to the first melodic preset (usually a piano).
    for (int i = 0; i < count; ++i)
        if (tsfx_preset_bank (f, i) < 128)
            return i;

    return count > 0 ? 0 : -1;
}

std::unique_ptr<EngineSet> EngineSet::build (const SoundSet& soundSet, double sampleRate)
{
    auto s = std::make_unique<EngineSet>();
    juce::StringArray paths; // lower-case path for each engine

    auto engineFor = [&] (const juce::String& path) -> int
    {
        const auto key = juce::File (path).getFullPathName().toLowerCase();
        const int existing = paths.indexOf (key);
        if (existing >= 0)
            return existing;

        juce::String error;
        if (auto* e = SoundFontCache::createEngine (path, &error))
        {
            s->engines.push_back (e);
            paths.add (key);
            return (int) s->engines.size() - 1;
        }

        s->problems.addIfNotAlreadyThere (error);
        return -1;
    };

    if (soundSet.mainFile.isNotEmpty())
        s->hasMain = (engineFor (soundSet.mainFile) == 0);
    else
        s->problems.add ("No main SoundFont chosen yet - click Browse to pick one.");

    if (! s->hasMain && ! s->engines.empty())
    {
        // Main failed but nothing else should take its place at index 0.
        for (auto* e : s->engines) SoundFontCache::releaseEngine (e);
        s->engines.clear();
        paths.clear();
    }

    auto resolveOverride = [&] (const SlotOverride& o, const juce::String& label) -> Slot
    {
        if (! o.active || o.file.isEmpty())
            return {};

        if (! s->hasMain && s->engines.empty())
            paths.add ("<no main>"), s->engines.push_back (nullptr); // keep index 0 reserved for main

        const int e = engineFor (o.file);
        if (e < 0)
            return {};

        const int idx = tsf_get_presetindex (s->engines[(size_t) e], o.bank, o.preset);
        if (idx < 0)
        {
            s->problems.add (label + ": preset " + juce::String (o.bank) + ":" + juce::String (o.preset)
                             + " is no longer in " + juce::File (o.file).getFileName());
            return {};
        }
        return { e, idx };
    };

    for (int p = 0; p < 128; ++p)
    {
        Slot slot = resolveOverride (soundSet.overrides[(size_t) p], GM::instrumentNames[p]);
        if (slot.engine < 0 && s->hasMain)
            slot = { 0, findMelodicPreset (s->engines[0], p) };
        s->melodic[(size_t) p] = slot;
    }

    s->drumOverride = resolveOverride (soundSet.overrides[GM::drumSlot], "Drums");
    s->hasDrumOverride = s->drumOverride.engine >= 0;

    for (auto* e : s->engines)
    {
        if (e == nullptr) continue;
        tsf_set_output (e, TSF_STEREO_UNWEAVED, (int) sampleRate, 0.0f);
        tsf_set_max_voices (e, maxVoices);                    // allocate now, not while playing
        tsf_channel_set_presetindex (e, numChannels - 1, 0);  // allocates all channels now
    }

    return s;
}

EngineSet::Slot EngineSet::drumSlotFor (int program) const
{
    if (hasDrumOverride)
        return drumOverride;
    if (! hasMain)
        return {};

    tsf* f = engines[0];
    int idx = tsf_get_presetindex (f, 128, program);           // the kit asked for (Standard, Room, Jazz...)
    if (idx < 0) idx = tsf_get_presetindex (f, 128, 0);        // Standard kit
    if (idx < 0)
    {
        const int count = tsf_get_presetcount (f);
        for (int i = 0; i < count && idx < 0; ++i)
            if (tsfx_preset_bank (f, i) >= 128)
                idx = i;
    }
    if (idx < 0) idx = findMelodicPreset (f, 0);
    return { 0, idx };
}

//==============================================================================
// OmniEngine

OmniEngine::OmniEngine()
{
    scratch.resize ((size_t) renderChunk * 2);
}

void OmniEngine::prepare (double newSampleRate)
{
    const juce::ScopedLock sl (lock);
    sampleRate = newSampleRate;
    reverb.setSampleRate (newSampleRate);
    reverb.reset();
    currentReverbSend = -1;

    if (set != nullptr)
        for (auto* e : set->engines)
            if (e != nullptr)
                tsf_set_output (e, TSF_STEREO_UNWEAVED, (int) newSampleRate, 0.0f);
}

void OmniEngine::setEngineSet (std::unique_ptr<EngineSet> newSet, bool reverbOn)
{
    {
        const juce::ScopedLock sl (lock);
        std::swap (set, newSet);
        reverbEnabled = reverbOn;
        needsReapply = true;
        previewEngine = previewNote = -1;
        previewSamplesLeft = 0;
    }
    // newSet now holds the old engines; they're released here, outside the lock.
}

void OmniEngine::reapplyAll()
{
    needsReapply = false;
    for (int ch = 0; ch < 16; ++ch)
    {
        auto& cs = channels[(size_t) ch];
        for (auto* e : set->engines)
        {
            if (e == nullptr) continue;
            tsf_channel_midi_control (e, ch, 7, cs.volume);
            tsf_channel_midi_control (e, ch, 11, cs.expression);
            tsf_channel_midi_control (e, ch, 10, cs.pan);
            tsf_channel_set_pitchwheel (e, ch, cs.pitchWheel);
            tsf_channel_set_sustain (e, ch, cs.sustain ? 1 : 0);
        }
        cs.engine = -1;
        applyProgram (ch);
    }
}

void OmniEngine::applyProgram (int ch)
{
    auto& cs = channels[(size_t) ch];
    const auto slot = (ch == GM::drumChannel) ? set->drumSlotFor (cs.program)
                                              : set->melodic[(size_t) cs.program];

    if (cs.engine >= 0 && cs.engine != slot.engine && set->engines[(size_t) cs.engine] != nullptr)
        tsf_channel_note_off_all (set->engines[(size_t) cs.engine], ch); // let old notes ring out

    if (slot.engine < 0 || set->engines[(size_t) slot.engine] == nullptr)
    {
        cs.engine = -1;
        return;
    }

    tsf_channel_set_presetindex (set->engines[(size_t) slot.engine], ch, slot.preset);
    cs.engine = slot.engine;
}

void OmniEngine::handleMessage (const juce::MidiMessage& m)
{
    int ch = m.getChannel() - 1;
    if (ch < 0 || ch >= 16)
        return;
    if (drumTrack.load())
        ch = GM::drumChannel; // drum track: everything goes to the drum kit
    auto& cs = channels[(size_t) ch];

    channelsSeen.fetch_or (1 << ch);

    if (m.isNoteOn())
    {
        if (cs.engine >= 0)
        {
            tsf_channel_note_on (set->engines[(size_t) cs.engine], ch, m.getNoteNumber(), m.getFloatVelocity());
            ++notesPlayed;
        }
        if (! cs.used)
        {
            cs.used = true;
            currentReverbSend = -1; // recalculate
        }
        lastChannel.store (ch);
        lastProgram.store (cs.program);
    }
    else if (m.isNoteOff())
    {
        for (auto* e : set->engines)
            if (e != nullptr)
                tsf_channel_note_off (e, ch, m.getNoteNumber());
    }
    else if (m.isProgramChange())
    {
        sawProgramChange = true;
        ++programChangesSeen;
        cs.explicitProgram = true;
        cs.program = m.getProgramChangeNumber() & 127;
        applyProgram (ch);
        lastChannel.store (ch);
        lastProgram.store (cs.program);
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const int value = m.getControllerValue();
        ++controllersSeen[(size_t) (cc & 127)];

        if (cc == GM::programCarrierCC)
        {
            // The patch number, smuggled past Mixcraft's import as a controller.
            sawCarrierCC = true;
            cs.explicitProgram = true;
            cs.program = value & 127;
            applyProgram (ch);
            lastChannel.store (ch);
            lastProgram.store (cs.program);
            return;
        }

        switch (cc)
        {
            case 7:   cs.volume = value; break;
            case 11:  cs.expression = value; break;
            case 10:  cs.pan = value; break;
            case 64:  cs.sustain = value >= 64; break;
            case 91:  cs.reverbSend = value; currentReverbSend = -1; break;
            case 121: cs.volume = 100; cs.expression = 127; cs.pan = 64; cs.pitchWheel = 8192; cs.sustain = false; break;
            default:  break;
        }

        for (auto* e : set->engines)
            if (e != nullptr)
                tsf_channel_midi_control (e, ch, cc, value);
    }
    else if (m.isPitchWheel())
    {
        cs.pitchWheel = m.getPitchWheelValue();
        for (auto* e : set->engines)
            if (e != nullptr)
                tsf_channel_set_pitchwheel (e, ch, cs.pitchWheel);
    }
}

void OmniEngine::stopPreview()
{
    if (previewEngine >= 0 && previewNote >= 0 && set->engines[(size_t) previewEngine] != nullptr)
        tsf_channel_note_off (set->engines[(size_t) previewEngine], EngineSet::previewChannel, previewNote);
    previewEngine = previewNote = -1;
    previewSamplesLeft = 0;
}

void OmniEngine::startPreview()
{
    const int slotNumber = previewRequest.exchange (-1);
    if (slotNumber < 0)
        return;

    stopPreview();

    const bool drums = slotNumber == GM::drumSlot;
    const auto slot = drums ? set->drumSlotFor (0) : set->melodic[(size_t) juce::jlimit (0, 127, slotNumber)];
    if (slot.engine < 0 || set->engines[(size_t) slot.engine] == nullptr)
        return;

    auto* e = set->engines[(size_t) slot.engine];
    tsf_channel_set_presetindex (e, EngineSet::previewChannel, slot.preset);
    previewNote = drums ? 38 : 60; // snare, or middle C
    tsf_channel_note_on (e, EngineSet::previewChannel, previewNote, 0.8f);
    previewEngine = slot.engine;
    previewSamplesLeft = (int) (sampleRate * 1.5);
}

void OmniEngine::render (juce::AudioBuffer<float>& buffer, int start, int numSamples)
{
    const int numOut = buffer.getNumChannels();
    while (numSamples > 0)
    {
        const int n = juce::jmin (numSamples, renderChunk);
        bool any = false;

        for (auto* e : set->engines)
        {
            if (e == nullptr) continue;
            tsf_render_float (e, scratch.data(), n, any ? 1 : 0);
            any = true;
        }

        if (any)
        {
            const float* left  = scratch.data();
            const float* right = scratch.data() + n; // "unweaved": all left samples, then all right
            if (numOut >= 2)
            {
                buffer.addFrom (0, start, left, n);
                buffer.addFrom (1, start, right, n);
            }
            else if (numOut == 1)
            {
                buffer.addFrom (0, start, left, n, 0.5f);
                buffer.addFrom (0, start, right, n, 0.5f);
            }
        }

        start += n;
        numSamples -= n;
    }
}

void OmniEngine::updateReverb()
{
    int send = 0;
    for (const auto& cs : channels)
        if (cs.used)
            send = juce::jmax (send, cs.reverbSend);

    if (send == currentReverbSend)
        return;

    currentReverbSend = send;
    juce::Reverb::Parameters p;
    p.roomSize = 0.55f;
    p.damping  = 0.45f;
    p.width    = 1.0f;
    p.dryLevel = 0.5f;                       // = unity gain inside juce::Reverb
    p.wetLevel = 0.25f * (float) send / 127.0f;
    reverb.setParameters (p);
}

void OmniEngine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
{
    buffer.clear();

    const juce::ScopedTryLock tl (lock);
    if (! tl.isLocked() || set == nullptr)
        return;

    const int newHostProgram = pendingHostProgram.exchange (-1);
    if (newHostProgram == GM::drumSlot)
    {
        // Drum track: every note is played on channel 10's drum kit.
        if (! drumTrack.load())
            for (int ch = 0; ch < 16; ++ch)
                for (auto* e : set->engines)
                    if (e != nullptr)
                        tsf_channel_note_off_all (e, ch);
        pendingOverride.store (false);
        drumTrack = true;
        lastProgram.store (GM::drumSlot);
    }
    else if (newHostProgram >= 0)
    {
        if (drumTrack.load())
            for (auto* e : set->engines)
                if (e != nullptr)
                    tsf_channel_note_off_all (e, GM::drumChannel);
        drumTrack = false;
        hostProgram = newHostProgram;
        const bool overrideAll = pendingOverride.exchange (false);
        for (int ch = 0; ch < 16; ++ch)
        {
            auto& cs = channels[(size_t) ch];
            if (ch == GM::drumChannel)
                continue;
            if (overrideAll)
                cs.explicitProgram = false; // a sound picked by hand beats the one from the file
            if (! cs.explicitProgram)
                cs.program = hostProgram;
        }
        needsReapply = true;
        lastProgram.store (hostProgram);
    }

    if (needsReapply)
        reapplyAll();

    startPreview();

    const int numSamples = buffer.getNumSamples();
    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, numSamples, meta.samplePosition);
        if (t > pos)
        {
            render (buffer, pos, t - pos);
            pos = t;
        }
        handleMessage (meta.getMessage());
    }
    if (pos < numSamples)
        render (buffer, pos, numSamples - pos);

    if (previewSamplesLeft > 0)
    {
        previewSamplesLeft -= numSamples;
        if (previewSamplesLeft <= 0)
            stopPreview();
    }

    if (reverbEnabled && buffer.getNumChannels() >= 2)
    {
        updateReverb();
        if (currentReverbSend > 0)
            reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
    }
}
