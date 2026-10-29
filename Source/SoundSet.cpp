#include "SoundSet.h"

juce::File SoundSet::getSettingsFile()
{
    // e.g. C:\Users\<you>\AppData\Roaming\OmniTone GM\soundset.json
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("OmniTone GM")
               .getChildFile ("soundset.json");
}

std::atomic<int>& SoundSet::generation()
{
    static std::atomic<int> g { 0 };
    return g;
}

juce::var SoundSet::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("version", 1);
    root->setProperty ("mainSoundFont", mainFile);
    root->setProperty ("reverb", reverbOn);

    juce::Array<juce::var> list;
    for (int slot = 0; slot < GM::numSlots; ++slot)
    {
        const auto& o = overrides[(size_t) slot];
        if (! o.active)
            continue;

        auto* item = new juce::DynamicObject();
        item->setProperty ("slot", slot == GM::drumSlot ? juce::var ("drums") : juce::var (slot));
        item->setProperty ("file", o.file);
        item->setProperty ("bank", o.bank);
        item->setProperty ("preset", o.preset);
        item->setProperty ("name", o.name);
        list.add (juce::var (item));
    }
    root->setProperty ("replaced", list);
    return juce::var (root);
}

void SoundSet::fromVar (const juce::var& v)
{
    *this = SoundSet();
    mainFile = v.getProperty ("mainSoundFont", "").toString();
    reverbOn = (bool) v.getProperty ("reverb", true);

    if (auto* list = v.getProperty ("replaced", juce::var()).getArray())
    {
        for (const auto& item : *list)
        {
            const auto slotVar = item.getProperty ("slot", -1);
            int slot = slotVar.toString() == "drums" ? GM::drumSlot : (int) slotVar;
            if (slot < 0 || slot >= GM::numSlots)
                continue;

            auto& o = overrides[(size_t) slot];
            o.active = true;
            o.file   = item.getProperty ("file", "").toString();
            o.bank   = (int) item.getProperty ("bank", 0);
            o.preset = (int) item.getProperty ("preset", 0);
            o.name   = item.getProperty ("name", "").toString();
        }
    }
}

bool SoundSet::load()
{
    const auto f = getSettingsFile();
    if (! f.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (f.loadFileAsString());
    if (! parsed.isObject())
        return false;

    fromVar (parsed);
    return true;
}

bool SoundSet::save() const
{
    const auto f = getSettingsFile();
    f.getParentDirectory().createDirectory();
    const bool ok = f.replaceWithText (juce::JSON::toString (toVar()));
    ++generation();
    return ok;
}
