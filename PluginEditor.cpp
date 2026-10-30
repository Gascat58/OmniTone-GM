#include "PluginEditor.h"
#include <map>

namespace Colours
{
    const juce::Colour background { 0xff1e2329 };
    const juce::Colour panel      { 0xff262c33 };
    const juce::Colour rowAlt     { 0xff2b3138 };
    const juce::Colour selected   { 0xff2f6fb5 };
    const juce::Colour text       { 0xffe6e9ec };
    const juce::Colour dim        { 0xff98a2ad };
    const juce::Colour replaced   { 0xfff0c35a };
    const juce::Colour warning    { 0xffff8a65 };
}

OmniToneEditor::OmniToneEditor (OmniToneProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    titleLabel.setText ("OmniTone GM", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, Colours::text);
    addAndMakeVisible (titleLabel);

    mainLabel.setText ("Main SoundFont:", juce::dontSendNotification);
    mainLabel.setColour (juce::Label::textColourId, Colours::dim);
    addAndMakeVisible (mainLabel);

    mainPath.setColour (juce::Label::textColourId, Colours::text);
    mainPath.setColour (juce::Label::backgroundColourId, Colours::panel);
    mainPath.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (mainPath);

    browseButton.onClick = [this] { browseMain(); };
    addAndMakeVisible (browseButton);

    reverbToggle.onClick = [this] { processor.setReverbOn (reverbToggle.getToggleState()); };
    reverbToggle.setColour (juce::ToggleButton::textColourId, Colours::text);
    addAndMakeVisible (reverbToggle);

    list.setModel (this);
    list.setRowHeight (24);
    list.setColour (juce::ListBox::backgroundColourId, Colours::panel);
    list.setColour (juce::ListBox::outlineColourId, juce::Colours::black);
    list.setOutlineThickness (1);
    addAndMakeVisible (list);

    replaceButton.onClick = [this] { replaceSelected(); };
    resetButton.onClick   = [this] { if (list.getSelectedRow() >= 0) processor.clearOverride (list.getSelectedRow()); };
    previewButton.onClick = [this] { previewRow (list.getSelectedRow()); };
    addAndMakeVisible (replaceButton);
    addAndMakeVisible (resetButton);
    addAndMakeVisible (previewButton);

    statusLabel.setColour (juce::Label::textColourId, Colours::dim);
    addAndMakeVisible (statusLabel);

    problemsLabel.setColour (juce::Label::textColourId, Colours::warning);
    problemsLabel.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (problemsLabel);

    diagLabel.setColour (juce::Label::textColourId, Colours::replaced);
    diagLabel.setColour (juce::Label::backgroundColourId, Colours::panel);
    diagLabel.setFont (juce::FontOptions (12.5f));
    diagLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (diagLabel);

    helpLabel.setText ("Click a sound to play it on this track. Double-click to preview. Sound set changes apply to every OmniTone track.",
                       juce::dontSendNotification);
    helpLabel.setColour (juce::Label::textColourId, Colours::dim);
    helpLabel.setFont (juce::FontOptions (12.5f));
    addAndMakeVisible (helpLabel);

    processor.onSoundSetChanged = [this] { refresh(); };

    setSize (680, 680);
    setResizable (true, true);
    setResizeLimits (560, 420, 1400, 1400);

    refresh();
    followHostProgram();
    startTimerHz (8);
}

OmniToneEditor::~OmniToneEditor()
{
    processor.onSoundSetChanged = nullptr;
    stopTimer();
}

void OmniToneEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);
}

void OmniToneEditor::resized()
{
    auto r = getLocalBounds().reduced (14);

    titleLabel.setBounds (r.removeFromTop (32));
    r.removeFromTop (6);

    auto mainRow = r.removeFromTop (28);
    mainLabel.setBounds (mainRow.removeFromLeft (120));
    browseButton.setBounds (mainRow.removeFromRight (100));
    mainRow.removeFromRight (8);
    mainPath.setBounds (mainRow);

    r.removeFromTop (6);
    reverbToggle.setBounds (r.removeFromTop (26).removeFromLeft (320));
    r.removeFromTop (6);

    helpLabel.setBounds (r.removeFromBottom (20));
    diagLabel.setBounds (r.removeFromBottom (54));
    r.removeFromBottom (4);
    statusLabel.setBounds (r.removeFromBottom (22));
    problemsLabel.setBounds (r.removeFromBottom (22));
    r.removeFromBottom (6);

    auto buttons = r.removeFromBottom (30);
    replaceButton.setBounds (buttons.removeFromLeft (130));
    buttons.removeFromLeft (8);
    resetButton.setBounds (buttons.removeFromLeft (130));
    buttons.removeFromLeft (8);
    previewButton.setBounds (buttons.removeFromLeft (100));
    r.removeFromBottom (8);

    list.setBounds (r);
}

