/*
 *   Effects handler for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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

#include <string>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "common.h"
#include "effects.h"
#include "channeloutputthread.h"
#include "log.h"
#include "Sequence.h"
#include "settings.h"

#define ESEQ_MODEL_COUNT_OFFSET     4 // hard-coded to 1 for now in Nutcracker
#define ESEQ_STEP_SIZE_OFFSET       8 // total step size of all models together
#define ESEQ_MODEL_START_OFFSET    12 // with current 1 model per file ability
#define ESEQ_MODEL_SIZE_OFFSET     16 // with current 1 model per file ability
#define ESEQ_CHANNEL_DATA_OFFSET   20 // with current 1 model per file ability

#define MAX_EFFECTS 100

int        effectCount = 0;
int        pauseBackgroundEffects = 0;
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

	char localFilename[2048];

	strcpy(localFilename, getEffectDirectory());
	strcat(localFilename, "/background.eseq");

	if ((getFPPmode() == REMOTE_MODE) &&
		(CheckForHostSpecificFile(getSetting("HostName"), localFilename)))
	{
		strcpy(localFilename, "background_");
		strcat(localFilename, getSetting("HostName"));

		LogInfo(VB_EFFECT, "Automatically starting background effect "
			"sequence %s\n", localFilename);

		StartEffect(localFilename, 0, 1);
	}
	else if (FileExists(localFilename))
	{
		LogInfo(VB_EFFECT, "Automatically starting background effect sequence "
			"background.eseq\n");
		StartEffect("background", 0, 1);
	}

	pauseBackgroundEffects = getSettingInt("pauseBackgroundEffects");

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
int IsEffectRunning(void)
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
int StartEffect(char *effectName, int startChannel, int loop)
{
	int   effectID = -1;
	FILE *fp = NULL;
	char  effectData[256];
	int   bytesRead;
	int   stepSize;
	int   modelSize;
	char  filename[1024];
	int   modelCount = 1;

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
		LogErr(VB_EFFECT, "Unable to open effect: %s\n", filename);
		pthread_mutex_unlock(&effectsLock);
		return effectID;
	}

	bytesRead = fread(effectData,1,4,fp);
	if ((effectData[0] != 'E') ||
		(effectData[1] != 'S') ||
		(effectData[2] != 'E') ||
		(effectData[3] != 'Q'))
	{
		LogErr(VB_EFFECT, "Invalid Effect file format: %s\n", effectName);
		pthread_mutex_unlock(&effectsLock);
		return effectID;
	}

	// Should already be at this position, but seek any to future-proof
	fseek(fp, ESEQ_MODEL_COUNT_OFFSET, SEEK_SET);
	bytesRead = fread(effectData,1,1,fp);
	if (bytesRead < 1)
	{
		LogErr(VB_EFFECT, "Unable to load effect: %s\n", effectName);
		pthread_mutex_unlock(&effectsLock);
		return effectID;
	}

	modelCount = effectData[0];

	if (modelCount > 1)
	{
		LogErr(VB_EFFECT, "This version of FPP does not support effect files with more than one model.");
		pthread_mutex_unlock(&effectsLock);
		return effectID;
	}

	// Should already be at this position, but seek any to future-proof
	fseek(fp, ESEQ_STEP_SIZE_OFFSET, SEEK_SET);
	bytesRead = fread(effectData,1,4,fp);
	if (bytesRead < 4)
	{
		LogErr(VB_EFFECT, "Unable to load effect: %s\n", effectName);
		pthread_mutex_unlock(&effectsLock);
		return effectID;
	}

	stepSize = effectData[0] + (effectData[1]<<8) + (effectData[2]<<16) + (effectData[3]<<24);

	if (startChannel == 0)
	{
		// This will need to change if/when we support multiple models per file
		fseek(fp, ESEQ_MODEL_START_OFFSET, SEEK_SET);
		bytesRead = fread(effectData,1,4,fp);
		if (bytesRead < 4)
		{
			LogErr(VB_EFFECT, "Unable to load effect: %s\n", effectName);
			pthread_mutex_unlock(&effectsLock);
			return effectID;
		}

		startChannel = effectData[0] + (effectData[1]<<8) + (effectData[2]<<16) + (effectData[3]<<24);
	}

	// This will need to change if/when we support multiple models per file
	fseek(fp, ESEQ_MODEL_SIZE_OFFSET, SEEK_SET);
	bytesRead = fread(effectData,1,4,fp);
	if (bytesRead < 4)
	{
		LogErr(VB_EFFECT, "Unable to load effect: %s\n", effectName);
		pthread_mutex_unlock(&effectsLock);
		return effectID;
	}

	modelSize = effectData[0] + (effectData[1]<<8) + (effectData[2]<<16) + (effectData[3]<<24);

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
	effects[effectID]->loop = loop;
	effects[effectID]->stepSize = stepSize;
	effects[effectID]->modelSize = modelSize;
	effects[effectID]->background = 0;

	if (!strcmp(effectName, "background"))
	{
		effects[effectID]->background = 1;
	}
	else if ((getFPPmode() == REMOTE_MODE) &&
			 (strstr(effectName, "background_") == effectName))
	{
		char localFilename[128];
		strcpy(localFilename, "background_");
		strcat(localFilename, getSetting("HostName"));

		if (!strcmp(effectName, localFilename))
			effects[effectID]->background = 1;
	}

	LogInfo(VB_EFFECT, "Effect %s step size %d, model size: %d\n",
		effectName, stepSize, modelSize);

	effectCount++;

	pthread_mutex_unlock(&effectsLock);

	StartChannelOutputThread();

	return effectID;
}

/*
 * Helper function to stop an effect, assumes effectsLock is already held
 */
