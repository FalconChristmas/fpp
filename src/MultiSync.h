#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <netinet/in.h>
#include <sys/types.h>
#include <mutex>
#include <pthread.h>
#include <set>

#include "SysSocket.h"
#include "settings.h"

#define FPP_CTRL_PORT 32320

#define CTRL_PKT_CMD 0 // deprecated in favor of FPP Commands
#define CTRL_PKT_SYNC 1
#define CTRL_PKT_EVENT 2 // deprecated in favor of FPP Commands
#define CTRL_PKT_BLANK 3
#define CTRL_PKT_PING 4
#define CTRL_PKT_PLUGIN 5
#define CTRL_PKT_FPPCOMMAND 6

typedef struct __attribute__((packed)) {
    char fppd[4];          // 'FPPD'
    uint8_t pktType;       // Control Packet Type
    uint16_t extraDataLen; // Length of extra data following Control Packet
} ControlPkt;

typedef struct {
    char command[1]; // Null-terminated Command to process
} CommandPkt;

#define SYNC_PKT_START 0
#define SYNC_PKT_STOP 1
#define SYNC_PKT_SYNC 2
#define SYNC_PKT_OPEN 3

#define SYNC_FILE_SEQ 0
#define SYNC_FILE_MEDIA 1

typedef struct __attribute__((packed)) {
    uint8_t pktType;      // Sync Packet Type
    uint8_t fileType;     // File Type being synced
    uint32_t frameNumber; // Sequence frames displayed on Master
    float secondsElapsed; // Seconds elapsed in file on Master
    char filename[1];     // Null-terminated File Name to play on Slave
                          // (data may continue past this header)
} SyncPkt;

typedef enum systemType {
    kSysTypeUnknown = 0x00,
    kSysTypeFPP = 0x01,
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
    kSysTypeFPPRaspberryPi5,
    kSysTypeFPPRaspberryPiZero2W,
    kSysTypeFPPBeagleBoneBlack = 0x41,
    kSysTypeFPPBeagleBoneBlackWireless = 0x42,
    kSysTypeFPPBeagleBoneGreen = 0x43,
    kSysTypeFPPBeagleBoneGreenWireless = 0x44,
    kSysTypeFPPPocketBeagle = 0x45,
    kSysTypeFPPSanCloudBeagleBoneEnhanced = 0x46,
    kSysTypeFPPArmbian = 0x60,
    kSysTypeMacOS = 0x70,
    // Values under 0x80 are "FPP based" and run full FPP
    kSysTypeFalconController = 0x80,
    kSysTypeFalconF16v2 = 0x81,
    kSysTypeFalconF4v2_64Mb = 0x82,
    kSysTypeFalconF16v2R = 0x83,
    kSysTypeFalconF4v2 = 0x84,
    kSysTypeFalconF16v3 = 0x85,
    kSysTypeFalconF4v3 = 0x86,
    kSysTypeFalconF48 = 0x87,
    kSysTypeFalconF16v4 = 0x88,
    kSysTypeFalconF48v4 = 0x89,
    kSysTypeFalconF16v5 = 0x90,
    kSysTypeExperienceGP16 = 0xA0,
    kSysTypeExperienceGP8 = 0xA1,
    kSysTypeExperienceGLR = 0xA2,
    kSysTypeExperienceG16Pro = 0xA3,
    kSysTypeExperienceG32Pro = 0xA4,
    kSysTypeExperienceGenius = 0xAF,
    kSysTypeOtherSystem = 0xC0,
    kSysTypexSchedule = 0xC1,
    kSysTypeESPixelStick = 0xC2,
    kSysTypeESPixelStickESP32 = 0xC3,
    kSysTypeNonMultiSyncCapable = 0xF0,
    kSysTypeWLED = 0xFB,
    kSysTypeDIYLEDExpress = 0xFC,
    kSysTypeHinksPix = 0xFD,
    kSysTypeAlphaPix = 0xFE,
    kSysTypeSanDevices = 0xFF
} MultiSyncSystemType;

class MultiSyncSystem {
public:
    MultiSyncSystem() {
    }
    unsigned long lastSeen = 0;
    std::string lastSeenStr;
    bool multiSync = false;
    MultiSyncSystemType type = kSysTypeUnknown;
    unsigned int majorVersion = 0;
    unsigned int minorVersion = 0;
    FPPMode fppMode = FPPMode::PLAYER_MODE;
    bool sendingMultiSync = false;
    std::string address;
    std::string hostname;
    std::string version;
    std::string model;
    std::string ranges;
    std::string uuid;
    unsigned char ipa = 0;
    unsigned char ipb = 0;
    unsigned char ipc = 0;
    unsigned char ipd = 0;

