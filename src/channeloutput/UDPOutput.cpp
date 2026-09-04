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

#include "fpp-json.h"

#ifndef PLATFORM_OSX
#include <linux/sockios.h>
#include <sys/ioctl.h>
#endif

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <unistd.h>

#include <curl/curl.h>
#include <filesystem>
#include <set>
#include <sstream>

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

#ifndef PLATFORM_OSX
// SO_MAX_PACING_RATE only works if the interface's root qdisc is fq.
// Some kernels expose the qdisc name in sysfs; fall back to asking tc.
static bool interfaceHasFQ(const std::string& iface) {
    std::string qdisc = GetFileContents("/sys/class/net/" + iface + "/qdisc");
    if (!qdisc.empty()) {
        TrimWhiteSpace(qdisc);
        return qdisc == "fq";
    }
    // some kernels don't expose the qdisc name in sysfs; ask tc
    std::string cmd = TcPath() + " qdisc show dev " + iface + " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[256];
        while (fgets(buf, sizeof(buf), pipe)) {
            if (strstr(buf, " root ") && strncmp(buf, "qdisc fq ", 9) == 0) {
                qdisc = "fq";
            }
        }
        pclose(pipe);
    }
    return qdisc == "fq";
}
// Unicast sockets follow the routing table, not the configured output
// interface, and the interface scan at startup can race the network coming
// up.  fppinit installs fq on every ethernet interface at boot, so consider
// pacing available if any of them has it.  Whether a specific destination's
// socket is actually paced is decided per destination by looking up the
// egress interface of its route (see findOrCreateSocket).
static bool anyEthernetHasFQ(const std::string& primary) {
    if (!primary.empty() && interfaceHasFQ(primary)) {
        return true;
    }
    for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net/")) {
        std::string dev = entry.path().filename();
        if (IsShowEthernetInterface(dev) && dev != primary && interfaceHasFQ(dev)) {
            return true;
        }
    }
    return false;
}
// Send buffer for paced sockets: the kernel silently caps SO_SNDBUF at
// wmem_max (set by fppinit at boot), so ask for exactly what it will grant.
static int pacedSndBufSize() {
    static int size = []() {
        int v = atoi(GetFileContents("/proc/sys/net/core/wmem_max").c_str());
        return v > 0 ? v : 393216;
    }();
    return size;
}
// Which interface will packets to this destination actually leave through?
// A connect() on a UDP socket does the route lookup without sending anything.
// (The address->interface tail is intentionally not FindInterfaceForIP(): that
// helper returns a static buffer and round-trips through strings; comparing
// s_addr directly is thread-safe and exact.)
static std::string egressInterfaceFor(in_addr_t dest) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        return "";
    }
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(53);
    sa.sin_addr.s_addr = dest;
    std::string ret;
    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) == 0) {
        struct sockaddr_in local;
        socklen_t len = sizeof(local);
        if (getsockname(s, (struct sockaddr*)&local, &len) == 0) {
            struct ifaddrs *interfaces, *tmp;
            if (getifaddrs(&interfaces) == 0) {
                for (tmp = interfaces; tmp; tmp = tmp->ifa_next) {
                    if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET &&
                        ((struct sockaddr_in*)tmp->ifa_addr)->sin_addr.s_addr == local.sin_addr.s_addr) {
                        ret = tmp->ifa_name;
                        break;
                    }
                }
                freeifaddrs(interfaces);
            }
        }
    }
    close(s);
    return ret;
}
#endif

class SendSocketInfo {
public:
    SendSocketInfo() {
        errCount = 0;
        curSocket = -1;
    }
    ~SendSocketInfo() {
        if (!preventClose) {
            for (int x : sockets) {
                close(x);
            }
        }
    }

    std::vector<int> sockets;
    // read/written by workers and the frame thread without a common lock;
    // atomic so the errCount >= 3 re-ping trigger can't act on a torn value
    std::atomic_int errCount;
    int curSocket;
    bool preventClose = false;
    // kernel (fq) pacing is active on these sockets; drain waits are the
    // qdisc's job and EAGAIN means "wait", not "get another socket".
    // pacedChecked latches once the destination's route has resolved; until
    // then (e.g. interface still waiting on DHCP at startup) the pacing
    // decision is re-attempted periodically (pacedCheckCountdown frames).
    bool paced = false;
    bool pacedChecked = false;
    int pacedCheckCountdown = 0;
    // Serializes all use/mutation of sockets/curSocket (errCount is atomic
    // and accessed outside the lock).  A worker that outlives its frame
    // (stuck send) can still be inside SendMessages when the next frame's
    // worker gets the same destination, and SendMessages rotates curSocket
    // and grows the sockets vector on EAGAIN.
    std::mutex sendLock;
};

