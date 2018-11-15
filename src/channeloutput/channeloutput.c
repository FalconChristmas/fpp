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

#include <errno.h>
#include <pthread.h>
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
#include "UDPOutput.h"
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

#ifdef USE_X11ChannelOutputs
#  include "X11Matrix.h"
#  include "X11VirtualDisplay.h"
#endif

#if defined(PLATFORM_PI) || defined(PLATFORM_ODROID)
#  include "RGBMatrix.h"
#endif

#ifdef USEWIRINGPI
#  include "Hill320.h"
#  include "ILI9488.h"
#  include "MAX7219Matrix.h"
#  include "MCP23017.h"
#  include "rpi_ws281x.h"
#endif

#ifdef PLATFORM_BBB
#  include "BBB48String.h"
#  include "BBBSerial.h"
#  include "BBBMatrix.h"
#endif

#ifdef USEOLA
#  include "OLAOutput.h"
#endif

#include "processors/OutputProcessor.h"

/////////////////////////////////////////////////////////////////////////////

int                      channelOutputCount  = 0;
unsigned long            channelOutputFrame  = 0;
float                    mediaElapsedSeconds = 0.0;
FPPChannelOutputInstance channelOutputs[FPPD_MAX_CHANNEL_OUTPUTS];

static int LoadOutputProcessors(void);

OutputProcessors         outputProcessors;

int minimumNeededChannel = 0;
int maximumNeededChannel = FPPD_MAX_CHANNELS;

/////////////////////////////////////////////////////////////////////////////

void ChannelOutputJSON2CSV(Json::Value config, char *configStr)
{
	Json::Value::Members memberNames = config.getMemberNames();

	configStr[0] = '\0';

	if (!config.isMember("type"))
	{
		strcpy(configStr, "");
		return;
	}

	if (config.isMember("enabled"))
		strcat(configStr, config["enabled"].asString().c_str());
	else
		strcat(configStr, "0");

	strcat(configStr, ",");

	strcat(configStr, config["type"].asString().c_str());
	strcat(configStr, ",");

	if (config.isMember("startChannel"))
		strcat(configStr, config["startChannel"].asString().c_str());
	else
		strcat(configStr, "1");

	strcat(configStr, ",");

	if (config.isMember("channelCount"))
		strcat(configStr, config["channelCount"].asString().c_str());
	else
		strcat(configStr, "1");

	std::string key;
	int first = 1;
	for (int i = 0; i < memberNames.size(); i++)
	{
		key = memberNames[i];

		if (first)
		{
			strcat(configStr, ",");
			first = 0;
		}
		else
			strcat(configStr, ";");

		strcat(configStr, key.c_str());
		strcat(configStr, "=");
		strcat(configStr, config[key].asString().c_str());
	}
}

