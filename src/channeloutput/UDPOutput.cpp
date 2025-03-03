/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "fpp-pch.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <unistd.h>

#include <curl/curl.h>
#include <set>

#include "../CurlManager.h"
#include "../Warnings.h"
#include "../common.h"
#include "../log.h"
#include "../settings.h"

#include "UDPOutput.h"
#include "ping.h"

#include "NetworkMonitor.h"

#include "ArtNet.h"
#include "DDP.h"
#include "E131.h"
#include "KiNet.h"
#include "Twinkly.h"
#include "../e131bridge.h"

#include "Plugin.h"

constexpr int UDP_PING_TIMEOUT = 250;

class UDPPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    UDPPlugin() :
        FPPPlugins::Plugin("UDP") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new UDPOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new UDPPlugin();
}
}

static inline std::string createWarning(const std::string& host, const std::string& type, const std::string& description) {
    return "Cannot Ping " + type + " Channel Data Target " + host + " " + description;
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
    for (auto& si : sendSockets) {
        delete si.second;
    }
    sendSockets.clear();
}
int UDPOutputMessages::GetSocket(unsigned int key) {
    SendSocketInfo* info = sendSockets[key];
    if (info && !info->sockets.empty()) {
        return info->sockets[0];
    }
    return -1;
}
void UDPOutputMessages::ForceSocket(unsigned int key, int socket) {
    SendSocketInfo* info = sendSockets[key];
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
std::vector<struct mmsghdr>& UDPOutputMessages::GetMessages(unsigned int key) {
    return messages[key];
}
void UDPOutputMessages::clearMessages() {
    for (auto& m : messages) {
        m.second.clear();
    }
}
void UDPOutputMessages::clearSockets() {
    for (auto& si : sendSockets) {
        delete si.second;
    }
    sendSockets.clear();
}

UDPOutput* UDPOutput::INSTANCE = nullptr;

UDPOutputData::UDPOutputData(const Json::Value& config) :
    valid(true),
    type(0),
    monitor(true),
    failCount(0),
    lastData(nullptr),
    skippedFrames(0) {
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
const std::string& UDPOutputData::GetOutputTypeString() const {
    return UNKNOWN_TYPE;
}

in_addr_t UDPOutputData::toInetAddr(const std::string& ipAddress, bool& valid) {
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

void UDPOutputData::SaveFrame(unsigned char* channelData, int len) {
    if (deDuplicate) {
        if (lastData == nullptr) {
            lastData = (unsigned char*)calloc(1, len);
        }
        /*
        printf("Saving: %d   %2X%2X%2X %2X%2X%2X %2X%2X%2X %2X%2X%2X\n", len,
               channelData[0], channelData[1], channelData[2], channelData[3], channelData[4], channelData[5],
               channelData[6], channelData[7], channelData[8], channelData[9], channelData[10], channelData[11]);
         */
        memcpy(lastData, channelData, len);
    }
}

bool UDPOutputData::NeedToOutputFrame(unsigned char* channelData, int startChannel, int savedIdx, int count) {
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
    return true;
}

UDPOutput::UDPOutput(unsigned int startChannel, unsigned int channelCount) :
    networkCallbackId(0),
    doneWorkCount(0),
    numWorkThreads(0),
    runWorkThreads(true),
    useThreadedOutput(true),
    blockingOutput(false) {
    INSTANCE = this;
}
UDPOutput::~UDPOutput() {
    runWorkThreads = false;
    workSignal.notify_all();

    INSTANCE = nullptr;
    NetworkMonitor::INSTANCE.removeCallback(networkCallbackId);
    for (auto a : outputs) {
        delete a;
    }
    while (numWorkThreads) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int UDPOutput::Init(Json::Value config) {
    enabled = config["enabled"].asInt();
    bool hasArtNet = false;
    for (int i = 0; i < config["universes"].size(); i++) {
        Json::Value s = config["universes"][i];
        int type = s["type"].asInt();
        switch (type) {
        case 0:
        case 1:
            // E1.31 types
            outputs.push_back(new E131OutputData(s));
            break;
        case 2:
        case 3:
            // ArtNet types
            outputs.push_back(new ArtNetOutputData(s));
            hasArtNet = true;
            break;
        case 4:
        case 5:
            // DDP types
            outputs.push_back(new DDPOutputData(s));
            break;
        case 6:
        case 7:
            // KiNet types
            outputs.push_back(new KiNetOutputData(s));
            break;
        case 8:
            // Twinkly types
            outputs.push_back(new TwinklyOutputData(s));
            break;
        default:
            LogErr(VB_CHANNELOUT, "Unknown IP output type %d\n", type);
            break;
        }
    }
    if (config.isMember("threaded")) {
        int style = config["threaded"].asInt();
        useThreadedOutput = style == 1 || style == 3;
        blockingOutput = style == 0 || style == 1;
    }
    if (config.isMember("interface")) {
        outInterface = config["interface"].asString();
    }

    std::set<std::string> myIps;
    // get all the addresses
    struct ifaddrs *interfaces, *tmp;
    getifaddrs(&interfaces);
    tmp = interfaces;
    // loop through all the interfaces and get the addresses
    char address[16];
    memset(address, 0, sizeof(address));
    std::string firstInterface = "";
    while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
            GetInterfaceAddress(tmp->ifa_name, address, NULL, NULL);
            myIps.emplace(address);
            if ((outInterface == "") && (strcmp(tmp->ifa_name, "lo"))) {
                if (firstInterface == "") {
                    firstInterface = tmp->ifa_name;
                }
                if (tmp->ifa_name[0] == 'e') {
                    outInterface = tmp->ifa_name;
                }
            }
        }
        tmp = tmp->ifa_next;
    }
    freeifaddrs(interfaces);
    if (outInterface == "") {
        outInterface = firstInterface;
    }
    if (outInterface == "") {
        outInterface = "eth0";
    }

    bool disableFakeBridges = getSettingInt("DisableFakeNetworkBridges");

    for (auto o : outputs) {
        if (o->IsPingable() && o->active) {
            if (!disableFakeBridges) {
                std::string host = o->ipAddress;
                if (myIps.find(host) != myIps.end()) {
                    // trying to send UDP data to myself, that's bad.  Disable
                    std::string msg = "UDP Output set to send data to myself.  Disabling ";
                    msg += host.c_str();
                    LogWarn(VB_CHANNELOUT, msg.c_str());
                    WarningHolder::AddWarning(msg);
                    o->active = false;
                }
            }
        }
    }

    std::function<void(NetworkMonitor::NetEventType, int, const std::string&)> f = [this](NetworkMonitor::NetEventType i, int up, const std::string& s) {
        std::string interface = outInterface;
        if (s == interface && i == NetworkMonitor::NetEventType::NEW_ADDR && up) {
            if (!interfaceUp) {
                LogInfo(VB_CHANNELOUT, "UDP Interface %s now up\n", s.c_str());
                PingControllers(false);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                InitNetwork();
                interfaceUp = true;
            }
        } else if (s == interface && i == NetworkMonitor::NetEventType::DEL_ADDR) {
            LogInfo(VB_CHANNELOUT, "UDP Interface %s now down\n", s.c_str());
            interfaceUp = false;
            CloseNetwork();
        }
    };
    networkCallbackId = NetworkMonitor::INSTANCE.registerCallback(f);

    // We are going to initialize everything and Ping the outputs
    // so we'll assume the interface is Up.
    interfaceUp = true;
    InitNetwork();
    failedCount = 0;
    // need to do three pings to detect down hosts
    for (auto& o : outputs) {
        if (o->active) {
            o->failCount = -1;
            ++failedCount;
        }
    }
    PingControllers(false);
    int sleepCount = 0;
    while (failedCount > 0 && sleepCount < (UDP_PING_TIMEOUT + 10)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ++sleepCount;
    }
    PingControllers(true);
    sleepCount = 0;
    while (failedCount > 0 && sleepCount < (UDP_PING_TIMEOUT * 2)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        CurlManager::INSTANCE.processCurls();
        ++sleepCount;
    }
    PingControllers(true);
    sleepCount = 0;
    while (failedCount > 0 && sleepCount < (UDP_PING_TIMEOUT * 2)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        CurlManager::INSTANCE.processCurls();
        ++sleepCount;
    }
    if (hasArtNet) {
        // we have artnet packets so we'll need to get the special artnet socket created,
        // but we want to make sure it's on the given interface
        CreateArtNetSocket(localAddress.sin_addr.s_addr);
    }
    return ChannelOutput::Init(config);
}
int UDPOutput::Close() {
    NetworkMonitor::INSTANCE.removeCallback(networkCallbackId);
    messages.clearMessages();
    messages.clearSockets();
    return ChannelOutput::Close();
}
void UDPOutput::PrepData(unsigned char* channelData) {
    if (enabled) {
        std::unique_lock<std::mutex> lk(socketMutex);
        messages.clearMessages();
        for (auto a : outputs) {
            if (a->valid && a->active) {
                a->PrepareData(channelData, messages);
            }
        }
        // add any sync packets or whatever that are needed
        for (auto a : outputs) {
            if (a->valid && a->active) {
                a->PostPrepareData(channelData, messages);
            }
        }
    }
}
void UDPOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
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
int UDPOutput::SendMessages(unsigned int socketKey, SendSocketInfo* socketInfo, std::vector<struct mmsghdr>& sendmsgs) {
    errno = 0;
    struct mmsghdr* msgs = &sendmsgs[0];
    int msgCount = sendmsgs.size();
    if (msgCount == 0) {
        return 0;
    }

    int newSockKey = socketKey;
    int sendSocket = socketInfo->sockets[socketInfo->curSocket];
    errno = 0;
    // uint64_t st = GetTimeMicros();

    int outputCount = 0;
    if (blockingOutput) {
        int errorCount = 0;
        for (int x = 0; x < msgCount; x++) {
            ssize_t s = sendmsg(sendSocket, &msgs[x].msg_hdr, 0);
            if (s != -1) {
                errorCount = 0;
                ++outputCount;
            } else if (errorCount) {
                return outputCount;
            } else {
                // didn't send, we'll yield once and re-send
                --x;
                ++errorCount;
                std::this_thread::yield();
            }
        }
    } else {
        int oc = sendmmsg(sendSocket, msgs, msgCount, MSG_DONTWAIT);
        if (oc > 0) {
            outputCount += oc;
        }
        if (outputCount != msgCount) {
            // in many cases, a simple thread yield will allow the network stack
            // to flush some data and free up space, give that a chance first
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            oc = sendmmsg(sendSocket, &msgs[outputCount], msgCount - outputCount, MSG_DONTWAIT);
            while (oc > 0) {
                outputCount += oc;
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                oc = sendmmsg(sendSocket, &msgs[outputCount], msgCount - outputCount, MSG_DONTWAIT);
            }
        }
    }
    // uint64_t ed = GetTimeMicros();
    // uint64_t tm = ed - st;
    // printf("MSG: %d/%d    %d     \n", outputCount, msgCount, (int)tm);

    int errCount = 0;
    while (outputCount != msgCount) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (socketKey != BROADCAST_MESSAGES_KEY) {
                ++socketInfo->curSocket;
                if (socketInfo->curSocket == socketInfo->sockets.size()) {
                    if (socketInfo->sockets.size() < 5) {
                        // this will trigger a create socket
                        socketInfo->sockets.push_back(-1);
                    } else {
                        socketInfo->curSocket = 0;
                    }
                }
                sendSocket = socketInfo->sockets[socketInfo->curSocket];
                if (sendSocket == -1) {
                    socketInfo->sockets[socketInfo->curSocket] = createSocket();
                    sendSocket = socketInfo->sockets[socketInfo->curSocket];
                }
            } else {
                return outputCount;
            }
        }
        ++errCount;
        if (errCount >= 10) {
            LogErr(VB_CHANNELOUT, "sendmmsg() failed for UDP output (IP: %s   Socket: %d   output count: %d/%d) with error: %d   %s\n",
                   HexToIP(socketKey).c_str(), sendSocket,
                   outputCount, msgCount,
                   errno,
                   strerror(errno));
            return outputCount;
        }
        errno = 0;
        int oc = sendmmsg(sendSocket, &msgs[outputCount], msgCount - outputCount, MSG_DONTWAIT);
        while (oc > 0) {
            outputCount += oc;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            oc = sendmmsg(sendSocket, &msgs[outputCount], msgCount - outputCount, MSG_DONTWAIT);
        }
    }
    return outputCount;
}