UDPOutputMessages::UDPOutputMessages() {
}
UDPOutputMessages::~UDPOutputMessages() {
    sendSockets.clear();
}
int UDPOutputMessages::GetSocket(unsigned int key) {
    std::shared_ptr<SendSocketInfo>& info = sendSockets[key];
    if (info) {
        std::lock_guard<std::mutex> lg(info->sendLock);
        if (!info->sockets.empty()) {
            return info->sockets[0];
        }
    }
    return -1;
}
void UDPOutputMessages::ForceSocket(unsigned int key, int socket, bool preventClose) {
    std::shared_ptr<SendSocketInfo>& info = sendSockets[key];
    if (!info) {
        info = std::make_shared<SendSocketInfo>();
    }
    std::lock_guard<std::mutex> lg(info->sendLock);
    if (!info->preventClose) {
        for (int x = 0; x < info->sockets.size(); x++) {
            close(info->sockets[x]);
        }
    }
    info->preventClose = preventClose;
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
    // sockets are shared_ptrs; any worker thread still mid-send keeps its
    // SendSocketInfo alive until it finishes
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
    // per-destination pacing override; absent (or -1) keeps the global rate
    if (config.isMember("pacingRate")) {
        pacingRateMbps = config["pacingRate"].asInt();
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
        // ResolveHostToIPv4() is thread-safe; gethostbyname() returns a shared
        // static hostent and can be corrupted by concurrent resolves on other
        // threads.  It also caches the answer, so a destination configured by a
        // name that no longer resolves costs its multi-second timeout once at
        // startup rather than once per caller.
        uint32_t addr = 0;
        if (!ResolveHostToIPv4(ipAddress, addr)) {
            LogErr(VB_CHANNELOUT,
                   "Error looking up hostname: %s\n",
                   ipAddress.c_str());
            valid = false;
            return 0;
        }
        return addr;
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
    workGeneration(0),
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
    while (numWorkThreads) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    while (statCheckRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // Need to make sure all curls are processed before we delete the outputs
    // or we may have curl callbacks trying to access deleted data.
    while (CurlManager::INSTANCE.processCurls()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    for (auto a : outputs) {
        delete a;
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
        case 9:
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

        needsBroadcast |= (type == 0 || type == 2);
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

    // Build the per-destination pacing overrides keyed by controller IP.  The
    // send socket is shared per destination IP, so when multiple outputs target
    // the same controller with different overrides we keep the most conservative
    // (lowest) cap; an explicit "unpaced" (0 Mbps) is stored as ~0U so any real
    // rate on the same IP wins over it.
    for (auto o : outputs) {
        if (o->pacingRateMbps < 0 || o->ipAddress.empty()) {
            continue;
        }
        bool valid = false;
        in_addr_t addr = UDPOutputData::toInetAddr(o->ipAddress, valid);
        if (!valid) {
            continue;
        }
        unsigned int rate = o->pacingRateMbps == 0 ? ~0U : (unsigned int)(o->pacingRateMbps * 1000000ULL / 8);
        auto it = perDestPacingRate.find(addr);
        if (it == perDestPacingRate.end() || rate < it->second) {
            perDestPacingRate[addr] = rate;
        }
    }

#ifndef PLATFORM_OSX
    // Kernel-level packet pacing.  Bursting a full frame at line rate overruns
    // switch buffers on the 1G->100M speed mismatch to most pixel controllers
    // and the drops are silent.  With the fq qdisc installed (fppinit does this
    // at boot), SO_MAX_PACING_RATE lets the kernel clock each destination's
    // packets out at a rate the far end can actually absorb.
    // pacing rate lives in the outputs config (hot-reloadable with the rest
    // of it); fall back to the old global setting for configs saved before
    // the UI moved, then to the 90Mbps default
    int pacingMbps = config.isMember("pacingRate") ? config["pacingRate"].asInt() : getSettingInt("UDPPacingRate", 90);
    // per-output overrides can request pacing even when the global rate is
    // Disabled (e.g. only a single ESP32 needs a cap); install fq and arm the
    // machinery if either the global rate or any override wants it
    bool anyPerDestPacing = false;
    for (const auto& e : perDestPacingRate) {
        if (e.second != ~0U) {
            anyPerDestPacing = true;
            break;
        }
    }
    if (pacingMbps > 0 || anyPerDestPacing) {
        if (!anyEthernetHasFQ(outInterface)) {
            // fppinit only installs fq at boot; if the user enabled pacing
            // since then, install it now so the setting takes effect on the
            // fppd restart it advertises rather than silently needing a
            // reboot.  Only interfaces with an IPv4 address can carry the
            // routed UDP we pace - an address-less interface dedicated to a
            // raw layer-2 output (ColorLight) keeps its default qdisc.
            for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net/")) {
                std::string dev = entry.path().filename();
                if (IsShowEthernetInterface(dev) && InterfaceHasRoutableIPv4(dev)) {
                    InstallShowTrafficQdisc(dev);
                }
            }
        }
        if (anyEthernetHasFQ(outInterface)) {
            pacingEnabled = true;
            // 0 when the global rate is Disabled: destinations without an
            // override then stay unpaced while overridden ones still get capped
            pacingRate = pacingMbps > 0 ? (unsigned int)(pacingMbps * 1000000ULL / 8) : 0;
            // slowest positive rate in play, for the drain-sleep math below
            pacingDrainRate = pacingRate;
            for (const auto& e : perDestPacingRate) {
                if (e.second != ~0U && (pacingDrainRate == 0 || e.second < pacingDrainRate)) {
                    pacingDrainRate = e.second;
                }
            }
            if (pacingMbps > 0) {
                LogInfo(VB_CHANNELOUT, "UDP output pacing enabled at %d Mbps per destination (%d per-destination override(s))\n",
                        pacingMbps, (int)perDestPacingRate.size());
            } else {
                LogInfo(VB_CHANNELOUT, "UDP output pacing enabled for %d per-destination override(s), global rate disabled\n",
                        (int)perDestPacingRate.size());
            }
        } else {
            LogInfo(VB_CHANNELOUT, "UDP output pacing requested but %s does not have the fq qdisc, using buffer drain pacing\n",
                    outInterface.c_str());
        }
    } else {
        LogInfo(VB_CHANNELOUT, "UDP output pacing disabled, using buffer drain pacing\n");
    }
#endif

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
                    WarningHolder::AddWarning(6, msg);
                    o->active = false;
                }
            }
        }
    }

    std::function<void(NetworkMonitor::NetEventType, int, const std::string&)> f = [this](NetworkMonitor::NetEventType i, int up, const std::string& s) {
        std::string interface = outInterface;
        if ((i == NetworkMonitor::NetEventType::NEW_ADDR && up) || i == NetworkMonitor::NetEventType::DEL_ADDR) {
            // any address change can move routes; re-evaluate the pacing
            // decision for every unicast destination.  This covers both
            // directions: a destination that couldn't be paced yet (route
            // resolved via wifi while ethernet waited on DHCP) and a paced
            // destination whose route failed over to a non-fq interface,
            // which must be un-paced or it sends unpaced line-rate bursts
            // with the large buffer and no drain waits.  Stagger the
            // re-checks across frames so one network event doesn't trigger
            // a route-lookup storm in a single frame.
            std::unique_lock<std::mutex> lk(socketMutex);
            int stagger = 0;
            for (auto& si : messages.sendSockets) {
                if (si.second) {
                    // try_lock: blocking here (while holding socketMutex) on a
                    // straggler worker would transitively stall the frame thread
                    std::unique_lock<std::mutex> lg(si.second->sendLock, std::try_to_lock);
                    if (lg.owns_lock()) {
                        si.second->pacedChecked = false;
                        si.second->pacedCheckCountdown = stagger++ % 16;
                    }
                }
            }
        }
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
        CreateArtNetSocket(localAddress.sin_addr.s_addr, true);
    }
    return ChannelOutput::Init(config);
}
int UDPOutput::Close() {
    NetworkMonitor::INSTANCE.removeCallback(networkCallbackId);
    messages.clearMessages();
    messages.clearSockets();
    for (auto o : outputs) {
        if (o->IsPingable() && o->Monitor()) {
            PingManager::INSTANCE.removePeriodicPing(o->ipAddress);
        }
    }
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

static void flushBuffers(int socket, int msgs, int total) {
#ifndef PLATFORM_OSX
    int bytes_in_buffer = 0;
    if (ioctl(socket, SIOCOUTQ, &bytes_in_buffer) == 0) {
        // Check bytes_in_buffer, if its too high, wait for it to drain
        int cnt = 0;
        int start = bytes_in_buffer;
        while (bytes_in_buffer > 1024 && cnt < 50) {
            cnt++;
            std::this_thread::sleep_for(std::chrono::microseconds(1));
            ioctl(socket, SIOCOUTQ, &bytes_in_buffer);
        }
        // if (cnt > 0) {
        //     printf("Flush: Socket %d had to wait %d  (%d->%d)      (%d/%d)...\n", socket, cnt, start, bytes_in_buffer, msgs, total);
        // }
    }
#endif
}

constexpr int MSGS_PER_SENDMMSG = 8;
// kernel-paced sockets take the whole frame in one call (kernel caps a single
// sendmmsg at UIO_MAXIOV=1024 anyway)
constexpr int PACED_MSGS_PER_SENDMMSG = 1024;
int UDPOutput::SendMessages(unsigned int socketKey, SendSocketInfo* socketInfo, std::vector<struct mmsghdr>& sendmsgs) {
    errno = 0;
    struct mmsghdr* msgs = &sendmsgs[0];
    int msgCount = sendmsgs.size();
    if (msgCount == 0) {
        return 0;
    }

    int newSockKey = socketKey;
    // Serialize against a straggler worker from a previous frame still sending
    // to this destination; SendMessages mutates curSocket/sockets.  Blocking
    // (rather than try_lock) is intentional and safe for the frame thread:
    // workers exclusively own the early-message keys, and the frame thread
    // only calls SendMessages for late/sync keys (threaded mode) or when no
    // workers exist at all (non-threaded mode), so it can never block on a
    // straggler here.
    std::lock_guard<std::mutex> sendGuard(socketInfo->sendLock);
    if (socketInfo->sockets.empty()) {
        return 0;
    }
    if (socketInfo->curSocket < 0 || socketInfo->curSocket >= socketInfo->sockets.size()) {
        socketInfo->curSocket = 0;
    }
    int sendSocket = socketInfo->sockets[socketInfo->curSocket];
    errno = 0;
    // uint64_t st = GetTimeMicros();

    // when the kernel is pacing this socket (fq + SO_MAX_PACING_RATE) the
    // socket buffer is sized for the whole frame; hand everything over at once
    // and skip the drain waits, the qdisc clocks packets out at the target rate
    const bool paced = socketInfo->paced;
    const int maxBatch = paced ? PACED_MSGS_PER_SENDMMSG : MSGS_PER_SENDMMSG;

    int outputCount = 0;
    if (blockingOutput && !paced) {
        // Blocking mode only applies to unpaced sockets.  For paced sockets
        // the kernel throttling that blocking mode approximates is done
        // properly by the fq qdisc, and the per-packet 1ms SO_SNDTIMEO would
        // abort a frame that legitimately sits in the paced buffer; they
        // always use the batched path below.
        int errorCount = 0;
        for (int x = 0; x < msgCount; x++) {
            flushBuffers(sendSocket, x, msgCount);
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
        int oc = sendmmsg(sendSocket, msgs, std::min(msgCount, maxBatch), MSG_DONTWAIT);
        if (oc > 0) {
            outputCount += oc;
        }
        if (outputCount != msgCount) {
#ifndef PLATFORM_OSX
            if (!paced) {
                flushBuffers(sendSocket, outputCount, msgCount);
            }
#else
            // On OSX we have no good way to check the socket buffer, so just
            // sleep a bit to allow the stack to flush
            std::this_thread::sleep_for(std::chrono::microseconds(100));
#endif
            int outMsgCnt = msgCount - outputCount;
            oc = sendmmsg(sendSocket, &msgs[outputCount], std::min(outMsgCnt, maxBatch), MSG_DONTWAIT);
            while (oc > 0) {
                outputCount += oc;
#ifndef PLATFORM_OSX
                if (!paced) {
                    flushBuffers(sendSocket, outputCount, msgCount);
                }
#else
                std::this_thread::sleep_for(std::chrono::microseconds(100));
#endif
                outMsgCnt = msgCount - outputCount;
                oc = sendmmsg(sendSocket, &msgs[outputCount], std::min(outMsgCnt, maxBatch), MSG_DONTWAIT);
            }
        }
    }
    // uint64_t ed = GetTimeMicros();
    // uint64_t tm = ed - st;
    // printf("MSG: %d/%d    %d     \n", outputCount, msgCount, (int)tm);

    int errCount = 0;
    auto retryStart = std::chrono::steady_clock::now();
    while (outputCount != msgCount) {
        // Absolute wall-clock bound in addition to the consecutive-failure
        // count: a destination that needs more data than the pacing rate
        // allows keeps making slow progress (resetting errCount), and without
        // this cap the worker would hold sendLock across whole frames as a
        // permanent straggler.
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - retryStart).count() > 100) {
            LogErr(VB_CHANNELOUT, "sendmmsg() send to %s exceeded time budget, dropping remainder of frame (output count: %d/%d)\n",
                   HexToIP(socketKey).c_str(), outputCount, msgCount);
            return outputCount;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (paced) {
                // the qdisc is intentionally holding packets; wait for the
                // buffer to drain at the pacing rate rather than spreading the
                // overflow across additional sockets, which would multiply the
                // per-controller rate cap and defeat the pacing
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            } else if (socketKey != BROADCAST_MESSAGES_KEY) {
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
                    // replacement must keep the multicast setup or the packets
                    // will egress the default route instead of the show network
                    socketInfo->sockets[socketInfo->curSocket] = createSocket(0, false,
                                                                              socketKey == MULTICAST_MESSAGES_KEY || socketKey == LATE_MULTICAST_MESSAGES_KEY);
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
                   FPPstrerror(errno));
            return outputCount;
        }
        errno = 0;
        int oc = sendmmsg(sendSocket, &msgs[outputCount], msgCount - outputCount, MSG_DONTWAIT);
        while (oc > 0) {
            if (paced) {
                // making progress; for a paced socket EAGAIN is the expected
                // steady state when a frame exceeds the buffer, so only give
                // up after consecutive no-progress attempts.  Unpaced sockets
                // keep the historical ~10-attempt bound so a congested
                // destination cannot pin a worker for a whole frame.
                errCount = 0;
            }
            outputCount += oc;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            oc = sendmmsg(sendSocket, &msgs[outputCount], msgCount - outputCount, MSG_DONTWAIT);
        }
    }
    return outputCount;
}

