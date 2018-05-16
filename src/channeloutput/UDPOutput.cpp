/*
 *   IP output handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
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
#include <chrono>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "UDPOutput.h"
#include "log.h"
#include "ping.h"

#include "common.h"
#include "settings.h"

#include "E131.h"
#include "DDP.h"
#include "ArtNet.h"


UDPOutputData::UDPOutputData(const Json::Value &config)
:  valid(true) {
    description = config["description"].asString();
    active = config["active"].asInt();
    startChannel = config["startChannel"].asInt();
    channelCount = config["channelCount"].asInt();
    type = config["type"].asInt();
}
UDPOutputData::~UDPOutputData() {
}



UDPOutput::UDPOutput(unsigned int startChannel, unsigned int channelCount) {
    sendSocket = -1;
}
UDPOutput::~UDPOutput() {
    for (auto a : outputs) {
        delete a;
    }
}

int UDPOutput::Init(Json::Value config) {
    m_useOutputThread = 0;
    
    enabled = config["enabled"].asInt();
    for (int i = 0; i < config["universes"].size(); i++) {
        Json::Value s = config["universes"][i];
        int type = s["type"].asInt();
        switch (type) {
            case 0:
            case 1:
                //E1.31 types
                outputs.push_back(new E131OutputData(s));
                break;
            case 2:
            case 3:
                //ArtNet types
                outputs.push_back(new ArtNetOutputData(s));
                break;
            case 4:
            case 5:
                //DDP types
                outputs.push_back(new DDPOutputData(s));
                break;
            default:
                LogErr(VB_CHANNELOUT, "Unknown IP output type %d\n", type);
                break;
        }
        
    }
    InitNetwork();
    PingControllers();
    return ChannelOutputBase::Init(config);
}
int  UDPOutput::Close() {
    return ChannelOutputBase::Close();
}
void UDPOutput::PrepData(unsigned char *channelData) {
    if (enabled) {
        for (auto a : outputs) {
            a->PrepareData(channelData);
        }
    }
}

int UDPOutput::SendMessages(int socket, std::vector<struct mmsghdr> &sendmsgs) {
    errno = 0;
    struct mmsghdr *msgs = &sendmsgs[0];
    int msgCount = sendmsgs.size();
    if (msgCount == 0) {
        return 0;
    }
    
    int oc = sendmmsg(socket, msgs, msgCount, 0);
    int outputCount = oc;
    while (oc > 0 && outputCount != msgCount) {
        int oc = sendmmsg(sendSocket, &msgs[outputCount], msgCount - outputCount, 0);
        if (oc >= 0) {
            outputCount += oc;
        }
    }
    return outputCount;
}

int UDPOutput::RawSendData(unsigned char *channelData) {
    if ((udpMsgs.size() == 0 && broadcastMsgs.size() == 0) || !enabled) {
        return 0;
    }
    std::chrono::high_resolution_clock clock;
    auto t1 = clock.now();
    int outputCount = SendMessages(sendSocket, udpMsgs);
    auto t2 = clock.now();
    long diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    if ((outputCount != udpMsgs.size()) || (diff > 200)) {
        //failed to send all messages or it took more than 200ms to send them
        LogErr(VB_CHANNELOUT, "sendmmsg() failed for UDP output (output count: %d/%d   time: %u ms) with error: %d   %s\n",
               outputCount, udpMsgs.size(), diff,
               errno,
               strerror(errno));
        
        //we'll ping the controllers and rebuild the valid message list, this could take time
        
        PingControllers();
        return 0;
    }
    outputCount = SendMessages(broadcastSocket, broadcastMsgs);
    
    return 1;
}
void UDPOutput::PingControllers() {
    std::map<std::string, int> done;
    bool hasInvalid = false;
    for (auto o : outputs) {
        if (o->IsPingable() && o->active) {
            std::string host = o->ipAddress;
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
    for (auto o : outputs) {
        if (o->IsPingable() && o->active) {
            std::string host = o->ipAddress;
            int p = done[host];
            if (p == -1) {
                //give a second chance before completely marking invalid
                p = ping(host);
                if (p <= 0) {
                    p = -2;
                }
                done[host] = p;
            }
            o->valid = p > 0;
        }
    }
    udpMsgs.clear();
    broadcastMsgs.clear();
    for (auto a : outputs) {
        if (a->valid && a->active) {
            a->CreateMessages(udpMsgs);
            a->CreateBroadcastMessages(broadcastMsgs);
        }
    }
    //add any sync packets or whatever that are needed
    for (auto a : outputs) {
        if (a->valid && a->active) {
            a->AddPostDataMessages(broadcastMsgs);
        }
    }
}
void UDPOutput::DumpConfig() {
    ChannelOutputBase::DumpConfig();
    for (auto u : outputs) {
        u->DumpConfig();
    }
}

bool UDPOutput::InitNetwork() {
    

    char E131LocalAddress[16];
    GetInterfaceAddress(getE131interface(), E131LocalAddress, NULL, NULL);
    LogDebug(VB_CHANNELOUT, "UDPLocalAddress = %s\n",E131LocalAddress);


    sendSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendSocket < 0) {
        LogErr(VB_CHANNELOUT, "Error opening datagram socket\n");
        exit(1);
    }

    static struct sockaddr_in   localAddress;
    memset(&localAddress, 0, sizeof(struct sockaddr_in));
    localAddress.sin_family = AF_INET;
    localAddress.sin_port = ntohs(0);
    localAddress.sin_addr.s_addr = inet_addr(E131LocalAddress);

    errno = 0;
    if (bind(sendSocket, (struct sockaddr *) &localAddress, sizeof(struct sockaddr_in)) == -1) {
        LogErr(VB_CHANNELOUT, "Error in bind:errno=%d, %s\n", errno, strerror(errno));
    }
    
    /* Disable loopback so I do not receive my own datagrams. */
    char loopch = 0;
    if (setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
        LogErr(VB_CHANNELOUT, "Error setting IP_MULTICAST_LOOP error\n");
        close(sendSocket);
        return false;
    }
    
    
    broadcastSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadcastSocket < 0) {
        LogErr(VB_CHANNELOUT, "Error opening datagram socket\n");
        exit(1);
    }
    errno = 0;
    if (bind(broadcastSocket, (struct sockaddr *) &localAddress, sizeof(struct sockaddr_in)) == -1) {
        LogErr(VB_CHANNELOUT, "Error in bind:errno=%d, %s\n", errno, strerror(errno));
    }
    /* Disable loopback so I do not receive my own datagrams. */
    loopch = 0;
    if(setsockopt(broadcastSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
        LogErr(VB_CHANNELOUT, "Error setting IP_MULTICAST_LOOP error\n");
        close(broadcastSocket);
        return 0;
    }
    int broadcast = 1;
    if (setsockopt(broadcastSocket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        LogErr(VB_SYNC, "Error setting SO_BROADCAST: \n", strerror(errno));
        exit(1);
    }
    return true;
}

