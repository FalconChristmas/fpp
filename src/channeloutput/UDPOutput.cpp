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
#include <set>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ifaddrs.h>

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



UDPOutput::UDPOutput(unsigned int startChannel, unsigned int channelCount)
    : pingThread(nullptr), rebuildOutputLists(false)
{
    sendSocket = -1;
}
UDPOutput::~UDPOutput() {
    runDisabledPings = false;
    for (auto a : outputs) {
        delete a;
    }
}

int UDPOutput::Init(Json::Value config) {
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
    
    std::set<std::string> myIps;
    //get all the addresses
    struct ifaddrs *interfaces,*tmp;
    getifaddrs(&interfaces);
    tmp = interfaces;
    //loop through all the interfaces and get the addresses
    while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
            char address[16];
            GetInterfaceAddress(tmp->ifa_name, address, NULL, NULL);
            myIps.emplace(address);
        }
        tmp = tmp->ifa_next;
    }
    freeifaddrs(interfaces);
    
    
    for (auto o : outputs) {
        if (o->IsPingable() && o->active) {
            std::string host = o->ipAddress;
            if (myIps.find(host) != myIps.end()) {
                // trying to send UDP data to myself, that's bad.  Disable
                LogWarn(VB_CHANNELOUT, "UDP Output set to send data to myself.  Disabling - %s\n",
                        host.c_str());
                o->active = false;
            }
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
void  UDPOutput::GetRequiredChannelRange(int &min, int & max) {
    min = FPPD_MAX_CHANNELS;
    max = 0;
    if (enabled) {
        for (auto a : outputs) {
            if (a->active) {
                min = std::min(min, a->startChannel - 1);
                max = std::max(max, (a->startChannel + a->channelCount - 2));
            }
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

int UDPOutput::SendData(unsigned char *channelData) {
    if (rebuildOutputLists) {
        RebuildOutputMessageLists();
    }
    
    if ((udpMsgs.size() == 0 && broadcastMsgs.size() == 0) || !enabled) {
        return 0;
    }
    std::chrono::high_resolution_clock clock;
    auto t1 = clock.now();
    int outputCount = SendMessages(sendSocket, udpMsgs);
    auto t2 = clock.now();
    long diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    if ((outputCount != udpMsgs.size()) || (diff > 100)) {
        //failed to send all messages or it took more than 100ms to send them
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

void DoPingThread(UDPOutput *output) {
    output->BackgroundThreadPing();
}
void UDPOutput::BackgroundThreadPing() {
    while (runDisabledPings) {
        std::this_thread::sleep_for (std::chrono::seconds(10));
        bool newValid = false;
        
        std::map<std::string, int> done;

        std::unique_lock<std::mutex> lck (invalidOutputsMutex);
        for (auto a : invalidOutputs) {
            if (!a->valid) {
                std::string host = a->ipAddress;
                if (done[host] == 0) {
                    int p = ping(host);
                    if (p <= 0) {
                        p = -1;
                    }
                    done[host] = p;
                    if (p > 0) {
                        LogWarn(VB_CHANNELOUT, "Can ping host %s, re-adding to outputs\n",
                                host.c_str());
                    }
                }
                a->valid = done[host] > 0;
            }
            if (a->valid) {
                newValid = true;
            }
        }
        
        if (newValid) {
            //at least one output became valid, let main thread know to rebuild the
            //output message maps
            rebuildOutputLists = true;
        }
    }
    std::thread *t = pingThread;
    pingThread = nullptr;
    delete t;
}
void UDPOutput::PingControllers() {
    LogDebug(VB_CHANNELOUT, "Pinging controllers to see what is online\n");

    std::map<std::string, int> done;
    for (auto o : outputs) {
        if (o->IsPingable() && o->active) {
            std::string host = o->ipAddress;
            if (done[host] == 0) {
                int p = ping(host);
                if (p <= 0) {
                    p = -1;
                }
                done[host] = p;

                if (p > 0 && !o->valid) {
                    LogWarn(VB_CHANNELOUT, "Could ping host %s, re-adding to outputs\n",
                            host.c_str());
                }
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
                
                if (p < 0 && o->valid) {
                    LogWarn(VB_CHANNELOUT, "Could not ping host %s, removing from output\n",
                            host.c_str());
                } else if (p > 0 && !o->valid) {
                    LogWarn(VB_CHANNELOUT, "Could ping host %s, re-adding to outputs\n",
                            host.c_str());
                }
            }
            o->valid = p > 0;
        }
    }
    RebuildOutputMessageLists();
}
void UDPOutput::RebuildOutputMessageLists() {
    LogDebug(VB_CHANNELOUT, "Rebuilding message lists\n");

    rebuildOutputLists = false;
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
    std::unique_lock<std::mutex> lck (invalidOutputsMutex);
    invalidOutputs.clear();
    for (auto a : outputs) {
        if (!a->valid && a->active && a->IsPingable()) {
            //active, but not valid, likely lost the ping
            //save it so we can try re-enabling
            invalidOutputs.push_back(a);
        }
    }

    if (!invalidOutputs.empty() && pingThread == nullptr) {
        runDisabledPings = true;
        pingThread = new std::thread(DoPingThread, this);
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
    /* Disable loopback so I do not receive my own datagrams. */
    char loopch = 0;
    if (setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
        LogErr(VB_CHANNELOUT, "Error setting IP_MULTICAST_LOOP error\n");
        close(sendSocket);
        return false;
    }
    if (bind(sendSocket, (struct sockaddr *) &localAddress, sizeof(struct sockaddr_in)) == -1) {
        LogErr(VB_CHANNELOUT, "Error in bind:errno=%d, %s\n", errno, strerror(errno));
    }
    if (connect(sendSocket, (struct sockaddr *)&localAddress, sizeof(localAddress)) < 0)  {
        LogErr(VB_CHANNELOUT, "Error connecting IP_MULTICAST_LOOP socket\n");
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
    if (connect(broadcastSocket, (struct sockaddr *)&localAddress, sizeof(localAddress)) < 0)  {
        LogErr(VB_CHANNELOUT, "Error connecting IP_MULTICAST_LOOP socket\n");
    }

    return true;
}

