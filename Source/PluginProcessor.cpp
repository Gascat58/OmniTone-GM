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

void OmniToneProcessor::setReverbOn (bool on)
{
    soundSet.reverbOn = on;
    saveAndRebuild();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OmniToneProcessor();
}
