/*
 *   Falcon Player MultiSync header file  
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
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
 
#ifndef _MULTISYNC_H
#define _MULTISYNC_H

#include <mutex>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <jsoncpp/json/json.h>

#include "settings.h"


#define FPP_CTRL_PORT 32320
#define FPP_CTRL_CSV_PORT 32321

#define CTRL_PKT_CMD    0
#define CTRL_PKT_SYNC   1
#define CTRL_PKT_EVENT  2
#define CTRL_PKT_BLANK  3
#define CTRL_PKT_PING   4
#define CTRL_PKT_PLUGIN 5
#define CTRL_PKT_FPPCOMMAND 6

typedef struct __attribute__((packed)) {
	char     fppd[4];        // 'FPPD'
	uint8_t  pktType;        // Control Packet Type
	uint16_t extraDataLen;   // Length of extra data following Control Packet
} ControlPkt;

typedef struct {
	char   command[1];       // Null-terminated Command to process
} CommandPkt;

typedef struct {
	char eventID[6];         // Event ID MAJOR_MINOR, NULL terminated
} EventPkt;

#define SYNC_PKT_START 0
#define SYNC_PKT_STOP  1
#define SYNC_PKT_SYNC  2
#define SYNC_PKT_OPEN  3

#define SYNC_FILE_SEQ   0
#define SYNC_FILE_MEDIA 1

typedef struct __attribute__((packed)) {
	uint8_t  pktType;        // Sync Packet Type
	uint8_t  fileType;       // File Type being synced
	uint32_t frameNumber;    // Sequence frames displayed on Master
	float    secondsElapsed; // Seconds elapsed in file on Master
	char     filename[1];    // Null-terminated File Name to play on Slave
	                         // (data may continue past this header)
} SyncPkt;


typedef enum systemType {
	kSysTypeUnknown                      = 0x00,
	kSysTypeFPP                          = 0x01,
	kSysTypeFPPRaspberryPiA,
	kSysTypeFPPRaspberryPiB,
	kSysTypeFPPRaspberryPiAPlus,
	kSysTypeFPPRaspberryPiBPlus,
	kSysTypeFPPRaspberryPi2B,
	kSysTypeFPPRaspberryPi2BNew,
	kSysTypeFPPRaspberryPi3B,
	kSysTypeFPPRaspberryPi3BPlus,
	kSysTypeFPPRaspberryPiZero,
	kSysTypeFPPRaspberryPiZeroW,
    kSysTypeFPPRaspberryPi3APlus,
    kSysTypeFPPRaspberryPi4,
	kSysTypeFPPBeagleBoneBlack           = 0x41,
	kSysTypeFPPBeagleBoneBlackWireless   = 0x42,
	kSysTypeFPPBeagleBoneGreen           = 0x43,
	kSysTypeFPPBeagleBoneGreenWireless   = 0x44,
	kSysTypeFPPPocketBeagle              = 0x45,
    kSysTypeFPPSanCloudBeagleBoneEnhanced = 0x46,
	kSysTypeFalconController             = 0x80,
	kSysTypeFalconF16v2                  = 0x81,
	kSysTypeFalconF16v3                  = 0x82,
	kSysTypeFalconF48v1                  = 0x83,
	kSysTypeOtherSystem                  = 0xC0,
	kSysTypexSchedule                    = 0xC1,
    kSysTypeESPixelStick                 = 0xC2
} MultiSyncSystemType;

class MultiSyncSystem {
public:
    MultiSyncSystem() {
    }
	unsigned long        lastSeen = 0;
	std::string          lastSeenStr;
	MultiSyncSystemType  type = kSysTypeUnknown;
	unsigned int         majorVersion = 0;
	unsigned int         minorVersion = 0;
	FPPMode              fppMode = FPPMode::PLAYER_MODE;
	std::string          address;
	std::string          hostname;
	std::string          version;
	std::string          model;
    std::string          ranges;
	unsigned char        ipa = 0;
	unsigned char        ipb = 0;
	unsigned char        ipc = 0;
	unsigned char        ipd = 0;
    
    
    void update(MultiSyncSystemType type,
                unsigned int majorVersion, unsigned int minorVersion,
                FPPMode fppMode,
                const std::string &address,
                const std::string &hostname,
                const std::string &version,
                const std::string &model,
                const std::string &ranges);
    Json::Value toJSON(bool local, bool timestamps);
};


class MultiSyncPlugin {
    public:
    MultiSyncPlugin() {}
    virtual ~MultiSyncPlugin() {}
    
    virtual void SendSeqOpenPacket(const std::string &filename) {}
    virtual void SendSeqSyncStartPacket(const std::string &filename) {}
    virtual void SendSeqSyncStopPacket(const std::string &filename) {}
    virtual void SendSeqSyncPacket(const std::string &filename, int frames, float seconds) {}
    virtual void ShutdownSync(void) {}
    
    virtual void SendMediaOpenPacket(const std::string &filename) {}
    virtual void SendMediaSyncStartPacket(const std::string &filename) {}
    virtual void SendMediaSyncStopPacket(const std::string &filename) {}
    virtual void SendMediaSyncPacket(const std::string &filename, float seconds) {}
    
    virtual void SendEventPacket(const std::string &eventID) {}
    virtual void SendBlankingDataPacket(void) {}
    
    virtual void SendPluginData(const std::string &name, const uint8_t *data, int len) {}
    virtual void SendFPPCommandPacket(const std::string &host, const std::string &cmd, const std::vector<std::string> &args) {}
};


class NetInterfaceInfo {
public:
    NetInterfaceInfo();
    ~NetInterfaceInfo();
    
    std::string interfaceName;
    std::string interfaceAddress;
    uint32_t    address;
    uint32_t    broadcastAddress;
    int         multicastSocket;
};


class MultiSync {
  public:
    static MultiSync INSTANCE;
    
	MultiSync();
	~MultiSync();

	int  Init(void);

	int  GetControlSocket(void) { return m_receiveSock; }
	void ProcessControlPacket(void);

	void UpdateSystem(MultiSyncSystemType type,
                      unsigned int majorVersion,
                      unsigned int minorVersion,
                      FPPMode fppMode,
                      const std::string &address,
                      const std::string &hostname,
                      const std::string &version,
                      const std::string &model,
                      const std::string &range);

	Json::Value GetSystems(bool localOnly = false, bool timestamps = true);

	void Ping(int discover = 0, bool broadcast = true);
	void Discover(void) { Ping(1); }
    void PeriodicPing();

    void SendSeqOpenPacket(const std::string &filename);
	void SendSeqSyncStartPacket(const std::string &filename);
	void SendSeqSyncStopPacket(const std::string &filename);
	void SendSeqSyncPacket(const std::string &filename, int frames, float seconds);
	void ShutdownSync(void);

    void SendMediaOpenPacket(const std::string &filename);
	void SendMediaSyncStartPacket(const std::string &filename);
	void SendMediaSyncStopPacket(const std::string &filename);
	void SendMediaSyncPacket(const std::string &filename, float seconds);

	void SendEventPacket(const std::string &eventID);
    void SendFPPCommandPacket(const std::string &host, const std::string &cmd, const std::vector<std::string> &args);
	void SendBlankingDataPacket(void);

    void SendPluginData(const std::string &name, const uint8_t *data, int len);

    void addMultiSyncPlugin(MultiSyncPlugin *p) {
        m_plugins.push_back(p);
    }
    
    
    void OpenSyncedSequence(const char *filename);
    void StartSyncedSequence(const char *filename);
    void StopSyncedSequence(const char *filename);
    void SyncSyncedSequence(const char *filename, int frameNumber, float secondsElapsed);
    
    void OpenSyncedMedia(const char *filename);
    void StartSyncedMedia(const char *filename);
    void StopSyncedMedia(const char *filename);
    void SyncSyncedMedia(const char *filename, int frameNumber, float secondsElapsed);

    int OpenControlSockets();

    static std::string GetTypeString(MultiSyncSystemType type, bool local = false);

  private:
    bool isSupportedForMultisync(const char *address, const char *intface);
    
    void setupMulticastReceive();
    void PingSingleRemote(MultiSyncSystem &sys);
    int CreatePingPacket(MultiSyncSystem &sys, char* outBuf, int discover);

	MultiSyncSystemType ModelStringToType(std::string model);
	bool FillLocalSystemInfo(void);
	std::string GetHardwareModel(void);

	int  OpenBroadcastSocket(void);
	void SendBroadcastPacket(void *outBuf, int len);
	void SendControlPacket(void *outBuf, int len);
    void SendMulticastPacket(void *outBuf, int len);
    bool FillInInterfaces();
    bool RemoveInterface(const std::string &interface);

	int  OpenCSVControlSockets(void);
	void SendCSVControlPacket(void *outBuf, int len);

	void InitControlPacket(ControlPkt *pkt);

	int  OpenReceiveSocket(void);


	void ProcessSyncPacket(ControlPkt *pkt, int len);
	void ProcessCommandPacket(ControlPkt *pkt, int len);
	void ProcessEventPacket(ControlPkt *pkt, int len);
	void ProcessPingPacket(ControlPkt *pkt, int len);
    void ProcessPluginPacket(ControlPkt *pkt, int len);
    void ProcessFPPCommandPacket(ControlPkt *pkt, int len);

	std::mutex                   m_systemsLock;
	std::vector<MultiSyncSystem> m_localSystems;
    std::vector<MultiSyncSystem> m_remoteSystems;

    std::map<std::string, NetInterfaceInfo> m_interfaces;
    bool m_sendMulticast;
    bool m_sendBroadcast;
    
    
	int  m_broadcastSock;
	int  m_controlSock;
	int  m_controlCSVSock;
	int  m_receiveSock;

    
	std::string         m_hostname;
	struct sockaddr_in  m_srcAddr;
	struct sockaddr_in  m_receiveSrcAddr;
	std::mutex          m_socketLock;

    unsigned long       m_lastPingTime;
    unsigned long       m_lastCheckTime;
    int m_lastMediaHalfSecond;
    int m_lastFrame;
    int m_lastFrameSent;

	float  m_remoteOffset;

    struct iovec m_destIovec;
    std::vector<struct mmsghdr> m_destMsgs;
	std::vector<struct sockaddr_in> m_destAddr;
    
    struct iovec m_destIovecCSV;
    std::vector<struct mmsghdr> m_destMsgsCSV;
	std::vector<struct sockaddr_in> m_destAddrCSV;
    
    std::vector<MultiSyncPlugin *> m_plugins;
    
    #define MAX_MS_RCV_MSG 12
    #define MAX_MS_RCV_BUFSIZE 1500
    struct mmsghdr rcvMsgs[MAX_MS_RCV_MSG];
    struct iovec rcvIovecs[MAX_MS_RCV_MSG];
    unsigned char rcvBuffers[MAX_MS_RCV_MSG][MAX_MS_RCV_BUFSIZE+1];
    unsigned char rcvCmbuf[MAX_MS_RCV_MSG][0x100];
    struct sockaddr_storage rcvSrcAddr[MAX_MS_RCV_MSG];
};

extern MultiSync *multiSync;

#endif /* _MULTISYNC_H */
