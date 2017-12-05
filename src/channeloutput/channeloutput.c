/*
 *   generic output channel handler for Falcon Pi Player (FPP)
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>

#include "channeloutput.h"
#include "DebugOutput.h"
#include "ArtNet.h"
#include "ColorLight-5a-75.h"
#include "DDP.h"
#include "E131.h"
#include "FBMatrix.h"
#include "FBVirtualDisplay.h"
#include "FPD.h"
#include "GenericSerial.h"
#include "Linsn-RV9.h"
#include "log.h"
#include "Sequence.h"
#include "settings.h"
#include "SPIws2801.h"
#include "LOR.h"
#include "SPInRF24L01.h"
#include "RHL_DVI_E131.h"
#include "USBDMX.h"
#include "USBPixelnet.h"
#include "USBRelay.h"
#include "USBRenard.h"
#include "Triks-C.h"
#include "GPIO.h"
#include "GPIO595.h"
#include "common.h"

#ifdef USE_X11Matrix
#  include "X11Matrix.h"
#  include "X11VirtualDisplay.h"
#endif

#if defined(PLATFORM_PI) || defined(PLATFORM_ODROID)
#  include "RGBMatrix.h"
#endif

#ifdef PLATFORM_PI
#  include "Hill320.h"
#  include "MCP23017.h"
#  include "rpi_ws281x.h"
#endif

#ifdef PLATFORM_BBB
#  include "BBB48String.h"
#  include "BBBSerial.h"
#  include "LEDscapeMatrix.h"
#endif

#ifdef USEOLA
#  include "OLAOutput.h"
#endif

/////////////////////////////////////////////////////////////////////////////

#define MAX_CHANNEL_REMAPS  512

typedef struct {
	int src;
	int count;
	int dest;
} ChannelRemap;

ChannelRemap             remappedChannels[MAX_CHANNEL_REMAPS];
int                      channelRemaps       = 0;
int                      channelOutputCount  = 0;
unsigned long            channelOutputFrame  = 0;
float                    mediaElapsedSeconds = 0.0;
FPPChannelOutputInstance channelOutputs[FPPD_MAX_CHANNEL_OUTPUTS];

/* Some prototypes for helpers below */
int LoadChannelRemapData(void);
void RemapChannels(char *channelData);
void PrintRemappedChannels(void);

/////////////////////////////////////////////////////////////////////////////

Json::Value ChannelOutputCSV2JSON(char *deviceConfig)
{
	Json::Value result;

	char *s = strtok(deviceConfig, ";");

	while (s)
	{
		char tmp[128];
		char *div = NULL;

		strcpy(tmp, s);
		div = strchr(tmp, '=');

		if (div)
		{
			*div = '\0';
			div++;

			result[tmp] = div;
		}

		s = strtok(NULL, ";");
	}

	return result;
}

/*
 *
 */
