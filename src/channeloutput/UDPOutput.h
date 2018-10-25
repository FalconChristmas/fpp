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
#include <string>
#include <thread>
#include <mutex>

#include <sys/socket.h>
#include <jsoncpp/json/json.h>

#include "ChannelOutputBase.h"



class UDPOutputData {
public:
    UDPOutputData(const Json::Value &config);
    virtual ~UDPOutputData();
    
    virtual bool IsPingable() = 0;
    virtual void PrepareData(unsigned char *channelData) = 0;

    // unicast and multicast messages for data
    virtual void CreateMessages(std::vector<struct mmsghdr> &udpMsgs) {}
    
    // purely broadcast data
    virtual void CreateBroadcastMessages(std::vector<struct mmsghdr> &bMsgs) {}
    
    // purely broadcast messages sent after the data (sync packets for example)
    virtual void AddPostDataMessages(std::vector<struct mmsghdr> &bMsgs) {}

    virtual void DumpConfig() = 0;
    
    std::string   description;
    bool          active;
    int           startChannel;
    int           channelCount;
    int           type;
    std::string   ipAddress;
    bool          valid;
};


class UDPOutput : public ChannelOutputBase {
public:
    UDPOutput(unsigned int startChannel, unsigned int channelCount);
    ~UDPOutput();
    
    
    int  Init(Json::Value config);
    int  Close(void);
    
    void PrepData(unsigned char *channelData);
    int  SendData(unsigned char *channelData);
    
    void DumpConfig(void);

    void BackgroundThreadPing();

    virtual void GetRequiredChannelRange(int &min, int & max);
private:
    int SendMessages(int socket, std::vector<struct mmsghdr> &sendmsgs);
    bool InitNetwork();
    void PingControllers();
    void RebuildOutputMessageLists();
    
    int sendSocket;
    int broadcastSocket;
    bool enabled;
    
    std::list<UDPOutputData*> outputs;
    std::vector<struct mmsghdr> udpMsgs;
    std::vector<struct mmsghdr> broadcastMsgs;
    
    std::thread *pingThread;
    volatile bool runDisabledPings;
    volatile bool rebuildOutputLists;
    std::mutex invalidOutputsMutex;
    std::list<UDPOutputData*> invalidOutputs;
};

#endif
