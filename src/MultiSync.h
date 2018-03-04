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

typedef struct __attribute__((packed)) {
	char     fppd[4];        // 'FPPD'
	uint8_t  pktType;        // Control Packet Type
	uint16_t extraDataLen;   // Length of extra data following Control Packet
} ControlPkt;

typedef struct {
	char   command[1];       // Null-terminated Command to process
} CommandPkt;

typedef struct {
	char eventID[5];         // Event ID MAJOR_MINOR, NULL terminated
} EventPkt;

#define SYNC_PKT_START 0
#define SYNC_PKT_STOP  1
#define SYNC_PKT_SYNC  2

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
	kSysTypeFPPRaspberryPiZero,
	kSysTypeFPPRaspberryPiZeroW,
	kSysTypeFPPBeagleBoneBlack,
	kSysTypeFPPBeagleBoneBlackWireless,
	kSysTypeFPPBeagleBoneGreen,
	kSysTypeFPPBeagleBoneGreenWireless,
	kSysTypeFPPPocketBeagle,
    kSysTypeFPPSanCloudBeagleBoneEnhanced,
	kSysTypeFalconController             = 0x80,
	kSysTypeFalconF16v2                  = 0x81,
	kSysTypeFalconF16v3                  = 0x82,
	kSysTypeFalconF48v1                  = 0x83,
	kSysTypeOtherSystem                  = 0xC0,
	kSysTypexLights                      = 0xC1
} MultiSyncSystemType;

typedef struct multiSyncSystem {
	unsigned long        lastSeen;
	std::string          lastSeenStr;
	MultiSyncSystemType  type;
	unsigned int         majorVersion;
	unsigned int         minorVersion;
	FPPMode              fppMode;
	std::string          address;
	std::string          hostname;
	std::string          version;
	std::string          model;
	unsigned char        ipa;
	unsigned char        ipb;
	unsigned char        ipc;
	unsigned char        ipd;
} MultiSyncSystem;

class MultiSync {
  public:
	MultiSync();
	~MultiSync();

	int  Init(void);

	int  GetControlSocket(void) { return m_receiveSock; }
	void ProcessControlPacket(void);

	void UpdateSystem(MultiSyncSystemType type, unsigned int majorVersion,
		unsigned int minorVersion, FPPMode fppMode, std::string address,
		std::string hostname, std::string version, std::string model);

	MultiSyncSystem GetLocalSystemInfo(void);

	Json::Value GetSystems(void);

	void Ping(int discover = 0);
	void Discover(void) { Ping(1); }

	void SendSeqSyncStartPacket(const char *filename);
	void SendSeqSyncStopPacket(const char *filename);
	void SendSeqSyncPacket(const char *filename, int frames, float seconds);
	void ShutdownSync(void);

	void SendMediaSyncStartPacket(const char *filename);
	void SendMediaSyncStopPacket(const char *filename);
	void SendMediaSyncPacket(const char *filename, int frames, float seconds);

	void SendEventPacket(const char *eventID);
	void SendBlankingDataPacket(void);

  private:
	MultiSyncSystemType ModelStringToType(std::string model);
	void FillLocalSystemInfo(void);
	std::string GetHardwareModel(void);
	std::string GetTypeString(MultiSyncSystemType type);

	int  OpenBroadcastSocket(void);
	void SendBroadcastPacket(void *outBuf, int len);

	int  OpenControlSockets(void);
	void SendControlPacket(void *outBuf, int len);

	int  OpenCSVControlSockets(void);
	void SendCSVControlPacket(void *outBuf, int len);

	void InitControlPacket(ControlPkt *pkt);

	int  OpenReceiveSocket(void);

	void StartSyncedSequence(char *filename);
	void StopSyncedSequence(char *filename);
	void SyncSyncedSequence(char *filename, int frameNumber,
		float secondsElapsed);

	void StartSyncedMedia(char *filename);
	void StopSyncedMedia(char *filename);
	void SyncSyncedMedia(char *filename, int frameNumber, float secondsElapsed);

	void ProcessSyncPacket(ControlPkt *pkt, int len);
	void ProcessCommandPacket(ControlPkt *pkt, int len);
	void ProcessEventPacket(ControlPkt *pkt, int len);
	void ProcessPingPacket(ControlPkt *pkt, int len);

	pthread_mutex_t              m_systemsLock;
	std::vector<MultiSyncSystem> m_systems;

	int  m_broadcastSock;
	int  m_controlSock;
	int  m_controlCSVSock;
	int  m_remoteCount;
	int  m_remoteCSVCount;
	int  m_receiveSock;

	std::string         m_hostname;
	char                m_localAddress[16];
	struct sockaddr_in  m_srcAddr;
	struct sockaddr_in  m_receiveSrcAddr;
	struct sockaddr_in  m_broadcastDestAddr;
	pthread_mutex_t     m_socketLock;

	float  m_remoteOffset;

	std::vector<struct sockaddr_in> m_destAddr;
	std::vector<struct sockaddr_in> m_destAddrCSV;

};

extern MultiSync *multiSync;

#endif /* _MULTISYNC_H */
