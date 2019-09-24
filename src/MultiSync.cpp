
/*
 *   Falcon Player MultiSync 
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

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/version.h>

#include <netinet/in.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <fstream>



#include <string>
#include <vector>
#include <set>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/tokenizer.hpp>
#include <jsoncpp/json/json.h>

#include "command.h"
#include "common.h"
#include "events.h"
#include "falcon.h"
#include "fppversion.h"
#include "log.h"
#include "MultiSync.h"
#include "Plugins.h"
#include "Sequence.h"
#include "settings.h"
#include "mediaoutput/mediaoutput.h"
#include "channeloutput/channeloutput.h"
#include "channeloutput/channeloutputthread.h"


MultiSync *multiSync;

static const char * MULTISYNC_MULTICAST_ADDRESS = "239.70.80.80"; // 239.F.P.P

/*
 *
 */
MultiSync::MultiSync()
  : m_broadcastSock(-1),
	m_controlSock(-1),
	m_controlCSVSock(-1),
	m_receiveSock(-1),
    m_lastMediaHalfSecond(0),
	m_remoteOffset(0.0),
    m_numLocalSystems(0),
    m_lastPingTime(0),
    m_lastCheckTime(0),
    m_lastFrame(0)
{
	pthread_mutex_init(&m_systemsLock, NULL);
	pthread_mutex_init(&m_socketLock, NULL);
}

/*
 *
 */
MultiSync::~MultiSync()
{
	ShutdownSync();

	pthread_mutex_destroy(&m_systemsLock);
	pthread_mutex_destroy(&m_socketLock);
}

/*
 *
 */
int MultiSync::Init(void)
{
	FillLocalSystemInfo();

	if (!OpenReceiveSocket())
		return 0;

	if (!OpenBroadcastSocket())
		return 0;

	if (getFPPmode() == MASTER_MODE) {
		if (!OpenControlSockets())
			return 0;

		if (!OpenCSVControlSockets())
			return 0;
	}

	return 1;
}

/*
 *
 */
void MultiSync::UpdateSystem(MultiSyncSystemType type,
                             unsigned int majorVersion, unsigned int minorVersion,
                             FPPMode fppMode,
                             const std::string &address,
                             const std::string &hostname,
                             const std::string &version,
                             const std::string &model,
                             const std::string &ranges
                             )
{
	pthread_mutex_lock(&m_systemsLock);
	int found = -1;
	for (int i = 0; i < m_systems.size(); i++) {
		if ((address == m_systems[i].address) &&
			(hostname == m_systems[i].hostname)) {
			found = i;
		}
	}

	if (found < 0) {
		MultiSyncSystem newSystem;

		m_systems.push_back(newSystem);

		found = m_systems.size() - 1;
	}

	char timeStr[32];
	time_t t = time(NULL);
	struct tm tm;

	m_systems[found].lastSeen     = (unsigned long)t;
	m_systems[found].type         = type;
	m_systems[found].majorVersion = majorVersion;
	m_systems[found].minorVersion = minorVersion;
	m_systems[found].address      = address;
	m_systems[found].hostname     = hostname;
	m_systems[found].fppMode      = fppMode;
	m_systems[found].version      = version;
	m_systems[found].model        = model;
    m_systems[found].ranges       = ranges;
	std::vector<std::string> parts = split(address, '.');
	m_systems[found].ipa = atoi(parts[0].c_str());
	m_systems[found].ipb = atoi(parts[1].c_str());
	m_systems[found].ipc = atoi(parts[2].c_str());
	m_systems[found].ipd = atoi(parts[3].c_str());

	localtime_r(&t, &tm);
	sprintf(timeStr,"%4d-%.2d-%.2d %.2d:%.2d:%.2d",
		1900+tm.tm_year, tm.tm_mon+1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);
	m_systems[found].lastSeenStr = timeStr;

	pthread_mutex_unlock(&m_systemsLock);
}

/*
 *
 */
MultiSyncSystemType MultiSync::ModelStringToType(std::string model)
{
	if (boost::starts_with(model, "Raspberry Pi Model A Rev"))
		return kSysTypeFPPRaspberryPiA;
	if (boost::starts_with(model, "Raspberry Pi Model B Rev"))
		return kSysTypeFPPRaspberryPiB;
	if (boost::starts_with(model, "Raspberry Pi Model A Plus"))
		return kSysTypeFPPRaspberryPiAPlus;
	if (boost::starts_with(model, "Raspberry Pi Model B Plus"))
		return kSysTypeFPPRaspberryPiBPlus;
	if ((boost::starts_with(model, "Raspberry Pi 2 Model B 1.1")) ||
		(boost::starts_with(model, "Raspberry Pi 2 Model B 1.0")))
		return kSysTypeFPPRaspberryPi2B;
	if (boost::starts_with(model, "Raspberry Pi 2 Model B"))
		return kSysTypeFPPRaspberryPi2BNew;
	if (boost::starts_with(model, "Raspberry Pi 3 Model B Rev"))
		return kSysTypeFPPRaspberryPi3B;
	if (boost::starts_with(model, "Raspberry Pi 3 Model B Plus"))
		return kSysTypeFPPRaspberryPi3BPlus;
	if (boost::starts_with(model, "Raspberry Pi Zero Rev"))
		return kSysTypeFPPRaspberryPiZero;
	if (boost::starts_with(model, "Raspberry Pi Zero W"))
		return kSysTypeFPPRaspberryPiZeroW;
    if (boost::starts_with(model, "Raspberry Pi 3 Model A Plus"))
        return kSysTypeFPPRaspberryPi3APlus;
    if (boost::starts_with(model, "Raspberry Pi 4"))
        return kSysTypeFPPRaspberryPi4;
    if (boost::starts_with(model, "SanCloud BeagleBone Enhanced"))
        return kSysTypeFPPSanCloudBeagleBoneEnhanced;
    if (boost::algorithm::contains(model, "BeagleBone Black")) {
        if (boost::algorithm::contains(model, "Wireless")) {
            return kSysTypeFPPBeagleBoneBlackWireless;
        }
        return kSysTypeFPPBeagleBoneBlack;
    }
    if (boost::algorithm::contains(model, "BeagleBone Green")) {
        if (boost::algorithm::contains(model, "Wireless")) {
            return kSysTypeFPPBeagleBoneGreenWireless;
        }
        return kSysTypeFPPBeagleBoneGreen;
    }
	if (boost::algorithm::contains(model, "PocketBeagle"))
		return kSysTypeFPPPocketBeagle;
	// FIXME, fill in the rest of the types

	return kSysTypeFPP;
}

/*
 *
 */
