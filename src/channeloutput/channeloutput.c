#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "channeloutput.h"
#include "E131.h"
#include "FPD.h"
#include "../log.h"
#include "../sequence.h"
#include "../settings.h"
#include "USBDMXPro.h"
#include "USBPixelnet.h"



/////////////////////////////////////////////////////////////////////////////

#define MAX_CHANNEL_REMAPS  512

typedef struct {
	int src;
	int count;
	int dest;
} ChannelRemap;

ChannelRemap             remappedChannels[MAX_CHANNEL_REMAPS];
int                      channelRemaps      = 0;
int                      channelOutputCount = 0;
unsigned long            channelOutputFrame = 0;
FPPChannelOutputInstance channelOutputs[FPPD_MAX_CHANNEL_OUTPUTS];

/* Some prototypes for helpers below */
int LoadChannelRemapData(void);
void RemapChannels(char *channelData);
void PrintRemappedChannels(void);

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
int InitializeChannelOutputs(void) {
	int i = 0;

	channelOutputFrame = 0;

	for (i = 0; i < FPPD_MAX_CHANNEL_OUTPUTS; i++) {
		bzero(&channelOutputs[i], sizeof(channelOutputs[i]));
	}

	// Reset index so we can start populating the outputs array
	i = 0;

	if (FPDOutput.isConfigured())
	{
		channelOutputs[i].startChannel = 0;
		channelOutputs[i].output       = &FPDOutput;

		if (FPDOutput.open("", &channelOutputs[i].privData)) {
			i++;
		} else {
			LogErr(VB_CHANNELOUT, "ERROR Opening FPD Channel Output\n");
		}
	}

	if ((getFPPmode() == PLAYER_MODE) &&
		(E131Output.isConfigured()))
	{
		channelOutputs[i].startChannel = 0;
		channelOutputs[i].output       = &E131Output;

		if (E131Output.open("", &channelOutputs[i].privData)) {
			i++;
		} else {
			LogErr(VB_CHANNELOUT, "ERROR Opening E1.31 Channel Output\n");
		}
	}

// FIXME
//	if ((getFPPmode() == PLAYER_MODE) &&
//		(USBPixelnetOutput.isConfigured()))
if (USBPixelnetOutput.isConfigured())
	{
		channelOutputs[i].startChannel = 0;
		channelOutputs[i].output       = &USBPixelnetOutput;

		if (USBPixelnetOutput.open(getUSBDonglePort(),
			&channelOutputs[i].privData))
		{
			i++;
		} else {
			LogErr(VB_CHANNELOUT, "ERROR Opening Channel Output %d\n", i);
		}
	}

	if ((getFPPmode() == PLAYER_MODE) &&
		(USBDMXProOutput.isConfigured()))
	{
		channelOutputs[i].startChannel = 0;
		channelOutputs[i].output       = &USBDMXProOutput;

		if (USBDMXProOutput.open(getUSBDonglePort(),
			&channelOutputs[i].privData))
		{
			i++;
		} else {
			LogErr(VB_CHANNELOUT, "ERROR Opening Channel Output %d\n", i);
		}
	}

	channelOutputCount = i;

	LoadChannelRemapData();
}

/*
 * Reset the output frame count
 */
void ResetChannelOutputFrameNumber(void) {
	channelOutputFrame = 0;
}

/*
 * Dump channel data for debugging
 */
void DumpChannelData(char *seqData) {
	LogDebug(VB_CHANNELDATA, "Ch Data: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		seqData[ 0] & 0xFF, seqData[ 1] & 0xFF,
		seqData[ 2] & 0xFF, seqData[ 3] & 0xFF,
		seqData[ 4] & 0xFF, seqData[ 5] & 0xFF,
		seqData[ 6] & 0xFF, seqData[ 7] & 0xFF,
		seqData[ 8] & 0xFF, seqData[ 9] & 0xFF,
		seqData[10] & 0xFF, seqData[11] & 0xFF,
		seqData[12] & 0xFF, seqData[13] & 0xFF,
		seqData[14] & 0xFF, seqData[15] & 0xFF);
}

/*
 *
 */