juce::String OmniToneEditor::slotTitle (int slot) const
{
    if (slot == GM::drumSlot)
        return "Drums (channel 10)";
    return juce::String (slot).paddedLeft ('0', 3) + "  " + GM::instrumentNames[slot];
}

void OmniToneEditor::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= GM::numSlots)
        return;

    g.fillAll (selected ? Colours::selected : (row % 2 ? Colours::rowAlt : Colours::panel));

    const auto& o = processor.getSoundSet().overrides[(size_t) row];
    const int split = juce::jmin (300, width / 2);

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (14.0f, row == GM::drumSlot ? juce::Font::bold : juce::Font::plain));
    g.drawText (slotTitle (row), 10, 0, split - 14, height, juce::Justification::centredLeft, true);

    juce::String source;
    if (o.active)
    {
        g.setColour (selected ? Colours::text : Colours::replaced);
        source = juce::File (o.file).getFileNameWithoutExtension() + "  >  " + o.name;
    }
    else
    {
        g.setColour (selected ? Colours::text : Colours::dim);
        source = "Main";
    }
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (source, split, 0, width - split - 8, height, juce::Justification::centredLeft, true);
}

void OmniToneEditor::refresh()
{
    const auto& s = processor.getSoundSet();
    mainPath.setText (s.mainFile.isEmpty() ? "  (none - click Browse)" : "  " + s.mainFile, juce::dontSendNotification);
    reverbToggle.setToggleState (s.reverbOn, juce::dontSendNotification);

    const auto problems = processor.getProblems();
    problemsLabel.setText (problems.isEmpty() ? juce::String() : problems.joinIntoString ("   |   "),
                           juce::dontSendNotification);

    list.updateContent();
    list.repaint();
    updateButtons();
}

void OmniToneEditor::updateButtons()
{
    const int row = list.getSelectedRow();
    replaceButton.setEnabled (row >= 0);
    previewButton.setEnabled (row >= 0);
    resetButton.setEnabled (row >= 0 && processor.getSoundSet().overrides[(size_t) row].active);
}

void OmniToneEditor::selectedRowsChanged (int row)
{
    updateButtons();

    // Clicking an instrument makes it this track's sound (the drums row is always channel 10).
    if (! syncingSelection && juce::isPositiveAndBelow (row, 128) && row != processor.getCurrentProgram())
    {
        processor.chooseProgramByHand (row);
        shownProgram = row;
    }
}

void OmniToneEditor::followHostProgram()
{
    // Keep the list in step when the sound is picked from Mixcraft's preset dropdown.
    const int program = processor.getCurrentProgram();
    if (program == shownProgram)
        return;

    shownProgram = program;
    const juce::ScopedValueSetter<bool> svs (syncingSelection, true);
    list.selectRow (program);
}

