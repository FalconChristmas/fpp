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

#ifndef _IPOUTPUT_H
#define _IPOUTPUT_H

#include <list>
#include <vector>
#include <map>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

#include <sys/socket.h>
#include <netinet/in.h>
#include <jsoncpp/json/json.h>

#include "ChannelOutputBase.h"

typedef void CURLM;

#define MULTICAST_MESSAGES_KEY 0x00000001
#define ANY_MESSAGES_KEY       0x00000002
#define LATE_MULTICAST_MESSAGES_KEY 0xFFFFFFFE
#define BROADCAST_MESSAGES_KEY 0xFFFFFFFF

class SendSocketInfo;

class UDPOutputMessages {
public:
    UDPOutputMessages();
    ~UDPOutputMessages();

    void ForceSocket(unsigned int key, int socket);
    int GetSocket(unsigned int key);
    
    std::vector<struct mmsghdr> &GetMessages(unsigned int key);
    std::vector<struct mmsghdr> &operator[](unsigned int key) {return GetMessages(key);}
        
private:
    std::map<unsigned int, std::vector<struct mmsghdr>> messages;
    std::map<unsigned int, SendSocketInfo*> sendSockets;
    
    void clearMessages();
    void clearSockets();
    
    friend class UDPOutput;
};

class UDPOutputData {
public:
    UDPOutputData(const Json::Value &config);
    virtual ~UDPOutputData();
    
    virtual bool IsPingable() = 0;
    virtual bool Monitor() const { return monitor; }
    virtual void PrepareData(unsigned char *channelData, UDPOutputMessages &msgs) = 0;
    virtual void PostPrepareData(unsigned char *channelData, UDPOutputMessages &msgs) {}

    virtual void DumpConfig() = 0;
    
    virtual void GetRequiredChannelRange(int &min, int & max) {
        min = startChannel - 1;
        max = startChannel + channelCount - 1;
    }
    
    virtual const std::string &GetOutputTypeString() const;
    
    static in_addr_t toInetAddr(const std::string &ip, bool &valid);
    
    
    std::string   description;
    bool          active;
    int           startChannel;
    int           channelCount;
    int           type;
    std::string   ipAddress;
    bool          valid;
    bool          monitor;
    
    int           failCount;

    
    UDPOutputData(UDPOutputData const &) = delete;
    void operator=(UDPOutputData const &x) = delete;
    
protected:
    void SaveFrame(unsigned char *channelData);
    bool NeedToOutputFrame(unsigned char *channelData, int startChannel, int start, int count);
    bool           deDuplicate;
    int            skippedFrames;
    unsigned char* lastData;

};

class UDPOutput : public ChannelOutputBase {
public:
    UDPOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~UDPOutput();
    
    virtual int  Init(Json::Value config) override;
    virtual int  Close(void) override;
    
    virtual void PrepData(unsigned char *channelData) override;
    virtual int  SendData(unsigned char *channelData) override;
    
    virtual void DumpConfig(void) override;

    void BackgroundThreadPing();
    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) override;
    
    void addOutput(UDPOutputData*);

    int createSocket(int port = 0, bool broadCast = false);

    static UDPOutput *INSTANCE;
private:
    int SendMessages(unsigned int key, SendSocketInfo *socketInfo, std::vector<struct mmsghdr> &sendmsgs);
    bool InitNetwork();
    SendSocketInfo *findOrCreateSocket(unsigned int key, int sc = 1);
    void CloseNetwork();
    
    struct sockaddr_in localAddress;
    std::mutex socketMutex;
    UDPOutputMessages messages;
    bool enabled;
    
    std::list<UDPOutputData*> outputs;

    int  networkCallbackId;

    
    bool PingControllers();
    volatile bool runPingThread;
    std::thread *pingThread;
    std::mutex pingThreadMutex;
    std::condition_variable pingThreadCondition;
    CURLM *m_curlm;
};

#endif
