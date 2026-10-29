// Command-line check of the OmniTone engine (no plugin host needed):
//   EngineTest <main.sf2> <out.wav>
// Plays a short multi-channel GM sequence where the patches are sent ONLY as CC 3
// (the way they'll arrive from Mixcraft), and also exercises a replaced slot.

#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/OmniEngine.h"

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

static float rms (const juce::AudioBuffer<float>& b, int start, int len)
{
    return b.getRMSLevel (0, start, len);
}

int main (int argc, char* argv[])
{
    if (argc < 3) { std::cout << "usage: EngineTest main.sf2 out.wav\n"; return 2; }
    const juce::String sf2 (argv[1]);
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    const double sr = 48000.0;
    const int block = 480;

    // --- presets list
    const auto presets = SoundFontCache::listPresets (sf2);
    check (presets.size() > 100, "SoundFont lists " + juce::String ((int) presets.size()) + " presets");

    // --- sound set with the bass slot (33) replaced by the main font's slap bass (36)
    SoundSet set;
    set.mainFile = sf2;
    set.overrides[33].active = true;
    set.overrides[33].file = sf2;
    set.overrides[33].bank = 0;
    set.overrides[33].preset = 36;
    set.overrides[33].name = "Slap Bass test";

    // JSON round trip
    SoundSet copy;
    copy.fromVar (juce::JSON::parse (juce::JSON::toString (set.toVar())));
    check (copy.mainFile == sf2 && copy.overrides[33].active && copy.overrides[33].preset == 36
             && ! copy.overrides[34].active, "settings save/load round trip");

    auto engines = EngineSet::build (set, sr);
    check (engines->hasMain && engines->problems.isEmpty(), "engine set builds with no problems");
    check (engines->melodic[33].preset == tsf_get_presetindex (engines->engines[0], 0, 36), "slot 33 points at the replacement preset");
    check (engines->melodic[0].preset == tsf_get_presetindex (engines->engines[0], 0, 0), "slot 0 points at main piano");
    check (tsfx_preset_bank (engines->engines[0], engines->drumSlotFor (0).preset) >= 128, "drums use a drum kit");

    OmniEngine engine;
    engine.prepare (sr);
    engine.setEngineSet (std::move (engines), true);

    // --- song: 4 seconds. ch1 bass (CC3=33), ch2 strings (CC3=48), ch10 drums.
    const int total = (int) (sr * 4);
    juce::AudioBuffer<float> song (2, total);
    song.clear();

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::controllerEvent (1, 3, 33), 0);
    seq.addEvent (juce::MidiMessage::controllerEvent (2, 3, 48), 0);
    seq.addEvent (juce::MidiMessage::controllerEvent (2, 91, 60), 0);
    for (int beat = 0; beat < 8; ++beat)
    {
        const double t = beat * 0.5;
        const int bassNote = (beat % 4 < 2) ? 33 : 40;
        seq.addEvent (juce::MidiMessage::noteOn (1, bassNote, (juce::uint8) 100), t);
        seq.addEvent (juce::MidiMessage::noteOff (1, bassNote), t + 0.45);
        seq.addEvent (juce::MidiMessage::noteOn (10, beat % 2 ? 38 : 36, (juce::uint8) 110), t);
        seq.addEvent (juce::MidiMessage::noteOff (10, beat % 2 ? 38 : 36), t + 0.1);
        seq.addEvent (juce::MidiMessage::noteOn (10, 42, (juce::uint8) 80), t + 0.25);
        seq.addEvent (juce::MidiMessage::noteOff (10, 42), t + 0.3);
    }
    for (int n : { 57, 61, 64 })
    {
        seq.addEvent (juce::MidiMessage::noteOn (2, n, (juce::uint8) 80), 0.0);
        seq.addEvent (juce::MidiMessage::noteOff (2, n), 3.5);
    }
    seq.updateMatchedPairs();
    seq.sort();

    int eventIndex = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        juce::MidiBuffer midi;
        while (eventIndex < seq.getNumEvents())
        {
            const auto& m = seq.getEventPointer (eventIndex)->message;
            const int samplePos = (int) (m.getTimeStamp() * sr);
            if (samplePos >= pos + block) break;
            midi.addEvent (m, samplePos - pos);
            ++eventIndex;
        }
        juce::AudioBuffer<float> chunk (2, block);
        engine.process (chunk, midi);
        song.copyFrom (0, pos, chunk, 0, 0, block);
        song.copyFrom (1, pos, chunk, 1, 0, block);
    }

    check (engine.sawCarrierCC.load(), "CC 3 patch numbers were recognised");
    check (engine.notesPlayed.load() > 20, "notes were played (" + juce::String (engine.notesPlayed.load()) + ")");
    check (rms (song, 0, total) > 0.005f, "song produces sound (rms " + juce::String (rms (song, 0, total), 4) + ")");
    check (song.getMagnitude (0, total) < 1.5f, "no runaway levels (peak " + juce::String (song.getMagnitude (0, total), 3) + ")");

    // --- program switch: piano vs replaced bass produce different audio
    auto renderNote = [&] (int program) {
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::controllerEvent (5, 3, program), 0);
        m.addEvent (juce::MidiMessage::noteOn (5, 48, (juce::uint8) 100), 1);
        juce::AudioBuffer<float> b (2, 9600);
        engine.process (b, m);
        juce::MidiBuffer off; off.addEvent (juce::MidiMessage::allNotesOff (5), 0);
        juce::AudioBuffer<float> rest (2, 48000); engine.process (rest, off);
        juce::MidiBuffer none; for (int i = 0; i < 10; ++i) engine.process (rest, none);
        return b;
    };
    const auto a = renderNote (0), b = renderNote (73);
    float diff = 0;
    for (int i = 0; i < 9600; ++i) diff += std::abs (a.getSample (0, i) - b.getSample (0, i));
    check (diff > 1.0f, "different CC 3 values give different sounds");

    // --- preview
    engine.requestPreview (24);
    juce::AudioBuffer<float> pv (2, 4800);
    engine.process (pv, juce::MidiBuffer());
    check (pv.getRMSLevel (0, 0, 4800) > 0.001f, "Preview makes sound");

    // --- a missing replacement file is reported but the rest still works
    SoundSet broken = set;
    broken.overrides[40].active = true;
    broken.overrides[40].file = "/nope/missing.sf2";
    auto brokenSet = EngineSet::build (broken, sr);
    check (! brokenSet->problems.isEmpty() && brokenSet->melodic[40].engine == 0, "missing replacement falls back to main");

    // --- no main soundfont at all is silent, not a crash
    SoundSet empty;
    OmniEngine silent;
    silent.prepare (sr);
    silent.setEngineSet (EngineSet::build (empty, sr), true);
    juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    juce::AudioBuffer<float> s (2, 4800);
    silent.process (s, m);
    check (s.getMagnitude (0, 4800) == 0.0f, "no SoundFont = silence, no crash");

    // write the song so it can be listened to
    out.deleteFile();
    juce::WavAudioFormat wav;
    if (auto fos = std::unique_ptr<juce::FileOutputStream> (out.createOutputStream()))
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (fos.get(), sr, 2, 16, {}, 0)))
        {
            fos.release();
            writer->writeFromAudioSampleBuffer (song, 0, total);
        }

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