void StopEffectHelper(int effectID)
{
	FPPeffect *e = NULL;

	e = effects[effectID];

	fclose(e->fp);
	free(e->name);
	free(e);
	effects[effectID] = NULL;

	effectCount--;
}

/*
 * Stop all effects named effectName
 */
int StopEffect(char *effectName)
{
	LogDebug(VB_EFFECT, "StopEffect(%s)\n", effectName);

	pthread_mutex_lock(&effectsLock);

	for (int i = 0; i < MAX_EFFECTS; i++)
	{
		if (effects[i] && !strcmp(effects[i]->name, effectName))
			StopEffectHelper(i);
	}

	pthread_mutex_unlock(&effectsLock);

	if ((!IsEffectRunning()) &&
		(!sequence->IsSequenceRunning()))
		sequence->SendBlankingData();

	return 1;
}

/*
 * Stop a single effect
 */
int StopEffect(int effectID)
{
	FPPeffect *e = NULL;

	LogDebug(VB_EFFECT, "StopEffect(%d)\n", effectID);

	pthread_mutex_lock(&effectsLock);

	if (!effects[effectID])
	{
		pthread_mutex_unlock(&effectsLock);
		return 0;
	}

	StopEffectHelper(effectID);

	pthread_mutex_unlock(&effectsLock);

	if ((!IsEffectRunning()) &&
		(!sequence->IsSequenceRunning()))
		sequence->SendBlankingData();

	return 1;
}

/*
 * Stop all effects
 */
void StopAllEffects(void)
{
	int i;

	LogDebug(VB_EFFECT, "Stopping all effects\n");

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
	char       effectData[131072];
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
		// Automatically loop effect sequence if it still exists
		if (e->loop)
		{
			std::string sequenceName(getEffectDirectory());
			sequenceName += "/";
			sequenceName += e->name;
			sequenceName += ".eseq";

			if (FileExists(sequenceName.c_str()))
			{
				LogDebug(VB_EFFECT,
					"Automatically looping effect sequence %s\n",
					e->name);
				fseek(e->fp, ESEQ_CHANNEL_DATA_OFFSET, SEEK_SET);
				bytesRead = fread(effectData, 1, e->stepSize, e->fp);
			}
		}

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
	}

	memcpy(channelData + e->startChannel - 1, effectData, e->modelSize);

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

	int skipBackground = 0;
	if (pauseBackgroundEffects && sequence->IsSequenceRunning())
		skipBackground = 1;

	for (i = 0; i < MAX_EFFECTS; i++)
	{
		if (effects[i])
		{
			if ((!skipBackground) ||
				(skipBackground && (!effects[i]->background)))
				dataRead |= OverlayEffect(i, channelData);
		}
	}

	pthread_mutex_unlock(&effectsLock);

	if ((dataRead == 0) &&
		(!IsEffectRunning()) &&
		(!sequence->IsSequenceRunning()))
		sequence->SendBlankingData();

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