static void DoWorkThread(UDPOutput* output) {
    SetThreadName("FPP-UDPWork");
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

                // failed to send all messages or it took more than 100ms to send them
                LogErr(VB_CHANNELOUT, "%s() failed for UDP output (IP: %s   output count: %d/%d   time: %u ms    errCount: %d) with error: %d   %s\n",
                       blockingOutput ? "sendmsg" : "sendmmsg", HexToIP(i.id).c_str(),
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

int UDPOutput::SendData(unsigned char* channelData) {
    std::unique_lock<std::mutex> lk(socketMutex);
    if (!enabled || messages.sendSockets.empty()) {
        return 0;
    }
    std::chrono::high_resolution_clock clock;
    if (useThreadedOutput) {
        doneWorkCount = 0;
        int total = 0;
        auto t1 = clock.now();
        for (auto& msgs : messages.messages) {
            if (!msgs.second.empty() && msgs.first < LATE_MULTICAST_MESSAGES_KEY) {
                SendSocketInfo* socketInfo = findOrCreateSocket(msgs.first);

                std::unique_lock<std::mutex> lock(workMutex);
                workQueue.push_back(WorkItem(msgs.first, socketInfo, msgs.second));
                lock.unlock();
                workSignal.notify_one();
                ++total;
            }
        }
        std::unique_lock<std::mutex> lock(workMutex);
        while (numWorkThreads < workQueue.size()) {
            std::thread(DoWorkThread, this).detach();
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
            // now output the LATE/Broadcast packets (likely sync packets)
            for (auto& msgs : messages.messages) {
                if (!msgs.second.empty()) {
                    SendSocketInfo* socketInfo = findOrCreateSocket(msgs.first);
                    if (msgs.first >= LATE_MULTICAST_MESSAGES_KEY) {
                        t1 = clock.now();
                        int outputCount = SendMessages(msgs.first, socketInfo, msgs.second);
                        t2 = clock.now();
                        long diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
                        if ((outputCount != msgs.second.size()) || (diff > 100)) {
                            socketInfo->errCount++;

                            // failed to send all messages or it took more than 100ms to send them
                            LogErr(VB_CHANNELOUT, "sendmmsg() failed for UDP output (IP: %s   output count: %d/%d   time: %u ms    errCount: %d) with error: %d   %s\n",
                                   HexToIP(msgs.first).c_str(),
                                   outputCount, msgs.second.size(), diff, socketInfo->errCount,
                                   errno,
                                   strerror(errno));
                        } else {
                            socketInfo->errCount = 0;
                        }
                    }
                    if (socketInfo->errCount >= 3) {
                        // we'll ping the controllers and rebuild the valid message list, this could take time
                        PingControllers(false);
                        socketInfo->errCount = 0;
                    }
                }
            }
        }
        return 1;
    }
    for (auto& msgs : messages.messages) {
        if (!msgs.second.empty()) {
            SendSocketInfo* socketInfo = findOrCreateSocket(msgs.first);
            auto t1 = clock.now();
            int outputCount = SendMessages(msgs.first, socketInfo, msgs.second);
            auto t2 = clock.now();
            long diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
            if ((outputCount != msgs.second.size()) || (diff > 100)) {
                socketInfo->errCount++;

                // failed to send all messages or it took more than 100ms to send them
                LogErr(VB_CHANNELOUT, "sendmmsg() failed for UDP output (IP: %s   output count: %d/%d   time: %u ms    errCount: %d) with error: %d   %s\n",
                       HexToIP(msgs.first).c_str(),
                       outputCount, msgs.second.size(), diff, socketInfo->errCount,
                       errno,
                       strerror(errno));

                if (socketInfo->errCount >= 3) {
                    // we'll ping the controllers and rebuild the valid message list, this could take time
                    PingControllers(false);
                    socketInfo->errCount = 0;
                }
            } else {
                socketInfo->errCount = 0;
            }
        }
    }
    return 1;
}

void UDPOutput::PingControllers(bool failedOnly) {
    LogExcess(VB_CHANNELOUT, "Pinging controllers to see what is online\n");
    for (auto o : outputs) {
        if (o->IsPingable() && o->Monitor() && o->active) {
            if (failedOnly && o->failCount == 0) {
                continue;
            }
            PingManager::INSTANCE.addPeriodicPing(o->ipAddress, UDP_PING_TIMEOUT, 15000, [o, this](int i) {
                if (o->failCount == -1) {
                    // first pass through, we got a response of some sort
                    // so decrement the failed count so the main thread may
                    // be able to continue
                    if (i > 0 && o->valid) {
                        --failedCount;
                    }
                    o->failCount = 0;
                }
                if (i > 0 && !o->valid) {
                    WarningHolder::RemoveWarning(createWarning(o->ipAddress, o->GetOutputTypeString(), o->description));
                    LogWarn(VB_CHANNELOUT, "Could ping host %s, re-adding to outputs\n", o->ipAddress.c_str());
                    o->failCount = 0;
                    o->valid = true;
                    --failedCount;
                } else if (i <= 0) {
                    LogDebug(VB_CHANNELOUT, "Could not ping host %s   Fail count: %d   Currently Valid: %d\n", o->ipAddress.c_str(), o->failCount, o->valid);
                    o->failCount++;
                    if (o->failCount == 1) {
                        // ignore a single ping failure, could be transient
                    } else if (o->failCount == 2) {
                        // if two pings fail, we'll try a HEAD request via HTTP
                        CurlManager::INSTANCE.add("http://" + o->ipAddress + "/", "HEAD", "", {}, [o, this](int rc, const std::string& resp) {
                            if (rc) {
                                o->failCount = 0;
                                if (!o->valid) {
                                    --failedCount;
                                    o->valid = true;
                                    WarningHolder::RemoveWarning(createWarning(o->ipAddress, o->GetOutputTypeString(), o->description));
                                    LogWarn(VB_CHANNELOUT, "Could ping host %s, re-adding to outputs\n",
                                            o->ipAddress.c_str());
                                }
                            }
                        });
                    } else if (o->failCount >= 3) {
                        // three pings an HEAD request failed, mark invalid
                        if (o->valid) {
                            WarningHolder::AddWarning(createWarning(o->ipAddress, o->GetOutputTypeString(), o->description));
                            LogWarn(VB_CHANNELOUT, "Could not ping host %s, removing from output\n", o->ipAddress.c_str());
                            o->valid = false;
                        }
                        ++failedCount;
                        if (o->failCount > 5) {
                            // make sure we wrap around so another HEAD request later may pick it up
                            o->failCount = 0;
                        }
                    }
                }
            });
        }
    }
}
void UDPOutput::DumpConfig() {
    ChannelOutput::DumpConfig();
    LogDebug(VB_CHANNELOUT, "    Interface        : %s\n", outInterface.c_str());
    LogDebug(VB_CHANNELOUT, "    Threaded         : %d\n", useThreadedOutput);
    LogDebug(VB_CHANNELOUT, "    Blocking         : %d\n", blockingOutput);
    for (auto u : outputs) {
        u->DumpConfig();
    }
}

void UDPOutput::CloseNetwork() {
    std::unique_lock<std::mutex> lk(socketMutex);
    messages.clearSockets();
    lk.unlock();
    PingControllers(false);
}
SendSocketInfo* UDPOutput::findOrCreateSocket(unsigned int socketKey, int sc) {
    auto it = messages.sendSockets.find(socketKey);
    SendSocketInfo* info;
    if (it == messages.sendSockets.end()) {
        info = new SendSocketInfo();
        messages.sendSockets[socketKey] = info;
    } else {
        info = it->second;
    }

    if (info->sockets.empty()) {
        for (int x = info->sockets.size(); x < sc; x++) {
            int s = createSocket(0, socketKey == BROADCAST_MESSAGES_KEY,
                                 socketKey == MULTICAST_MESSAGES_KEY || socketKey == LATE_MULTICAST_MESSAGES_KEY);
            info->sockets.push_back(s);
        }
    }
    if (info->curSocket == -1) {
        info->curSocket = 0;
    }
    return info;
}

int UDPOutput::createSocket(int port, bool broadCast, bool multiCast) {
    int sendSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (sendSocket < 0) {
        LogErr(VB_CHANNELOUT, "Error opening datagram socket\n");
        exit(1);
    }

    static struct sockaddr_in address;
    memcpy(&address, &localAddress, sizeof(localAddress));
    address.sin_port = ntohs(port);

    errno = 0;
    // Disable loopback so I do not receive my own datagrams.
    char loopch = 0;
    if (setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&loopch, sizeof(loopch)) < 0) {
        LogErr(VB_CHANNELOUT, "Error setting IP_MULTICAST_LOOP error\n");
        close(sendSocket);
        return -1;
    }
    // make sure the send buffer is actually set to a reasonable size for non-blocking mode
    int bufSize = 1024 * (blockingOutput ? 4 : 512);
    setsockopt(sendSocket, SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize));
    // these sockets are for sending only, don't need a large receive buffer so
    // free some memory by setting to just a single page
    bufSize = 4096;
    setsockopt(sendSocket, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));

    if (blockingOutput) {
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000; // 1ms timeout
        setsockopt(sendSocket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);
    }

    if (broadCast) {
        int broadcast = 1;
        if (setsockopt(sendSocket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
            LogErr(VB_SYNC, "Error setting SO_BROADCAST: \n", strerror(errno));
            exit(1);
        }
#ifndef PLATFORM_OSX
    } else if (multiCast) {
        if (setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_IF, (char*)&address, sizeof(address)) < 0) {
            LogErr(VB_CHANNELOUT, "Error setting IP_MULTICAST_IF error\n");
        }
#endif
    }
    if (broadCast || multiCast) {
        if (bind(sendSocket, (struct sockaddr*)&address, sizeof(struct sockaddr_in)) == -1) {
            LogErr(VB_CHANNELOUT, "Error in bind:errno=%d, %s\n", errno, strerror(errno));
        }
#ifndef PLATFORM_OSX
        if (connect(sendSocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
            LogErr(VB_CHANNELOUT, "Error connecting IP_MULTICAST_LOOP socket\n");
        }
#endif
    }
    return sendSocket;
}

