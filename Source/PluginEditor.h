#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class OmniToneEditor : public juce::AudioProcessorEditor,
                       private juce::ListBoxModel,
                       private juce::Timer
{
public:
    explicit OmniToneEditor (OmniToneProcessor&);
    ~OmniToneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // ListBoxModel
    int getNumRows() override { return GM::numSlots; }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int) override { updateButtons(); }
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { previewRow (row); }

    void timerCallback() override;
    void refresh();
    void updateButtons();
    void browseMain();
    void replaceSelected();
    void choosePreset (int slot, const juce::String& path);
    void previewRow (int row);
    juce::String slotTitle (int slot) const;

    OmniToneProcessor& processor;

    juce::Label titleLabel, mainLabel, mainPath, statusLabel, problemsLabel, helpLabel;
    juce::TextButton browseButton { "Browse..." };
    juce::ToggleButton reverbToggle { "Built-in reverb (follows CC91)" };
    juce::ListBox list;
    juce::TextButton replaceButton { "Replace..." }, resetButton { "Reset to main" }, previewButton { "Preview" };

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmniToneEditor)
};
