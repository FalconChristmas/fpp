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

#include <netdb.h>

#include "UDPOutput.h"
#include "Warnings.h"
#include "log.h"
#include "ping.h"

#include "common.h"
#include "settings.h"
#include "NetworkMonitor.h"

#include "E131.h"
#include "DDP.h"
#include "ArtNet.h"

extern "C" {
    UDPOutput *createOutputUDPOutput(unsigned int startChannel,
                               unsigned int channelCount) {
        return new UDPOutput(startChannel, channelCount);
    }
}


static inline std::string createWarning(const std::string &host, const std::string &type) {
    return "Cannot Ping " + type + " Channel Data Target " + host;
}

static void DoPingThread(UDPOutput *output) {
    output->BackgroundThreadPing();
}


UDPOutput* UDPOutput::INSTANCE = nullptr;

UDPOutputData::UDPOutputData(const Json::Value &config)
:  valid(true), type(0) {
    
    if (config.isMember("description")) {
        description = config["description"].asString();
    }
    startChannel = config["startChannel"].asInt();
    channelCount = config["channelCount"].asInt();
    if (config.isMember("active")) {
        active = config["active"].asInt();
    } else {
        active = 1;
    }
    if (config.isMember("type")) {
        type = config["type"].asInt();
    }
    if (config.isMember("address")) {
        ipAddress = config["address"].asString();
    }
}
UDPOutputData::~UDPOutputData() {
}

static const std::string UNKNOWN_TYPE = "UDP";
const std::string &UDPOutputData::GetOutputTypeString() const {
    return UNKNOWN_TYPE;
}


in_addr_t UDPOutputData::toInetAddr(const std::string &ipAddress, bool &valid) {
    valid = true;
    bool isAlpha = false;
    for (int x = 0; x < ipAddress.length(); x++) {
        isAlpha |= isalpha(ipAddress[x]);
    }
    
    if (isAlpha) {
        struct hostent* uhost = gethostbyname(ipAddress.c_str());
        if (!uhost) {
            LogErr(VB_CHANNELOUT,
                   "Error looking up hostname: %s\n",
                   ipAddress.c_str());
            valid = false;
            return 0;
        } else {
            return *((unsigned long*)uhost->h_addr);
        }
    }
    return inet_addr(ipAddress.c_str());
}


UDPOutput::UDPOutput(unsigned int startChannel, unsigned int channelCount)
    : pingThread(nullptr), runPingThread(true), rebuildOutputLists(false), errCount(0)
{
    INSTANCE = this;
    sendSocket = -1;
    broadcastSocket = -1;
}
UDPOutput::~UDPOutput() {
    INSTANCE = nullptr;
    runPingThread = false;
    pingThreadCondition.notify_all();
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
    char address[16];
    memset(address, 0, sizeof(address));
    while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
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
    
    
    std::function<void(NetworkMonitor::NetEventType, int, const std::string &)> f = [this](NetworkMonitor::NetEventType i, int up, const std::string &s) {
        std::string interface = getE131interface();
        if (s == interface && i == NetworkMonitor::NetEventType::NEW_ADDR && up) {
            LogInfo(VB_CHANNELOUT, "UDP Interface now up\n");
            PingControllers();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            InitNetwork();
            rebuildOutputLists = true;
        } else if (s == interface && i == NetworkMonitor::NetEventType::DEL_ADDR) {
            LogInfo(VB_CHANNELOUT, "UDP Interface now down\n");
            CloseNetwork();
        }
    };
    NetworkMonitor::INSTANCE.registerCallback(f);

    
    InitNetwork();
    PingControllers();
    rebuildOutputLists = true;
    pingThread = new std::thread(DoPingThread, this);
    return ChannelOutputBase::Init(config);
}
int  UDPOutput::Close() {
    runPingThread = false;
    pingThreadCondition.notify_all();
    return ChannelOutputBase::Close();
}
void UDPOutput::PrepData(unsigned char *channelData) {
    if (enabled) {
        if (rebuildOutputLists) {
            RebuildOutputMessageLists();
        }
        for (auto a : outputs) {
            a->PrepareData(channelData);
        }
    }
}
void  UDPOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    if (enabled) {
        for (auto a : outputs) {
            if (a->active) {
                int mi, mx;
                a->GetRequiredChannelRange(mi, mx);
                addRange(mi, mx);
            }
        }
    }
}

void UDPOutput::addOutput(UDPOutputData* out) {
    outputs.push_back(out);
    rebuildOutputLists = true;
}