int SendChannelData(char *channelData) {
	int i = 0;
	FPPChannelOutputInstance *inst;

	RemapChannels(channelData);

	DumpChannelData(channelData);

	for (i = 0; i < channelOutputCount; i++) {
		inst = &channelOutputs[i];
		inst->output->send(
			inst->privData,
			channelData + inst->startChannel,
			inst->output->maxChannels < (FPPD_MAX_CHANNELS - inst->startChannel) ? inst->output->maxChannels : (FPPD_MAX_CHANNELS - inst->startChannel));
	}

	channelOutputFrame++;
}

/*
 *
 */
int CloseChannelOutputs(void) {
	FPPChannelOutputInstance *output;
	int i = 0;

	for (i = 0; i < channelOutputCount; i++) {
		channelOutputs[i].output->close(channelOutputs[i].privData);
		if (channelOutputs[i].privData)
			free(channelOutputs[i].privData);
	}
}

/*
 *
 * NOTE: We subtract 1 from all source and destination channel numbers
 *       because our array is 0-based and the channel numbers start at 1.
 */
int LoadChannelRemapData(void) {
	FILE *fp;
	char filename[1024];
	char buf[32];
	char *s;
	int src;
	int dest;
	int count;

	strcpy(filename, getMediaDirectory());
	strcat(filename, "/channelremap");

	channelRemaps = 0;

	LogDebug(VB_CHANNELOUT, "Loading Channel Remap data.\n");
	fp = fopen(filename, "r");
	if (fp == NULL) 
	{
		LogErr(VB_CHANNELOUT, "Could not open Channel Remap file %s\n", filename);
		return 0;
	}

	while(fgets(buf, 32, fp) != NULL)
	{
		if (buf[0] == '#') // Allow # comments for testing
			continue;

		// Source
		s = strtok(buf, ",");
		src = strtol(s, NULL, 10);
		if (src <= 0)
			continue;

		remappedChannels[channelRemaps].src = src - 1;

		// Destination
		s = strtok(NULL, ",");
		dest = strtol(s, NULL, 10);
		if (dest <= 0)
			continue;

		remappedChannels[channelRemaps].dest = dest - 1;

		// Count
		s=strtok(NULL,",");
		count = strtol(s, NULL, 10);
		if (count <= 0)
			continue;

		remappedChannels[channelRemaps].count = count;

		if ((src + count - 1) > FPPD_MAX_CHANNELS) {
			LogErr(VB_CHANNELOUT, "ERROR: Source + Count exceeds max channel count in: %s\n", buf );
		} else if ((dest + count - 1) > FPPD_MAX_CHANNELS) {
			LogErr(VB_CHANNELOUT, "ERROR: Destination + Count exceeds max channel count in: %s\n", buf );
		} else {
		    channelRemaps++;
		}
	}
	fclose(fp);

	PrintRemappedChannels();

	return 1;
}

/*
 *
 */
inline void RemapChannels(char *channelData) {
	int i = 0;
	ChannelRemap *mptr;

	if (!channelRemaps)
		return;

	for (i = 0, mptr = &remappedChannels[0]; i < channelRemaps; i++, mptr++) {
		if (mptr->count > 1) {
			memcpy(channelData + mptr->dest,
				   channelData + mptr->src,
				   mptr->count);
		} else {
			channelData[mptr->dest] = channelData[mptr->src];
		}
	}
}

/*
 *
 */
void PrintRemappedChannels(void) {
	int i = 0;
	ChannelRemap *mptr;

	if (!channelRemaps) {
		LogDebug(VB_CHANNELOUT, "No channels are remapped.\n");
		return;
	}

	LogDebug(VB_CHANNELOUT, "Remapped Channels:\n");
	for (i = 0, mptr = &remappedChannels[0]; i < channelRemaps; i++, mptr++) {
		if (mptr->count > 1) {
			LogDebug(VB_CHANNELOUT, "  %d-%d => %d-%d (%d channels)\n",
				mptr->src, mptr->src + mptr->count - 1,
				mptr->dest, mptr->dest + mptr->count - 1, mptr->count);
		} else {
			LogDebug(VB_CHANNELOUT, "  %d => %d\n",
				mptr->src, mptr->dest);
		}
	}
}