    void update(MultiSyncSystemType type,
                unsigned int majorVersion, unsigned int minorVersion,
                FPPMode fppMode,
                const std::string& address,
                const std::string& hostname,
                const std::string& version,
                const std::string& model,
                const std::string& ranges,
                const std::string& uuid,
                const bool multiSync,
                const bool sendingMultiSync);
    Json::Value toJSON(bool local, bool timestamps);
};

class MultiSyncStats {
public:
    MultiSyncStats(std::string ip, std::string host);

    Json::Value toJSON();

    std::string sourceIP;
    std::string hostname;
    time_t lastReceiveTime;
    uint32_t pktCommand;
    uint32_t pktSyncSeqOpen;
    uint32_t pktSyncSeqStart;
    uint32_t pktSyncSeqStop;
    uint32_t pktSyncSeqSync;
    uint32_t pktSyncMedOpen;
    uint32_t pktSyncMedStart;
    uint32_t pktSyncMedStop;
    uint32_t pktSyncMedSync;
    uint32_t pktBlank;
    uint32_t pktPing;
    uint32_t pktPlugin;
    uint32_t pktFPPCommand;
    uint32_t pktError;
};

class MultiSyncPlugin {
public:
    MultiSyncPlugin() {}
    virtual ~MultiSyncPlugin() {}

    virtual void SendSeqOpenPacket(const std::string& filename) {}
    virtual void SendSeqSyncStartPacket(const std::string& filename) {}
    virtual void SendSeqSyncStopPacket(const std::string& filename) {}
    virtual void SendSeqSyncPacket(const std::string& filename, int frames, float seconds) {}
    virtual void ShutdownSync(void) {}

    virtual void SendMediaOpenPacket(const std::string& filename) {}
    virtual void SendMediaSyncStartPacket(const std::string& filename) {}
    virtual void SendMediaSyncStopPacket(const std::string& filename) {}
    virtual void SendMediaSyncPacket(const std::string& filename, float seconds) {}

    virtual void SendBlankingDataPacket(void) {}

    virtual void SendPluginData(const std::string& name, const uint8_t* data, int len) {}
    virtual void SendFPPCommandPacket(const std::string& host, const std::string& cmd, const std::vector<std::string>& args) {}
};

class NetInterfaceInfo {
public:
    NetInterfaceInfo();
    ~NetInterfaceInfo();

    std::string interfaceName;
    std::string interfaceAddress;
    uint32_t address;
    uint32_t broadcastAddress;
    int multicastSocket;
};

class MultiSync {
public:
    static MultiSync INSTANCE;

    MultiSync();
    ~MultiSync();

    int Init(void);

    bool isMultiSyncEnabled() { return m_multiSyncEnabled; }

    int GetControlSocket(void) { return m_receiveSock; }
    void ProcessControlPacket(bool pingOnly = false);

    void UpdateSystem(MultiSyncSystemType type,
                      unsigned int majorVersion,
                      unsigned int minorVersion,
                      FPPMode fppMode,
                      const std::string& address,
                      const std::string& hostname,
                      const std::string& version,
                      const std::string& model,
                      const std::string& range,
                      const std::string& uuid,
                      const bool multiSync,
                      const bool sendingMultiSync);

    Json::Value GetSystems(bool localOnly = false, bool timestamps = true);
    Json::Value GetSyncStats();
    void ResetSyncStats();

    void Ping(int discover = 0, bool broadcast = true);
    void PingSingleRemote(const char* address, int discover = 0);
    void Discover(void);
    void PeriodicPing();

    void SendSeqOpenPacket(const std::string& filename);
    void SendSeqSyncStartPacket(const std::string& filename);
    void SendSeqSyncStopPacket(const std::string& filename);
    void SendSeqSyncPacket(const std::string& filename, int frames, float seconds);
    void ShutdownSync(void);

    void SendMediaOpenPacket(const std::string& filename);
    void SendMediaSyncStartPacket(const std::string& filename);
    void SendMediaSyncStopPacket(const std::string& filename);
    void SendMediaSyncPacket(const std::string& filename, float seconds);

    void SendFPPCommandPacket(const std::string& host, const std::string& cmd, const std::vector<std::string>& args);
    void SendBlankingDataPacket(void);

    void SendPluginData(const std::string& name, const uint8_t* data, int len);

    void addMultiSyncPlugin(MultiSyncPlugin* p);
    void removeMultiSyncPlugin(MultiSyncPlugin* p);

    void OpenSyncedSequence(const char* filename);
    void StartSyncedSequence(const char* filename);
    void StopSyncedSequence(const char* filename);
    void SyncSyncedSequence(const char* filename, int frameNumber, float secondsElapsed);

