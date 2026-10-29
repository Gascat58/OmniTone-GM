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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    // The sound set lives in a shared settings file, not in each Mixcraft project.
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    //==============================================================================
    // Used by the editor (message thread only).
    const SoundSet& getSoundSet() const { return soundSet; }
    juce::StringArray getProblems() const { return problems; }
    void setMainSoundFont (const juce::String& path);
    void setOverride (int slot, const SlotOverride& o);
    void clearOverride (int slot);
    void setReverbOn (bool on);
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmniToneProcessor)
};