void MultiSync::FillLocalSystemInfo(void)
{
	pthread_mutex_lock(&m_systemsLock);

	MultiSyncSystem newSystem;

	std::string model = GetHardwareModel();
	MultiSyncSystemType type = ModelStringToType(model);

	std::string multiSyncInterface = getSetting("MultiSyncInterface");
    char addressBuf[128];
    std::list<std::string> addresses;
    if (multiSyncInterface == "") {
        //get all the addresses
        struct ifaddrs *interfaces,*tmp;
        getifaddrs(&interfaces);
        tmp = interfaces;
        while (tmp) {
            if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
                if (strncmp("usb", tmp->ifa_name, 3) != 0) {
                    //skip the usb* interfaces as we won't support multisync on those
                    GetInterfaceAddress(tmp->ifa_name, addressBuf, NULL, NULL);
                    if (strcmp(addressBuf, "127.0.0.1")) {
                        addresses.push_back(addressBuf);
                    }
                }
            } else if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET6) {
                //FIXME for ipv6 multisync
            }
            tmp = tmp->ifa_next;
        }
        freeifaddrs(interfaces);
    } else {
        GetInterfaceAddress(multiSyncInterface.c_str(), addressBuf, NULL, NULL);
        addresses.push_back(addressBuf);
    }

	m_hostname = getSetting("HostName");

	if (m_hostname == "")
		m_hostname = "FPP";

	newSystem.lastSeen     = (unsigned long)time(NULL);
	newSystem.type         = type;
	newSystem.majorVersion = atoi(getFPPMajorVersion());
	newSystem.minorVersion = atoi(getFPPMinorVersion());
	newSystem.hostname     = m_hostname;
	newSystem.fppMode      = getFPPmode();
	newSystem.version      = getFPPVersion();
	newSystem.model        = model;

    for (auto address : addresses) {
        if (m_localAddress == "") {
            m_localAddress = address;
        }
        newSystem.address = address;
        std::vector<std::string> parts = split(newSystem.address, '.');
        newSystem.ipa = atoi(parts[0].c_str());
        newSystem.ipb = atoi(parts[1].c_str());
        newSystem.ipc = atoi(parts[2].c_str());
        newSystem.ipd = atoi(parts[3].c_str());

        m_systems.push_back(newSystem);
        m_numLocalSystems++;
    }
    LogDebug(VB_SYNC, "m_localAddress = %s\n", m_localAddress.c_str());

	pthread_mutex_unlock(&m_systemsLock);
}

/*
 *
 */
std::string MultiSync::GetHardwareModel(void)
{
	std::string result;
	std::string filename;

	if (FileExists("/sys/firmware/devicetree/base/model"))
		filename = "/sys/firmware/devicetree/base/model";
	else if (FileExists("/sys/class/dmi/id/product_name"))
		filename = "/sys/class/dmi/id/product_name";

	if (filename != "") {
		char buf[128];
		FILE *fd = fopen(filename.c_str(), "r");
		if (fd) {
			if (fgets(buf, 127, fd))
				result = buf;
			else
				result = "Unknown Hardware Platform";
			fclose(fd);
		} else {
			result = "Unknown Hardware Platform";
		}
	} else {
		result = "Unknown Hardware Platform";
	}

	if (boost::ends_with(result, "\n"))
		boost::replace_first(result, "\n", "");

	return result;
}

/*
 *
 */
std::string MultiSync::GetTypeString(MultiSyncSystemType type, bool local)
{
    if (local && type == kSysTypeFPP) {
        //unknown hardware, but we can figure out the OS version
        if (FileExists("/etc/os-release")) {
            std::map<std::string, std::string> values;
            std::ifstream in("/etc/os-release");
            std::string str;
            while (std::getline(in, str)) {
                size_t pos = str.find("=");
                if (pos != std::string::npos) {
                    std::string key = str.substr(0, pos);
                    std::string val = str.substr(pos + 1);
                    if (val[0] == '"') {
                        val = val.substr(1, val.length() - 2);
                    }
                    values[key] = val;
                }
            }
            if (values["PRETTY_NAME"] != "") {
                return "FPP (" + values["PRETTY_NAME"] + ")";
            }
            if (values["NAME"] != "") {
                return "FPP (" + values["NAME"] + ")";
            }
        }
        return "FPP (unknown hardware)";
    }
	switch (type) {
		case kSysTypeUnknown:                 return "Unknown System Type";
		case kSysTypeFPP:                     return "FPP (unknown hardware)";
		case kSysTypeFPPRaspberryPiA:         return "Raspberry Pi A";
		case kSysTypeFPPRaspberryPiB:         return "Raspberry Pi B";
		case kSysTypeFPPRaspberryPiAPlus:     return "Raspberry Pi A Plus";
		case kSysTypeFPPRaspberryPiBPlus:     return "Raspberry Pi B+";
		case kSysTypeFPPRaspberryPi2B:        return "Raspberry Pi 2 B";
		case kSysTypeFPPRaspberryPi2BNew:     return "Raspberry Pi 2 B v1.2+";
		case kSysTypeFPPRaspberryPi3B:        return "Raspberry Pi 3 B";
		case kSysTypeFPPRaspberryPi3BPlus:    return "Raspberry Pi 3 B+";
		case kSysTypeFPPRaspberryPiZero:      return "Raspberry Pi Zero";
		case kSysTypeFPPRaspberryPiZeroW:     return "Raspberry Pi Zero W";
        case kSysTypeFPPRaspberryPi3APlus:    return "Raspberry Pi 3 A+";
        case kSysTypeFPPRaspberryPi4:         return "Raspberry Pi 4";
		case kSysTypeFalconController:        return "Falcon Controller";
        case kSysTypeFalconF16v2:             return "Falcon F16v2";
        case kSysTypeFalconF16v3:             return "Falcon F16v3";
        case kSysTypeFalconF48v1:             return "Falcon F48v1";
		case kSysTypeOtherSystem:             return "Other Unknown System";
        case kSysTypeFPPBeagleBoneBlack:      return "BeagleBone Black";
        case kSysTypeFPPBeagleBoneBlackWireless: return "BeagleBone Black Wireless";
        case kSysTypeFPPBeagleBoneGreen:      return "BeagleBone Green";
        case kSysTypeFPPBeagleBoneGreenWireless: return "BeagleBone Green Wireless";
        case kSysTypeFPPPocketBeagle:         return "PocketBeagle";
        case kSysTypeFPPSanCloudBeagleBoneEnhanced: return "SanCloud BeagleBone Enhanced";
            
        case kSysTypexSchedule:               return "xSchedule";
        case kSysTypeESPixelStick:            return "ESPixelStick";
		default:                              return "Unknown System Type";
	}
}

/*
 *
 */
Json::Value MultiSync::GetSystems(bool localOnly, bool timestamps)
{
	Json::Value result;
	Json::Value systems(Json::arrayValue);

	pthread_mutex_lock(&m_systemsLock);

    int max = localOnly ? m_numLocalSystems : m_systems.size();
    
    const std::vector<std::pair<uint32_t, uint32_t>> &ranges = GetOutputRanges();
    bool first = true;
    std::string range;
    for (auto &a : ranges) {
        if (!first) {
            range += ",";
        }
        char buf[64];
        sprintf(buf, "%d-%d", a.first, (a.first + a.second - 1));
        range += buf;
        first = false;
    }

    
	for (int i = 0; i < max; i++) {
		Json::Value system;

		system["type"] = GetTypeString(m_systems[i].type, i < m_numLocalSystems);
        if (timestamps) {
            system["lastSeen"]     = (Json::UInt64)m_systems[i].lastSeen;
            system["lastSeenStr"]  = m_systems[i].lastSeenStr;
        }
		system["majorVersion"] = m_systems[i].majorVersion;
		system["minorVersion"] = m_systems[i].minorVersion;
		system["fppMode"]      = m_systems[i].fppMode;
        
        char *s = modeToString(m_systems[i].fppMode);
        system["fppModeString"] = s;
        free(s);
        
		system["address"]      = m_systems[i].address;
		system["hostname"]     = m_systems[i].hostname;
		system["version"]      = m_systems[i].version;
		system["model"]        = m_systems[i].model;
        if (i < m_numLocalSystems) {
            m_systems[i].ranges = range;
        }
        system["channelRanges"] = m_systems[i].ranges;

		systems.append(system);
	}

	pthread_mutex_unlock(&m_systemsLock);

	result["systems"] = systems;

	return result;
}

/*
 *
 */