Json::Value ChannelOutputCSV2JSON(char *deviceConfig)
{
	Json::Value result;

	char *s;

	s = strtok(deviceConfig, ",");
	if (!s)
	{
		LogErr(VB_CHANNELOUT, "Error parsing CSV, empty string??");
		return result;
	}

	result["enabled"] = atoi(s);

	s = strtok(NULL, ",");
	if (!s)
	{
		LogErr(VB_CHANNELOUT,
			"Error parsing CSV '%s', could not determine type",
			deviceConfig);
		result["enabled"] = 0;
		return result;
	}

	result["type"] = s;

	s = strtok(NULL, ",");
	if (!s)
	{
		LogErr(VB_CHANNELOUT,
			"Error parsing CSV '%s', could not determine startChannel",
			deviceConfig);
		result["enabled"] = 0;
		return result;
	}

	result["startChannel"] = atoi(s);

	s = strtok(NULL, ",");
	if (!s)
	{
		LogErr(VB_CHANNELOUT,
			"Error parsing CSV '%s', could not determine channelCount",
			deviceConfig);
		result["enabled"] = 0;
		return result;
	}

	result["channelCount"] = atoi(s);

	s = strtok(NULL, ";");

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
			LogDebug(VB_CHANNELOUT, "Configured FPD Channel Output\n");
		} else {
			LogErr(VB_CHANNELOUT, "ERROR Opening FPD Channel Output\n");
		}
	}
    
	// FIXME, build this list dynamically
	char *configFiles[] = {
        "/config/co-universes.json",
		"/config/channeloutputs.json",
		"/config/co-other.json",
		"/config/co-pixelStrings.json",
        "/config/co-bbbStrings.json",
		NULL
		};

	FILE *fp;
	char filename[1024];
	char csvConfig[2048];

    maximumNeededChannel = 0;
    minimumNeededChannel = FPPD_MAX_CHANNELS;
    
	// Parse the JSON channel outputs config files
	for (int f = 0; configFiles[f]; f++)
	{
		strcpy(filename, getMediaDirectory());
		strcat(filename, configFiles[f]);

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
				csvConfig[0] = '\0';

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
						channelOutputs[i].output = new BBBMatrix(start, count);
#endif
					else
					{
						LogErr(VB_CHANNELOUT, "LEDPanelmatrix subType '%s' not valid\n", outputs[c]["subType"].asString().c_str());
						continue;
					}
#ifdef PLATFORM_BBB
				} else if (type == "BBB48String" && f != 0) {
					channelOutputs[i].output = new BBB48StringOutput(start, count);
				} else if (type == "BBBSerial" && f != 0) {
					channelOutputs[i].output = new BBBSerialOutput(start, count);
#endif
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
				} else if (type == "VirtualDisplay") {
					channelOutputs[i].output = (ChannelOutputBase*)new FBVirtualDisplayOutput(0, FPPD_MAX_CHANNELS);
				} else if (type == "USBRelay") {
					channelOutputs[i].output = new USBRelayOutput(start, count);
#if defined(PLATFORM_PI)
				} else if (type == "Hill320") {
					channelOutputs[i].output = new Hill320Output(start, count);
				} else if (type == "MAX7219Matrix") {
					channelOutputs[i].output = new MAX7219MatrixOutput(start, count);
				} else if (type == "MCP23017") {
					channelOutputs[i].output = new MCP23017Output(start, count);
#endif
#ifdef PLATFORM_PI
				} else if (type == "ILI9488") {
					channelOutputs[i].output = new ILI9488Output(start, count);
				} else if (type == "RPIWS281X") {
					channelOutputs[i].output = new RPIWS281xOutput(start, count);
				} else if (type == "SPI-WS2801") {
					channelOutputs[i].output = new SPIws2801Output(start, count);
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
				} else if (type == "SPI-nRF24L01") {
					channelOutputs[i].outputOld = &SPInRF24L01Output;
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
#endif
#ifdef USE_X11ChannelOutputs
				} else if (type == "X11Matrix") {
					channelOutputs[i].output = new X11MatrixOutput(start, count);
				} else if (type == "X11VirtualDisplay") {
					channelOutputs[i].output = (ChannelOutputBase*)new X11VirtualDisplayOutput(0, FPPD_MAX_CHANNELS);
#endif
				}else if ((type == "Pixelnet-Lynx") ||
						  (type == "Pixelnet-Open"))
				{
					channelOutputs[i].output = new USBPixelnetOutput(start, count);
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
				} else if ((type == "DMX-Pro") ||
						   (type == "DMX-Open")) {
					channelOutputs[i].output = new USBDMXOutput(start, count);
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
				} else if ((type == "VirtualMatrix") ||
						   (type == "FBMatrix")) {
					channelOutputs[i].output = new FBMatrixOutput(start, count);
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
				} else if (type == "GPIO") {
					channelOutputs[i].output = new GPIOOutput(start, count);
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
				} else if (type == "GPIO-595") {
					channelOutputs[i].output = new GPIO595Output(start, count);
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
				} else if (type == "GenericSerial") {
					channelOutputs[i].output = new GenericSerialOutput(start, count);
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
				} else if (type == "LOR") {
					channelOutputs[i].outputOld = &LOROutput;
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
				} else if (type == "Renard") {
					channelOutputs[i].outputOld = &USBRenardOutput;
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
				} else if (type == "Triks-C") {
					channelOutputs[i].outputOld = &TriksCOutput;
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
				} else if (type == "Debug") {
					channelOutputs[i].output = new DebugOutput(start, count);
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
                } else if (type == "universes") {
                    channelOutputs[i].output = new UDPOutput(start, count);
				} else {
					LogErr(VB_CHANNELOUT, "Unknown Channel Output type: %s\n", type.c_str());
					continue;
				}

				if ((channelOutputs[i].outputOld) &&
					(channelOutputs[i].outputOld->open(csvConfig, &channelOutputs[i].privData)))
				{
					if (channelOutputs[i].channelCount > channelOutputs[i].outputOld->maxChannels(channelOutputs[i].privData)) {
						LogWarn(VB_CHANNELOUT,
							"Channel Output config, count (%d) exceeds max (%d) channel for configured output\n",
							channelOutputs[i].channelCount, channelOutputs[i].outputOld->maxChannels(channelOutputs[i].privData));

						channelOutputs[i].channelCount = channelOutputs[i].outputOld->maxChannels(channelOutputs[i].privData);

						LogWarn(VB_CHANNELOUT,
							"Count suppressed to %d for config: %s\n", channelOutputs[i].channelCount, csvConfig);
					}
                    
                    int m1 = channelOutputs[i].startChannel;
                    int m2 = m1 + channelOutputs[i].channelCount - 1;
                    minimumNeededChannel = std::min(minimumNeededChannel, m1);
                    maximumNeededChannel = std::max(maximumNeededChannel, m2);
					i++;
				} else if ((channelOutputs[i].output) &&
						   (((!csvConfig[0]) && (channelOutputs[i].output->Init(outputs[c]))) ||
							((csvConfig[0]) && (channelOutputs[i].output->Init(csvConfig))))) {
                               
                               
                    int m1, m2;
                    channelOutputs[i].output->GetRequiredChannelRange(m1, m2);
                    minimumNeededChannel = std::min(minimumNeededChannel, m1);
                    maximumNeededChannel = std::max(maximumNeededChannel, m2);
                    LogInfo(VB_CHANNELOUT, "%s %d:  Determined range needed %d - %d\n",
                            type.c_str(), i, minimumNeededChannel, maximumNeededChannel);

                    i++;
				} else {
					LogErr(VB_CHANNELOUT, "ERROR Opening %s Channel Output\n", type.c_str());
					continue;
				}

				LogDebug(VB_CHANNELOUT, "Configured %s Channel Output\n", type.c_str());
			}
		}
	}

	channelOutputCount = i;

	LogDebug(VB_CHANNELOUT, "%d Channel Outputs configured\n", channelOutputCount);

	LoadOutputProcessors();
    int m1, m2;
    outputProcessors.GetRequiredChannelRange(m1, m2);
    minimumNeededChannel = std::min(minimumNeededChannel, m1);
    maximumNeededChannel = std::max(maximumNeededChannel, m2);
    if (maximumNeededChannel < minimumNeededChannel) {
        maximumNeededChannel = minimumNeededChannel = 0;
    }
    // having the reads be aligned to intervals of 8 can help performance so
    // we'll expand the range a bit to align things better
    //round minimum down to interval of 8
    if (minimumNeededChannel > 0) {
        minimumNeededChannel--;
    }
    minimumNeededChannel &= 0xFFFFFFF8;
    maximumNeededChannel += 8;
    maximumNeededChannel &= 0xFFFFFFF8;
    maximumNeededChannel -= 1;

    LogInfo(VB_CHANNELOUT, "Determined range needed %d - %d\n", minimumNeededChannel, maximumNeededChannel);

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


