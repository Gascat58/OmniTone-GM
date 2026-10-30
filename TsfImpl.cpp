// Compiles the TinySoundFont engine (single-header library, MIT licence) exactly once.
#include "TsfExtras.h"      // header part of tsf.h (with TSF_NO_STDIO: we load files ourselves)

#define TSF_IMPLEMENTATION
#include "ThirdParty/tsf.h" // implementation part

int tsfx_preset_bank (const tsf* f, int presetIndex)
{
    return (f && presetIndex >= 0 && presetIndex < f->presetNum) ? (int) f->presets[presetIndex].bank : -1;
}

int tsfx_preset_number (const tsf* f, int presetIndex)
{
    return (f && presetIndex >= 0 && presetIndex < f->presetNum) ? (int) f->presets[presetIndex].preset : -1;
}