void MultiSync::Ping(int discover)
{
	LogDebug(VB_SYNC, "MultiSync::Ping(%d)\n", discover);
    time_t t = time(NULL);
    m_lastPingTime = (unsigned long)t;

	if (m_broadcastSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send ping packet but control socket is not open.\n");
		return;
	}
    
    //update the range for local systems so it's accurate
    const std::vector<std::pair<uint32_t, uint32_t>> &ranges = GetOutputRanges();
    bool first = true;
    std::string range;
    for (auto &a : ranges) {
        if (!first) {
            range += ",";
        }
        char buf[64];
        sprintf(buf, "%d-%d", a.first, (a.first + a.second - 1));
        range += buf;
        first = false;
    }
    for (int x = 0; x < m_numLocalSystems; x++) {
        pthread_mutex_lock(&m_systemsLock);
        m_systems[x].ranges = range;
        MultiSyncSystem sysInfo = m_systems[x];
        pthread_mutex_unlock(&m_systemsLock);
        
        char           outBuf[512];
        bzero(outBuf, sizeof(outBuf));
        int len = CreatePingPacket(sysInfo, outBuf, discover);
        SendBroadcastPacket(outBuf, len);
    }
}
void MultiSync::PeriodicPing() {
    time_t t = time(NULL);
    if (m_lastCheckTime == 0) {
        m_lastCheckTime = (unsigned long)t;
    }
    unsigned long lpt = m_lastPingTime + 60*60;
    if (lpt < (unsigned long)t) {
        //once an hour, we'll send a ping letting everyone know we're still here
        //mark ourselves as seen
        pthread_mutex_lock(&m_systemsLock);
        for (int x = 0; x < m_numLocalSystems; x++) {
            m_systems[x].lastSeen = (unsigned long)t;
        }
        pthread_mutex_unlock(&m_systemsLock);
        Ping();
    }
    //every 10 minutes we'll loop through real quick and check for instances
    //we haven't heard from in a while
    lpt = m_lastCheckTime + 60*10;
    if (lpt < (unsigned long)t) {
        m_lastCheckTime = (unsigned long)t;
        //anything we haven't heard from in 80 minutes we will re-ping to force
        unsigned long timeoutRePing = (unsigned long)t - 60*80;
        //anything we haven't heard from in 2 hours we will remove.   That would
        //have caused at least 4 pings to have been sent.  If it has responded
        //to any of those 4, it's got to be down/gone.   Remove it.
        unsigned long timeoutRemove = (unsigned long)t - 60*120;
        pthread_mutex_lock(&m_systemsLock);
        for (auto it = m_systems.begin(); it != m_systems.end(); ) {
            if (it->lastSeen < timeoutRemove) {
                LogInfo(VB_SYNC, "Have not seen %s in over 2 hours, removing\n", it->address.c_str());
                m_systems.erase(it);
            } else if (it->lastSeen < timeoutRePing) {
                //do a ping
                PingSingleRemote(it - m_systems.begin());
                ++it;
            } else {
                ++it;
            }
        }
        pthread_mutex_unlock(&m_systemsLock);
    }
}
void MultiSync::PingSingleRemote(int sysIdx) {
    char           outBuf[512];
    memset(outBuf, 0, sizeof(outBuf));
    int len = CreatePingPacket(m_systems[0], outBuf, 1);
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(m_systems[sysIdx].address.c_str());
    dest_addr.sin_port   = htons(FPP_CTRL_PORT);
    pthread_mutex_lock(&m_socketLock);
    sendto(m_controlSock, outBuf, len, MSG_DONTWAIT, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    pthread_mutex_unlock(&m_socketLock);
}
int MultiSync::CreatePingPacket(MultiSyncSystem &sysInfo, char* outBuf, int discover) {
    ControlPkt    *cpkt = (ControlPkt*)outBuf;
    InitControlPacket(cpkt);
    
    cpkt->pktType        = CTRL_PKT_PING;
    cpkt->extraDataLen   = 214; // v2 ping length
    
    unsigned char *ed = (unsigned char*)(outBuf + 7);
    memset(ed, 0, cpkt->extraDataLen - 7);
    
    ed[0]  = 2;                    // ping version 1
    ed[1]  = discover > 0 ? 1 : 0; // 0 = ping, 1 = discover
    ed[2]  = sysInfo.type;
    ed[3]  = (sysInfo.majorVersion & 0xFF00) >> 8;
    ed[4]  = (sysInfo.majorVersion & 0x00FF);
    ed[5]  = (sysInfo.minorVersion & 0xFF00) >> 8;
    ed[6]  = (sysInfo.minorVersion & 0x00FF);
    ed[7]  = sysInfo.fppMode;
    ed[8]  = sysInfo.ipa;
    ed[9]  = sysInfo.ipb;
    ed[10] = sysInfo.ipc;
    ed[11] = sysInfo.ipd;
    
    strncpy((char *)(ed + 12), sysInfo.hostname.c_str(), 65);
    strncpy((char *)(ed + 77), sysInfo.version.c_str(), 41);
    strncpy((char *)(ed + 118), sysInfo.model.c_str(), 41);
    strncpy((char *)(ed + 159), sysInfo.ranges.c_str(), 41);
    SendBroadcastPacket(outBuf, sizeof(ControlPkt) + cpkt->extraDataLen);
    return sizeof(ControlPkt) + cpkt->extraDataLen;
}

void MultiSync::SendSeqOpenPacket(const std::string &filename)
{
    if (filename.empty()) {
        return;
    }
    
    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send start packet but sync socket is not open.\n");
        return;
    }
    
    LogDebug(VB_SYNC, "SendSeqOpenPacket('%s')\n", filename.c_str());
    for (auto a : m_plugins) {
        a->SendSeqOpenPacket(filename);
    }
    m_lastFrame = -1;
    m_lastFrameSent = -1;

    char           outBuf[2048];
    bzero(outBuf, sizeof(outBuf));
    
    ControlPkt    *cpkt = (ControlPkt*)outBuf;
    SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));
    
    InitControlPacket(cpkt);
    
    cpkt->pktType        = CTRL_PKT_SYNC;
    cpkt->extraDataLen   = sizeof(SyncPkt) + filename.length();
    
    spkt->pktType  = SYNC_PKT_OPEN;
    spkt->fileType = SYNC_FILE_SEQ;
    spkt->frameNumber = 0;
    spkt->secondsElapsed = 0;
    strcpy(spkt->filename, filename.c_str());
    
    SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());
}

void MultiSync::SendSeqSyncStartPacket(const std::string &filename)
{
    if (filename.empty()) {
        return;
    }

	if (m_controlSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send start packet but sync socket is not open.\n");
		return;
	}

    LogDebug(VB_SYNC, "SendSeqSyncStartPacket('%s')\n", filename.c_str());
    for (auto a : m_plugins) {
        a->SendSeqSyncStartPacket(filename);
    }
    m_lastFrame = -1;
    m_lastFrameSent = -1;
    
	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPacket(cpkt);

	cpkt->pktType        = CTRL_PKT_SYNC;
	cpkt->extraDataLen   = sizeof(SyncPkt) + filename.length();
	
	spkt->pktType  = SYNC_PKT_START;
	spkt->fileType = SYNC_FILE_SEQ;
	spkt->frameNumber = 0;
	spkt->secondsElapsed = 0;
	strcpy(spkt->filename, filename.c_str());

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());

	if (m_destAddrCSV.size() > 0) {
		// Now send the Broadcast CSV version
		sprintf(outBuf, "FPP,%d,%d,%d,%s\n",
			CTRL_PKT_SYNC, SYNC_FILE_SEQ, SYNC_PKT_START, filename.c_str());
		SendCSVControlPacket(outBuf, strlen(outBuf));
	}
}

