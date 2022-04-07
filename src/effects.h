#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

// Effect Sequence file format and header definition
#include <string>

int GetRunningEffects(char* msg, char** result);
Json::Value GetRunningEffectsJson();
int IsEffectRunning(void);
int InitEffects(void);
void CloseEffects(void);
int StartEffect(const std::string& effectName, int startChannel, int loop = 0, bool bg = false);
int StartFSEQAsEffect(const std::string& effectName, int loop = 0, bool bg = false);
int StopEffect(const std::string& effectName);
int StopEffect(int effectID);
void StopAllEffects(void);
int OverlayEffects(char* channelData);
