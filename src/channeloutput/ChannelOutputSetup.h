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

#include <pthread.h>
#include "fpp-json-fwd.h"
#include <memory>
#include <set>
#include <stdint.h>
#include <string>
#include <vector>

class ChannelOutput;
class OutputProcessors;

extern unsigned long channelOutputFrame;
extern float mediaElapsedSeconds;
extern OutputProcessors outputProcessors;

bool HasChannelOutputs();
bool HasUniverseOutputs();
int InitializeChannelOutputs();
int PrepareChannelData(char* channelData);
int SendChannelData(const char* channelData);
void OverlayOutputTestData(std::set<std::string> types, unsigned char* channelData, int cycleCnt, float percentOfCycle, int testType, const Json::Value& extraConfig);
std::set<std::string> GetOutputTypes();
void CloseChannelOutputs();
void SetChannelOutputFrameNumber(int frameNumber);
void ResetChannelOutputFrameNumber();

void StartingOutput();
void StoppingOutput();

// Returns a snapshot of the current output ranges. The caller should hold the
// returned shared_ptr for as long as it iterates the ranges, rather than
// calling this once per element -- a later ComputeOutputRanges() on another
// thread publishes a new vector instead of mutating this one. Never null,
// never empty (defaults to a single {0,8} range).
std::shared_ptr<const std::vector<std::pair<uint32_t, uint32_t>>> GetOutputRangesSnapshot(bool precise = true);
// Plugin-compatibility form (external plugins call this; do not remove or
// change the signature -- the return type is not mangled, so a change breaks
// compiled plugins silently instead of at link time). Returns a reference to
// a thread-local copy that stays stable until the same thread calls again
// with the same `precise` value. In-tree code should prefer the snapshot
// form above.
const std::vector<std::pair<uint32_t, uint32_t>>& GetOutputRanges(bool precise = true);
std::string GetOutputRangesAsString(bool precise = true, bool oneBased = false);