int PrepareChannelData(char *channelData) {
    outputProcessors.ProcessData((unsigned char *)channelData);
    FPPChannelOutputInstance *inst;
    for (int i = 0; i < channelOutputCount; i++) {
        inst = &channelOutputs[i];
        if (inst->output) {
            inst->output->PrepData((unsigned char *)channelData);
        }
    }
    return 0;
}

/*
 *
 */
int SendChannelData(char *channelData) {
	int i = 0;
	FPPChannelOutputInstance *inst;

    outputProcessors.ProcessData((unsigned char *)channelData);
	if (logMask & VB_CHANNELDATA) {
        char buf[128];
        sprintf(buf, "Channel Data starting at channel %d", minimumNeededChannel);
		HexDump("Channel Data", &channelData[minimumNeededChannel], 16);
	}

    for (i = 0; i < channelOutputCount; i++) {
        inst = &channelOutputs[i];
        if (inst->outputOld) {
            inst->outputOld->send(
                    inst->privData,
                    channelData + inst->startChannel,
                    inst->channelCount < (FPPD_MAX_CHANNELS - inst->startChannel) ? inst->channelCount : (FPPD_MAX_CHANNELS - inst->startChannel));
        } else if (inst->output) {
            inst->output->SendData((unsigned char *)(channelData + inst->startChannel));
        }
    }


	channelOutputFrame++;

	// Reset channelOutputFrame every week @ 50ms timing
	if (channelOutputFrame > 12096000)
		channelOutputFrame = 0;
    return 0;
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
	int i = 0;

	for (i = 0; i < channelOutputCount; i++) {
		if (channelOutputs[i].outputOld)
			channelOutputs[i].outputOld->close(channelOutputs[i].privData);
		else if (channelOutputs[i].output)
			channelOutputs[i].output->Close();

		if (channelOutputs[i].privData)
			free(channelOutputs[i].privData);
	}
    
    for (i = 0; i < channelOutputCount; i++) {
        if (channelOutputs[i].output) {
            delete channelOutputs[i].output;
            channelOutputs[i].output = NULL;
        }
    }
}


int LoadOutputProcessors(void) {
	char filename[1024];
	Json::Value root;
	Json::Reader reader;

	strcpy(filename, getMediaDirectory());
	strcat(filename, "/config/outputprocessors.json");
    
	if (!FileExists(filename))
		return 0;

	LogDebug(VB_CHANNELOUT, "Loading Output Processors.\n");

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

    outputProcessors.loadFromJSON(root);

	return 1;
}

