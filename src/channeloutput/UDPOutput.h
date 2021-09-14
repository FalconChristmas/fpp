#pragma once
/*
 *   IP Channel Output driver for Falcon Player (FPP)
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
#include <atomic>
#include <condition_variable>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <jsoncpp/json/json.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "ChannelOutputBase.h"

typedef void CURLM;

#define MULTICAST_MESSAGES_KEY 0x00000001
#define ANY_MESSAGES_KEY 0x00000002
#define LATE_MULTICAST_MESSAGES_KEY 0xFFFFFFFE
#define BROADCAST_MESSAGES_KEY 0xFFFFFFFF

class SendSocketInfo;

class UDPOutputMessages {
public:
    UDPOutputMessages();
    ~UDPOutputMessages();

    void ForceSocket(unsigned int key, int socket);
    int GetSocket(unsigned int key);

    std::vector<struct mmsghdr>& GetMessages(unsigned int key);
    std::vector<struct mmsghdr>& operator[](unsigned int key) { return GetMessages(key); }

private:
    std::map<unsigned int, std::vector<struct mmsghdr>> messages;
    std::map<unsigned int, SendSocketInfo*> sendSockets;

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

    int failCount;

    UDPOutputData(UDPOutputData const&) = delete;
    void operator=(UDPOutputData const& x) = delete;

protected:
    void SaveFrame(unsigned char* channelData, int len);
    bool NeedToOutputFrame(unsigned char* channelData, int startChannel, int savedIdx, int count);
    bool deDuplicate = false;
    int skippedFrames;
    unsigned char* lastData;
};

class UDPOutput : public ChannelOutputBase {
public:
    UDPOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~UDPOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual void PrepData(unsigned char* channelData) override;
    virtual int SendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    void BackgroundThreadPing();
    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

    void addOutput(UDPOutputData*);

    int createSocket(int port = 0, bool broadCast = false);

    static UDPOutput* INSTANCE;

    void BackgroundOutputWork();

private:
    int SendMessages(unsigned int key, SendSocketInfo* socketInfo, std::vector<struct mmsghdr>& sendmsgs);
    struct sockaddr_in localAddress;
    std::string e131Interface;

    bool InitNetwork();
    SendSocketInfo* findOrCreateSocket(unsigned int key, int sc = 1);
    void CloseNetwork();

    std::mutex socketMutex;
    UDPOutputMessages messages;
    bool enabled;

    std::list<UDPOutputData*> outputs;

    int networkCallbackId;

    bool PingControllers(bool failedOnly = false);
    volatile bool runPingThread;
    std::thread* pingThread;
    std::mutex pingThreadMutex;
    std::condition_variable pingThreadCondition;
    CURLM* m_curlm;

    class WorkItem {
    public:
        WorkItem(unsigned int i, SendSocketInfo* si, std::vector<struct mmsghdr>& m) :
            id(i),
            socketInfo(si),
            msgs(m) {}
        unsigned int id;
        SendSocketInfo* socketInfo;
        std::vector<struct mmsghdr>& msgs;
    };

    std::mutex workMutex;
    std::condition_variable workSignal;
    std::list<WorkItem> workQueue;
    std::atomic_int doneWorkCount;
    std::atomic_int numWorkThreads;
    volatile bool runWorkThreads;
    bool useThreadedOutput;
};
