#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "SoundSet.h"
#include "SoundFontCache.h"

// A ready-to-play snapshot of the sound set: one voice engine per SoundFont file in use,
// plus a lookup table saying which engine and preset each GM slot plays.
struct EngineSet
{
    struct Slot { int engine = -1; int preset = -1; };

    std::vector<tsf*> engines;          // engines[0] is the main SoundFont (when loaded)
    std::array<Slot, 128> melodic {};
    Slot drumOverride;
    bool hasDrumOverride = false;
    bool hasMain = false;
    juce::StringArray problems;         // shown in the editor

    ~EngineSet();

    static std::unique_ptr<EngineSet> build (const SoundSet& set, double sampleRate);
    Slot drumSlotFor (int program) const;

    static constexpr int numChannels = 17; // 16 MIDI channels + 1 private channel for Preview
    static constexpr int previewChannel = 16;
    static constexpr int maxVoices = 96;
};

// The real-time part: turns incoming MIDI into audio.
class OmniEngine
{
public:
    OmniEngine();

    void prepare (double sampleRate);
    void setEngineSet (std::unique_ptr<EngineSet> newSet, bool reverbOn); // message thread
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi); // audio thread
    void requestPreview (int slot) { previewRequest.store (slot); }

    // A program chosen by the host as a plugin preset (not a MIDI event). It becomes the
    // sound for every channel that hasn't received its own CC 3 / program change.
    void setHostProgram (int program, bool overrideFileSounds = false)
    {
        pendingOverride.store (overrideFileSounds);
        pendingHostProgram.store (program & 127);
    }

    double getSampleRate() const { return sampleRate; }

    // Read by the editor to show what this track is doing.
    std::atomic<int>  lastChannel { -1 };
    std::atomic<int>  lastProgram { -1 };
    std::atomic<bool> sawCarrierCC { false };
    std::atomic<bool> sawProgramChange { false };
    std::atomic<int>  notesPlayed { 0 };

    // Diagnostics: what MIDI actually arrives from the host.
    std::atomic<int> channelsSeen { 0 };                 // bit per MIDI channel
    std::atomic<int> programChangesSeen { 0 };
    std::array<std::atomic<int>, 128> controllersSeen {};

private:
    struct ChannelState
    {
        int program = 0;
        int volume = 100, expression = 127, pan = 64, pitchWheel = 8192;
        int reverbSend = 40;
        bool sustain = false;
        bool used = false;
        bool explicitProgram = false; // set by CC 3 or a program change
        int engine = -1;
    };

    void handleMessage (const juce::MidiMessage& m);
    void applyProgram (int channel);
    void reapplyAll();
    void render (juce::AudioBuffer<float>& buffer, int start, int numSamples);
    void startPreview();
    void stopPreview();
    void updateReverb();

    juce::CriticalSection lock;
    std::unique_ptr<EngineSet> set;
    bool needsReapply = false;
    bool reverbEnabled = true;

    double sampleRate = 44100.0;
    std::array<ChannelState, 16> channels {};

    static constexpr int renderChunk = 512;
    std::vector<float> scratch;

    juce::Reverb reverb;
    int currentReverbSend = -1;

    std::atomic<int> previewRequest { -1 };
    std::atomic<int> pendingHostProgram { -1 };
    std::atomic<bool> pendingOverride { false };
    int hostProgram = -1;
    int previewEngine = -1, previewNote = -1;
    int previewSamplesLeft = 0;
};