/*
 *
 */
void MultiSync::SendSeqSyncStopPacket(const std::string &filename)
{
    if (filename.empty()) {
        return;
    }
	if (m_controlSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send stop packet but sync socket is not open.\n");
		return;
	}
    LogDebug(VB_SYNC, "SendSeqSyncStopPacket(%s)\n", filename.c_str());

    for (auto a : m_plugins) {
        a->SendSeqSyncStopPacket(filename);
    }
    
	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPacket(cpkt);

	cpkt->pktType        = CTRL_PKT_SYNC;
	cpkt->extraDataLen   = sizeof(SyncPkt) + filename.length();
	
	spkt->pktType  = SYNC_PKT_STOP;
	spkt->fileType = SYNC_FILE_SEQ;
	spkt->frameNumber = 0;
	spkt->secondsElapsed = 0;
	strcpy(spkt->filename, filename.c_str());

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());

    if (m_destAddrCSV.size() > 0) {
		// Now send the Broadcast CSV version
		sprintf(outBuf, "FPP,%d,%d,%d,%s\n",
			CTRL_PKT_SYNC, SYNC_FILE_SEQ, SYNC_PKT_STOP, filename.c_str());
		SendCSVControlPacket(outBuf, strlen(outBuf));
	}
    
    m_lastFrame = -1;
    m_lastFrameSent = -1;
}

/*
 *
 */
void MultiSync::SendSeqSyncPacket(const std::string &filename, int frames, float seconds)
{
    if (filename.empty()) {
        return;
    }
	if (m_controlSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send sync packet but sync socket is not open.\n");
		return;
	}
    for (auto a : m_plugins) {
        a->SendSeqSyncPacket(filename, frames, seconds);
    }
    m_lastFrame = frames;
    int diff = frames - m_lastFrameSent;
    if (frames > 32) {
        //after 32 frames, we send every 10
        // that's either twice a second (50ms sequences) or 4 times (25ms)
        if (diff < 10) {
            return;
        }
    } else if (frames && diff < 4) {
        //under 32 frames, we send every 4
        return;
    }
    m_lastFrameSent = frames;
    
    LogDebug(VB_SYNC, "SendSeqSyncPacket( '%s', %d, %.2f)\n",
             filename.c_str(), frames, seconds);
	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPacket(cpkt);

	cpkt->pktType        = CTRL_PKT_SYNC;
	cpkt->extraDataLen   = sizeof(SyncPkt) + filename.length();
	
	spkt->pktType  = SYNC_PKT_SYNC;
	spkt->fileType = SYNC_FILE_SEQ;
	spkt->frameNumber = frames;
	spkt->secondsElapsed = seconds;
	strcpy(spkt->filename, filename.c_str());

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());

    if (m_destAddrCSV.size() > 0) {
		// Now send the Broadcast CSV version
		sprintf(outBuf, "FPP,%d,%d,%d,%s,%d,%d\n",
			CTRL_PKT_SYNC, SYNC_FILE_SEQ, SYNC_PKT_SYNC, filename.c_str(),
			(int)seconds, (int)(seconds * 1000) % 1000);
		SendCSVControlPacket(outBuf, strlen(outBuf));
	}
}

void MultiSync::SendMediaOpenPacket(const std::string &filename)
{
    if (filename.empty()) {
        return;
    }

	if (m_controlSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send start packet but sync socket is not open.\n");
		return;
	}
    LogDebug(VB_SYNC, "SendMediaOpenPacket('%s')\n", filename.c_str());
    
    for (auto a : m_plugins) {
        a->SendMediaOpenPacket(filename);
    }
    
    m_lastMediaHalfSecond = 0;

	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPacket(cpkt);

	cpkt->pktType        = CTRL_PKT_SYNC;
	cpkt->extraDataLen   = sizeof(SyncPkt) + filename.length();

	spkt->pktType  = SYNC_PKT_OPEN;
	spkt->fileType = SYNC_FILE_MEDIA;
	spkt->frameNumber = 0;
	spkt->secondsElapsed = 0;
	strcpy(spkt->filename, filename.c_str());

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());
}
void MultiSync::SendMediaSyncStartPacket(const std::string &filename)
{
    if (filename.empty()) {
        return;
    }

	if (m_controlSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send start packet but sync socket is not open.\n");
		return;
	}
    LogDebug(VB_SYNC, "SendMediaSyncStartPacket('%s')\n", filename.c_str());
    
    for (auto a : m_plugins) {
        a->SendMediaSyncStartPacket(filename);
    }
    
    m_lastMediaHalfSecond = 0;

	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPacket(cpkt);

	cpkt->pktType        = CTRL_PKT_SYNC;
	cpkt->extraDataLen   = sizeof(SyncPkt) + filename.length();

	spkt->pktType  = SYNC_PKT_START;
	spkt->fileType = SYNC_FILE_MEDIA;
	spkt->frameNumber = 0;
	spkt->secondsElapsed = 0;
	strcpy(spkt->filename, filename.c_str());

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());
}

/*
 *
 */
void MultiSync::SendMediaSyncStopPacket(const std::string &filename)
{
    if (filename.empty()) {
        return;
    }

	if (m_controlSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send stop packet but sync socket is not open.\n");
		return;
	}
    LogDebug(VB_SYNC, "SendMediaSyncStopPacket(%s)\n", filename);
    for (auto a : m_plugins) {
        a->SendMediaSyncStopPacket(filename);
    }

	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPacket(cpkt);

	cpkt->pktType        = CTRL_PKT_SYNC;
	cpkt->extraDataLen   = sizeof(SyncPkt) + filename.length();

	spkt->pktType  = SYNC_PKT_STOP;
	spkt->fileType = SYNC_FILE_MEDIA;
	spkt->frameNumber = 0;
	spkt->secondsElapsed = 0;
	strcpy(spkt->filename, filename.c_str());

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());
}

/*
 *
 */
void MultiSync::SendMediaSyncPacket(const std::string &filename, float seconds)
{
    if (filename.empty()) {
        return;
    }

    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send sync packet but sync socket is not open.\n");
        return;
    }

    for (auto a : m_plugins) {
        a->SendMediaSyncPacket(filename, seconds);
    }
    
    int curTS = (seconds * 2.0f);
    if (m_lastMediaHalfSecond == curTS) {
        //not time to send
        return;
    }
    m_lastMediaHalfSecond = curTS;

    LogExcess(VB_SYNC, "SendMediaSyncPacket( '%s', %.2f)\n",
              filename.c_str(), seconds);

	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPacket(cpkt);

	cpkt->pktType        = CTRL_PKT_SYNC;
	cpkt->extraDataLen   = sizeof(SyncPkt) + filename.length();

	spkt->pktType  = SYNC_PKT_SYNC;
	spkt->fileType = SYNC_FILE_MEDIA;
	spkt->frameNumber = 0;
	spkt->secondsElapsed = seconds;
	strcpy(spkt->filename, filename.c_str());

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());
}

/*
 *
 */
