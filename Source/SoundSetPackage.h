#pragma once

#include <juce_core/juce_core.h>
#include "SoundSet.h"

// A sound set packed into one file (.otgm): the settings plus every SoundFont they use.
// It's a standard zip inside, so it can also be opened with Windows if ever needed.
namespace SoundSetPackage
{
    static constexpr const char* fileExtension = ".otgm";

    // Where imported sound sets are unpacked, e.g. ...\AppData\Roaming\OmniTone GM\Sound Sets
    juce::File getSoundSetsFolder();

    // Lists the SoundFont files a set needs (main + replacements), without duplicates.
    juce::Array<juce::File> getFilesUsed (const SoundSet& set);

    // Writes the package. progress (optional) goes from 0 to 1. Safe to call off the message thread.
    bool exportTo (const SoundSet& set, const juce::File& destination, juce::String& error, double* progress = nullptr);

    // Unpacks a package into its own new folder under getSoundSetsFolder() and returns the set
    // with its paths pointing there. Safe to call off the message thread.
    bool importFrom (const juce::File& package, SoundSet& result, juce::File& installedFolder, juce::String& error);
}
