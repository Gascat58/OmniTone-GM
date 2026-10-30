#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "OmniEngine.h"

class OmniToneProcessor : public juce::AudioProcessor,
                          private juce::Timer
{
public:
    OmniToneProcessor();
    ~OmniToneProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "OmniTone GM"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    // The 128 GM sounds are offered to the host as presets, so a host can pick one directly.
    int getNumPrograms() override { return GM::numSlots; } // 128 instruments + Drums
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int index) override;
    void chooseProgramByHand (int index); // from the OmniTone window
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    // The sound set lives in a shared settings file, not in each Mixcraft project.
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int size) override;
    void updateTrackProperties (const TrackProperties& properties) override;

    // Diagnostics: what the host has told this copy of the plugin.
    std::atomic<int> setProgramCalls { 0 }, ignoredHostPicks { 0 };
    std::atomic<int> stateLoads { 0 }, lastStateSize { 0 };
    juce::String getTrackName() const { const juce::SpinLock::ScopedLockType sl (trackNameLock); return trackName; }

    //==============================================================================
    // Used by the editor (message thread only).
    const SoundSet& getSoundSet() const { return soundSet; }
    juce::StringArray getProblems() const { return problems; }
    void setMainSoundFont (const juce::String& path);
    void setOverride (int slot, const SlotOverride& o);
    void clearOverride (int slot);
    void setReverbOn (bool on);
    void replaceSoundSet (const SoundSet& newSet); // used by Import
    OmniEngine& getEngine() { return engine; }

    std::function<void()> onSoundSetChanged; // the editor listens to refresh its list

private:
    void timerCallback() override;
    void saveAndRebuild();
    void reloadFromDisk();
    void rebuild();

    SoundSet soundSet;
    OmniEngine engine;
    juce::StringArray problems;
    int seenGeneration = -1;
    juce::Time seenFileTime;

    std::atomic<int> currentProgram { 0 };
    std::atomic<int> lastHostChoice { 0 };   // the preset the host last sent us
    std::atomic<bool> handPicked { false };  // true after a click in the OmniTone window
public:
    std::atomic<bool> soundChosen { false }; // a sound has been picked by hand or by the host
private:
    juce::SpinLock trackNameLock;
    juce::String trackName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmniToneProcessor)
};