int UDPOutput::SendMessages(int socket, std::vector<struct mmsghdr> &sendmsgs) {
    errno = 0;
    struct mmsghdr *msgs = &sendmsgs[0];
    int msgCount = sendmsgs.size();
    if (msgCount == 0) {
        return 0;
    }
    
    if (sendSocket == -1) {
        return msgCount;
    }
    
    errno = 0;
    int oc = sendmmsg(socket, msgs, msgCount, MSG_DONTWAIT);
    int outputCount = oc;
    while (outputCount != msgCount) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return outputCount;
        }
        if (outputCount != msgCount) {
            errno = 0;
            int oc = sendmmsg(socket, &msgs[outputCount], msgCount - outputCount, MSG_DONTWAIT);
            if (oc > 0) {
                outputCount += oc;
            }
        }
    }
    return outputCount;
}

int UDPOutput::SendData(unsigned char *channelData) {
    
    if ((udpMsgs.size() == 0 && broadcastMsgs.size() == 0) || !enabled) {
        return 0;
    }
    isSending = true;
    if (udpMsgs.size() > 0) {
        std::chrono::high_resolution_clock clock;
        auto t1 = clock.now();
        int outputCount = SendMessages(sendSocket, udpMsgs);
        auto t2 = clock.now();
        long diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        if ((outputCount != udpMsgs.size()) || (diff > 100)) {
            errCount++;
            
            //failed to send all messages or it took more than 100ms to send them
            LogErr(VB_CHANNELOUT, "sendmmsg() failed for UDP output (output count: %d/%d   time: %u ms    errCount: %d) with error: %d   %s\n",
                   outputCount, udpMsgs.size(), diff, errCount,
                   errno,
                   strerror(errno));
                
            if (errCount >= 3) {
                //we'll ping the controllers and rebuild the valid message list, this could take time
                pingThreadCondition.notify_all();
                errCount = 0;
            }
            return 0;
        } else {
            errCount = 0;
        }
    }
    SendMessages(broadcastSocket, broadcastMsgs);
    isSending = false;
    return 1;
}

void UDPOutput::BackgroundThreadPing() {
    std::unique_lock<std::mutex> lk(pingThreadMutex);
    pingThreadCondition.wait_for(lk, std::chrono::seconds(10));
    while (runPingThread) {
        bool newValid = PingControllers();
        if (newValid) {
            //at least one output became valid or invalid, let main thread know to rebuild the
            //output message maps
            rebuildOutputLists = true;
        }
        pingThreadCondition.wait_for(lk, std::chrono::seconds(10));
    }
    std::thread *t = pingThread;
    pingThread = nullptr;
    delete t;
}
bool UDPOutput::PingControllers() {
    LogExcess(VB_CHANNELOUT, "Pinging controllers to see what is online\n");

    std::map<std::string, int> done;
    bool newOutputs = false;
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
                    WarningHolder::RemoveWarning(createWarning(host, o->GetOutputTypeString()));
                    LogWarn(VB_CHANNELOUT, "Could ping host %s, re-adding to outputs\n",
                            host.c_str());
                    newOutputs = true;
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
                    WarningHolder::AddWarning(createWarning(host, o->GetOutputTypeString()));
                    LogWarn(VB_CHANNELOUT, "Could not ping host %s, removing from output\n",
                            host.c_str());
                    newOutputs = true;
                } else if (p > 0 && !o->valid) {
                    WarningHolder::RemoveWarning(createWarning(host, o->GetOutputTypeString()));
                    LogWarn(VB_CHANNELOUT, "Could ping host %s, re-adding to outputs\n",
                            host.c_str());
                    newOutputs = true;
                }
            }
            o->valid = p > 0;
        }
    }
    return newOutputs;
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
}
void UDPOutput::DumpConfig() {
    ChannelOutputBase::DumpConfig();
    for (auto u : outputs) {
        u->DumpConfig();
    }
}
void UDPOutput::CloseNetwork() {
    int ts = sendSocket;
    int bs = broadcastSocket;
        
    sendSocket = -1;
    broadcastSocket = -1;
    //if other thread is sending data, give it time to compete
    while (isSending) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    close(sendSocket);
    close(broadcastSocket);

    PingControllers();
    rebuildOutputLists = true;
}

bool UDPOutput::InitNetwork() {
    if (sendSocket != -1) {
        return true;
    }
    char E131LocalAddress[16];
    GetInterfaceAddress(getE131interface(), E131LocalAddress, NULL, NULL);
    LogDebug(VB_CHANNELOUT, "UDPLocalAddress = %s\n",E131LocalAddress);

    if (strcmp(E131LocalAddress, "127.0.0.1") == 0) {
        return false;
    }


    int sendSocket = socket(AF_INET, SOCK_DGRAM, 0);

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
    
    if (setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localAddress, sizeof(localAddress)) < 0) {
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
    
    int broadcastSocket = socket(AF_INET, SOCK_DGRAM, 0);
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

    this->sendSocket = sendSocket;
    this->broadcastSocket = broadcastSocket;
    return true;
}