int InitializeChannelOutputs(void) {
	Json::Value root;
	Json::Reader reader;
	int i = 0;

	channelOutputFrame = 0;

	for (i = 0; i < FPPD_MAX_CHANNEL_OUTPUTS; i++) {
		bzero(&channelOutputs[i], sizeof(channelOutputs[i]));
	}

	// Reset index so we can start populating the outputs array
	i = 0;

	if (FPDOutput.isConfigured())
	{
		channelOutputs[i].startChannel = getSettingInt("FPDStartChannelOffset");
		channelOutputs[i].outputOld = &FPDOutput;

		if (FPDOutput.open("", &channelOutputs[i].privData)) {
			channelOutputs[i].channelCount = channelOutputs[i].outputOld->maxChannels(channelOutputs[i].privData);

			i++;
		} else {
			LogErr(VB_CHANNELOUT, "ERROR Opening FPD Channel Output\n");
		}
	}

	if (((getFPPmode() != BRIDGE_MODE) ||
		 (getSettingInt("E131Bridging"))) &&
		 (E131Output.isConfigured()))
	{
		channelOutputs[i].startChannel = 0;
		channelOutputs[i].outputOld  = &E131Output;

		if (E131Output.open("", &channelOutputs[i].privData)) {
			channelOutputs[i].channelCount = channelOutputs[i].outputOld->maxChannels(channelOutputs[i].privData);

			i++;
		} else {
			LogErr(VB_CHANNELOUT, "ERROR Opening E1.31 Channel Output\n");
		}
	}

	if (ArtNetOutput.isConfigured())
	{
		channelOutputs[i].startChannel = 0;
		channelOutputs[i].outputOld  = &ArtNetOutput;

		if (ArtNetOutput.open("", &channelOutputs[i].privData)) {
			channelOutputs[i].channelCount = channelOutputs[i].outputOld->maxChannels(channelOutputs[i].privData);

			i++;
		} else {
			LogErr(VB_CHANNELOUT, "ERROR Opening ArtNet Channel Output\n");
		}
	}

	FILE *fp;
	char filename[1024];
	char buf[2048];

	// Parse the channeloutputs.json config file
	strcpy(filename, getMediaDirectory());
	strcat(filename, "/config/channeloutputs.json");

	LogDebug(VB_CHANNELOUT, "Loading %s\n", filename);

	if (FileExists(filename))
	{
		std::ifstream t(filename);
		std::stringstream buffer;

		buffer << t.rdbuf();

		std::string config = buffer.str();

		bool success = reader.parse(buffer.str(), root);
		if (!success)
		{
			LogErr(VB_CHANNELOUT, "Error parsing %s\n", filename);
			return 0;
		}

		const Json::Value outputs = root["channelOutputs"];
		std::string type;
		int start = 0;
		int count = 0;

		for (int c = 0; c < outputs.size(); c++)
		{
			type = outputs[c]["type"].asString();

			if (!outputs[c]["enabled"].asInt())
			{
				LogDebug(VB_CHANNELOUT, "Skipping Disabled Channel Output: %s\n", type.c_str());
				continue;
			}

			start = outputs[c]["startChannel"].asInt();
			count = outputs[c]["channelCount"].asInt();

			// internally we start channel counts at zero
			start -= 1;

			channelOutputs[i].startChannel = start;
			channelOutputs[i].channelCount = count;

			// First some Channel Outputs enabled everythwere
			if (type == "LEDPanelMatrix") {
				if (outputs[c]["subType"] == "ColorLight5a75")
					channelOutputs[i].output = new ColorLight5a75Output(start, count);
				else if (outputs[c]["subType"] == "LinsnRV9")
					channelOutputs[i].output = new LinsnRV9Output(start, count);
#if defined(PLATFORM_PI) || defined(PLATFORM_ODROID)
				else if (outputs[c]["subType"] == "RGBMatrix")
					channelOutputs[i].output = new RGBMatrixOutput(start, count);
#endif
#ifdef PLATFORM_BBB
				else if (outputs[c]["subType"] == "LEDscapeMatrix")
					channelOutputs[i].output = new LEDscapeMatrixOutput(start, count);
#endif
				else
				{
					LogErr(VB_CHANNELOUT, "LEDPanelmatrix subType '%s' not valid\n", outputs[c]["subType"].asString().c_str());
					continue;
				}
#ifdef PLATFORM_BBB
			} else if (type == "BBB48String") {
				channelOutputs[i].output = new BBB48StringOutput(start, count);
			} else if (type == "BBBSerial") {
				channelOutputs[i].output = new BBBSerialOutput(start, count);
#endif
			} else if (type == "DDP") {
				channelOutputs[i].output = new DDPOutput(start, count);
			} else if (type == "FBVirtualDisplay") {
				channelOutputs[i].output = (ChannelOutputBase*)new FBVirtualDisplayOutput(0, FPPD_MAX_CHANNELS);
			} else if (type == "RHLDVIE131") {
				channelOutputs[i].output = (ChannelOutputBase*)new RHLDVIE131Output(start, count);
			} else if (type == "USBRelay") {
				channelOutputs[i].output = new USBRelayOutput(start, count);
			// NOW some platform or config specific Channel Outputs
#ifdef USEOLA
			} else if (type == "OLA") {
				channelOutputs[i].output = new OLAOutput(start, count);
#endif
			} else if (type == "FBVirtualDisplay") {
				channelOutputs[i].output = (ChannelOutputBase*)new FBVirtualDisplayOutput(0, FPPD_MAX_CHANNELS);
			} else if (type == "USBRelay") {
				channelOutputs[i].output = new USBRelayOutput(start, count);
#if defined(PLATFORM_PI)
			} else if (type == "Hill320") {
				channelOutputs[i].output = new Hill320Output(start, count);
			} else if (type == "MCP23017") {
				channelOutputs[i].output = new MCP23017Output(start, count);
#endif
#ifdef USE_X11Matrix
			} else if (type == "X11Matrix") {
				channelOutputs[i].output = new X11MatrixOutput(start, count);
			} else if (type == "X11VirtualDisplay") {
				channelOutputs[i].output = (ChannelOutputBase*)new X11VirtualDisplayOutput(0, FPPD_MAX_CHANNELS);
#endif
			} else {
				LogErr(VB_CHANNELOUT, "Unknown Channel Output type: %s\n", type.c_str());
				continue;
			}

			if (channelOutputs[i].output->Init(outputs[c])) {
				i++;
			} else {
				LogErr(VB_CHANNELOUT, "ERROR Opening %s Channel Output\n", type.c_str());
			}
		}
	}

	// Parse the channeloutputs config file
	strcpy(filename, getMediaDirectory());
	strcat(filename, "/channeloutputs");

	if (FileExists(filename))
	{
		LogDebug(VB_CHANNELOUT, "Loading %s\n", filename);

		fp = fopen(filename, "r");

		if (fp == NULL)
		{
			LogErr(VB_CHANNELOUT,
				"Could not open Channel Outputs config file %s: %s\n",
				filename, strerror(errno));
			channelOutputCount = 0;

			return 0;
		}

		while(fgets(buf, 2048, fp) != NULL)
		{
			Json::Value jsonConfig;
			int  useJSON = 0;
			int  enabled = 0;
			char type[32];
			int  start = 0;
			int  count = 0;
			char deviceConfig[256];

			if (buf[0] == '#') // Allow # comments for testing
				continue;

			int fields = sscanf(buf, "%d,%[^,],%d,%d,%s",
				&enabled, type, &start, &count, deviceConfig);

			if (fields != 5) {
				LogErr(VB_CHANNELOUT,
					"Invalid line in channeloutputs config file: %s\n", buf);
				continue;
			}

			if (!enabled) {
				LogDebug(VB_CHANNELOUT, "Skipping disabled channel output: %s", buf);
				continue;
			}

			if (count > (FPPD_MAX_CHANNELS - start)) {
				LogWarn(VB_CHANNELOUT,
					"Channel Output config, start (%d) + count (%d) exceeds max (%d) channel\n",
					start, count, FPPD_MAX_CHANNELS);

				count = FPPD_MAX_CHANNELS - start;

				LogWarn(VB_CHANNELOUT,
					"Count suppressed to %d for config line: %s\n", count, buf);
			}

			if (strlen(deviceConfig))
				strcat(deviceConfig, ";");

			strcat(deviceConfig, "type=");
			strcat(deviceConfig, type);

			LogDebug(VB_CHANNELOUT, "ChannelOutput: %d %s %d %d %s\n", enabled, type, start, count, deviceConfig);

			// internally we start channel counts at zero
			start -= 1;

			channelOutputs[i].startChannel = start;
			channelOutputs[i].channelCount = count;

			if ((!strcmp(type, "Pixelnet-Lynx")) ||
				(!strcmp(type, "Pixelnet-Open")))
			{
				channelOutputs[i].output = new USBPixelnetOutput(start, count);
			} else if ((!strcmp(type, "DMX-Pro")) ||
					   (!strcmp(type, "DMX-Open"))) {
				channelOutputs[i].output = new USBDMXOutput(start, count);
			} else if ((!strcmp(type, "VirtualMatrix")) ||
						(!strcmp(type, "FBMatrix"))) {
				channelOutputs[i].output = new FBMatrixOutput(start, count);
			} else if (!strcmp(type, "GPIO")) {
				channelOutputs[i].output = new GPIOOutput(start, count);
			} else if (!strcmp(type, "GenericSerial")) {
				channelOutputs[i].output = new GenericSerialOutput(start, count);
			} else if (!strcmp(type, "LOR")) {
				channelOutputs[i].outputOld = &LOROutput;
			} else if (!strcmp(type, "Renard")) {
				channelOutputs[i].outputOld = &USBRenardOutput;
#ifdef PLATFORM_PI
			} else if (!strcmp(type, "RPIWS281X")) {
				channelOutputs[i].output = new RPIWS281xOutput(start, count);
#endif
			} else if (!strcmp(type, "SPI-WS2801")) {
				channelOutputs[i].output = new SPIws2801Output(start, count);
			} else if (!strcmp(type, "SPI-nRF24L01")) {
				channelOutputs[i].outputOld = &SPInRF24L01Output;
			} else if (!strcmp(type, "Triks-C")) {
				channelOutputs[i].outputOld = &TriksCOutput;
			} else if (!strcmp(type, "GPIO-595")) {
				channelOutputs[i].output = new GPIO595Output(start, count);
			} else if (!strcmp(type, "Debug")) {
				channelOutputs[i].output = new DebugOutput(start, count);
			} else if (!strcmp(type, "USBRelay")) {
				channelOutputs[i].output = new USBRelayOutput(start, count);
				useJSON = 1;
				jsonConfig = ChannelOutputCSV2JSON(deviceConfig);
			} else {
				LogErr(VB_CHANNELOUT, "Unknown Channel Output type: %s\n", type);
				continue;
			}

			jsonConfig["type"] = type;
			jsonConfig["startChannel"] = start;
			jsonConfig["channelCount"] = count;

			if ((channelOutputs[i].outputOld) &&
				(channelOutputs[i].outputOld->open(deviceConfig, &channelOutputs[i].privData)))
			{
				if (channelOutputs[i].channelCount > channelOutputs[i].outputOld->maxChannels(channelOutputs[i].privData)) {
					LogWarn(VB_CHANNELOUT,
						"Channel Output config, count (%d) exceeds max (%d) channel for configured output\n",
						channelOutputs[i].channelCount, channelOutputs[i].outputOld->maxChannels(channelOutputs[i].privData));

					channelOutputs[i].channelCount = channelOutputs[i].outputOld->maxChannels(channelOutputs[i].privData);

					LogWarn(VB_CHANNELOUT,
						"Count suppressed to %d for config: %s\n", channelOutputs[i].channelCount, buf);
				}
				i++;
			} else if ((channelOutputs[i].output) &&
					   (((useJSON) && (channelOutputs[i].output->Init(jsonConfig))) ||
						((!useJSON) && (channelOutputs[i].output->Init(deviceConfig))))) {
				i++;
			} else {
				LogErr(VB_CHANNELOUT, "ERROR Opening %s Channel Output\n", type);
			}
		}

		fclose(fp);
	}

	channelOutputCount = i;

	LogDebug(VB_CHANNELOUT, "%d Channel Outputs configured\n", channelOutputCount);

	LoadChannelRemapData();

	return 1;
}

