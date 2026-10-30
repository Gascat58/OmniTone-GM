#pragma once

#ifndef TSF_NO_STDIO
 #define TSF_NO_STDIO
#endif
#include "ThirdParty/tsf.h"

// Small helpers TinySoundFont doesn't expose publicly.
int tsfx_preset_bank   (const tsf* f, int presetIndex);
int tsfx_preset_number (const tsf* f, int presetIndex);
