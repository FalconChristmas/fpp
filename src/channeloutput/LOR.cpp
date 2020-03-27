/*
 *   Light-O-Rama (LOR) channel output driver for Falcon Player (FPP)
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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "common.h"
#include "log.h"
#include "settings.h"
#include "serialutil.h"

#include "LOR.h"

#define LOR_INTENSITY_SIZE 6
#define LOR_HEARTBEAT_SIZE 5
#define LOR_MAX_CHANNELS   3840

/////////////////////////////////////////////////////////////////////////////

class LOROutputData {
public:
    LOROutputData() : fd(-1), speed(19200), controllerOffset(0), lastHeartbeat(0) {
        filename[0] = 0;
        memset(lastValue, 0, LOR_MAX_CHANNELS);
        memset(intensityData, 0, LOR_INTENSITY_SIZE);
        memset(heartbeatData, 0, LOR_HEARTBEAT_SIZE);
    }
    ~LOROutputData() {}
	char          filename[1024];
	int           fd;
	int           speed;
    int           controllerOffset;
	int           maxIntensity;
	long long     lastHeartbeat;
	unsigned char intensityMap[256];
	unsigned char intensityData[LOR_INTENSITY_SIZE];
	unsigned char heartbeatData[LOR_HEARTBEAT_SIZE];
	char          lastValue[LOR_MAX_CHANNELS];
};

/////////////////////////////////////////////////////////////////////////////

extern "C" {
    LOROutput *createLOROutput(unsigned int startChannel,
                               unsigned int channelCount) {
        return new LOROutput(startChannel, channelCount);
    }
}

LOROutput::LOROutput(unsigned int startChannel, unsigned int channelCount)
: ChannelOutputBase(startChannel, channelCount), data(nullptr)
{}
LOROutput::~LOROutput() {
    if (data) {
        delete data;
    }
}

void LOROutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

static void LOR_SetupIntensityMap(LOROutputData *privData) {
    int t = 0;
    int i = 0;
    
    for (i = 0; i <= privData->maxIntensity; i++) {
        t = (int)(100.0 * (double)i/(double)privData->maxIntensity + 0.5);
        
        if (!t)
            privData->intensityMap[i] = 0xF0;
        else if (t == 100)
            privData->intensityMap[i] = 0x01;
        else
            privData->intensityMap[i] = 228 - (t * 2);
    }
}


int LOROutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "LOROutput::Close()\n");
    
    SerialClose(data->fd);
    data->fd = -1;
    return ChannelOutputBase::Close();
}


void LOROutput::DumpConfig(void) {
    ChannelOutputBase::DumpConfig();
    if (data) {
        LogDebug(VB_CHANNELOUT, "    filename     : %s\n", data->filename);
        LogDebug(VB_CHANNELOUT, "    fd           : %d\n", data->fd);
        LogDebug(VB_CHANNELOUT, "    port speed   : %d\n", data->speed);
        LogDebug(VB_CHANNELOUT, "    maxIntensity : %d\n", data->maxIntensity);
        LogDebug(VB_CHANNELOUT, "    lastHeartbeat: %d\n", data->lastHeartbeat);
        LogDebug(VB_CHANNELOUT, "    controllerOffset: %d\n", data->controllerOffset);
    }
}
int LOROutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "LOROutput::Init()\n");
    data = new LOROutputData();

    
    std::string deviceName = "UNKNOWN";
    if (config.isMember("device")) {
        deviceName = config["device"].asString();
        LogDebug(VB_CHANNELOUT, "Using %s for LOR output\n", deviceName.c_str());
    }
    if (config.isMember("speed")) {
        data->speed = config["speed"].asInt();
    }
    if (config.isMember("firstControllerId")) {
        data->controllerOffset = config["firstControllerId"].asInt();
    }


	if (deviceName == "UNKNOWN") {
		LogErr(VB_CHANNELOUT, "Missing Device Name\n");
		return 0;
	}

	strcpy(data->filename, "/dev/");
    strcat(data->filename, deviceName.c_str());

	data->fd = SerialOpen(data->filename, data->speed, "8N1");
	if (data->fd < 0) {
		LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
			errno, data->filename, strerror(errno));
		return 0;
	}

	data->maxIntensity = 255;
	LOR_SetupIntensityMap(data);

	data->intensityData[0] = 0x00;
	data->intensityData[1] = 0x00;
	data->intensityData[2] = 0x03;
	data->intensityData[3] = 0x00;
	data->intensityData[4] = 0x00;
	data->intensityData[5] = 0x00;

	data->heartbeatData[0] = 0x00;
	data->heartbeatData[1] = 0xFF;
	data->heartbeatData[2] = 0x81;
	data->heartbeatData[3] = 0x56;
	data->heartbeatData[4] = 0x00;

    return ChannelOutputBase::Init(config);
}

/*
 *
 */
static void LOR_SendHeartbeat(LOROutputData *privData) {
    long long now = GetTime();
    
    // Only send a heartbeat every 300ms
    if (privData->lastHeartbeat > (now - 300000))
        return;
    
    if (privData->fd > 0) {
        write(privData->fd, privData->heartbeatData, LOR_HEARTBEAT_SIZE);
        privData->lastHeartbeat = now;
    }
}

int LOROutput::SendData(unsigned char *channelData) {
	LogDebug(VB_CHANNELDATA, "LOROutput::SendData()\n");

	if (m_channelCount > LOR_MAX_CHANNELS) {
		LogErr(VB_CHANNELOUT,
			"LOR_SendData() tried to send %d bytes when max is %d at %d baud\n",
			m_channelCount, LOR_MAX_CHANNELS, data->speed);
		return 0;
	}

	int i = 0;
	for (i = 0; i < m_channelCount; i++) {
		if (data->lastValue[i] != channelData[i]) {
			data->intensityData[1] = data->controllerOffset + (i >> 4);
		
			if (data->intensityData[1] < 0xF0)
				data->intensityData[1]++;

			data->intensityData[3] = data->intensityMap[channelData[i]];
			data->intensityData[4] = 0x80 | (i % 16);

			write(data->fd, data->intensityData, LOR_INTENSITY_SIZE);

			data->lastValue[i] = channelData[i];
		}
	}
	LOR_SendHeartbeat(data);
    return m_channelCount;
}
