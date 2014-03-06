/*
 *   Effects handler for Falcon Pi Player (FPP)
 *
 *   Copyright (C) Chris Pinkham 2013
 *
 *   Falcon Pi Player is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "effects.h"
#include "channeloutput/channeloutputthread.h"
#include "log.h"
#include "sequence.h"

// FIXME, these two may get changed when we parse the real .eseq files
#define ESEQ_STEP_SIZE_OFFSET      10
#define ESEQ_CHANNEL_DATA_OFFSET   28

#define MAX_EFFECTS 100

int        effectCount = 0;
FPPeffect *effects[MAX_EFFECTS];

pthread_mutex_t effectsLock;

/*
 * Initialize effects constructs
 */
int InitEffects(void)
{
	if (pthread_mutex_init(&effectsLock, NULL) != 0)
	{
		LogErr(VB_EFFECT, "Effects mutex init failed\n");
		return 0;
	}

	bzero(effects, sizeof(effects));

	return 1;
}

/*
 * Close effects constructs
 */
void CloseEffects(void)
{
	pthread_mutex_destroy(&effectsLock);
}

/*
 * Get next available effect ID
 *
 * Assumes effectsLock is held already
 */
int GetNextEffectID(void)
{
	int i = -1;

	for (i = 0; i < MAX_EFFECTS; i++)
	{
		if (!effects[i])
			return i;
	}

	return -1;
}

/*
 * Check to see if any effects are running
 */
inline int IsEffectRunning(void)
{
	int result = 0;

	pthread_mutex_lock(&effectsLock);
	result = effectCount;
	pthread_mutex_unlock(&effectsLock);

	return result;
}

/*
 * Start a new effect offset at the specified channel number
 */
int StartEffect(char *effectName, int startChannel)
{
	int   effectID = -1;
	FILE *fp = NULL;
	char  effectData[256];
	int   bytesRead;
	int   stepSize;
	char  filename[1024];

	LogInfo(VB_EFFECT, "Starting effect %s at channel %d\n", effectName, startChannel);

	pthread_mutex_lock(&effectsLock);

	if (effectCount >= MAX_EFFECTS)
	{
		LogErr(VB_EFFECT, "Unable to start effect %s, maximum number of effects already running\n", effectName);
		pthread_mutex_unlock(&effectsLock);
		return effectID;
	}

	if (snprintf(filename, 1024, "%s/%s.eseq", getEffectDirectory(), effectName) >= 1024)
	{
		LogErr(VB_EFFECT, "Unable to open effects file: %s, filename too long\n",
			filename);
		pthread_mutex_unlock(&effectsLock);
		return effectID;
	}

	fp = fopen(filename, "r");

	if (!fp)
	{
		LogErr(VB_EFFECT, "Unable to open effect: %s\n", effectName);
		pthread_mutex_unlock(&effectsLock);
		return effectID;
	}

	fseek(fp, ESEQ_STEP_SIZE_OFFSET, SEEK_SET);
	bytesRead = fread(effectData,1,4,fp);
	if (bytesRead < 4)
	{
		LogErr(VB_EFFECT, "Unable to load effect: %s\n", effectName);
		pthread_mutex_unlock(&effectsLock);
		return effectID;
	}

    stepSize = effectData[0] + (effectData[1]<<8) + (effectData[2]<<16) + (effectData[3]<<24);
	if (fseek(fp, ESEQ_CHANNEL_DATA_OFFSET, SEEK_SET))
	{
		LogErr(VB_EFFECT, "Unable to load effect: %s\n", effectName);
		pthread_mutex_unlock(&effectsLock);
		return effectID;
	}

	effectID = GetNextEffectID();

	if (effectID < 0)
	{
		LogErr(VB_EFFECT, "Unable to start effect %s, unable to determine next effect ID\n", effectName);
		pthread_mutex_unlock(&effectsLock);
		return effectID;
	}

	effects[effectID] = (FPPeffect*)malloc(sizeof(FPPeffect));
	effects[effectID]->name = strdup(effectName);
	effects[effectID]->fp = fp;
	effects[effectID]->startChannel = (startChannel >= 1) ? startChannel : 1;
    effects[effectID]->stepSize = stepSize;

    LogInfo(VB_EFFECT, "Effect %s Stepsize %d\n", effectName, effects[effectID]->stepSize);

	effectCount++;

	pthread_mutex_unlock(&effectsLock);

	StartChannelOutputThread();

	return effectID;
}