static void DoWorkThread(UDPOutput* output) {
    SetThreadName("FPP-UDPWork");
    // Keep the send threads from being delayed by other load (video decode,
    // php, etc.).  A delayed frame bunches with the next one and doubles the
    // burst hitting the switch.  Priority sits below the output thread (10)
    // so the frame loop always preempts stragglers; worker count can grow to
    // the destination fan-out and these all park on workSignal when idle.
    SetThreadRealtimePriority(5);
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
            WorkItem i = std::move(workQueue.front());
            workQueue.pop_front();
            lock.unlock();

            auto t1 = clock.now();
            int outputCount = SendMessages(i.id, i.socketInfo.get(), i.msgs);
            auto t2 = clock.now();

            long diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
            if ((outputCount != i.msgs.size()) || (diff > 100)) {
                i.socketInfo->errCount++;

                // failed to send all messages or it took more than 100ms to send them
                LogErr(VB_CHANNELOUT, "%s() failed for UDP output (IP: %s   output count: %d/%d   time: %u ms    errCount: %d) with error: %d   %s\n",
                       blockingOutput ? "sendmsg" : "sendmmsg", HexToIP(i.id).c_str(),
                       outputCount, i.msgs.size(), diff, i.socketInfo->errCount.load(),
                       errno,
                       FPPstrerror(errno));
            } else {
                i.socketInfo->errCount = 0;
            }

            // only count work for the current frame; if SendData timed out and
            // moved on, this completion must not skew the next frame's count.
            // The check and increment must be one unit under workMutex or a
            // generation bump between them counts old work toward the new frame.
            {
                std::unique_lock<std::mutex> countLock(workMutex);
                if (i.generation == workGeneration) {
                    doneWorkCount++;
                }
            }
        }
    }

    --numWorkThreads;
}

