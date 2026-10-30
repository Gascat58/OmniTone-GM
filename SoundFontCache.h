#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include <mutex>
#include <vector>
#include "TsfExtras.h"

struct PresetInfo
{
    int bank = 0;
    int preset = 0;
    int index = 0;
    juce::String name;
};

// Loads each .sf2 file once per Mixcraft session and shares its samples between
// every copy of the plugin (each track only gets its own lightweight voice engine).
class SoundFontCache
{
public:
    // Returns a new engine that shares the file's samples, or nullptr if it can't be loaded.
    // The caller owns it and must give it back with releaseEngine().
    static tsf* createEngine (const juce::String& path, juce::String* errorOut = nullptr);
    static void releaseEngine (tsf* engine);

    static std::vector<PresetInfo> listPresets (const juce::String& path);

private:
    static tsf* getMaster (const juce::String& path, juce::String* errorOut);
    static std::mutex& mutex();
    static std::map<juce::String, tsf*>& masters();
};