/*
 * Set the channel output frame counter to a specific value
 */
void SetChannelOutputFrameNumber(int frameNumber)
{
	channelOutputFrame = frameNumber;
}

/*
 * Reset the output frame count
 */
void ResetChannelOutputFrameNumber(void) {
	channelOutputFrame = 0;
	mediaElapsedSeconds = 0.0;
}

/*
 *
 */
int SendChannelData(char *channelData) {
	int i = 0;
	FPPChannelOutputInstance *inst;

	RemapChannels(channelData);

	if (logMask & VB_CHANNELDATA) {
		HexDump("Channel Data", channelData, 16);
	}

	for (i = 0; i < channelOutputCount; i++) {
		inst = &channelOutputs[i];
		if (inst->outputOld)
			inst->outputOld->send(
				inst->privData,
				channelData + inst->startChannel,
				inst->channelCount < (FPPD_MAX_CHANNELS - inst->startChannel) ? inst->channelCount : (FPPD_MAX_CHANNELS - inst->startChannel));
		else if (inst->output)
		{
			// FIXME, get this call to PrepData into another thread
			inst->output->PrepData((unsigned char *)channelData);
			inst->output->SendData((unsigned char *)(channelData + inst->startChannel));
		}
	}

	channelOutputFrame++;
}

/*
 *
 */
void StartOutputThreads(void) {
	FPPChannelOutputInstance *output;
	int i = 0;

	for (i = 0; i < channelOutputCount; i++) {
		if ((channelOutputs[i].outputOld) &&
			(channelOutputs[i].outputOld->startThread))
			channelOutputs[i].outputOld->startThread(channelOutputs[i].privData);
	}
}

/*
 *
 */
void StopOutputThreads(void) {
	FPPChannelOutputInstance *output;
	int i = 0;

	for (i = 0; i < channelOutputCount; i++) {
		if ((channelOutputs[i].outputOld) &&
			(channelOutputs[i].outputOld->stopThread))
			channelOutputs[i].outputOld->stopThread(channelOutputs[i].privData);
	}
}

/*
 *
 */
int CloseChannelOutputs(void) {
	FPPChannelOutputInstance *output;
	int i = 0;

	for (i = 0; i < channelOutputCount; i++) {
		if (channelOutputs[i].outputOld)
			channelOutputs[i].outputOld->close(channelOutputs[i].privData);
		else if (channelOutputs[i].output)
			channelOutputs[i].output->Close();

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

	if (!FileExists(filename))
		return 0;

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
				mptr->src + 1, mptr->dest + 1);
		}
	}
}