bool UDPOutput::InitNetwork() {
    memset(&localAddress, 0, sizeof(struct sockaddr_in));
    localAddress.sin_family = AF_INET;
    localAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (!messages.sendSockets.empty()) {
        return true;
    }

    char E131LocalAddress[16];
    int rv = GetInterfaceAddress(outInterface.c_str(), E131LocalAddress, NULL, NULL);
    LogDebug(VB_CHANNELOUT, "UDPLocalAddress = %s\n", E131LocalAddress);
    localAddress.sin_addr.s_addr = inet_addr(E131LocalAddress);

    if (rv) {
        LogErr(VB_CHANNELOUT, "Invalid interface %s\n", outInterface.c_str());
        WarningHolder::AddWarning("Invalid interface for UDP broadcast/multicast: " + outInterface);
        return -1;
    }

    if (strlen(E131LocalAddress) > 3 && E131LocalAddress[0] == '1' && E131LocalAddress[1] == '2' && E131LocalAddress[2] == '7') {
        // the entire 127.* subnet is localhost
        return -1;
    }

    int broadcastSocket = createSocket(0, true);
    messages.ForceSocket(BROADCAST_MESSAGES_KEY, broadcastSocket);
    return true;
}

void UDPOutput::StartingOutput() {
    for (auto a : outputs) {
        if (a->valid && a->active) {
            a->StartingOutput();
        }
    }
}
void UDPOutput::StoppingOutput() {
    for (auto a : outputs) {
        if (a->valid && a->active) {
            a->StoppingOutput();
        }
    }
}

std::string UDPOutput::HexToIP(unsigned int hex) {
    // Extract each byte in LSB order
    uint8_t octet1 = hex & 0xFF; // Least significant byte
    uint8_t octet2 = (hex >> 8) & 0xFF;
    uint8_t octet3 = (hex >> 16) & 0xFF;
    uint8_t octet4 = (hex >> 24) & 0xFF; // Most significant byte

    return std::to_string(octet1) + "." +
           std::to_string(octet2) + "." +
           std::to_string(octet3) + "." +
           std::to_string(octet4);
}
