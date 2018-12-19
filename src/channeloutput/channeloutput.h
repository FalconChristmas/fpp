/*
 *   generic output channel handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CHANNELOUTPUT_H
#define _CHANNELOUTPUT_H

#include <pthread.h>
#include <vector>
#include <stdint.h>

#define FPPD_MAX_CHANNEL_OUTPUTS   64

class ChannelOutputBase;
class OutputProcessors;

typedef struct fppChannelOutput {
	int              (*maxChannels)(void *data);
	int              (*open)(char *device, void **privDataPtr);
	int              (*close)(void *data);
	int              (*isConfigured)(void);
	int              (*isActive)(void *data);
	int              (*send)(void *data, char *channelData, int channelCount);
	int              (*startThread)(void *data);
	int              (*stopThread)(void *data);
} FPPChannelOutput;

typedef struct fppChannelOutputInstance {
	unsigned int      startChannel;
	unsigned int      channelCount;
	FPPChannelOutput *outputOld;
	ChannelOutputBase *output;
	void             *privData;
} FPPChannelOutputInstance;

extern char            channelData[];
extern pthread_mutex_t channelDataLock;
extern unsigned long   channelOutputFrame;
extern float           mediaElapsedSeconds;
extern OutputProcessors outputProcessors;

int  InitializeChannelOutputs(void);
int  PrepareChannelData(char *channelData);
int  SendChannelData(char *channelData);
int  CloseChannelOutputs(void);
void SetChannelOutputFrameNumber(int frameNumber);
void ResetChannelOutputFrameNumber(void);
void StartOutputThreads(void);
void StopOutputThreads(void);

const std::vector<std::pair<uint32_t, uint32_t>> GetOutputRanges();

#endif /* _CHANNELOUTPUT_H */
