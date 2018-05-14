/*
 *   E131 output handler for Falcon Player (FPP)
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <chrono>
#include <vector>
#include <list>

#include <fstream>
#include <sstream>
#include <string>

#include <jsoncpp/json/json.h>

#include "channeloutput.h"
#include "channeloutputthread.h"
#include "common.h"
#include "e131defs.h"
#include "E131.h"
#include "FPD.h"
#include "fpp.h"
#include "log.h"
#include "settings.h"
#include "Universe.h"
#include "ping.h"

struct sockaddr_in    localAddress;
int                   sendSocket;

const char  E131header[] = {
	0x00,0x10,0x00,0x00,0x41,0x53,0x43,0x2d,0x45,0x31,0x2e,0x31,0x37,0x00,0x00,0x00,
	0x72,0x6e,0x00,0x00,0x00,0x04,'F','A','L','C','O','N',' ','F','P','P',
	0x00,0x00,0x00,0x00,0x00,0x00,0x72,0x58,0x00,0x00,0x00,0x02,'F','A','L','C',
	'O','N','C','H','R','I','S','T','M','A','S','.','C','O','M',' ',
	'B','Y',' ','D','P','I','T','T','S',' ','A','N','D',' ','M','Y',
	'K','R','O','F','T',' ','F','A','L','C','O','N',' ','P','I',' ',
	'P','L','A','Y','E','R',0x00,0x00,0x00,0x00,0x00,0x00,0x64,0x00,0x00,0x00,
	0x00,0x00,0x01,0x72,0x0b,0x02,0xa1,0x00,0x00,0x00,0x01,0x02,0x01,0x00
	};


char E131LocalAddress[16];
char E131sequenceNumber=1;

/* prototypes for functions below */
void LoadUniversesFromFile();
void UniversesPrint();


class OutputUniverseData {
public:
    OutputUniverseData() {
        memset((char *) &e131Address, 0, sizeof(sockaddr_in));
    }
    ~OutputUniverseData() {}

    UniverseEntry universe;
    sockaddr_in e131Address;
    struct iovec e131Iovecs[2];
    unsigned char e131Buffer[E131_HEADER_LENGTH];
    bool valid;
};

static std::list<OutputUniverseData> universes;
static struct mmsghdr *e131Msgs = nullptr;
static int UniverseCount = 0;
static int ValidUniverseCount = 0;


int E131_InitializeNetwork()
{
	int UniverseOctet[2];
	char sOctet1[4];
	char sOctet2[4];
	char sAddress[32];

	sendSocket = socket(AF_INET, SOCK_DGRAM, 0);

	if (!UniverseCount)
		return 1;

	if (sendSocket < 0) 
	{
		LogErr(VB_CHANNELOUT, "Error opening datagram socket\n");

		exit(1);
	}

	localAddress.sin_family = AF_INET;
	localAddress.sin_port = htons(E131_SOURCE_PORT);
	localAddress.sin_addr.s_addr = inet_addr(E131LocalAddress);

	if(bind(sendSocket, (struct sockaddr *) &localAddress, sizeof(struct sockaddr_in)) == -1)
	{
		LogErr(VB_CHANNELOUT, "Error in bind:errno=%d, %s\n", errno, strerror(errno));
	} 

	/* Disable loopback so I do not receive my own datagrams. */
	char loopch = 0;
	if(setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0)
	{
		LogErr(VB_CHANNELOUT, "Error setting IP_MULTICAST_LOOP error\n");
		close(sendSocket);
		return 0;
	}

    e131Msgs = (struct mmsghdr *)calloc(UniverseCount, sizeof(struct mmsghdr));
    ValidUniverseCount = 0;
	/* Initialize the sockaddr structure. */
    for (auto &u : universes) {
		u.e131Address.sin_family = AF_INET;
		u.e131Address.sin_port = htons(E131_DEST_PORT);

		if(u.universe.type == E131_TYPE_MULTICAST) {
			UniverseOctet[0] = u.universe.universe/256;
			UniverseOctet[1] = u.universe.universe%256;
			sprintf(sAddress, "239.255.%d.%d", UniverseOctet[0],UniverseOctet[1]);
			u.e131Address.sin_addr.s_addr = inet_addr(sAddress);
		} else {
			char *c = u.universe.unicastAddress;
			int isAlpha = 0;
			for (; *c && !isAlpha; c++)
				isAlpha = isalpha(*c);

			if (isAlpha) {
				struct hostent* uhost = gethostbyname(u.universe.unicastAddress);
				if (!uhost) {
					LogErr(VB_CHANNELOUT,
						"Error looking up E1.31 hostname: %s\n",
						u.universe.unicastAddress);
					close(sendSocket);
					return 0;
				}

				u.e131Address.sin_addr.s_addr = *((unsigned long*)uhost->h_addr);
            } else {
				u.e131Address.sin_addr.s_addr = inet_addr(u.universe.unicastAddress);
			}
		}
        memcpy(u.e131Buffer, E131header, E131_HEADER_LENGTH);

        u.e131Buffer[E131_PRIORITY_INDEX] = u.universe.priority;
        u.e131Buffer[E131_UNIVERSE_INDEX] = (char)(u.universe.universe/256);
        u.e131Buffer[E131_UNIVERSE_INDEX+1] = (char)(u.universe.universe%256);
        
        // Property Value Count
        u.e131Buffer[E131_COUNT_INDEX] = ((u.universe.size+1)/256);
        u.e131Buffer[E131_COUNT_INDEX+1] = ((u.universe.size+1)%256);
        
        // RLP Protocol flags and length
        int count = 638 - 16 - (512 - (u.universe.size));
        u.e131Buffer[E131_RLP_COUNT_INDEX] = (count/256)+0x70;
        u.e131Buffer[E131_RLP_COUNT_INDEX+1] = count%256;
        
        // Framing Protocol flags and length
        count = 638 - 38 - (512 - (u.universe.size));
        u.e131Buffer[E131_FRAMING_COUNT_INDEX] = (count/256)+0x70;
        u.e131Buffer[E131_FRAMING_COUNT_INDEX+1] = count%256;
        
        // DMP Protocol flags and length
        count = 638 - 115 - (512 - (u.universe.size));
        u.e131Buffer[E131_DMP_COUNT_INDEX] = (count/256)+0x70;
        u.e131Buffer[E131_DMP_COUNT_INDEX+1] = count%256;

        // use scatter/gather for the packet.   One IOV will contain
        // the header, the second will point into the raw channel data
        // and will be set at output time.   This avoids any memcpy.
        u.e131Iovecs[0].iov_base = u.e131Buffer;
        u.e131Iovecs[0].iov_len = E131_HEADER_LENGTH;
        u.e131Iovecs[1].iov_base = nullptr;
        u.e131Iovecs[1].iov_len = u.universe.size;
        
        e131Msgs[ValidUniverseCount].msg_hdr.msg_name = &u.e131Address;
        e131Msgs[ValidUniverseCount].msg_hdr.msg_namelen = sizeof(sockaddr_in);
        e131Msgs[ValidUniverseCount].msg_hdr.msg_iov = u.e131Iovecs;
        e131Msgs[ValidUniverseCount].msg_hdr.msg_iovlen = 2;
        e131Msgs[ValidUniverseCount].msg_len = u.universe.size + E131_HEADER_LENGTH;
        u.valid = true;
        ValidUniverseCount++;
	}
	// Set E131 header Data 

	return 1;
}

