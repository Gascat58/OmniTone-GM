# OmniTone GM

A General MIDI SoundFont instrument (VST3, Windows 64-bit) built to be Mixcraft's
**Default General MIDI Instrument**.

- Load one main GM SoundFont (.sf2) for all 128 instruments and the drum kit.
- Replace any single instrument (or the drum kit) with a sound from another .sf2 file.
- Picks each track's sound from **CC 3**, because Mixcraft strips Patch (program change)
  events when it imports a MIDI file but keeps controllers. Real program changes work too.
- Built-in reverb that follows each track's CC 91.
- One shared settings file, so every track and every project uses the same sound set.

See [INSTALL.txt](INSTALL.txt) for installing and using it.

## Building

GitHub builds it automatically on every push (see the **Actions** tab); the finished
plugin is attached to each run as **OmniTone-GM-VST3**.

To build locally: `cmake -S . -B build -A x64` then `cmake --build build --config Release`.

## Credits and licences

- [JUCE](https://juce.com) plugin framework (AGPLv3 / commercial). Because of JUCE's
  open-source licence, this project is released under the AGPLv3.
- [TinySoundFont](https://github.com/schellingb/TinySoundFont) by Bernhard Schelling (MIT,
  see `Source/ThirdParty/TinySoundFont-LICENSE.txt`).
