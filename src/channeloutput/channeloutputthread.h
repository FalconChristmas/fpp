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