/*
 * Stop a single effect
 */
int StopEffect(int effectID)
{
	FPPeffect *e = NULL;

	LogDebug(VB_EFFECT, "StopEffect(%d)", effectID);

	pthread_mutex_lock(&effectsLock);

	if (!effects[effectID])
	{
		pthread_mutex_unlock(&effectsLock);
		return 0;
	}

	e = effects[effectID];

	fclose(e->fp);
	free(e->name);
	free(e);
	effects[effectID] = NULL;

	effectCount--;

	pthread_mutex_unlock(&effectsLock);

	if (!IsEffectRunning() && !IsSequenceRunning())
		SendBlankingData();

	return 1;
}

/*
 * Stop all effects
 */
void StopAllEffects(void)
{
	int i;

	LogDebug(VB_EFFECT, "Stopping all effects");

	pthread_mutex_lock(&effectsLock);

	for (i = 0; i < MAX_EFFECTS; i++)
	{
		if (effects[i])
			StopEffect(i);
	}

	pthread_mutex_unlock(&effectsLock);
}

/*
 * Overlay a single effect onto raw channel data
 */
int OverlayEffect(int effectID, char *channelData)
{
	char       effectData[65536];
	int        bytesRead;
	FPPeffect *e = NULL;

	if (!effects[effectID])
	{
		LogErr(VB_EFFECT, "Invalid Effect ID %d\n", effectID);
		return 0;
	}

	e = effects[effectID];

	bytesRead = fread(effectData, 1, e->stepSize, e->fp);
	if (bytesRead == 0)
	{
		LogInfo(VB_EFFECT, "Effect %s finished\n", e->name);
		fclose(e->fp);
		free(e->name);
		free(e);
		effects[effectID] = NULL;
		effectCount--;

		return 0;
	}

	memcpy(channelData + e->startChannel - 1, effectData, e->stepSize);

	return 1;
}

/*
 * Overlay current effects onto raw channel data
 */
int OverlayEffects(char *channelData)
{
	int  i;
	int  dataRead = 0;

	pthread_mutex_lock(&effectsLock);

	if (effectCount == 0)
	{
		pthread_mutex_unlock(&effectsLock);
		return 0;
	}

	for (i = 0; i < MAX_EFFECTS; i++)
	{
		if (effects[i])
			dataRead |= OverlayEffect(i, channelData);
	}

	pthread_mutex_unlock(&effectsLock);

	if ((dataRead == 0) && (!IsEffectRunning()) && (!IsSequenceRunning()))
		SendBlankingData();

	return 1;
}

/*
 * Get list of running effects and their IDs
 *
 * Format: [EFFECTID1,EFFECTNAME1[,EFFECTID2,EFFECTNAME2]...]
 *
 * NOTE: Caller is responsible for freeing string allocated
 */
int GetRunningEffects(char *msg, char **result)
{
	int length = strlen(msg) + 2; // 1 for LF, 1 for NULL termination
	int i = 0;

	pthread_mutex_lock(&effectsLock);

	for (i = 0; i < MAX_EFFECTS; i++)
	{
		if (effects[i])
		{
			// delimiters
			length += 2;

			// ID
			length++;
			if (i > 9)
				length++;
			if (i > 99)
				length++;

			// Name
			length += strlen(effects[i]->name);
		}
	}

	*result = (char *)malloc(length);
	char *cptr = *result;
	*cptr = '\0';

	strcat(cptr, msg);
	cptr += strlen(msg);

	for (i = 0; i < MAX_EFFECTS; i++)
	{
		if (effects[i])
		{
			strcat(cptr,";");
			cptr++;

			cptr += snprintf(cptr, 4, "%d", i);

			strcat(cptr, ",");
			cptr++;

			strcat(cptr, effects[i]->name);
			cptr += strlen(effects[i]->name);
		}
	}

	strcat(cptr, "\n");

	pthread_mutex_unlock(&effectsLock);

	return strlen(*result);
}

