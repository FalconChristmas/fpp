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

#include <curl/curl.h>


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
:  valid(true), type(0), monitor(true), failCount(99), lastData(nullptr), skippedFrames(0) {
    
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
    if (config.isMember("monitor")) {
        monitor = config["monitor"].asInt() ? true : false;
    }
    if (config.isMember("deDuplicate")) {
        deDuplicate = config["deDuplicate"].asInt() ? true : false;
    }
}
UDPOutputData::~UDPOutputData() {
    if (lastData) {
        free(lastData);
    }
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
void UDPOutputData::SaveFrame(unsigned char *channelData) {
    if (deDuplicate) {
        int min = 999999999;
        int max = 0;
        GetRequiredChannelRange(min, max);
        if (max >= min) {
            max -= min;
            if (lastData == nullptr) {
                lastData = (unsigned char *)malloc(max);
            }
            memcpy(lastData, &channelData[min], max);
        }
    }
}

bool UDPOutputData::NeedToOutputFrame(unsigned char *channelData, int startChannel, int start, int count) {
    if (deDuplicate && skippedFrames < 10) {
        if (lastData == nullptr) {
            return true;
        }
        for (int x = 0; x < count; x++) {
            if (channelData[x + start + startChannel] != lastData[x + start]) {
                return true;
            }
        }
        return false;
    }
    skippedFrames = 0;
    return true;
}

UDPOutput::UDPOutput(unsigned int startChannel, unsigned int channelCount)
    : pingThread(nullptr), runPingThread(true),
      errCount(0), networkCallbackId(0), sendIdx(-1)
{
    INSTANCE = this;
    broadcastSocket = -1;
    m_curlm = curl_multi_init();
}
UDPOutput::~UDPOutput() {
    INSTANCE = nullptr;
    runPingThread = false;
    pingThreadCondition.notify_all();
    if (pingThread) {
        pingThread->join();
        delete pingThread;
        pingThread = nullptr;
    }
    NetworkMonitor::INSTANCE.removeCallback(networkCallbackId);
    for (auto a : outputs) {
        delete a;
    }
    curl_multi_cleanup(m_curlm);
    m_curlm = nullptr;
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
            LogInfo(VB_CHANNELOUT, "UDP Interface %s now up\n", s.c_str());
            PingControllers();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            InitNetwork();
        } else if (s == interface && i == NetworkMonitor::NetEventType::DEL_ADDR) {
            LogInfo(VB_CHANNELOUT, "UDP Interface %s now down\n", s.c_str());
            CloseNetwork();
        }
    };
    networkCallbackId = NetworkMonitor::INSTANCE.registerCallback(f);

    InitNetwork();
    PingControllers();
    pingThread = new std::thread(DoPingThread, this);
    return ChannelOutputBase::Init(config);
}
int UDPOutput::Close() {
    runPingThread = false;
    pingThreadCondition.notify_all();
    if (pingThread) {
        pingThread->join();
        delete pingThread;
        pingThread = nullptr;
    }
    NetworkMonitor::INSTANCE.removeCallback(networkCallbackId);
    return ChannelOutputBase::Close();
}
void UDPOutput::PrepData(unsigned char *channelData) {
    if (enabled) {
        udpMsgs.clear();
        broadcastMsgs.clear();
        for (auto a : outputs) {
            if (a->valid && a->active) {
                a->PrepareData(channelData, udpMsgs, broadcastMsgs);
            }
        }
        //add any sync packets or whatever that are needed
        for (auto a : outputs) {
            if (a->valid && a->active) {
                a->PostPrepareData(channelData, udpMsgs, broadcastMsgs);
            }
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
}


int UDPOutput::SendMessages(int socket, std::vector<struct mmsghdr> &sendmsgs) {
    errno = 0;
    struct mmsghdr *msgs = &sendmsgs[0];
    int msgCount = sendmsgs.size();
    if (msgCount == 0) {
        return 0;
    }
    
    if (sendIdx == -1) {
        return msgCount;
    }
    
    errno = 0;
    int oc = sendmmsg(socket, msgs, msgCount, MSG_DONTWAIT);
    int outputCount = oc;
    int newSock = socket;
    while (outputCount != msgCount) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (socket != broadcastSocket) {
                ++sendIdx;
                if (sendIdx == 6) {
                    sendIdx = 0;
                }
                newSock = sendSockets[sendIdx];
                if (newSock == socket) {
                    return outputCount;
                }
            } else {
                return outputCount;
            }
        }
        errno = 0;
        int oc = sendmmsg(newSock, &msgs[outputCount], msgCount - outputCount, MSG_DONTWAIT);
        if (oc > 0) {
            outputCount += oc;
        }
    }
    return outputCount;
}

