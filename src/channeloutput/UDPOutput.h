#pragma once
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

#include <atomic>
#include "fpp-json-fwd.h"
#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../SysSocket.h"
#include <netinet/in.h>

#include "ChannelOutput.h"

typedef void CURLM;

#define MULTICAST_MESSAGES_KEY 0x00000001
#define ANY_MESSAGES_KEY 0x00000002
#define ARTNET_MESSAGES_KEY 0x00000003

#define LATE_MESSAGES_START 0xFFFFFFF0
#define E131_SYNC_KEY 0xFFFFFFFC
#define ARTNET_SYNC_KEY 0xFFFFFFFD
#define LATE_MULTICAST_MESSAGES_KEY 0xFFFFFFFE
#define BROADCAST_MESSAGES_KEY 0xFFFFFFFF

class SendSocketInfo;

class UDPOutputMessages {
public:
    UDPOutputMessages();
    ~UDPOutputMessages();

    void ForceSocket(unsigned int key, int socket, bool preventClose = false);
    int GetSocket(unsigned int key);

    std::vector<struct mmsghdr>& GetMessages(unsigned int key);
    std::vector<struct mmsghdr>& operator[](unsigned int key) { return GetMessages(key); }

private:
    std::map<unsigned int, std::vector<struct mmsghdr>> messages;
    std::map<unsigned int, std::shared_ptr<SendSocketInfo>> sendSockets;

    void clearMessages();
    void clearSockets();

    friend class UDPOutput;
};

class UDPOutputData {
public:
    UDPOutputData(const Json::Value& config);
    virtual ~UDPOutputData();

    virtual bool IsPingable() = 0;
    virtual bool Monitor() const { return monitor; }
    virtual void PrepareData(unsigned char* channelData, UDPOutputMessages& msgs) = 0;
    virtual void PostPrepareData(unsigned char* channelData, UDPOutputMessages& msgs) {}

    virtual void DumpConfig() = 0;

    virtual void GetRequiredChannelRange(int& min, int& max) {
        min = startChannel - 1;
        max = startChannel + channelCount - 1;
    }

    virtual void StartingOutput() {}
    virtual void StoppingOutput() {}

    virtual const std::string& GetOutputTypeString() const;

    static in_addr_t toInetAddr(const std::string& ip, bool& valid);

    std::string description;
    bool active;
    int startChannel;
    int channelCount;
    int type;
    std::string ipAddress;
    bool valid;
    bool monitor;

    // per-destination pacing override in Mbps; -1 means "use the global rate".
    // 0 means this controller is explicitly unpaced (line rate).  A mix of a
    // gigabit FPP instance and a 30Mbps-class ESP32 controller can each get an
    // appropriate cap instead of one global setting for all of them.
    int pacingRateMbps = -1;

    int failCount;

    // Set while StartingOutput() is owed its StoppingOutput().  valid can change
    // at runtime (a failed ping clears it), so it cannot say which outputs were
    // started; UDPOutput pairs the two calls through this instead.
    bool started = false;

    UDPOutputData(UDPOutputData const&) = delete;
    void operator=(UDPOutputData const& x) = delete;

protected:
    void SaveFrame(unsigned char* channelData, int len);
    bool NeedToOutputFrame(unsigned char* channelData, int startChannel, int savedIdx, int count);
    bool deDuplicate = false;
    int skippedFrames;
    unsigned char* lastData;
};

class UDPOutput : public ChannelOutput {
public:
    UDPOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~UDPOutput();

    virtual std::string GetOutputType() const override {
        return "UDPOutput";
    }
    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual void PrepData(unsigned char* channelData) override;
    virtual int SendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

    void addOutput(UDPOutputData*);

    int createSocket(int port = 0, bool broadCast = false, bool multiCast = false);

    static UDPOutput* INSTANCE;

    void BackgroundOutputWork();

    virtual void StartingOutput() override;
    virtual void StoppingOutput() override;

private:
    int SendMessages(unsigned int key, SendSocketInfo* socketInfo, std::vector<struct mmsghdr>& sendmsgs);
    struct sockaddr_in localAddress;
    std::string outInterface;
    bool needsBroadcast = false;
    bool interfaceUp;

    bool InitNetwork();
    std::shared_ptr<SendSocketInfo> findOrCreateSocket(unsigned int key, int sc = 1);
    void CloseNetwork();

    std::mutex socketMutex;
    UDPOutputMessages messages;
    bool enabled;

    std::list<UDPOutputData*> outputs;

    int networkCallbackId;

    void PingControllers(bool failedOnly);
    std::atomic_int failedCount;
    std::string HexToIP(unsigned int hex);

    void CheckLocalDrops();

    // A WorkItem owns a copy of the mmsghdr list and a reference on the socket
    // info so a worker that outlives its frame (stuck send to a dead controller)
    // cannot hit freed memory when PrepData rebuilds the message vectors or
    // CloseNetwork drops the sockets.  Note the copy is shallow: the iovecs
    // still point at per-output buffers, so a late send may transmit the
    // *current* frame's bytes - acceptable, and no worse than it ever was.
    // generation identifies which frame queued it.
    class WorkItem {
    public:
        WorkItem(unsigned int i, std::shared_ptr<SendSocketInfo> si, const std::vector<struct mmsghdr>& m, unsigned int g) :
            id(i),
            socketInfo(si),
            msgs(m),
            generation(g) {}
        unsigned int id;
        std::shared_ptr<SendSocketInfo> socketInfo;
        std::vector<struct mmsghdr> msgs;
        unsigned int generation;
    };

    std::mutex workMutex;
    std::condition_variable workSignal;
    std::list<WorkItem> workQueue;
    std::atomic_int doneWorkCount;
    std::atomic_int numWorkThreads;
    std::atomic_uint workGeneration;
    volatile bool runWorkThreads;
    bool useThreadedOutput;
    bool blockingOutput;

    // fq/SO_MAX_PACING_RATE based pacing; when active the kernel clocks packets
    // out at pacingRate per socket and the SIOCOUTQ drain waits are skipped
    bool pacingEnabled = false;
    unsigned int pacingRate = 0; // bytes per second (global default)

    // rate (bytes/sec) used only for the pre-sync drain-sleep estimate; the
    // smallest positive rate among the global rate and all per-destination
    // overrides, so a mixed set sleeps long enough for the slowest queue and we
    // never divide by a zero global rate when only overrides are paced.
    unsigned int pacingDrainRate = 0;

    // per-destination-IP pacing rate in bytes/sec, built in Init() from the
    // per-output pacingRate overrides.  A destination not present here uses the
    // global pacingRate; a value of ~0U means the controller was explicitly set
    // to unpaced.  The socket is shared per destination IP, so when several
    // outputs target the same controller with differing overrides the most
    // conservative (lowest) rate wins.
    std::map<in_addr_t, unsigned int> perDestPacingRate;

    // local drop diagnostics; the check runs on a short detached thread since
    // reading qdisc stats forks tc, which is too slow for the frame path
    int statCheckCounter = 0;
    std::atomic_bool statCheckRunning{ false };
    bool statBaselineValid = false;
    bool dropWarningActive = false;
    uint64_t lastSndbufErrors = 0;
    uint64_t lastTxDropped = 0;
    uint64_t lastQdiscDropped = 0;
};