void OmniToneEditor::timerCallback()
{
    followHostProgram();

    auto& e = processor.getEngine();
    const int ch = e.lastChannel.load();
    const int prog = e.lastProgram.load();

    juce::String text;
    if (ch < 0)
    {
        text = "This track: waiting for notes...";
    }
    else
    {
        text = "This track: MIDI channel " + juce::String (ch + 1) + " playing ";
        text += (ch == GM::drumChannel) ? juce::String ("Drums") : slotTitle (prog).trim();

        if (e.sawCarrierCC.load())
            text += "   (patch set by CC 3)";
        else if (processor.setProgramCalls.load() > 0 && ch != GM::drumChannel)
            text += "   (preset chosen by Mixcraft)";
        else if (ch != GM::drumChannel && ! e.sawProgramChange.load())
            text += "   (no patch number received - defaulting to piano)";
    }
    statusLabel.setText (text, juce::dontSendNotification);

    // Diagnostics: exactly what Mixcraft has sent this copy of OmniTone.
    juce::String chans;
    const int mask = e.channelsSeen.load();
    for (int i = 0; i < 16; ++i)
        if (mask & (1 << i))
            chans << (chans.isEmpty() ? "" : ",") << (i + 1);

    juce::String ccs;
    for (int i = 0; i < 128; ++i)
        if (const int n = e.controllersSeen[(size_t) i].load())
            ccs << (ccs.isEmpty() ? "" : "  ") << "CC" << i << " x" << n;

    const auto trackName = processor.getTrackName();
    juce::String diag;
    diag << "Mixcraft -> preset picks: " << processor.setProgramCalls.load()
         << " (current " << juce::String (processor.getCurrentProgram()).paddedLeft ('0', 3) << ")"
         << "   settings loads: " << processor.stateLoads.load() << " (last " << processor.lastStateSize.load() << " bytes)"
         << "   track name: " << (trackName.isEmpty() ? juce::String ("(not given)") : "\"" + trackName + "\"") << "\n"
         << "MIDI received -> channels: " << (chans.isEmpty() ? juce::String ("none yet") : chans)
         << "   program changes: " << e.programChangesSeen.load() << "\n"
         << "Controllers: " << (ccs.isEmpty() ? juce::String ("none yet") : ccs);
    diagLabel.setText (diag, juce::dontSendNotification);
}

void OmniToneEditor::previewRow (int row)
{
    if (row >= 0)
        processor.getEngine().requestPreview (row);
}

void OmniToneEditor::browseMain()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose your main General MIDI SoundFont",
                                                   juce::File (processor.getSoundSet().mainFile),
                                                   "*.sf2");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file.existsAsFile())
                                  processor.setMainSoundFont (file.getFullPathName());
                          });
}

void OmniToneEditor::replaceSelected()
{
    const int slot = list.getSelectedRow();
    if (slot < 0)
        return;

    const auto& o = processor.getSoundSet().overrides[(size_t) slot];
    const juce::File start (o.active ? o.file : processor.getSoundSet().mainFile);

    chooser = std::make_unique<juce::FileChooser> ("Pick a SoundFont for " + slotTitle (slot).trim(), start, "*.sf2");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, slot] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file.existsAsFile())
                                  choosePreset (slot, file.getFullPathName());
                          });
}

void OmniToneEditor::choosePreset (int slot, const juce::String& path)
{
    const auto presets = SoundFontCache::listPresets (path);
    if (presets.empty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "OmniTone GM",
                                                juce::File (path).getFileName() + " couldn't be opened as a SoundFont.");
        return;
    }

    // One sub-menu per bank keeps long lists manageable.
    juce::PopupMenu menu;
    menu.addSectionHeader ("Choose the sound for " + slotTitle (slot).trim());

    const bool wantDrums = slot == GM::drumSlot;
    std::map<int, juce::PopupMenu> banks;
    for (size_t i = 0; i < presets.size(); ++i)
    {
        const auto& p = presets[i];
        const bool suggested = wantDrums ? (p.bank >= 128) : (p.bank < 128 && p.preset == slot);
        const auto label = juce::String (p.preset).paddedLeft ('0', 3) + "  " + p.name;
        banks[p.bank].addItem ((int) i + 1, label, true, suggested);
    }

    if (banks.size() == 1)
    {
        // Only one bank (a single-instrument SoundFont, usually): list its sounds directly.
        for (size_t i = 0; i < presets.size(); ++i)
            menu.addItem ((int) i + 1, juce::String (presets[i].preset).paddedLeft ('0', 3) + "  " + presets[i].name);
    }
    else
    {
        for (auto& [bank, sub] : banks)
            menu.addSubMenu (bank >= 128 ? "Bank " + juce::String (bank) + " (drum kits)"
                                         : "Bank " + juce::String (bank), sub);
    }

    juce::Component::SafePointer<OmniToneEditor> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&replaceButton),
                        [safeThis, slot, path, presets] (int result)
                        {
                            if (safeThis == nullptr || result <= 0 || result > (int) presets.size())
                                return;
                            const auto& p = presets[(size_t) result - 1];
                            SlotOverride o;
                            o.active = true;
                            o.file = path;
                            o.bank = p.bank;
                            o.preset = p.preset;
                            o.name = p.name;
                            safeThis->processor.setOverride (slot, o);
                            safeThis->processor.getEngine().requestPreview (slot);
                        });
}