int UDPOutput::SendData(unsigned char *channelData) {
    
    if ((udpMsgs.size() == 0 && broadcastMsgs.size() == 0) || !enabled || sendIdx == -1) {
        return 0;
    }
    isSending = true;
    if (udpMsgs.size() > 0) {
        std::chrono::high_resolution_clock clock;
        auto t1 = clock.now();
        int outputCount = SendMessages(sendSockets[sendIdx], udpMsgs);
        ++sendIdx;
        if (sendIdx == 6) {
            sendIdx = 0;
        }
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
        PingControllers();
        pingThreadCondition.wait_for(lk, std::chrono::seconds(10));
    }
}
bool UDPOutput::PingControllers() {
    LogExcess(VB_CHANNELOUT, "Pinging controllers to see what is online\n");

    std::map<std::string, int> done;
    std::map<std::string, CURL*> curls;
    bool newOutputs = false;
    for (auto o : outputs) {
        if (o->IsPingable() && o->Monitor() && o->active) {
            std::string host = o->ipAddress;
            if (done[host] == 0) {
                int p = ping(host);
                if (p <= 0) {
                    p = -1;
                }
                done[host] = p;

                if (p < 0) {
                    // ping failed, try GET
                    std::string url = "http://" + host;
                    CURL *curl = curl_easy_init();
                    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 3000);
                    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 6000);
                    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
                    curl_multi_add_handle(m_curlm, curl);
                    curls[host] = curl;
                }
            }
        }
    }
    int numCurls = curls.size();
    while (numCurls) {
        int handleCount;
        curl_multi_perform(m_curlm, &handleCount);
        if (handleCount != numCurls) {
            //progress
            int msgs;
            CURLMsg * curlm = curl_multi_info_read(m_curlm, &msgs);
            while (curlm) {
                if (curlm->msg == CURLMSG_DONE && curlm->data.result == CURLE_OK) {
                    for (auto &c : curls) {
                        if (c.second == curlm->easy_handle) {
                            // the HEAD request worked, mark as OK
                            done[c.first] = 1;
                        }
                    }
                }
                curlm = curl_multi_info_read(m_curlm, &msgs);
            }
        }
        numCurls = handleCount;
    }
    for (auto &c : curls) {
        curl_multi_remove_handle(m_curlm, c.second);
        curl_easy_cleanup(c.second);
    }
    for (auto o : outputs) {
        if (o->IsPingable() && o->Monitor() && o->active) {
            std::string host = o->ipAddress;
            int p = done[host];
            if (p == -1 && o->valid) {
                //give a second chance before completely marking invalid
                p = ping(host, 1000);
                if (p <= 0) {
                    p = -2;
                }
                done[host] = p;
            }
            
            if (p > 0 && !o->valid) {
                WarningHolder::RemoveWarning(createWarning(host, o->GetOutputTypeString()));
                LogWarn(VB_CHANNELOUT, "Could ping host %s, re-adding to outputs\n",
                        host.c_str());
                newOutputs = true;
                o->failCount = 0;
                o->valid = true;
            } else if (p < 0 && o->valid) {
                if (o->failCount < 1) {
                    o->failCount++;
                } else {
                    WarningHolder::AddWarning(createWarning(host, o->GetOutputTypeString()));
                    LogWarn(VB_CHANNELOUT, "Could not ping host %s, removing from output\n",
                            host.c_str());
                    newOutputs = true;
                    o->valid = false;
                }
            }

        }
    }
    return newOutputs;
}
void UDPOutput::DumpConfig() {
    ChannelOutputBase::DumpConfig();
    for (auto u : outputs) {
        u->DumpConfig();
    }
}
void UDPOutput::CloseNetwork() {
    int bs = broadcastSocket;
        
    sendIdx = -1;
    broadcastSocket = -1;
    //if other thread is sending data, give it time to complete
    while (isSending) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    for (int x = 0; x < 6; x++) {
        close(sendSockets[x]);
    }
    close(bs);

    PingControllers();
}

bool UDPOutput::InitNetwork() {
    if (sendIdx != -1) {
        return true;
    }
    char E131LocalAddress[16];
    GetInterfaceAddress(getE131interface(), E131LocalAddress, NULL, NULL);
    LogDebug(VB_CHANNELOUT, "UDPLocalAddress = %s\n",E131LocalAddress);

    if (strcmp(E131LocalAddress, "127.0.0.1") == 0) {
        return false;
    }
    static struct sockaddr_in   localAddress;
    memset(&localAddress, 0, sizeof(struct sockaddr_in));
    localAddress.sin_family = AF_INET;
    localAddress.sin_port = ntohs(0);
    localAddress.sin_addr.s_addr = inet_addr(E131LocalAddress);

    for (int x = 0; x < 6; x++) {
        int sendSocket = socket(AF_INET, SOCK_DGRAM, 0);

        if (sendSocket < 0) {
            LogErr(VB_CHANNELOUT, "Error opening datagram socket\n");
            exit(1);
        }

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
        sendSockets[x] = sendSocket;
    }
    sendIdx = 0;
    
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
    char loopch = 0;
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
    this->broadcastSocket = broadcastSocket;
    return true;
}