int UDPOutput::SendData(unsigned char* channelData) {
    std::unique_lock<std::mutex> lk(socketMutex);
    if (!enabled || messages.messages.empty()) {
        return 0;
    }
    if (++statCheckCounter >= 1500) {
        // roughly once a minute at typical frame rates; run off-thread so the
        // tc fork in the qdisc check can't hiccup the frame
        statCheckCounter = 0;
        if (!statCheckRunning.exchange(true)) {
            std::thread([this]() {
                SetThreadName("FPP-UDPStats");
                try {
                    CheckLocalDrops();
                } catch (...) {
                    // an escaped exception on a detached thread would call
                    // std::terminate and take down the show over a diagnostic
                }
                statCheckRunning = false;
            }).detach();
        }
    }
    std::chrono::high_resolution_clock clock;
    if (useThreadedOutput) {
        unsigned int generation;
        {
            // start a new generation so late completions of the previous
            // frame's work don't count toward this frame.  Stale items are
            // left in the queue: workers will still send them (late) so a
            // frame that carried a change the dedup logic won't resend is
            // never silently dropped.
            std::unique_lock<std::mutex> lock(workMutex);
            generation = ++workGeneration;
            doneWorkCount = 0;
        }
        int total = 0;
        auto t1 = clock.now();
        for (auto& msgs : messages.messages) {
            if (!msgs.second.empty() && msgs.first < LATE_MESSAGES_START) {
                std::shared_ptr<SendSocketInfo> socketInfo = findOrCreateSocket(msgs.first);
                std::unique_lock<std::mutex> lock(workMutex);
                workQueue.emplace_back(msgs.first, socketInfo, msgs.second, generation);
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
#ifndef PLATFORM_OSX
            // now make sure the buffers are drained for the early packets so that they are
            // fully received before we send the late packets (sync or broadcast).
            // Only worth the wait if there actually are late packets this frame.
            bool haveLate = false;
            for (auto& msgs : messages.messages) {
                if (!msgs.second.empty() && msgs.first >= LATE_MESSAGES_START) {
                    haveLate = true;
                    break;
                }
            }
            if (haveLate) {
                // Collect every early-message socket that may still have data
                // queued, then wait on them together: the drains overlap in
                // wall-clock, so the bound is one drain, not one per destination.
                std::vector<int> drainFds;
                bool anyPaced = false;
                for (auto& msgs : messages.messages) {
                    if (!msgs.second.empty() && msgs.first < LATE_MESSAGES_START) {
                        std::shared_ptr<SendSocketInfo> socketInfo = findOrCreateSocket(msgs.first);
                        // try_lock: a straggler worker still sending here means
                        // sync ordering is already lost for this destination;
                        // don't stall the frame thread waiting for it
                        std::unique_lock<std::mutex> lg(socketInfo->sendLock, std::try_to_lock);
                        if (lg.owns_lock() && !socketInfo->sockets.empty()) {
                            drainFds.push_back(socketInfo->sockets[socketInfo->curSocket]);
                            anyPaced |= socketInfo->paced;
                        }
                    }
                }
                // Paced packets sit in the fq qdisc for several ms; sleep
                // proportionally to the deepest queue instead of busy-polling.
                // The bound must cover a full send buffer draining at the
                // pacing rate (~34ms for 384KB at 90Mbps) or the sync packet
                // can beat the tail of the data it synchronizes.
                int totalBudget = anyPaced ? 35000 : 5000;
                int waited = 0;
                while (waited < totalBudget) {
                    int maxBytes = 0;
                    for (int fd : drainFds) {
                        int b = 0;
                        if (ioctl(fd, SIOCOUTQ, &b) == 0 && b > maxBytes) {
                            maxBytes = b;
                        }
                    }
                    if (maxBytes <= 1024) {
                        break;
                    }
                    int us = 100;
                    if (anyPaced && pacingDrainRate > 0) {
                        us = std::clamp((int)(((uint64_t)maxBytes * 8000000ULL) / pacingDrainRate), 100, 2000);
                    }
                    us = std::min(us, totalBudget - waited);
                    std::this_thread::sleep_for(std::chrono::microseconds(us));
                    waited += us;
                }
            }
#endif
            // now output the LATE/Broadcast packets (likely sync packets)
            for (auto& msgs : messages.messages) {
                if (!msgs.second.empty()) {
                    std::shared_ptr<SendSocketInfo> socketInfo = findOrCreateSocket(msgs.first);
                    if (msgs.first >= LATE_MESSAGES_START) {
                        t1 = clock.now();
                        int outputCount = SendMessages(msgs.first, socketInfo.get(), msgs.second);
                        t2 = clock.now();
                        long diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
                        if ((outputCount != msgs.second.size()) || (diff > 100)) {
                            socketInfo->errCount++;

                            // failed to send all messages or it took more than 100ms to send them
                            LogErr(VB_CHANNELOUT, "sendmmsg() failed for UDP output (IP: %s   output count: %d/%d   time: %u ms    errCount: %d) with error: %d   %s\n",
                                   HexToIP(msgs.first).c_str(),
                                   outputCount, msgs.second.size(), diff, socketInfo->errCount.load(),
                                   errno,
                                   FPPstrerror(errno));
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
            std::shared_ptr<SendSocketInfo> socketInfo = findOrCreateSocket(msgs.first);
            auto t1 = clock.now();
            int outputCount = SendMessages(msgs.first, socketInfo.get(), msgs.second);
            auto t2 = clock.now();
            long diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
            if ((outputCount != msgs.second.size()) || (diff > 100)) {
                socketInfo->errCount++;

                // failed to send all messages or it took more than 100ms to send them
                LogErr(VB_CHANNELOUT, "sendmmsg() failed for UDP output (IP: %s   output count: %d/%d   time: %u ms    errCount: %d) with error: %d   %s\n",
                       HexToIP(msgs.first).c_str(),
                       outputCount, msgs.second.size(), diff, socketInfo->errCount.load(),
                       errno,
                       FPPstrerror(errno));

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

static const std::string LOCAL_DROP_WARNING = "The network interface is dropping outgoing packets before they reach the wire. The link may be down, saturated, or misconfigured. See logs for details.";

#ifndef PLATFORM_OSX
// Total packets dropped by the qdiscs on the ethernet interfaces.  This is
// the only place fq enqueue drops (e.g. flow_limit) are visible: they return
// NET_XMIT_CN, which UDP reports to the sender as success, so neither
// SndbufErrors nor tx_dropped ever increments for them.
static uint64_t qdiscDroppedCount() {
    uint64_t total = 0;
    for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net/")) {
        std::string dev = entry.path().filename();
        if (!IsShowEthernetInterface(dev)) {
            continue;
        }
        std::string cmd = TcPath() + " -s qdisc show dev " + dev + " 2>/dev/null";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512];
            while (fgets(buf, sizeof(buf), pipe)) {
                const char* d = strstr(buf, "(dropped ");
                if (d) {
                    total += strtoull(d + 9, nullptr, 10);
                }
            }
            pclose(pipe);
        }
    }
    return total;
}
#endif

void UDPOutput::CheckLocalDrops() {
#ifndef PLATFORM_OSX
    // The protocols have no acks, so the only drops we can actually see are the
    // local ones.  Surface them so users can tell "player is dropping" apart
    // from "switch/controller is dropping".
    uint64_t sndbufErrors = 0;
    std::istringstream snmp(GetFileContents("/proc/net/snmp"));
    std::string line;
    std::vector<std::string> headers;
    while (std::getline(snmp, line)) {
        if (line.rfind("Udp:", 0) == 0) {
            std::istringstream ss(line);
            std::vector<std::string> tokens;
            std::string token;
            while (ss >> token) {
                tokens.push_back(token);
            }
            if (headers.empty()) {
                headers = tokens;
            } else {
                for (size_t x = 1; x < headers.size() && x < tokens.size(); x++) {
                    if (headers[x] == "SndbufErrors") {
                        // strtoull rather than stoull: this runs on a detached
                        // thread where an unexpected exception is fatal
                        sndbufErrors = strtoull(tokens[x].c_str(), nullptr, 10);
                    }
                }
                break;
            }
        }
    }
    uint64_t txDropped = strtoull(GetFileContents("/sys/class/net/" + outInterface + "/statistics/tx_dropped").c_str(), nullptr, 10);
    uint64_t qdiscDropped = qdiscDroppedCount();

    // counters can go backwards if the interface bounces or a driver reloads;
    // rebase silently rather than computing a huge unsigned delta
    if (statBaselineValid && sndbufErrors >= lastSndbufErrors && txDropped >= lastTxDropped && qdiscDropped >= lastQdiscDropped) {
        uint64_t sbDelta = sndbufErrors - lastSndbufErrors;
        uint64_t txDelta = txDropped - lastTxDropped;
        uint64_t qdDelta = qdiscDropped - lastQdiscDropped;
        if (sbDelta) {
            // host-wide counter (covers every UDP socket of every process),
            // so log it as a diagnostic breadcrumb but don't raise a warning
            LogWarn(VB_CHANNELOUT, "System-wide UDP send buffer errors since last check: %llu (may include non-FPP traffic)\n",
                    (unsigned long long)sbDelta);
        }
        if (txDelta || qdDelta) {
            LogWarn(VB_CHANNELOUT, "Local packet drops since last check: %llu on interface %s, %llu in ethernet qdiscs\n",
                    (unsigned long long)txDelta, outInterface.c_str(), (unsigned long long)qdDelta);
            if (!dropWarningActive) {
                WarningHolder::AddWarning(57, LOCAL_DROP_WARNING);
                dropWarningActive = true;
            }
        } else if (dropWarningActive) {
            WarningHolder::RemoveWarning(57, LOCAL_DROP_WARNING);
            dropWarningActive = false;
        }
    }
    lastSndbufErrors = sndbufErrors;
    lastTxDropped = txDropped;
    lastQdiscDropped = qdiscDropped;
    statBaselineValid = true;
#endif
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
                    WarningHolder::RemoveWarning(27, createWarning(o->ipAddress, o->GetOutputTypeString(), o->description));
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
                                    WarningHolder::RemoveWarning(27, createWarning(o->ipAddress, o->GetOutputTypeString(), o->description));
                                    LogWarn(VB_CHANNELOUT, "Could ping host %s, re-adding to outputs\n",
                                            o->ipAddress.c_str());
                                }
                            }
                        });
                    } else if (o->failCount >= 3) {
                        // three pings an HEAD request failed, mark invalid
                        if (o->valid) {
                            WarningHolder::AddWarning(27, createWarning(o->ipAddress, o->GetOutputTypeString(), o->description));
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
    LogDebug(VB_CHANNELOUT, "    Needs Broadcast  : %d\n", needsBroadcast);
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
std::shared_ptr<SendSocketInfo> UDPOutput::findOrCreateSocket(unsigned int socketKey, int sc) {
    auto it = messages.sendSockets.find(socketKey);
    std::shared_ptr<SendSocketInfo> info;
    if (it == messages.sendSockets.end()) {
        info = std::make_shared<SendSocketInfo>();
        messages.sendSockets[socketKey] = info;
    } else {
        info = it->second;
    }

    // try_lock: if a straggler worker from a previous frame is still sending
    // to this destination, its sockets already exist and the pacing check can
    // wait a frame - nothing here is worth stalling the frame thread on
    std::unique_lock<std::mutex> lg(info->sendLock, std::try_to_lock);
    if (!lg.owns_lock()) {
        return info;
    }
    if (info->sockets.empty()) {
        bool broadCast = socketKey == BROADCAST_MESSAGES_KEY;
        bool multiCast = socketKey == MULTICAST_MESSAGES_KEY || socketKey == LATE_MULTICAST_MESSAGES_KEY;
        for (int x = info->sockets.size(); x < sc; x++) {
            int s = createSocket(0, broadCast, multiCast);
            info->sockets.push_back(s);
        }
    }
#if !defined(PLATFORM_OSX) && defined(SO_MAX_PACING_RATE)
    // Pacing is only applied to unicast destination sockets: the key IS
    // the destination IP, so the rate cap is truly per controller, and we
    // can check that the route's egress interface actually has fq (wifi
    // and other non-fq routes keep the legacy drain-based pacing).  The
    // shared multicast socket is left unpaced - it carries the aggregate
    // of all multicast controllers so a per-controller rate would be wrong.
    if (pacingEnabled && !info->pacedChecked &&
        socketKey > ARTNET_MESSAGES_KEY && socketKey < LATE_MESSAGES_START) {
        if (info->pacedCheckCountdown > 0) {
            --info->pacedCheckCountdown;
        } else {
            std::string iface = egressInterfaceFor((in_addr_t)socketKey);
            if (!iface.empty()) {
                // only latch the decision once the route has resolved; during
                // startup the route may not exist yet (interface waiting on
                // DHCP) and we want to try again later or on an address change
                info->pacedChecked = true;
                // per-destination rate override (e.g. a slower cap for an
                // ESP32-class controller, or a faster one for a gigabit FPP
                // instance) falls back to the global pacingRate.  ~0U in the
                // map means the controller was explicitly set to unpaced.
                unsigned int destRate = pacingRate;
                auto rit = perDestPacingRate.find((in_addr_t)socketKey);
                if (rit != perDestPacingRate.end()) {
                    destRate = rit->second;
                }
                // pace only if this destination actually has a rate cap (a
                // global/override rate that isn't 0 or explicitly-unpaced) AND
                // the route egresses an fq interface
                bool shouldPace = destRate != 0 && destRate != ~0U && interfaceHasFQ(iface);
                if (shouldPace != info->paced) {
                    LogInfo(VB_CHANNELOUT, "UDP output to %s via %s is %s\n",
                            HexToIP(socketKey).c_str(), iface.c_str(),
                            shouldPace ? "paced" : "not paced (no fq qdisc on egress)");
                    info->paced = shouldPace;
                    // ~0 = unlimited; used when un-pacing after the route
                    // moved to a non-fq interface (e.g. eth -> wifi failover)
                    unsigned int rate = shouldPace ? destRate : ~0U;
                    // paced sockets need the buffer to hold a full frame for
                    // the qdisc to clock out; unpaced revert to the small
                    // buffer the drain-based pacing depends on
                    int bufSize = shouldPace ? pacedSndBufSize() : ((blockingOutput ? 4096 : (MSGS_PER_SENDMMSG * 1511)) - 1);
                    for (int s : info->sockets) {
                        setsockopt(s, SOL_SOCKET, SO_MAX_PACING_RATE, &rate, sizeof(rate));
                        setsockopt(s, SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize));
                    }
                }
            } else {
                // no route: don't re-run the lookup syscalls on the hot path
                // every frame, retry after a few seconds worth of frames
                info->pacedCheckCountdown = 150;
            }
        }
    }
#endif
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

    errno = 0;
    // Disable loopback so I do not receive my own datagrams.
    char loopch = 0;
    if (setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&loopch, sizeof(loopch)) < 0) {
        LogErr(VB_CHANNELOUT, "Error setting IP_MULTICAST_LOOP error\n");
        close(sendSocket);
        return -1;
    }
    // Mark show traffic as Expedited Forwarding (DSCP EF).  Managed switches
    // prioritize it out of the box and on wifi it maps to the WMM voice queue.
    int tos = 0xB8;
    setsockopt(sendSocket, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
#ifndef PLATFORM_OSX
    // ahead of fppd's own HTTP/MQTT/curl traffic in the local qdisc
    int prio = 6;
    setsockopt(sendSocket, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio));
#endif
    // make sure the send buffer is actually set to a reasonable size for non-blocking mode
    // (paced sockets get a larger buffer in findOrCreateSocket)
    int bufSize = (blockingOutput ? 4096 : (MSGS_PER_SENDMMSG * 1511)) - 1;
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
            LogErr(VB_SYNC, "Error setting SO_BROADCAST: \n", FPPstrerror(errno));
            exit(1);
        }
#ifndef PLATFORM_OSX
    } else if (multiCast) {
        static struct sockaddr_in address;
        memcpy(&address, &localAddress, sizeof(localAddress));
        address.sin_port = ntohs(port);

        if (setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_IF, (char*)&address, sizeof(address)) < 0) {
            LogErr(VB_CHANNELOUT, "Error setting IP_MULTICAST_IF error\n");
        }
#endif
    }
    if (broadCast || multiCast) {
        static struct sockaddr_in address;
        memcpy(&address, &localAddress, sizeof(localAddress));
        address.sin_port = ntohs(port);
        if (bind(sendSocket, (struct sockaddr*)&address, sizeof(struct sockaddr_in)) == -1) {
            LogErr(VB_CHANNELOUT, "Error in bind:errno=%d, %s\n", errno, FPPstrerror(errno));
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

    if (needsBroadcast) {
        if (rv) {
            LogErr(VB_CHANNELOUT, "Invalid interface %s\n", outInterface.c_str());
            WarningHolder::AddWarning(22, "Invalid interface for UDP broadcast/multicast: " + outInterface);
            return -1;
        }

        if (strlen(E131LocalAddress) > 3 && E131LocalAddress[0] == '1' && E131LocalAddress[1] == '2' && E131LocalAddress[2] == '7') {
            // the entire 127.* subnet is localhost
            return -1;
        }

        int broadcastSocket = createSocket(0, true);
        messages.ForceSocket(BROADCAST_MESSAGES_KEY, broadcastSocket);
    }
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