    void OpenSyncedMedia(const char* filename);
    void StartSyncedMedia(const char* filename);
    void StopSyncedMedia(const char* filename);
    void SyncSyncedMedia(const char* filename, int frameNumber, float secondsElapsed);

    void SyncPlaylistToMS(uint64_t ms, const std::string& pl = "", bool sendSyncPackets = false);
    void SyncPlaylistToMS(uint64_t ms, int pos, const std::string& pl = "", bool sendSyncPackets = false);
    void SyncStopAll();

    int OpenControlSockets();

    static std::string GetTypeString(MultiSyncSystemType type, bool local = false);
    static MultiSyncSystemType ModelStringToType(std::string model);

    void StoreHTTPResponse(std::string* ipp, uint8_t* data, int sz);

    [[nodiscard]] std::vector<MultiSyncSystem> const& GetLocalSystems() { return m_localSystems; }
    [[nodiscard]] std::vector<MultiSyncSystem> const& GetRemoteSystems() { return m_remoteSystems; }

private:
    bool isSupportedForMultisync(const char* address, const char* intface);

    void setupMulticastReceive();
    void PingSingleRemote(MultiSyncSystem& sys, int discover = 0);
    void PingSingleRemoteViaHTTP(const std::string& address);
    int CreatePingPacket(MultiSyncSystem& sys, char* outBuf, int discover);

    bool FillLocalSystemInfo(void);
    std::string GetHardwareModel(void);

    int OpenBroadcastSocket(void);
    void SendBroadcastPacket(void* outBuf, int len);
    void SendControlPacket(void* outBuf, int len);
    void SendMulticastPacket(void* outBuf, int len);
    void SendUnicastPacket(const std::string& address, void* outBuf, int len);
    bool FillInInterfaces();
    bool RemoveInterface(const std::string& interface);

    void InitControlPacket(ControlPkt* pkt);

    int OpenReceiveSocket(void);

    void PerformHTTPDiscovery(void);
    void DiscoverViaHTTP(const std::set<std::string>& ips, const std::set<std::string>& exacts);
    void DiscoverIPViaHTTP(const std::string& ip, bool allowUnknown = false);

    void ProcessSyncPacket(ControlPkt* pkt, int len, MultiSyncStats* stats);
    void ProcessCommandPacket(ControlPkt* pkt, int len, MultiSyncStats* stats);
    void ProcessPingPacket(ControlPkt* pkt, int len, const std::string& src, MultiSyncStats* stats, const std::string& incomingIp = "");
    void ProcessPluginPacket(ControlPkt* pkt, int len, MultiSyncStats* stats);
    void ProcessFPPCommandPacket(ControlPkt* pkt, int len, MultiSyncStats* stats);

    std::recursive_mutex m_systemsLock;
    std::vector<MultiSyncSystem> m_localSystems;
    std::vector<MultiSyncSystem> m_remoteSystems;

    std::map<std::string, NetInterfaceInfo> m_interfaces;
    bool m_sendMulticast;
    bool m_sendBroadcast;

    int m_broadcastSock;
    int m_controlSock;
    int m_receiveSock;

    std::string m_hostname;
    struct sockaddr_in m_srcAddr;
    struct sockaddr_in m_receiveSrcAddr;
    std::mutex m_socketLock;

    unsigned long m_lastPingTime;
    unsigned long m_lastCheckTime;
    int m_lastMediaHalfSecond;
    int m_lastFrame;
    int m_lastFrameSent;

    float m_remoteOffset;

    struct iovec m_destIovec;
    std::vector<struct mmsghdr> m_destMsgs;
    std::vector<struct sockaddr_in> m_destAddr;

    std::vector<MultiSyncPlugin*> m_plugins;

#define MAX_MS_RCV_MSG 12
#define MAX_MS_RCV_BUFSIZE 1500
    struct mmsghdr rcvMsgs[MAX_MS_RCV_MSG];
    struct iovec rcvIovecs[MAX_MS_RCV_MSG];
    unsigned char rcvBuffers[MAX_MS_RCV_MSG][MAX_MS_RCV_BUFSIZE + 1];
    unsigned char rcvCmbuf[MAX_MS_RCV_MSG][0x100];
    struct sockaddr_storage rcvSrcAddr[MAX_MS_RCV_MSG];

    std::mutex m_httpResponsesLock;
    std::map<std::string, std::vector<uint8_t>> m_httpResponses;

    std::recursive_mutex m_statsLock;
    std::map<std::string, MultiSyncStats*> m_syncStats;
    std::string m_syncMaster;
    bool m_multiSyncEnabled = false;
};

extern MultiSync* multiSync;