void E131_Initialize()
{
	LogInfo(VB_CHANNELOUT, "Initializing E1.31 output\n");

	E131sequenceNumber=1;
	LoadUniversesFromFile();
	if (UniverseCount)
	{
		GetInterfaceAddress(getE131interface(), E131LocalAddress, NULL, NULL);
		LogDebug(VB_CHANNELOUT, "E131LocalAddress = %s\n",E131LocalAddress);
		E131_InitializeNetwork();
	}
}

void PingE131Controllers() {
    std::map<std::string, int> done;
    bool hasInvalid = false;
    for (auto &u : universes) {
        if (u.universe.type == 1) {
            std::string host = u.universe.unicastAddress;
            if (done[host] == 0) {
                int p = ping(host);
                if (p <= 0) {
                    p = -1;
                }
                done[host] = p;
                hasInvalid = true;
            }
        }
    }
    for (auto &u : universes) {
        if (u.universe.type == 1) {
            std::string host = u.universe.unicastAddress;
            int p = done[host];
            if (p == -1) {
                //give a second chance before completely marking invalid
                p = ping(host);
                if (p <= 0) {
                    p = -2;
                }
                done[host] = p;
            }
            u.valid = p > 0;
        }
    }
    
    ValidUniverseCount = 0;
    //recreate the msgs array based on universes that are now valid
    for (auto &u : universes) {
        if (u.valid) {
            e131Msgs[ValidUniverseCount].msg_hdr.msg_name = &u.e131Address;
            e131Msgs[ValidUniverseCount].msg_hdr.msg_namelen = sizeof(sockaddr_in);
            e131Msgs[ValidUniverseCount].msg_hdr.msg_iov = u.e131Iovecs;
            e131Msgs[ValidUniverseCount].msg_hdr.msg_iovlen = 2;
            e131Msgs[ValidUniverseCount].msg_len = u.universe.size + E131_HEADER_LENGTH;
            ValidUniverseCount++;
        } else {
            LogWarn(VB_CHANNELOUT, "Could not ping E1.31 controller at address %s for universe %d.  Disabling.\n", u.universe.unicastAddress, u.universe.universe);
        }
    }
}

/*
 *
 */
