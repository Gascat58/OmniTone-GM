#include "PluginProcessor.h"
#include "PluginEditor.h"

OmniToneProcessor::OmniToneProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    reloadFromDisk();
    startTimer (1500); // pick up changes made from another track's OmniTone window
}

OmniToneProcessor::~OmniToneProcessor()
{
    stopTimer();
}

bool OmniToneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void OmniToneProcessor::prepareToPlay (double sampleRate, int)
{
    if (! juce::approximatelyEqual (sampleRate, engine.getSampleRate()))
    {
        engine.prepare (sampleRate);
        rebuild();
    }
    else
    {
        engine.prepare (sampleRate);
    }
}

void OmniToneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    engine.process (buffer, midi);
}

juce::AudioProcessorEditor* OmniToneProcessor::createEditor()
{
    return new OmniToneEditor (*this);
}

//==============================================================================
void OmniToneProcessor::rebuild()
{
    auto newSet = EngineSet::build (soundSet, engine.getSampleRate());
    problems = newSet->problems;
    engine.setEngineSet (std::move (newSet), soundSet.reverbOn);
    if (onSoundSetChanged)
        onSoundSetChanged();
}

void OmniToneProcessor::reloadFromDisk()
{
    seenGeneration = SoundSet::generation().load();
    const auto file = SoundSet::getSettingsFile();
    seenFileTime = file.getLastModificationTime();

    if (! soundSet.load())
        soundSet = SoundSet();
    rebuild();
}

void OmniToneProcessor::saveAndRebuild()
{
    soundSet.save();
    seenGeneration = SoundSet::generation().load();
    seenFileTime = SoundSet::getSettingsFile().getLastModificationTime();
    rebuild();
}

void OmniToneProcessor::timerCallback()
{
    const bool otherCopySaved = SoundSet::generation().load() != seenGeneration;
    const bool fileChanged = SoundSet::getSettingsFile().getLastModificationTime() != seenFileTime;
    if (otherCopySaved || fileChanged)
        reloadFromDisk();
}

void OmniToneProcessor::setMainSoundFont (const juce::String& path)
{
    soundSet.mainFile = path;
    saveAndRebuild();
}

void OmniToneProcessor::setOverride (int slot, const SlotOverride& o)
{
    if (slot < 0 || slot >= GM::numSlots) return;
    soundSet.overrides[(size_t) slot] = o;
    soundSet.overrides[(size_t) slot].active = true;
    saveAndRebuild();
}

void OmniToneProcessor::clearOverride (int slot)
{
    if (slot < 0 || slot >= GM::numSlots) return;
    soundSet.overrides[(size_t) slot] = SlotOverride();
    saveAndRebuild();
}

void OmniToneProcessor::replaceSoundSet (const SoundSet& newSet)
{
    // Keep a copy of the set being replaced, just in case.
    const auto settings = SoundSet::getSettingsFile();
    if (settings.existsAsFile())
        settings.copyFileTo (settings.getSiblingFile ("soundset-before-import-"
                                                      + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S")
                                                      + ".json"));
    soundSet = newSet;
    saveAndRebuild();
}

void OmniToneProcessor::setReverbOn (bool on)
{
    soundSet.reverbOn = on;
    saveAndRebuild();
}

//==============================================================================
void OmniToneProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, (int) GM::drumSlot, index);
    ++setProgramCalls;

    // The host remembers the last preset it chose and may send it again at any time
    // (when the window updates, on playback reset...). After a sound has been clicked in
    // the OmniTone window, only a NEW choice from the host's dropdown is obeyed.
    const int previousHostChoice = lastHostChoice.exchange (index);
    if (handPicked.load() && index == previousHostChoice && index != currentProgram.load())
    {
        ++ignoredHostPicks;
        return;
    }

    handPicked = false;
    soundChosen = true;
    currentProgram.store (index);
    engine.setHostProgram (index);
}

void OmniToneProcessor::chooseProgramByHand (int index)
{
    index = juce::jlimit (0, (int) GM::drumSlot, index);
    handPicked = true;
    soundChosen = true;
    currentProgram.store (index);
    engine.setHostProgram (index, true);
    updateHostDisplay (ChangeDetails().withProgramChanged (true)); // let the dropdown show it too
}

const juce::String OmniToneProcessor::getProgramName (int index)
{
    if (index == GM::drumSlot)
        return "Drums";
    if (! juce::isPositiveAndBelow (index, 128))
        return {};
    return juce::String (index).paddedLeft ('0', 3) + " " + GM::instrumentNames[index];
}

void OmniToneProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    // Only remember which GM preset the track uses; the sound set itself is global.
    juce::MemoryOutputStream out (dest, false);
    out.writeString ("OmniToneGM");
    out.writeInt (currentProgram.load());
}

void OmniToneProcessor::setStateInformation (const void* data, int size)
{
    ++stateLoads;
    lastStateSize.store (size);

    juce::MemoryInputStream in (data, (size_t) juce::jmax (0, size), false);
    if (size >= 14 && in.readString() == "OmniToneGM")
    {
        // A saved project: restore the track's sound exactly as it was.
        const int saved = juce::jlimit (0, (int) GM::drumSlot, in.readInt());
        handPicked = false;
        soundChosen = true;
        lastHostChoice.store (saved);
        currentProgram.store (saved);
        engine.setHostProgram (saved, true);
    }
}

void OmniToneProcessor::updateTrackProperties (const TrackProperties& properties)
{
    const juce::SpinLock::ScopedLockType sl (trackNameLock);
    trackName = properties.name;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OmniToneProcessor();
}
