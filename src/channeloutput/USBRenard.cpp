/*
 *   Renard USB handler for Falcon Player (FPP)
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

#include "log.h"
#include "settings.h"
#include "serialutil.h"

#include "USBRenard.h"

/////////////////////////////////////////////////////////////////////////////

class USBRenardOutputData {
public:
    USBRenardOutputData() :fd(-1), maxChannels(0), speed(0), outputData(nullptr) {
    }
    ~USBRenardOutputData() {
        if (outputData) {
            delete [] outputData;
        }
    }
	char filename[1024];
	char *outputData;
	int  fd;
	int  maxChannels;
	int  speed;
	char parm[4];
};

// Assume clocks are accurate to 1%, so insert a pad byte every 100 bytes.
#define PAD_DISTANCE 100

/////////////////////////////////////////////////////////////////////////////
extern "C" {
    USBRenardOutput *createRenardOutput(unsigned int startChannel,
                                        unsigned int channelCount) {
        return new USBRenardOutput(startChannel, channelCount);
    }
}

/*
 * Data for this was gathered from the DIYC wiki pages on Renard channels at
 * 50ms refresh rate.  For the newer (faster) speeds the PX1 page was
 * referenced.
 *
 * http://www.doityourselfchristmas.com/wiki/index.php?title=Renard#Number_of_Circuits_.28Channels.29_per_Serial_Port
 * http://www.doityourselfchristmas.com/wiki/index.php?title=Renard_PX1_Pixel_Controller#Maximum_Number_of_Pixels_per_Controller
 *
 */
static int USBRenard_MaxChannels(USBRenardOutputData *privData)
{
    if (privData->maxChannels != 0)
        return privData->maxChannels;
    
    switch (privData->speed) {
        case 921600:    privData->maxChannels = 4584;
        break;
        case 460800:    privData->maxChannels = 2292;
        break;
        case 230400:    privData->maxChannels = 1146;
        break;
        case 115200:    privData->maxChannels = 574;
        break;
        case  57600:    privData->maxChannels = 286;
        break;
        case  38400:    privData->maxChannels = 190;
        break;
        case  19200:    privData->maxChannels = 94;
        break;
    }
    return privData->maxChannels;
}


USBRenardOutput::USBRenardOutput(unsigned int startChannel, unsigned int channelCount)
    : ChannelOutputBase(startChannel, channelCount), data(nullptr)
{
}
USBRenardOutput::~USBRenardOutput() {
    if (data) {
        delete data;
    }
}
void USBRenardOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}
int USBRenardOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "USBRenard_Open()\n");
    
    data = new USBRenardOutputData();
    std::string deviceName = "UNKNOWN";
    strcpy(data->parm, "8N1");
    if (config.isMember("device")) {
        deviceName = config["device"].asString();
        LogDebug(VB_CHANNELOUT, "Using %s for Renard output\n", deviceName.c_str());
    }
    if (config.isMember("renardspeed")) {
        data->speed = config["renardspeed"].asInt();
        LogDebug(VB_CHANNELOUT, "Sending Renard at %d speed\n", data->speed);
    }
    if (config.isMember("renardparm")) {
        std::string parm = config["renardparm"].asString();
        if (parm.length() != 3) {
            LogWarn(VB_CHANNELOUT, "Invalid length on serial parameters: %s\n", parm.c_str());
        } else {
            strcpy(data->parm, parm.c_str());
        }
    }
    
    if (deviceName == "UNKNOWN") {
        LogErr(VB_CHANNELOUT, "Invalid Config: %s\n", deviceName.c_str());
        return 0;
    }
    
    strcpy(data->filename, "/dev/");
    strcat(data->filename, deviceName.c_str());
    
    LogInfo(VB_CHANNELOUT, "Opening %s for Renard output\n",
            data->filename);
    
    data->fd = SerialOpen(data->filename, data->speed, data->parm);
    
    if (data->fd < 0) {
        LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
               errno, data->filename, strerror(errno));
        
        return 0;
    }
    
    data->outputData = new char[USBRenard_MaxChannels(data)];
    if (data->outputData == NULL) {
        LogErr(VB_CHANNELOUT, "Error %d allocating channel memory: %s\n",
               errno, strerror(errno));
        
        return 0;
    }
    bzero(data->outputData, data->maxChannels);
    
    return ChannelOutputBase::Init(config);
}

int USBRenardOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "USBRenard_Close()\n");
    if (data) {
        SerialClose(data->fd);
        data->fd = -1;
    }
    return ChannelOutputBase::Close();
}

int USBRenardOutput::SendData(unsigned char *channelData) {
    LogDebug(VB_CHANNELDATA, "USBRenard_SendData()\n");
    
    
    if (m_channelCount <= data->maxChannels) {
        bzero(data->outputData, data->maxChannels);
    } else {
        LogErr(VB_CHANNELOUT,
               "USBRenard_SendData() tried to send %d bytes when max is %d\n",
               m_channelCount, data->maxChannels);
        return 0;
    }
    
    memcpy(data->outputData, channelData, m_channelCount);
    
    // Act like "Renard (modified)" and don't output special codes.  There are
    // 3 we need to worry about.
    // 0x7D - Pad Byte    - map to 0x7C
    // 0x7E - Sync Byte   - map to 0x7C
    // 0x7F - Escape Byte - map to 0x80
    char *dptr = data->outputData;
    int i = 0;
    for (i = 0; i < data->maxChannels; i++) {
        if (*dptr == '\x7D')
            *dptr = '\x7C';
        else if (*dptr == '\x7E')
            *dptr = '\x7C';
        else if (*dptr == '\x7F')
            *dptr = '\x80';
        dptr++;
    }
    
    // Send start of packet byte
    write(data->fd, "\x7E\x80", 2);
    dptr = data->outputData;
    
    // Assume clocks are accurate to 1%, so insert a pad byte every 100 bytes.
    for (i = 0; i*PAD_DISTANCE < m_channelCount; i++) {
        // Send our pad byte
        write(data->fd, "\x7D", 1);
        
        // Send Renard Data (Only send the channels we're given, not max)
        if ( (i+1)*PAD_DISTANCE > m_channelCount )
            write(data->fd, dptr, (m_channelCount - (i * PAD_DISTANCE)));
        else
            write(data->fd, dptr, PAD_DISTANCE);
        
        dptr += PAD_DISTANCE;
    }
    return m_channelCount;
}

void USBRenardOutput::DumpConfig(void) {
    ChannelOutputBase::DumpConfig();
    if (!data)
        return;
    
    LogDebug(VB_CHANNELOUT, "    filename   : %s\n", data->filename);
    LogDebug(VB_CHANNELOUT, "    fd         : %d\n", data->fd);
    LogDebug(VB_CHANNELOUT, "    maxChannels: %i\n", data->maxChannels);
    LogDebug(VB_CHANNELOUT, "    speed      : %d\n", data->speed);
    LogDebug(VB_CHANNELOUT, "    serial Parm: %s\n", data->parm);
}




