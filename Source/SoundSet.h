#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include "GMNames.h"

// Where one slot's sound comes from when it has been replaced.
struct SlotOverride
{
    bool active = false;
    juce::String file;      // full path of the .sf2
    int bank = 0;           // SoundFont bank (128 = drum kits in most GM fonts)
    int preset = 0;         // SoundFont preset number inside that bank
    juce::String name;      // preset name, for display
};

// The user's sound set: one main SoundFont plus any replaced slots.
// It is stored in one settings file shared by every copy of the plugin.
struct SoundSet
{
    juce::String mainFile;
    bool reverbOn = true;
    std::array<SlotOverride, GM::numSlots> overrides; // 0..127 instruments, 128 = drums

    static juce::File getSettingsFile();

    bool load();        // returns false if there is no settings file yet
    bool save() const;

    juce::var toVar() const;
    void fromVar (const juce::var& v);

    // Bumped whenever any copy of the plugin saves, so the others reload straight away.
    static std::atomic<int>& generation();
};
