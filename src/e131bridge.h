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

#include <functional>
#include "fpp-json-fwd.h"
#include <map>

void Bridge_Initialize(std::map<int, std::function<bool(int)>>& callbacks);
void Bridge_Shutdown(void);

int CreateArtNetSocket(uint32_t addr = 0, bool allowPortChange = false);
// Forget a failed attempt to open the ArtNet socket, and its warning, on a
// config reload; whoever still needs ArtNet then retries at once.
void ResetArtNetSocketFailure();
bool Bridge_ReceiveArtNetData();
// Re-size the DDP input buffer to the channels the outputs now use.  Main
// loop only, as for the rest of the bridge.
void Bridge_OutputRangesChanged();
void AddArtNetOpcodeHandler(int opCode, std::function<bool(uint8_t* data, long long packetTime)> handler);
void RemoveArtNetOpcodeHandler(int opCode);

void ResetBytesReceived();
bool HasBridgeData();
Json::Value GetE131UniverseBytesReceived();

bool InputsEnabled();