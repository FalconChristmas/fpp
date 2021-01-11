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
#include "fpp-pch.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

#include <netdb.h>

#include <curl/curl.h>

#include "UDPOutput.h"
#include "ping.h"

#include "NetworkMonitor.h"

#include "E131.h"
#include "DDP.h"
#include "ArtNet.h"
#include "KiNet.h"

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

class SendSocketInfo {
public:
    SendSocketInfo() {
        errCount = 0;
        curSocket = -1;
    }
    ~SendSocketInfo() {
        for (int x : sockets) {
            close(x);
        }
    }

    std::vector<int> sockets;
    int errCount;
    int curSocket;
};

UDPOutputMessages::UDPOutputMessages() {
}
UDPOutputMessages::~UDPOutputMessages() {
}
int UDPOutputMessages::GetSocket(unsigned int key) {
    SendSocketInfo *info = sendSockets[key];
    if (info && !info->sockets.empty()) {
        return info->sockets[0];
    }
    return -1;
}
void UDPOutputMessages::ForceSocket(unsigned int key, int socket) {
    SendSocketInfo *info = sendSockets[key];
    if (info == nullptr) {
        info = new SendSocketInfo();
        sendSockets[key] = info;
    }
    for (int x = 0; x < info->sockets.size(); x++) {
        close(info->sockets[x]);
    }
    info->sockets.clear();
    info->sockets.push_back(socket);
}
std::vector<struct mmsghdr> & UDPOutputMessages::GetMessages(unsigned int key) {
    return messages[key];
}
void UDPOutputMessages::clearMessages() {
    for (auto &m : messages) {
        m.second.clear();
    }
}
void UDPOutputMessages::clearSockets() {
    for (auto &si : sendSockets) {
        delete si.second;
    }
    sendSockets.clear();
}




UDPOutput* UDPOutput::INSTANCE = nullptr;