void MultiSync::SendEventPacket(const std::string &eventID)
{
    if (eventID.empty()) {
        return;
    }
    for (auto a : m_plugins) {
        a->SendEventPacket(eventID);
    }

	LogDebug(VB_SYNC, "SendEventPacket('%s')\n", eventID);

	if (m_controlSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send event packet but control socket is not open.\n");
		return;
	}

	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	EventPkt *epkt = (EventPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPacket(cpkt);

	cpkt->pktType        = CTRL_PKT_EVENT;
	cpkt->extraDataLen   = sizeof(EventPkt);

	strcpy(epkt->eventID, eventID.c_str());

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(EventPkt));
}
void MultiSync::SendPluginData(const std::string &name, const uint8_t *data, int len) {
    if (name.empty()) {
        return;
    }
    for (auto a : m_plugins) {
        a->SendPluginData(name, data, len);
    }
    
    LogDebug(VB_SYNC, "SendPluginData('%s')\n", name.c_str());
    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send event packet but control socket is not open.\n");
        return;
    }
    
    char           outBuf[2048];
    bzero(outBuf, sizeof(outBuf));
    
    ControlPkt    *cpkt = (ControlPkt*)outBuf;
    CommandPkt *epkt = (CommandPkt*)(outBuf + sizeof(ControlPkt));
    
    InitControlPacket(cpkt);
    int nlen = strlen(name.c_str()) + 1;  //add the null
    cpkt->pktType        = CTRL_PKT_PLUGIN;
    cpkt->extraDataLen   = len + nlen;
    
    strcpy(epkt->command, name.c_str());
    memcpy(&epkt->command[nlen], data, len);
    
    SendControlPacket(outBuf, sizeof(ControlPkt) + len + nlen);
}

/*
 *
 */
void MultiSync::SendBlankingDataPacket(void)
{
	LogDebug(VB_SYNC, "SendBlankingDataPacket()\n");

	if (m_controlSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send blanking data packet but control socket is not open.\n");
		return;
	}
    for (auto a : m_plugins) {
        a->SendBlankingDataPacket();
    }
	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;

	InitControlPacket(cpkt);

	cpkt->pktType        = CTRL_PKT_BLANK;
	cpkt->extraDataLen   = 0;

	SendControlPacket(outBuf, sizeof(ControlPkt));

	if (m_controlCSVSock >= 0) {
		// Now send the Broadcast CSV version
		sprintf(outBuf, "FPP,%d\n", CTRL_PKT_BLANK);
		SendCSVControlPacket(outBuf, strlen(outBuf));
	}
}

/*
 *
 */
void MultiSync::ShutdownSync(void)
{
	LogDebug(VB_SYNC, "ShutdownSync()\n");

    for (auto a : m_plugins) {
        a->ShutdownSync();
    }

	pthread_mutex_lock(&m_socketLock);

	if (m_broadcastSock >= 0) {
		close(m_broadcastSock);
		m_broadcastSock = -1;
	}

	if (m_controlSock >= 0) {
		close(m_controlSock);
		m_controlSock = -1;
	}

	if (m_controlCSVSock >= 0) {
		close(m_controlCSVSock);
		m_controlCSVSock = -1;
	}

	if (m_receiveSock >= 0) {
		close(m_receiveSock);
		m_receiveSock = -1;
	}

	pthread_mutex_unlock(&m_socketLock);
}

/*
 *
 */
