#include "SoundFontCache.h"
#include <algorithm>

std::mutex& SoundFontCache::mutex()
{
    static std::mutex m;
    return m;
}

std::map<juce::String, tsf*>& SoundFontCache::masters()
{
    // Masters stay loaded until Mixcraft closes; their memory is shared, not duplicated per track.
    static std::map<juce::String, tsf*> m;
    return m;
}

tsf* SoundFontCache::getMaster (const juce::String& path, juce::String* errorOut)
{
    const juce::File file (path);
    const auto key = file.getFullPathName().toLowerCase();

    auto it = masters().find (key);
    if (it != masters().end())
        return it->second;

    if (! file.existsAsFile())
    {
        if (errorOut) *errorOut = "SoundFont not found: " + path;
        return nullptr;
    }

    juce::MemoryBlock data;
    if (! file.loadFileAsData (data) || data.getSize() < 12)
    {
        if (errorOut) *errorOut = "Couldn't read: " + file.getFileName();
        return nullptr;
    }

    tsf* master = tsf_load_memory (data.getData(), (int) data.getSize());
    if (master == nullptr || tsf_get_presetcount (master) == 0)
    {
        if (master) tsf_close (master);
        if (errorOut) *errorOut = file.getFileName() + " isn't a valid SoundFont (.sf2) file";
        return nullptr;
    }

    masters()[key] = master;
    return master;
}

tsf* SoundFontCache::createEngine (const juce::String& path, juce::String* errorOut)
{
    std::lock_guard<std::mutex> lock (mutex());
    if (auto* master = getMaster (path, errorOut))
        return tsf_copy (master);
    return nullptr;
}

void SoundFontCache::releaseEngine (tsf* engine)
{
    if (engine == nullptr)
        return;
    std::lock_guard<std::mutex> lock (mutex()); // the shared sample counter isn't thread-safe on its own
    tsf_close (engine);
}

std::vector<PresetInfo> SoundFontCache::listPresets (const juce::String& path)
{
    std::lock_guard<std::mutex> lock (mutex());
    std::vector<PresetInfo> result;

    if (auto* master = getMaster (path, nullptr))
    {
        const int count = tsf_get_presetcount (master);
        for (int i = 0; i < count; ++i)
        {
            PresetInfo p;
            p.index  = i;
            p.bank   = tsfx_preset_bank (master, i);
            p.preset = tsfx_preset_number (master, i);
            p.name   = juce::String::fromUTF8 (tsf_get_presetname (master, i)).trim();
            result.push_back (p);
        }

        std::sort (result.begin(), result.end(), [] (const PresetInfo& a, const PresetInfo& b)
        {
            return a.bank != b.bank ? a.bank < b.bank : a.preset < b.preset;
        });
    }

    return result;
}
