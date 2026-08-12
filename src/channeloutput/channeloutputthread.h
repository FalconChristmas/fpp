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

#include <mutex>

// Held by the channel output thread for the whole of each output cycle -- the
// cycle that runs PrepareChannelData()/SendChannelData(), both of which walk
// the channelOutputs list and call into every ChannelOutput on it.  It is
// released only while the thread waits out the rest of the frame interval, so
// anything that mutates that list, and in particular anything that destroys a
// ChannelOutput, has to hold this to avoid pulling an instance out from under
// a walk that is already in progress.
//
// Two things it is NOT safe to do with it.  Do not call
// StartChannelOutputThread() while holding it: that function waits for
// ThreadIsRunning, which the new thread sets only after taking this lock.  And
// do not take it on a path an FSEQ-triggered command can reach -- the output
// thread holds it across Sequence::SendSequenceData(), which runs those
// commands, so anything they call that takes it deadlocks that thread.  It is
// a plain mutex on purpose: the output thread waits on a condition_variable
// with it, which rules out making it recursive.
extern std::mutex outputThreadLock;

void DisableChannelOutput(void);
void EnableChannelOutput(void);
void InitChannelOutputSyncVars(void);
void DestroyChannelOutputSyncVars(void);
void ForceChannelOutputNow(void);

int ChannelOutputThreadIsRunning(void);
int ChannelOutputThreadIsEnabled();
void SetChannelOutputRefreshRate(float rate);
float GetChannelOutputRefreshRate();
void StartChannelOutputThread(void);
int StopChannelOutputThread(void);
void StartForcingChannelOutput(void);
void StopForcingChannelOutput(void);
void ResetMasterPosition(void);
void UpdateMasterPosition(int frameNumber);
void CalculateNewChannelOutputDelay(float mediaPosition);
void CalculateNewChannelOutputDelayForFrame(int expectedFramesSent);