int MultiSync::OpenBroadcastSocket(void)
{
	LogDebug(VB_SYNC, "OpenBroadcastSocket()\n");

	m_broadcastSock = socket(AF_INET, SOCK_DGRAM, 0);

	if (m_broadcastSock < 0) {
		LogErr(VB_SYNC, "Error opening MultiSync broadcast socket\n");
		return 0;
	}

	char loopch = 0;
	if(setsockopt(m_broadcastSock, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
		LogErr(VB_SYNC, "Error setting IP_MULTICAST_LOOP: \n",
			strerror(errno));
		return 0;
	}

	int broadcast = 1;
	if(setsockopt(m_broadcastSock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
		LogErr(VB_SYNC, "Error setting SO_BROADCAST: \n", strerror(errno));
		return 0;
	}

	memset((void *)&m_broadcastDestAddr, 0, sizeof(struct sockaddr_in));

	m_broadcastDestAddr.sin_family      = AF_INET;
	m_broadcastDestAddr.sin_port        = htons(FPP_CTRL_PORT);
	m_broadcastDestAddr.sin_addr.s_addr = inet_addr("255.255.255.255");

	return 1;
}

/*
 *
 */
void MultiSync::SendBroadcastPacket(void *outBuf, int len)
{
	if ((logLevel == LOG_EXCESSIVE) &&
		(logMask & VB_SYNC)) {
		HexDump("Sending Broadcast packet with contents:", outBuf, len);
	}

	pthread_mutex_lock(&m_socketLock);

	if (sendto(m_broadcastSock, outBuf, len, 0, (struct sockaddr*)&m_broadcastDestAddr, sizeof(struct sockaddr_in)) < 0)
		LogErr(VB_SYNC, "Error: Unable to send packet: %s\n", strerror(errno));

	pthread_mutex_unlock(&m_socketLock);
}

/*
 *
 */
int MultiSync::OpenControlSockets()
{
	LogDebug(VB_SYNC, "OpenControlSockets()\n");
    if (m_controlSock >= 0) {
        return 1;
    }

	m_controlSock = socket(AF_INET, SOCK_DGRAM, 0);

	if (m_controlSock < 0) {
		LogErr(VB_SYNC, "Error opening MultiSync socket\n");
		return 0;
	}

	char loopch = 0;
	if(setsockopt(m_controlSock, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
		LogErr(VB_SYNC, "Error setting IP_MULTICAST_LOOP: \n",
			strerror(errno));
		return 0;
	}

    std::string remotesString = getSetting("MultiSyncRemotes");
    boost::char_separator<char> sep(", ");
    boost::tokenizer< boost::char_separator<char> > tokens(remotesString, sep);
    std::set<std::string> remotes;
    for (auto &token : tokens) {
        remotes.insert(token);
    }

    bool needBroadcast = false;
    if (remotes.find("255.255.255.255") != remotes.end()) {
        needBroadcast = true;
    }
    if (remotes.find(MULTISYNC_MULTICAST_ADDRESS) != remotes.end()) {
        needBroadcast = true;
    }
	if (needBroadcast) {
		int broadcast = 1;
		if (setsockopt(m_controlSock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
			LogErr(VB_SYNC, "Error setting SO_BROADCAST: \n", strerror(errno));
			return 0;
		}
	}
    for (auto &s : remotes) {
		LogDebug(VB_SYNC, "Setting up Remote Sync for %s\n", s.c_str());
		struct sockaddr_in newRemote;

		newRemote.sin_family      = AF_INET;
		newRemote.sin_port        = htons(FPP_CTRL_PORT);
        
        bool isAlpha = std::find_if(s.begin(), s.end(), [](char c) { return (isalpha(c) || (c == ' ')); }) == s.end();
        bool valid = true;
        if (isAlpha) {
            struct hostent* uhost = gethostbyname(s.c_str());
            if (!uhost) {
                LogErr(VB_SYNC,
                       "Error looking up Remote hostname: %s\n",
                       s.c_str());
                valid = false;
            } else {
                newRemote.sin_addr.s_addr = *((unsigned long*)uhost->h_addr);
            }
        } else {
            newRemote.sin_addr.s_addr = inet_addr(s.c_str());
        }
        if (valid) {
            m_destAddr.push_back(newRemote);
        }
	}
    for (int x = 0; x < m_destAddr.size(); x++) {
        struct mmsghdr msg;
        memset(&msg, 0, sizeof(msg));
        
        msg.msg_hdr.msg_name = &m_destAddr[x];
        msg.msg_hdr.msg_namelen = sizeof(sockaddr_in);
        msg.msg_hdr.msg_iov = &m_destIovec;
        msg.msg_hdr.msg_iovlen = 1;
        msg.msg_len = 0;
        m_destMsgs.push_back(msg);
    }

	LogDebug(VB_SYNC, "%d Remote Sync systems configured\n",
		m_destAddr.size());

	return 1;
}

/*
 *
 */
void MultiSync::SendControlPacket(void *outBuf, int len)
{
	if ((logLevel == LOG_EXCESSIVE) &&
		(logMask & VB_SYNC)) {
		LogExcess(VB_SYNC, "SendControlPacket()\n");
		HexDump("Sending Control packet with contents:", outBuf, len);
	}

    m_destIovec.iov_base = outBuf;
    m_destIovec.iov_len = len;

    int msgCount = m_destMsgs.size();
    if (msgCount == 0) {
        return;
    }
    
    pthread_mutex_lock(&m_socketLock);
    int oc = sendmmsg(m_controlSock, &m_destMsgs[0], msgCount, 0);
    int outputCount = oc;
    while (oc > 0 && outputCount != msgCount) {
        int oc = sendmmsg(m_controlSock, &m_destMsgs[outputCount], msgCount - outputCount, 0);
        if (oc >= 0) {
            outputCount += oc;
        }
    }
    if (outputCount != msgCount) {
        LogErr(VB_SYNC, "Error: Unable to send multisync packet: %s\n", strerror(errno));
    }

	pthread_mutex_unlock(&m_socketLock);
}

/*
 *
 */
int MultiSync::OpenCSVControlSockets(void)
{
	LogDebug(VB_SYNC, "OpenCSVControlSockets()\n");

	m_controlCSVSock = socket(AF_INET, SOCK_DGRAM, 0);

	if (m_controlCSVSock < 0) {
		LogErr(VB_SYNC, "Error opening Master/Remote CSV Sync socket; %s\n",
			strerror(errno));
		return 0;
	}

	char loopch = 0;
	if(setsockopt(m_controlCSVSock, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
		LogErr(VB_SYNC, "Error setting IP_MULTICAST_LOOP: \n",
			strerror(errno));
		return 0;
	}

	char *tmpRemotes = strdup(getSetting("MultiSyncCSVRemotes"));

	if (!strcmp(tmpRemotes, "255.255.255.255")) {
		int broadcast = 1;
		if(setsockopt(m_controlCSVSock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
			LogErr(VB_SYNC, "Error setting SO_BROADCAST: \n", strerror(errno));
			return 0;
		}
	}

	char *s = strtok(tmpRemotes, ",");

	while (s) {
		LogDebug(VB_SYNC, "Setting up CSV Remote Sync for %s\n", s);
		struct sockaddr_in newRemote;

		newRemote.sin_family      = AF_INET;
		newRemote.sin_port        = htons(FPP_CTRL_CSV_PORT);
		newRemote.sin_addr.s_addr = inet_addr(s);

		m_destAddrCSV.push_back(newRemote);

		s = strtok(NULL, ",");
	}
    for (int x = 0; x < m_destAddrCSV.size(); x++) {
        struct mmsghdr msg;
        memset(&msg, 0, sizeof(msg));
        
        msg.msg_hdr.msg_name = &m_destAddrCSV[x];
        msg.msg_hdr.msg_namelen = sizeof(sockaddr_in);
        msg.msg_hdr.msg_iov = &m_destIovecCSV;
        msg.msg_hdr.msg_iovlen = 1;
        msg.msg_len = 0;
        m_destMsgsCSV.push_back(msg);
    }


	LogDebug(VB_SYNC, "%d CSV Remote Sync systems configured\n",
		m_destAddrCSV.size());

	free(tmpRemotes);

	return 1;
}

/*
 *
 */
void MultiSync::SendCSVControlPacket(void *outBuf, int len)
{
	LogExcess(VB_SYNC, "SendCSVControlPacket: '%s'\n", (char *)outBuf);

	if (m_controlCSVSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send CSV Sync packet but CSV sync socket is not open.\n");
		return;
	}


    m_destIovecCSV.iov_base = outBuf;
    m_destIovecCSV.iov_len = len;
    int msgCount = m_destMsgsCSV.size();
    if (msgCount == 0) {
        return;
    }
    
    pthread_mutex_lock(&m_socketLock);

    int oc = sendmmsg(m_controlCSVSock, &m_destMsgsCSV[0], msgCount, 0);
    int outputCount = oc;
    while (oc > 0 && outputCount != msgCount) {
        int oc = sendmmsg(m_controlCSVSock, &m_destMsgsCSV[outputCount], msgCount - outputCount, 0);
        if (oc >= 0) {
            outputCount += oc;
        }
    }
    if (outputCount != msgCount) {
        LogErr(VB_SYNC, "Error: Unable to send CSV multisync packet: %s\n", strerror(errno));
    }

	pthread_mutex_unlock(&m_socketLock);
}

/*
 *
 */
void MultiSync::InitControlPacket(ControlPkt *pkt)
{
	bzero(pkt, sizeof(ControlPkt));

	pkt->fppd[0]      = 'F';
	pkt->fppd[1]      = 'P';
	pkt->fppd[2]      = 'P';
	pkt->fppd[3]      = 'D';
	pkt->pktType      = 0;
	pkt->extraDataLen = 0;
}

/*
 *
 */
int MultiSync::OpenReceiveSocket(void)
{
	LogDebug(VB_SYNC, "OpenReceiveSocket()\n");

	int            UniverseOctet[2];
	int            i;
	char           strMulticastGroup[16];

	/* set up socket */
	m_receiveSock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	if (m_receiveSock < 0) {
		LogErr(VB_SYNC, "Error opening Receive socket; %s\n", strerror(errno));
		return 0;
	}
    LogDebug(VB_SYNC, "Receive socket: %d\n", m_receiveSock);

	bzero((char *)&m_receiveSrcAddr, sizeof(m_receiveSrcAddr));
	m_receiveSrcAddr.sin_family = AF_INET;
	m_receiveSrcAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	m_receiveSrcAddr.sin_port = htons(FPP_CTRL_PORT);

	int optval = 1;
	if (setsockopt(m_receiveSock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
		LogErr(VB_SYNC, "Error turning on SO_REUSEPORT; %s\n", strerror(errno));
		return 0;
	}

	// Bind the socket to address/port
	if (bind(m_receiveSock, (struct sockaddr *) &m_receiveSrcAddr, sizeof(m_receiveSrcAddr)) < 0) {
		LogErr(VB_SYNC, "Error binding socket; %s\n", strerror(errno));
		return 0;
	}

	if (setsockopt(m_receiveSock, IPPROTO_IP, IP_PKTINFO, &optval, sizeof(optval)) < 0) {
		LogErr(VB_SYNC, "Error calling setsockopt; %s\n", strerror(errno));
		return 0;
	}

    struct ip_mreq mreq;
    struct ifaddrs *interfaces,*tmp;
    getifaddrs(&interfaces);
    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr.s_addr = inet_addr(MULTISYNC_MULTICAST_ADDRESS);
    int multicastJoined = 0;
    tmp = interfaces;
    //loop through all the interfaces and subscribe to the group
    while (tmp) {
        //struct sockaddr_in *sin = (struct sockaddr_in *)tmp->ifa_addr;
        //strcpy(address, inet_ntoa(sin->sin_addr));
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
            char address[16];
            address[0] = 0;
            GetInterfaceAddress(tmp->ifa_name, address, NULL, NULL);
            if (strcmp(address, "127.0.0.1")) {
                LogDebug(VB_SYNC, "   Adding interface %s - %s\n", tmp->ifa_name, address);
                mreq.imr_interface.s_addr = inet_addr(address);
                if (setsockopt(m_receiveSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
                    LogWarn(VB_SYNC, "   Could not setup Multicast Group for interface %s\n", tmp->ifa_name);
                }
                multicastJoined = 1;
            }
        } else if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET6) {
            //FIXME for ipv6 multicast
            //LogDebug(VB_SYNC, "   Inet6 interface %s\n", tmp->ifa_name);
        }
        tmp = tmp->ifa_next;
    }
    freeifaddrs(interfaces);

	int remoteOffsetInt = getSettingInt("remoteOffset");
	if (remoteOffsetInt)
		m_remoteOffset = (float)remoteOffsetInt * -0.001;
	else
		m_remoteOffset = 0.0;
    
    memset(rcvMsgs, 0, sizeof(rcvMsgs));
    for (int i = 0; i < MAX_MS_RCV_MSG; i++) {
        rcvIovecs[i].iov_base         = rcvBuffers[i];
        rcvIovecs[i].iov_len          = MAX_MS_RCV_BUFSIZE;
        rcvMsgs[i].msg_hdr.msg_iov    = &rcvIovecs[i];
        rcvMsgs[i].msg_hdr.msg_iovlen = 1;
        rcvMsgs[i].msg_hdr.msg_name   = &rcvSrcAddr[i];
        rcvMsgs[i].msg_hdr.msg_namelen  = sizeof(struct sockaddr_storage);
        rcvMsgs[i].msg_hdr.msg_control = &rcvCmbuf[i];
        rcvMsgs[i].msg_hdr.msg_controllen = 0x100;
    }

	return 1;
}

/*
 *
 */
void MultiSync::ProcessControlPacket(void)
{
	LogExcess(VB_SYNC, "ProcessControlPacket()\n");

	ControlPkt *pkt;
    
    int msgcnt = recvmmsg(m_receiveSock, rcvMsgs, MAX_MS_RCV_MSG, MSG_DONTWAIT, nullptr);
    while (msgcnt > 0) {
        LogExcess(VB_SYNC, "ProcessControlPacket msgcnt: %d\n", msgcnt);
        for (int msg = 0; msg < msgcnt; msg++) {
            int len = rcvMsgs[msg].msg_len;
            if (len <= 0) {
                LogErr(VB_SYNC, "Error: recvmsg failed: %s\n", strerror(errno));
                continue;
            }
            unsigned char *inBuf = rcvBuffers[msg];

            if (inBuf[0] == 0x55 || inBuf[0] == 0xCC) {
                struct in_addr  recvAddr;
                struct cmsghdr *cmsg;

                for (cmsg = CMSG_FIRSTHDR(&rcvMsgs[msg].msg_hdr); cmsg != NULL; cmsg = CMSG_NXTHDR(&rcvMsgs[msg].msg_hdr, cmsg)) {
                    if (cmsg->cmsg_level != IPPROTO_IP || cmsg->cmsg_type != IP_PKTINFO) {
                        continue;
                    }

                    struct in_pktinfo *pi = (struct in_pktinfo *)CMSG_DATA(cmsg);
                    recvAddr = pi->ipi_addr;
                    recvAddr = pi->ipi_spec_dst;
                }

                ProcessFalconPacket(m_receiveSock, (struct sockaddr_in *)&rcvSrcAddr[msg], recvAddr, inBuf);
                continue;
            }

            if (len < sizeof(ControlPkt)) {
                LogErr(VB_SYNC, "Error: Received control packet too short\n");
                HexDump("Received data:", (void*)inBuf, len);
                continue;
            }

            pkt = (ControlPkt*)inBuf;

            if ((pkt->fppd[0] != 'F') ||
                (pkt->fppd[1] != 'P') ||
                (pkt->fppd[2] != 'P') ||
                (pkt->fppd[3] != 'D')) {
                LogErr(VB_SYNC, "Error: Invalid Received Control Packet, missing 'FPPD' header\n");
                HexDump("Received data:", (void*)inBuf, len);
                continue;
            }

            if (len != (sizeof(ControlPkt) + pkt->extraDataLen)) {
                LogErr(VB_SYNC, "Error: Expected %d data bytes, received %d\n",
                    pkt->extraDataLen, len - sizeof(ControlPkt));
                HexDump("Received data:", (void*)inBuf, len);
                continue;
            }

            if ((logLevel == LOG_EXCESSIVE) &&
                (logMask & VB_SYNC)) {
                HexDump("Received MultiSync packet with contents:", (void*)inBuf, len);
            }

            switch (pkt->pktType) {
                case CTRL_PKT_CMD:
                    ProcessCommandPacket(pkt, len);
                    break;
                case CTRL_PKT_SYNC:
                    if (getFPPmode() == REMOTE_MODE)
                        ProcessSyncPacket(pkt, len);
                    break;
                case CTRL_PKT_EVENT:
                    if (getFPPmode() == REMOTE_MODE)
                        ProcessEventPacket(pkt, len);
                    break;
                case CTRL_PKT_BLANK:
                    if (getFPPmode() == REMOTE_MODE)
                        sequence->SendBlankingData();
                    break;
                case CTRL_PKT_PING:
                    ProcessPingPacket(pkt, len);
                    break;
                case CTRL_PKT_PLUGIN:
                    ProcessPluginPacket(pkt, len);
                    break;
            }
        }
        msgcnt = recvmmsg(m_receiveSock, rcvMsgs, MAX_MS_RCV_MSG, MSG_DONTWAIT, nullptr);
    }
}


void MultiSync::OpenSyncedSequence(const char *filename)
{
    LogDebug(VB_SYNC, "OpenSyncedSequence(%s)\n", filename);
    
    ResetMasterPosition();
    sequence->OpenSequenceFile(filename);
}

void MultiSync::StartSyncedSequence(const char *filename)
{
	LogDebug(VB_SYNC, "StartSyncedSequence(%s)\n", filename);

    ResetMasterPosition();
    if (!strcmp(sequence->m_seqFilename.c_str(), filename) && !sequence->IsSequenceRunning()) {
        sequence->StartSequence();
    }
}

/*
 *
 */
void MultiSync::StopSyncedSequence(const char *filename)
{
	LogDebug(VB_SYNC, "StopSyncedSequence(%s)\n", filename);

	sequence->CloseIfOpen(filename);
}

/*
 *
 */
void MultiSync::SyncSyncedSequence(const char *filename, int frameNumber, float secondsElapsed)
{
	LogExcess(VB_SYNC, "SyncSyncedSequence('%s', %d, %.2f)\n",
		filename, frameNumber, secondsElapsed);

	if (!sequence->IsSequenceRunning(filename)) {
        sequence->OpenSequenceFile(filename, frameNumber);
        sequence->StartSequence();
	}
    if (sequence->IsSequenceRunning(filename)) {
		UpdateMasterPosition(frameNumber);
    }
}

void MultiSync::OpenSyncedMedia(const char *filename)
{
    LogDebug(VB_SYNC, "OpenSyncedMedia(%s)\n", filename);
    
    if (mediaOutput) {
        LogDebug(VB_SYNC, "Start media %s received while playing media %s\n",
                 filename, mediaOutput->m_mediaFilename.c_str());
        
        CloseMediaOutput();
    }
    
    OpenMediaOutput(filename);
}

void MultiSync::StartSyncedMedia(const char *filename)
{
	LogDebug(VB_SYNC, "StartSyncedMedia(%s)\n", filename);

	StartMediaOutput(filename);
}

/*
 *
 */
void MultiSync::StopSyncedMedia(const char *filename)
{
	LogDebug(VB_SYNC, "StopSyncedMedia(%s)\n", filename);

    if (!mediaOutput) {
		return;
    }

	if (MatchesRunningMediaFilename(filename)) {
		LogDebug(VB_SYNC, "Stopping synced media: %s\n", mediaOutput->m_mediaFilename.c_str());
		CloseMediaOutput();
	}
}

/*
 *
 */
void MultiSync::SyncSyncedMedia(const char *filename, int frameNumber, float secondsElapsed)
{
	LogExcess(VB_SYNC, "SyncSyncedMedia('%s', %d, %.2f)\n",
		filename, frameNumber, secondsElapsed);

	if (!mediaOutput) {
		LogExcess(VB_SYNC, "Received sync for media %s but no media playing\n",
			filename);
		return;
	}

    UpdateMasterMediaPosition(filename, secondsElapsed);
}

/*
 *
 */
void MultiSync::ProcessSyncPacket(ControlPkt *pkt, int len)
{
	if (pkt->extraDataLen < sizeof(SyncPkt)) {
		LogErr(VB_SYNC, "Error: Invalid length of received sync packet\n");
		HexDump("Received data:", (void*)&pkt, len);
		return;
	}

	SyncPkt *spkt = (SyncPkt*)(((char*)pkt) + sizeof(ControlPkt));

    LogDebug(VB_SYNC, "ProcessSyncPacket()   filename: %s    type: %d   filetype: %d   frameNumber: %d\n",
             spkt->filename, spkt->pktType, spkt->fileType, spkt->frameNumber);

	float secondsElapsed = 0.0;

	if (spkt->fileType == SYNC_FILE_SEQ) {
		switch (spkt->pktType) {
			case SYNC_PKT_OPEN:  OpenSyncedSequence(spkt->filename);
								 break;
			case SYNC_PKT_START: StartSyncedSequence(spkt->filename);
								 break;
			case SYNC_PKT_STOP:  StopSyncedSequence(spkt->filename);
								 break;
			case SYNC_PKT_SYNC:  secondsElapsed = spkt->secondsElapsed - m_remoteOffset;
								 if (secondsElapsed < 0)
									secondsElapsed = 0.0;

								 SyncSyncedSequence(spkt->filename,
									spkt->frameNumber, secondsElapsed);
								 break;
		}
	} else if (spkt->fileType == SYNC_FILE_MEDIA) {
		switch (spkt->pktType) {
            case SYNC_PKT_OPEN:  OpenSyncedMedia(spkt->filename);
                                 break;
			case SYNC_PKT_START: StartSyncedMedia(spkt->filename);
								 break;
			case SYNC_PKT_STOP:  StopSyncedMedia(spkt->filename);
								 break;
			case SYNC_PKT_SYNC:  secondsElapsed = spkt->secondsElapsed - m_remoteOffset;
								 if (secondsElapsed < 0)
									secondsElapsed = 0.0;

								 SyncSyncedMedia(spkt->filename,
									spkt->frameNumber, secondsElapsed);
								 break;
		}
	}
}

/*
 *
 */
void MultiSync::ProcessCommandPacket(ControlPkt *pkt, int len)
{
	LogDebug(VB_SYNC, "ProcessCommandPacket()\n");

	if (pkt->extraDataLen < sizeof(CommandPkt)) {
		LogErr(VB_SYNC, "Error: Invalid length of received command packet\n");
		HexDump("Received data:", (void*)&pkt, len);
		return;
	}

	CommandPkt *cpkt = (CommandPkt*)(((char*)pkt) + sizeof(ControlPkt));

    char response[1500];
	char *r2 = ProcessCommand(cpkt->command, response);
    if (r2) {
        free(r2);
    }
}

/*
 *
 */
void MultiSync::ProcessEventPacket(ControlPkt *pkt, int len)
{
	LogDebug(VB_SYNC, "ProcessEventPacket()\n");

	if (pkt->extraDataLen < sizeof(EventPkt)) {
		LogErr(VB_SYNC, "Error: Invalid length of received Event packet\n");
		HexDump("Received data:", (void*)&pkt, len);
		return;
	}

	EventPkt *epkt = (EventPkt*)(((char*)pkt) + sizeof(ControlPkt));

    PluginManager::INSTANCE.eventCallback(epkt->eventID, "remote");
	TriggerEventByID(epkt->eventID);
}

/*
 *
 */
void MultiSync::ProcessPingPacket(ControlPkt *pkt, int len)
{
	LogDebug(VB_SYNC, "ProcessPingPacket()\n");

	if (pkt->extraDataLen < 169) { // v1 packet length
		LogErr(VB_SYNC, "ERROR: Invalid length of received Ping packet\n");
		HexDump("Received data:", (void*)&pkt, len);
		return;
	}

	unsigned char *extraData = (unsigned char *)(((char*)pkt) + sizeof(ControlPkt));

	unsigned char pingVersion = extraData[0];

	if ((pingVersion == 1) && (pkt->extraDataLen > 169)) {
        LogErr(VB_SYNC, "ERROR: Ping v1 packet too long: %d\n", pkt->extraDataLen);
		HexDump("Received data:", (void*)&pkt, len);
		return;
	}
    if ((pingVersion == 2) && (pkt->extraDataLen > 214)) {
        LogErr(VB_SYNC, "ERROR: Ping v2 packet too long %d\n", pkt->extraDataLen);
        HexDump("Received data:", (void*)&pkt, len);
        return;
    }
	int discover = extraData[1];

	MultiSyncSystemType type = (MultiSyncSystemType)extraData[2];

	unsigned int majorVersion = ((unsigned int)extraData[3] << 8) | extraData[4];
	unsigned int minorVersion = ((unsigned int)extraData[5] << 8) | extraData[6];

	FPPMode systemMode = (FPPMode)extraData[7];

	char addrStr[16];
    bool isInstance = true;
    if (extraData[8] == 0
        && extraData[9] == 0
        && extraData[10] == 0
        && extraData[11] == 0
        && discover) {
        //No ip address in packet, this is a ping/discovery packet
        //from something (xLights?) that is just trying to
        //get a list of FPP instances, we won't record this
        isInstance = false;
    }

	snprintf(addrStr, 16, "%d.%d.%d.%d", extraData[8], extraData[9],
		extraData[10], extraData[11]);

	std::string address = addrStr;

	char tmpStr[128];
	strcpy(tmpStr, (char*)(extraData + 12));
	std::string hostname(tmpStr);

	strcpy(tmpStr, (char*)(extraData + 77));
	std::string version(tmpStr);

	strcpy(tmpStr, (char*)(extraData + 118));
	std::string typeStr(tmpStr);
    // End of v1 packet fields
    
    std::string ranges;
    if ((pkt->extraDataLen) > 169) {
        strcpy(tmpStr, (char*)(extraData + 166-7));
        ranges = tmpStr;
    }

    if (isInstance) {
        multiSync->UpdateSystem(type, majorVersion, minorVersion,
                                systemMode, address, hostname, version, typeStr, ranges);
    }

	if ((discover) &&
		(hostname != m_hostname) &&
		(address != m_localAddress))
		multiSync->Ping();
}


void MultiSync::ProcessPluginPacket(ControlPkt *pkt, int plen) {
    LogDebug(VB_SYNC, "ProcessPluginPacket()\n");
    CommandPkt *cpkt = (CommandPkt*)(((char*)pkt) + sizeof(ControlPkt));
    int len = pkt->extraDataLen;
    char *pn = &cpkt->command[0];
    int nlen = strlen(pn) + 1;
    len -= nlen;
    uint8_t *data = (uint8_t*)&cpkt->command[nlen];
    PluginManager::INSTANCE.multiSyncData(pn, data, len);
}
