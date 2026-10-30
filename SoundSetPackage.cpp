#include "SoundSetPackage.h"

namespace SoundSetPackage
{

static constexpr const char* settingsEntry = "soundset.json";
static constexpr const char* fontsFolder   = "SoundFonts";

juce::File getSoundSetsFolder()
{
    return SoundSet::getSettingsFile().getParentDirectory().getChildFile ("Sound Sets");
}

juce::Array<juce::File> getFilesUsed (const SoundSet& set)
{
    juce::Array<juce::File> files;
    auto add = [&files] (const juce::String& path)
    {
        if (path.isEmpty())
            return;
        const juce::File f (path);
        for (const auto& existing : files)
            if (existing.getFullPathName().equalsIgnoreCase (f.getFullPathName()))
                return;
        files.add (f);
    };

    add (set.mainFile);
    for (const auto& o : set.overrides)
        if (o.active)
            add (o.file);
    return files;
}

bool exportTo (const SoundSet& set, const juce::File& destination, juce::String& error, double* progress)
{
    const auto files = getFilesUsed (set);
    if (files.isEmpty())
    {
        error = "There's nothing to export yet - choose a main SoundFont first.";
        return false;
    }

    juce::StringArray missing;
    for (const auto& f : files)
        if (! f.existsAsFile())
            missing.add (f.getFullPathName());
    if (! missing.isEmpty())
    {
        error = "These SoundFonts can't be found, so the set can't be packed:\n\n" + missing.joinIntoString ("\n");
        return false;
    }

    // Give every file a unique name inside the package.
    juce::StringPairArray storedNames; // lower-case full path -> name in package
    juce::StringArray usedNames;
    juce::ZipFile::Builder builder;

    for (const auto& f : files)
    {
        auto name = f.getFileName();
        for (int n = 2; usedNames.contains (name, true); ++n)
            name = f.getFileNameWithoutExtension() + " (" + juce::String (n) + ")" + f.getFileExtension();
        usedNames.add (name);

        const auto stored = juce::String (fontsFolder) + "/" + name;
        storedNames.set (f.getFullPathName().toLowerCase(), stored);
        builder.addFile (f, 1, stored); // light compression: SoundFonts don't shrink much, so keep it quick
    }

    auto relative = [&storedNames] (const juce::String& path)
    {
        return path.isEmpty() ? juce::String() : storedNames[juce::File (path).getFullPathName().toLowerCase()];
    };

    SoundSet packed = set;
    packed.mainFile = relative (set.mainFile);
    for (auto& o : packed.overrides)
        if (o.active)
            o.file = relative (o.file);

    auto json = packed.toVar();
    if (auto* obj = json.getDynamicObject())
    {
        obj->setProperty ("package", "OmniTone GM sound set");
        obj->setProperty ("exported", juce::Time::getCurrentTime().toISO8601 (true));
    }
    const auto jsonText = juce::JSON::toString (json);
    builder.addEntry (new juce::MemoryInputStream (jsonText.toRawUTF8(), jsonText.getNumBytesAsUTF8(), true),
                      9, settingsEntry, juce::Time::getCurrentTime());

    // Write to a temporary file first so a failed export never leaves half a file behind.
    juce::TemporaryFile temp (destination);
    {
        juce::FileOutputStream out (temp.getFile());
        if (! out.openedOk())
        {
            error = "Couldn't write to " + destination.getFullPathName();
            return false;
        }
        if (! builder.writeToStream (out, progress))
        {
            error = "Writing the sound set failed (is the disk full?).";
            return false;
        }
    }

    if (! temp.overwriteTargetFileWithTemporary())
    {
        error = "Couldn't save " + destination.getFileName() + " (is it open somewhere else?).";
        return false;
    }
    return true;
}

bool importFrom (const juce::File& package, SoundSet& result, juce::File& installedFolder, juce::String& error)
{
    juce::ZipFile zip (package);
    const int settingsIndex = zip.getIndexOfFileName (settingsEntry, true);
    if (settingsIndex < 0)
    {
        error = package.getFileName() + " isn't an OmniTone sound set.";
        return false;
    }

    juce::String jsonText;
    if (std::unique_ptr<juce::InputStream> in { zip.createStreamForEntry (settingsIndex) })
        jsonText = in->readEntireStreamAsString();

    const auto parsed = juce::JSON::parse (jsonText);
    if (! parsed.isObject())
    {
        error = "The settings inside " + package.getFileName() + " are damaged.";
        return false;
    }

    SoundSet set;
    set.fromVar (parsed);

    // Each import gets its own folder, so nothing in use is ever overwritten.
    auto root = getSoundSetsFolder();
    root.createDirectory();
    auto folder = root.getNonexistentChildFile (juce::File::createLegalFileName (package.getFileNameWithoutExtension()), {}, true);
    if (! folder.createDirectory())
    {
        error = "Couldn't create " + folder.getFullPathName();
        return false;
    }

    const auto unpacked = zip.uncompressTo (folder, true);
    if (unpacked.failed())
    {
        folder.deleteRecursively();
        error = "Unpacking failed: " + unpacked.getErrorMessage();
        return false;
    }

    juce::StringArray missing;
    auto absolute = [&folder, &missing] (const juce::String& stored) -> juce::String
    {
        if (stored.isEmpty())
            return {};
        const auto f = folder.getChildFile (stored);
        if (! f.isAChildOf (folder) || ! f.existsAsFile())
        {
            missing.add (stored);
            return {};
        }
        return f.getFullPathName();
    };

    set.mainFile = absolute (set.mainFile);
    for (auto& o : set.overrides)
        if (o.active)
        {
            o.file = absolute (o.file);
            if (o.file.isEmpty())
                o = SlotOverride(); // its SoundFont wasn't in the package: fall back to main
        }

    if (! missing.isEmpty())
        error = "Some SoundFonts were missing from the package and were skipped:\n" + missing.joinIntoString ("\n");

    result = set;
    installedFolder = folder;
    return true;
}

} // namespace SoundSetPackage