UDPOutputData::UDPOutputData(const Json::Value &config)
:  valid(true), type(0), monitor(true), failCount(0), lastData(nullptr), skippedFrames(0) {
    
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

void UDPOutputData::SaveFrame(unsigned char *channelData, int len) {
    if (deDuplicate) {
        if (lastData == nullptr) {
            lastData = (unsigned char *)calloc(1, len);
        }
        /*
        printf("Saving: %d   %2X%2X%2X %2X%2X%2X %2X%2X%2X %2X%2X%2X\n", len,
               channelData[0], channelData[1], channelData[2], channelData[3], channelData[4], channelData[5],
               channelData[6], channelData[7], channelData[8], channelData[9], channelData[10], channelData[11]);
         */
        memcpy(lastData, channelData, len);
    }
}

bool UDPOutputData::NeedToOutputFrame(unsigned char *channelData, int startChannel, int savedIdx, int count) {
    if (deDuplicate && skippedFrames < 10) {
        if (lastData == nullptr) {
            return true;
        }
        for (int x = 0; x < count; x++) {
            if (channelData[x + savedIdx + startChannel] != lastData[x + savedIdx]) {
                /*
                printf("ND: %d   %2X%2X%2X %2X%2X%2X %2X%2X%2X %2X%2X%2X\n", x,
                       lastData[0], lastData[1], lastData[2], lastData[3], lastData[4], lastData[5],
                       lastData[6], lastData[7], lastData[8], lastData[9], lastData[10], lastData[11]);
                printf("%d %d    %2X%2X%2X %2X%2X%2X %2X%2X%2X %2X%2X%2X\n", start, startChannel, x,
                       channelData[start + startChannel + 0], channelData[start + startChannel + 1], channelData[start + startChannel + 2],
                       channelData[start + startChannel + 3], channelData[start + startChannel + 4], channelData[start + startChannel + 5],
                       channelData[start + startChannel + 6], channelData[start + startChannel + 7], channelData[start + startChannel + 8],
                       channelData[start + startChannel + 9], channelData[start + startChannel + 10], channelData[start + startChannel + 11]);
                 printf("New data %d  %d  %d      %X %X\n", x, startChannel, savedIdx, channelData[x + savedIdx + startChannel], lastData[x + savedIdx]);
                 */
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
      networkCallbackId(0), doneWorkCount(0), numWorkThreads(0), runWorkThreads(true), useThreadedOutput(true)
{
    INSTANCE = this;
    m_curlm = curl_multi_init();
}
UDPOutput::~UDPOutput() {
    runWorkThreads = false;
    workSignal.notify_all();
    
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
    
    while (numWorkThreads) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
            case 6:
            case 7:
                //KiNet types
                outputs.push_back(new KiNetOutputData(s));
                break;
            default:
                LogErr(VB_CHANNELOUT, "Unknown IP output type %d\n", type);
                break;
        }
        
    }
    
    if (config.isMember("threaded")) {
        useThreadedOutput = config["threaded"].asInt() ? true : false;
    }
    if (config.isMember("interface")) {
        e131Interface = config["interface"].asString();
    }
    if (e131Interface == "") {
        e131Interface = getE131interface();
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
        std::string interface = e131Interface;
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
    // need to do three pings to detect down hosts
    PingControllers();
    PingControllers(true);
    PingControllers(true);
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
        std::unique_lock<std::mutex> lk(socketMutex);
        messages.clearMessages();
        for (auto a : outputs) {
            if (a->valid && a->active) {
                a->PrepareData(channelData, messages);
            }
        }
        //add any sync packets or whatever that are needed
        for (auto a : outputs) {
            if (a->valid && a->active) {
                a->PostPrepareData(channelData, messages);
            }
        }
    }
}
void UDPOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
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


int UDPOutput::SendMessages(unsigned int socketKey, SendSocketInfo *socketInfo, std::vector<struct mmsghdr> &sendmsgs) {
    errno = 0;
    struct mmsghdr *msgs = &sendmsgs[0];
    int msgCount = sendmsgs.size();
    if (msgCount == 0) {
        return 0;
    }

    int newSockKey = socketKey;
    int sendSocket = socketInfo->sockets[socketInfo->curSocket];
    ++socketInfo->curSocket;
    if (socketInfo->curSocket == socketInfo->sockets.size()) {
        socketInfo->curSocket = 0;
    }

    errno = 0;
    int oc = sendmmsg(sendSocket, msgs, msgCount, MSG_DONTWAIT);
    int outputCount = 0;
    if (oc > 0) {
        outputCount = oc;
    }

    int errCount = 0;
    while (outputCount != msgCount) {
        LogErr(VB_CHANNELOUT, "sendmmsg() failed for UDP output (key: %X   Socket: %d   output count: %d/%d) with error: %d   %s\n",
               socketKey, sendSocket,
               outputCount, msgCount,
               errno,
               strerror(errno));

        
        int newSock = sendSocket;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (socketKey != BROADCAST_MESSAGES_KEY) {
                ++socketInfo->curSocket;
                if (socketInfo->curSocket == socketInfo->sockets.size()) {
                    socketInfo->curSocket = 0;
                }
                newSock = socketInfo->sockets[socketInfo->curSocket];
                if (newSock == -1) {
                    socketInfo->sockets[socketInfo->curSocket] = createSocket();
                    newSock = socketInfo->sockets[socketInfo->curSocket];
                }
            } else {
                return outputCount;
            }
        }
        ++errCount;
        if (errCount >= 10) {
            return outputCount;
        }
        errno = 0;
        int oc = sendmmsg(newSock, &msgs[outputCount], msgCount - outputCount, MSG_DONTWAIT);
        if (oc > 0) {
            outputCount += oc;
        }
    }
    return outputCount;
}


static void DoWorkThread(UDPOutput *output) {
    output->BackgroundOutputWork();
}

void UDPOutput::BackgroundOutputWork() {
    std::chrono::high_resolution_clock clock;
    while (runWorkThreads) {
        std::unique_lock<std::mutex> lock(workMutex);
        if (workQueue.empty()) {
            workSignal.wait(lock);
        }
        if (!workQueue.empty()) {
            WorkItem i = workQueue.front();
            workQueue.pop_front();
            lock.unlock();
            
            auto t1 = clock.now();
            int outputCount = SendMessages(i.id, i.socketInfo, i.msgs);
            auto t2 = clock.now();
            
            long diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
            if ((outputCount != i.msgs.size()) || (diff > 100)) {
                i.socketInfo->errCount++;
                
                //failed to send all messages or it took more than 100ms to send them
                LogErr(VB_CHANNELOUT, "sendmmsg() failed for UDP output (key: %X   output count: %d/%d   time: %u ms    errCount: %d) with error: %d   %s\n",
                       i.id,
                       outputCount, i.msgs.size(), diff, i.socketInfo->errCount,
                       errno,
                       strerror(errno));
            } else {
                i.socketInfo->errCount = 0;
            }

            doneWorkCount++;
        }
    }
    
    --numWorkThreads;
}



int UDPOutput::SendData(unsigned char *channelData) {
    std::unique_lock<std::mutex> lk(socketMutex);
    if (!enabled || messages.sendSockets.empty()) {
        return 0;
    }
    std::chrono::high_resolution_clock clock;
    if (useThreadedOutput) {
        doneWorkCount = 0;
        int total = 0;
        auto t1 = clock.now();
        for (auto &msgs : messages.messages) {
            if (!msgs.second.empty() && msgs.first < LATE_MULTICAST_MESSAGES_KEY) {
                SendSocketInfo *socketInfo = findOrCreateSocket(msgs.first, 5);

                std::unique_lock<std::mutex> lock(workMutex);
                workQueue.push_back(WorkItem(msgs.first, socketInfo, msgs.second));
                lock.unlock();
                workSignal.notify_one();
                ++total;
            }
        }
        std::unique_lock<std::mutex> lock(workMutex);
        while (numWorkThreads < workQueue.size()) {
            std::thread (DoWorkThread, this).detach();
            numWorkThreads++;
        }
        if (workQueue.size()) {
            workSignal.notify_all();
        }
        lock.unlock();
        auto t2 = clock.now();
        while (doneWorkCount != total && std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() < 50) {
            std::this_thread::sleep_for(std::chrono::microseconds(250));
            t2 = clock.now();
        }
        if (doneWorkCount == total) {
            //now output the LATE/Broadcast packets (likely sync packets)
            for (auto &msgs : messages.messages) {
                if (!msgs.second.empty()) {
                    SendSocketInfo *socketInfo = findOrCreateSocket(msgs.first, 5);
                    if (msgs.first >= LATE_MULTICAST_MESSAGES_KEY) {
                        t1 = clock.now();
                        int outputCount = SendMessages(msgs.first, socketInfo, msgs.second);
                        t2 = clock.now();
                        long diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
                        if ((outputCount != msgs.second.size()) || (diff > 100)) {
                            socketInfo->errCount++;
                            
                            //failed to send all messages or it took more than 100ms to send them
                            LogErr(VB_CHANNELOUT, "sendmmsg() failed for UDP output (key: %X   output count: %d/%d   time: %u ms    errCount: %d) with error: %d   %s\n",
                                   msgs.first,
                                   outputCount, msgs.second.size(), diff, socketInfo->errCount,
                                   errno,
                                   strerror(errno));
                        } else {
                            socketInfo->errCount = 0;
                        }
                    }
                    if (socketInfo->errCount >= 3) {
                        //we'll ping the controllers and rebuild the valid message list, this could take time
                        pingThreadCondition.notify_all();
                        socketInfo->errCount = 0;
                    }
                }
            }
        }
        return 1;
    }
    for (auto &msgs : messages.messages) {
        if (!msgs.second.empty()) {
            SendSocketInfo *socketInfo = findOrCreateSocket(msgs.first, 5);
            auto t1 = clock.now();
            int outputCount = SendMessages(msgs.first, socketInfo, msgs.second);
            auto t2 = clock.now();
            long diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
            if ((outputCount != msgs.second.size()) || (diff > 100)) {
                socketInfo->errCount++;
                
                //failed to send all messages or it took more than 100ms to send them
                LogErr(VB_CHANNELOUT, "sendmmsg() failed for UDP output (key: %X   output count: %d/%d   time: %u ms    errCount: %d) with error: %d   %s\n",
                       msgs.first,
                       outputCount, msgs.second.size(), diff, socketInfo->errCount,
                       errno,
                       strerror(errno));
                    
                if (socketInfo->errCount >= 3) {
                    //we'll ping the controllers and rebuild the valid message list, this could take time
                    pingThreadCondition.notify_all();
                    socketInfo->errCount = 0;
                }
            } else {
                socketInfo->errCount = 0;
            }
        }
    }
    return 1;
}

void UDPOutput::BackgroundThreadPing() {
    std::unique_lock<std::mutex> lk(pingThreadMutex);
    pingThreadCondition.wait_for(lk, std::chrono::seconds(10));
    while (runPingThread) {
        PingControllers();
        pingThreadCondition.wait_for(lk, std::chrono::seconds(15));
    }
}
bool UDPOutput::PingControllers(bool failedOnly) {
    LogExcess(VB_CHANNELOUT, "Pinging controllers to see what is online\n");

    std::map<std::string, int> done;
    std::map<std::string, CURL*> curls;
    bool newOutputs = false;
    for (auto o : outputs) {
        if (o->IsPingable() && o->Monitor() && o->active) {
            if (failedOnly && o->failCount == 0) {
                continue;
            }
            std::string host = o->ipAddress;
            int p = done[host];
            if (p == 0) {
                int timeout = 250;
                if (o->failCount == 1) {
                    timeout = 500;
                } else if (o->failCount == 2) {
                    timeout = 1000;
                }
                p = ping(host, timeout);
                if (p <= 0) {
                    p = -1;
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
            } else if (p <= 0) {
                o->failCount++;
                LogDebug(VB_CHANNELOUT, "Could not ping host %s   Fail count: %d   Currently Valid: %d\n", host.c_str(), o->failCount, o->valid);
                if (o->valid) {
                    if (o->failCount == 2) {
                        // two pings failed, lets try an HTTP HEAD request
                        if (curls[host] == nullptr) {
                            std::string url = "http://" + host;
                            CURL *curl = curl_easy_init();
                            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 2000);
                            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000);
                            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
                            curl_multi_add_handle(m_curlm, curl);
                            curls[host] = curl;
                        }
                    } else if (o->failCount == 3) {
                        // two shorter pings, a HEAD request, and one long ping failed
                        // must not be valid anymore
                        WarningHolder::AddWarning(createWarning(host, o->GetOutputTypeString()));
                        LogWarn(VB_CHANNELOUT, "Could not ping host %s, removing from output\n",
                                host.c_str());
                        newOutputs = true;
                        o->valid = false;
                    }
                } else if (o->failCount > 4) {
                    // make sure we wrap around so another HEAD request later may pick it up
                    o->failCount = 0;
                }
            }
        }
    }
    
    int numCurls = curls.size();
    if (numCurls) {
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
                                for (auto o : outputs) {
                                    if (o->IsPingable() && o->Monitor() && o->active && (o->ipAddress == c.first)) {
                                        o->failCount = 0;
                                        if (!o->valid) {
                                            o->valid = true;
                                            newOutputs = true;
                                            WarningHolder::RemoveWarning(createWarning(o->ipAddress, o->GetOutputTypeString()));
                                            LogWarn(VB_CHANNELOUT, "Could ping host %s, re-adding to outputs\n",
                                                    o->ipAddress.c_str());
                                        }
                                    }
                                }
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
    std::unique_lock<std::mutex> lk(socketMutex);
    messages.clearSockets();
    lk.unlock();
    PingControllers();
}
SendSocketInfo* UDPOutput::findOrCreateSocket(unsigned int socketKey, int sc) {
    auto it = messages.sendSockets.find(socketKey);
    SendSocketInfo *info;
    if (it == messages.sendSockets.end()) {
        info = new SendSocketInfo();
        messages.sendSockets[socketKey] = info;
    } else {
        info = it->second;
    }
    
    if (info->sockets.empty()) {
        for (int x = info->sockets.size(); x < sc; x++) {
            int s = createSocket();
            info->sockets.push_back(s);
        }
    }
    if (info->curSocket == -1) {
        info->curSocket = 0;
    }
    return info;
}

int UDPOutput::createSocket(int port, bool broadCast) {
    int sendSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (sendSocket < 0) {
        LogErr(VB_CHANNELOUT, "Error opening datagram socket\n");
        exit(1);
    }
    
    static struct sockaddr_in address;
    memcpy(&address, &localAddress, sizeof(localAddress));
    address.sin_port = ntohs(port);

    errno = 0;
    /* Disable loopback so I do not receive my own datagrams. */
    char loopch = 0;
    if (setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
        LogErr(VB_CHANNELOUT, "Error setting IP_MULTICAST_LOOP error\n");
        close(sendSocket);
        return -1;
    }
    if (broadCast) {
        int broadcast = 1;
        if (setsockopt(sendSocket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
            LogErr(VB_SYNC, "Error setting SO_BROADCAST: \n", strerror(errno));
            exit(1);
        }
    } else {
        if (setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_IF, (char *)&address, sizeof(address)) < 0) {
            LogErr(VB_CHANNELOUT, "Error setting IP_MULTICAST_IF error\n");
            close(sendSocket);
            return -1;
        }
    }
    if (bind(sendSocket, (struct sockaddr *) &address, sizeof(struct sockaddr_in)) == -1) {
        LogErr(VB_CHANNELOUT, "Error in bind:errno=%d, %s\n", errno, strerror(errno));
    }
    if (connect(sendSocket, (struct sockaddr *)&address, sizeof(address)) < 0)  {
        LogErr(VB_CHANNELOUT, "Error connecting IP_MULTICAST_LOOP socket\n");
    }
    return sendSocket;
}

bool UDPOutput::InitNetwork() {
    if (!messages.sendSockets.empty()) {
        return true;
    }
    
    
    
    char E131LocalAddress[16];
    GetInterfaceAddress(e131Interface.c_str(), E131LocalAddress, NULL, NULL);
    LogDebug(VB_CHANNELOUT, "UDPLocalAddress = %s\n",E131LocalAddress);

    if (strcmp(E131LocalAddress, "127.0.0.1") == 0) {
        return -1;
    }
    memset(&localAddress, 0, sizeof(struct sockaddr_in));
    localAddress.sin_family = AF_INET;
    localAddress.sin_addr.s_addr = inet_addr(E131LocalAddress);

    int broadcastSocket = createSocket(0, true);
    messages.ForceSocket(BROADCAST_MESSAGES_KEY, broadcastSocket);
    return true;
}