int E131_SendData(void *data, char *channelData, int channelCount)
{
	struct itimerval tout_val;
	int i = 0;

	LogExcess(VB_CHANNELDATA, "Sending %d E1.31 universes\n",
		UniverseCount);

    for (auto &u : universes) {
        u.e131Buffer[E131_SEQUENCE_INDEX] = E131sequenceNumber;
        u.e131Iovecs[1].iov_base = (void*)(channelData + u.universe.startAddress - 1);
    }

    std::chrono::high_resolution_clock clock;
    auto t1 = clock.now();
    errno = 0;
    int oc = sendmmsg(sendSocket, e131Msgs, ValidUniverseCount, 0);
    int outputCount = oc;
    while (oc > 0 && outputCount != ValidUniverseCount) {
        int oc = sendmmsg(sendSocket, &e131Msgs[outputCount], ValidUniverseCount - outputCount, 0);
        if (oc >= 0) {
            outputCount += oc;
        }
    }
    auto t2 = clock.now();
    long diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    if ((outputCount != ValidUniverseCount) || (diff > 200)) {
        //failed to send all messages or it took more than 200ms to send them
        LogErr(VB_CHANNELOUT, "sendto() failed for E1.31 (output count: %d/%d   time: %u ms) with error: %d   %s\n",
               outputCount, ValidUniverseCount, diff,
               errno,
               strerror(errno));
        
        //we'll ping the controllers and rebuild the valid message list, this could take time
        PingE131Controllers();
        return 0;
    }
	++E131sequenceNumber;

	return 1;
}

/*
 *
 */
void LoadUniversesFromFile()
{
    if (UniverseCount > 0) {
        //already loaded
        return;
    }
	FILE *fp;
	char buf[512];
	char *s;
	UniverseCount=0;
	char active =0;
	char filename[1024];

	strcpy(filename, getMediaDirectory());
	strcat(filename, "/config/co-universes.json");

	LogDebug(VB_CHANNELOUT, "Opening File Now %s\n", filename);

	if (!FileExists(filename)) {
		LogErr(VB_CHANNELOUT, "Universe file %s does not exist\n",
			filename);
		return;
	}

	Json::Value root;
    Json::Reader reader;
	std::ifstream t(filename);
	std::stringstream buffer;

	buffer << t.rdbuf();

	std::string config = buffer.str();

	bool success = reader.parse(buffer.str(), root);
	if (!success) {
		LogErr(VB_CHANNELOUT, "Error parsing %s\n", filename);
		return;
	}

	Json::Value outputs = root["channelOutputs"];

	std::string type;
	int start = 0;
	int count = 0;

	for (int c = 0; c < outputs.size(); c++) {
        if (outputs[c]["type"].asString() != "universes") {
			continue;
        }

        if (outputs[c]["enabled"].asInt() == 0) {
            continue;
        }

		Json::Value univs = outputs[c]["universes"];

		for (int i = 0; i < univs.size(); i++) {
			Json::Value u = univs[i];
			if (u["active"].asInt()) {
                OutputUniverseData uv;
				uv.universe.active = u["active"].asInt();
				uv.universe.universe = u["id"].asInt();
				uv.universe.startAddress = u["startChannel"].asInt();
				uv.universe.size = u["channelCount"].asInt();
				uv.universe.type = u["type"].asInt();
                uv.universe.priority = u["priority"].asInt();

				switch (uv.universe.type) {
					case 0: // Multicast
							strcpy(uv.universe.unicastAddress,"\0");
							break;
					case 1: //UnicastAddress
							strcpy(uv.universe.unicastAddress,
								u["address"].asString().c_str());
							break;
					default: // ArtNet
							continue;
				}
	
                universes.push_back(uv);
			    UniverseCount++;
			}
		}
	}
	UniversesPrint();
}

void UniversesPrint()
{
    for(auto &u : universes) {
		LogDebug(VB_CHANNELOUT, "E1.31 Universe: %d:%d:%d:%d:%d  %s\n",
				  u.universe.active,
				  u.universe.universe,
				  u.universe.startAddress,
				  u.universe.size,
				  u.universe.type,
				  u.universe.unicastAddress);
	}
}

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
int E131_Open(char *configStr, void **privDataPtr) {
	LogDebug(VB_CHANNELOUT, "E131_Open()\n");

	E131_Initialize();

	*privDataPtr = NULL;

	return 1;
}

/*
 *
 */
int E131_Close(void *data) {
	LogDebug(VB_CHANNELOUT, "E131_Close(%p)\n", data);
    if (e131Msgs)  {
        free(e131Msgs);
        e131Msgs = nullptr;
    }
}

/*
 *
 */
int E131_IsConfigured(void) {
	LogDebug(VB_CHANNELOUT, "E131_IsConfigured()\n");

    if (!getSettingInt("E131Enabled")) {
		return 0;
    }

	LoadUniversesFromFile();

    if (UniverseCount > 0) {
		return 1;
    }
	return 0;
}

/*
 *
 */
int E131_IsActive(void *data) {
	LogDebug(VB_CHANNELOUT, "E131_IsActive(%p)\n", data);

	if (UniverseCount > 0)
		return 1;

	return 0;
}

/*
 *
 */
int E131_MaxChannels(void *data) {
	return 32768;
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput E131Output = {
	E131_MaxChannels, //maxChannels
	E131_Open, //open
	E131_Close, //close
	E131_IsConfigured, //isConfigured
	E131_IsActive, //isActive
	E131_SendData, //send
	NULL, //startThread
	NULL, //stopThread
};
