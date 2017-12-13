/*
 *   Falcon Pi Player control socket send file
 *   Falcon Pi Player project (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"
#include "control.h"
#include "log.h"
#include "settings.h"

#define MAX_SYNC_REMOTES 64

int ctrlSendSock    = -1;
int ctrlSendCSVSock = -1;
int remoteCount     = 0;
int remoteCSVCount     = 0;

char                cLocalAddress[16];
struct sockaddr_in  cSrcAddr;
struct sockaddr_in  cDestAddr[MAX_SYNC_REMOTES];
struct sockaddr_in  cDestAddrCSV[MAX_SYNC_REMOTES];

pthread_mutex_t      syncSocketLock;

/*
 * Send a Control Packet
 */
void SendControlPacket(void *outBuf, int len) {
	if ((logLevel == LOG_EXCESSIVE) &&
		(logMask & VB_SYNC)) {
		HexDump("Sending Control packet with contents:", outBuf, len);
	}

	pthread_mutex_lock(&syncSocketLock);

	int i = 0;
	for (i = 0; i < remoteCount; i++) {
		if (sendto(ctrlSendSock, outBuf, len, 0, (struct sockaddr*)&cDestAddr[i], sizeof(struct sockaddr_in)) < 0)
			LogErr(VB_SYNC, "Error: Unable to send packet: %s\n", strerror(errno));
	}

	pthread_mutex_unlock(&syncSocketLock);
}

/*
 *
 */
void SendControlCSVPacket(void *outBuf, int len) {
	LogExcess(VB_SYNC, "CSV Sync: '%s'\n", (char *)outBuf);

	if (ctrlSendCSVSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send CSV Sync packet but CSV sync socket is not open.\n");
		return;
	}

	pthread_mutex_lock(&syncSocketLock);

	int i = 0;
	for (i = 0; i < remoteCSVCount; i++) {
		if (sendto(ctrlSendCSVSock, outBuf, len, 0, (struct sockaddr*)&cDestAddrCSV[i], sizeof(struct sockaddr_in)) < 0)
			LogErr(VB_SYNC, "Error: Unable to send packet: %s\n", strerror(errno));
	}

	pthread_mutex_unlock(&syncSocketLock);
}

/*
 *
 */
void InitControlPkt(ControlPkt *pkt) {
	bzero(pkt, sizeof(ControlPkt));

	pkt->fppd[0]      = 'F';
	pkt->fppd[1]      = 'P';
	pkt->fppd[2]      = 'P';
	pkt->fppd[3]      = 'D';
	pkt->pktType      = 0;
	pkt->extraDataLen = 0;
}

/*****************************************************************************
 * Master Functions
 *****************************************************************************/

/*
 *
 */
int InitSyncMasterCSV(void) {
	LogDebug(VB_SYNC, "InitSyncMasterCSV()\n");

	ctrlSendCSVSock = socket(AF_INET, SOCK_DGRAM, 0);

	if (ctrlSendCSVSock < 0) {
		LogErr(VB_SYNC, "Error opening Master/Remote CSV Sync socket\n");
		exit(1);
	}

	char loopch = 0;
	if(setsockopt(ctrlSendCSVSock, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
		LogErr(VB_SYNC, "Error setting IP_MULTICAST_LOOP: \n",
			strerror(errno));
		exit(1);
	}

	char *tmpRemotes = strdup(getSetting("MultiSyncCSVRemotes"));

	if (!strcmp(tmpRemotes, "255.255.255.255"))
	{
		int broadcast = 1;
		if(setsockopt(ctrlSendSock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
			LogErr(VB_SYNC, "Error setting SO_BROADCAST: \n", strerror(errno));
			exit(1);
		}
	}

	remoteCSVCount = 0;
	memset((void *)&cDestAddrCSV, 0, sizeof(struct sockaddr_in) * MAX_SYNC_REMOTES);

	char *s = strtok(tmpRemotes, ",");

	while (s && (remoteCSVCount < MAX_SYNC_REMOTES)) {
		LogDebug(VB_SYNC, "Setting up CSV Remote Sync for %s\n", s);
		cDestAddrCSV[remoteCSVCount].sin_family      = AF_INET;
		cDestAddrCSV[remoteCSVCount].sin_port        = htons(FPP_CTRL_CSV_PORT);
		cDestAddrCSV[remoteCSVCount].sin_addr.s_addr = inet_addr(s);
		s = strtok(NULL, ",");
		remoteCSVCount++;
	}

	if (s && (remoteCSVCount >= MAX_SYNC_REMOTES)) {
		LogErr(VB_SYNC, "Maximum number of %d remotes configured\n",
			MAX_SYNC_REMOTES);
	}

	LogDebug(VB_SYNC, "%d Remote Sync systems configured\n", remoteCSVCount);

	free(tmpRemotes);

	return 1;
}

/*
 *
 */
int InitSyncMaster(void) {
	LogDebug(VB_SYNC, "InitSyncMaster()\n");

	ctrlSendSock = socket(AF_INET, SOCK_DGRAM, 0);

	if (ctrlSendSock < 0) {
		LogErr(VB_SYNC, "Error opening Master/Remote Sync socket\n");
		exit(1);
	}

	char loopch = 0;
	if(setsockopt(ctrlSendSock, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
		LogErr(VB_SYNC, "Error setting IP_MULTICAST_LOOP: \n",
			strerror(errno));
		exit(1);
	}

	char *tmpRemotes = strdup(getSetting("MultiSyncRemotes"));

	if (!strcmp(tmpRemotes, "255.255.255.255"))
	{
		int broadcast = 1;
		if(setsockopt(ctrlSendSock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
			LogErr(VB_SYNC, "Error setting SO_BROADCAST: \n", strerror(errno));
			exit(1);
		}
	}

	remoteCount = 0;
	memset((void *)&cDestAddr, 0, sizeof(struct sockaddr_in) * MAX_SYNC_REMOTES);

	char *s = strtok(tmpRemotes, ",");

	while (s && (remoteCount < MAX_SYNC_REMOTES)) {
		LogDebug(VB_SYNC, "Setting up Remote Sync for %s\n", s);
		cDestAddr[remoteCount].sin_family      = AF_INET;
		cDestAddr[remoteCount].sin_port        = htons(FPP_CTRL_PORT);
		cDestAddr[remoteCount].sin_addr.s_addr = inet_addr(s);
		s = strtok(NULL, ",");
		remoteCount++;
	}

	if (s && (remoteCount >= MAX_SYNC_REMOTES)) {
		LogErr(VB_SYNC, "Maximum number of %d remotes configured\n",
			MAX_SYNC_REMOTES);
	}

	LogDebug(VB_SYNC, "%d Remote Sync systems configured\n", remoteCount);

	free(tmpRemotes);

	pthread_mutex_init(&syncSocketLock, NULL);

	InitSyncMasterCSV();

	return 1;
}

/*
 *
 */
void ShutdownSync(void) {
	LogDebug(VB_SYNC, "ShutdownMSSync()\n");

	pthread_mutex_lock(&syncSocketLock);

	if (ctrlSendSock >= 0)
	{
		close(ctrlSendSock);
		ctrlSendSock = -1;
	}

	if (ctrlSendCSVSock >= 0)
	{
		close(ctrlSendCSVSock);
		ctrlSendCSVSock = -1;
	}

	pthread_mutex_unlock(&syncSocketLock);

	pthread_mutex_destroy(&syncSocketLock);
}

/*
 *
 */
void SendSeqSyncStartPacket(const char *filename) {
	LogDebug(VB_SYNC, "SendSeqSyncStartPacket('%s')\n", filename);

	if (!filename || !filename[0])
		return;

	if (ctrlSendSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send start packet but sync socket is not open.\n");
		return;
	}

	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPkt(cpkt);

	cpkt->pktType        = CTRL_PKT_SYNC;
	cpkt->extraDataLen   = sizeof(SyncPkt) + strlen(filename);
	
	spkt->pktType  = SYNC_PKT_START;
	spkt->fileType = SYNC_FILE_SEQ;
	spkt->frameNumber = 0;
	spkt->secondsElapsed = 0;
	strcpy(spkt->filename, filename);

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + strlen(filename));

	if (remoteCSVCount > 0)
	{
		// Now send the Broadcast CSV version
		sprintf(outBuf, "FPP,%d,%d,%d,%s\n",
			CTRL_PKT_SYNC, SYNC_FILE_SEQ, SYNC_PKT_START, filename);
		SendControlCSVPacket(outBuf, strlen(outBuf));
	}
}

/*
 *
 */
void SendSeqSyncStopPacket(const char *filename) {
	LogDebug(VB_SYNC, "SendSeqSyncStopPacket(%s)\n", filename);

	if (!filename || !filename[0])
		return;

	if (ctrlSendSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send stop packet but sync socket is not open.\n");
		return;
	}

	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPkt(cpkt);

	cpkt->pktType        = CTRL_PKT_SYNC;
	cpkt->extraDataLen   = sizeof(SyncPkt) + strlen(filename);
	
	spkt->pktType  = SYNC_PKT_STOP;
	spkt->fileType = SYNC_FILE_SEQ;
	spkt->frameNumber = 0;
	spkt->secondsElapsed = 0;
	strcpy(spkt->filename, filename);

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + strlen(filename));

	if (remoteCSVCount > 0)
	{
		// Now send the Broadcast CSV version
		sprintf(outBuf, "FPP,%d,%d,%d,%s\n",
			CTRL_PKT_SYNC, SYNC_FILE_SEQ, SYNC_PKT_STOP, filename);
		SendControlCSVPacket(outBuf, strlen(outBuf));
	}
}

/*
 *
 */
void SendSeqSyncPacket(const char *filename, int frames, float seconds) {
	LogExcess(VB_SYNC, "SendSeqSyncPacket( '%s', %d, %.2f)\n",
		filename, frames, seconds);

	if (!filename || !filename[0])
		return;

	if (ctrlSendSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send sync packet but sync socket is not open.\n");
		return;
	}

	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPkt(cpkt);

	cpkt->pktType        = CTRL_PKT_SYNC;
	cpkt->extraDataLen   = sizeof(SyncPkt) + strlen(filename);
	
	spkt->pktType  = SYNC_PKT_SYNC;
	spkt->fileType = SYNC_FILE_SEQ;
	spkt->frameNumber = frames;
	spkt->secondsElapsed = seconds; // FIXME, does this have endianness issues?
	strcpy(spkt->filename, filename);

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + strlen(filename));

	if (remoteCSVCount > 0)
	{
		// Now send the Broadcast CSV version
		sprintf(outBuf, "FPP,%d,%d,%d,%s,%d,%d\n",
			CTRL_PKT_SYNC, SYNC_FILE_SEQ, SYNC_PKT_SYNC, filename,
			(int)seconds, (int)(seconds * 1000) % 1000);
		SendControlCSVPacket(outBuf, strlen(outBuf));
	}
}

/*
 *
 */
void SendMediaSyncStartPacket(const char *filename) {
	LogDebug(VB_SYNC, "SendMediaSyncStartPacket('%s')\n", filename);

	if (!filename || !filename[0])
		return;

	if (ctrlSendSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send start packet but sync socket is not open.\n");
		return;
	}

	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPkt(cpkt);

	cpkt->pktType        = CTRL_PKT_SYNC;
	cpkt->extraDataLen   = sizeof(SyncPkt) + strlen(filename);

	spkt->pktType  = SYNC_PKT_START;
	spkt->fileType = SYNC_FILE_MEDIA;
	spkt->frameNumber = 0;
	spkt->secondsElapsed = 0;
	strcpy(spkt->filename, filename);

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + strlen(filename));

	if (remoteCSVCount > 0)
	{
		// Now send the Broadcast CSV version
		sprintf(outBuf, "FPP,%d,%d,%d,%s\n",
			CTRL_PKT_SYNC, SYNC_FILE_MEDIA, SYNC_PKT_START, filename);
		SendControlCSVPacket(outBuf, strlen(outBuf));
	}
}

/*
 *
 */
void SendMediaSyncStopPacket(const char *filename) {
	LogDebug(VB_SYNC, "SendMediaSyncStopPacket(%s)\n", filename);

	if (!filename || !filename[0])
		return;

	if (ctrlSendSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send stop packet but sync socket is not open.\n");
		return;
	}

	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPkt(cpkt);

	cpkt->pktType        = CTRL_PKT_SYNC;
	cpkt->extraDataLen   = sizeof(SyncPkt) + strlen(filename);

	spkt->pktType  = SYNC_PKT_STOP;
	spkt->fileType = SYNC_FILE_MEDIA;
	spkt->frameNumber = 0;
	spkt->secondsElapsed = 0;
	strcpy(spkt->filename, filename);

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + strlen(filename));

	if (remoteCSVCount > 0)
	{
		// Now send the Broadcast CSV version
		sprintf(outBuf, "FPP,%d,%d,%d,%s\n",
			CTRL_PKT_SYNC, SYNC_FILE_MEDIA, SYNC_PKT_STOP, filename);
		SendControlCSVPacket(outBuf, strlen(outBuf));
	}
}

/*
 *
 */
void SendMediaSyncPacket(const char *filename, int frames, float seconds) {
	LogExcess(VB_SYNC, "SendMediaSyncPacket( '%s', %d, %.2f)\n",
		filename, frames, seconds);

	if (!filename || !filename[0])
		return;

	if (ctrlSendSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send sync packet but sync socket is not open.\n");
		return;
	}

	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPkt(cpkt);

	cpkt->pktType        = CTRL_PKT_SYNC;
	cpkt->extraDataLen   = sizeof(SyncPkt) + strlen(filename);

	spkt->pktType  = SYNC_PKT_SYNC;
	spkt->fileType = SYNC_FILE_MEDIA;
	spkt->frameNumber = frames;
	spkt->secondsElapsed = seconds; // FIXME, does this have endianness issues?
	strcpy(spkt->filename, filename);

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + strlen(filename));

	if (remoteCSVCount > 0)
	{
		// Now send the Broadcast CSV version
		sprintf(outBuf, "FPP,%d,%d,%d,%s,%d,%d\n",
			CTRL_PKT_SYNC, SYNC_FILE_MEDIA, SYNC_PKT_SYNC, filename,
			(int)seconds, (int)(seconds * 1000) % 1000);
		SendControlCSVPacket(outBuf, strlen(outBuf));
	}
}

/*
 *
 */
void SendEventPacket(const char *eventID) {
	LogDebug(VB_SYNC, "SendEventPacket('%s')\n", eventID);

	if (!eventID || !eventID[0])
		return;

	if (ctrlSendSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send event packet but control socket is not open.\n");
		return;
	}

	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	EventPkt *epkt = (EventPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPkt(cpkt);

	cpkt->pktType        = CTRL_PKT_EVENT;
	cpkt->extraDataLen   = sizeof(EventPkt);

	strcpy(epkt->eventID, eventID);

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(EventPkt));
}

/*
 *
 */
void SendBlankingDataPacket(void)
{
	LogDebug(VB_SYNC, "SendBlankingDataPacket()\n");

	if (ctrlSendSock < 0) {
		LogErr(VB_SYNC, "ERROR: Tried to send blanking data packet but control socket is not open.\n");
		return;
	}

	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;

	InitControlPkt(cpkt);

	cpkt->pktType        = CTRL_PKT_BLANK;
	cpkt->extraDataLen   = 0;

	SendControlPacket(outBuf, sizeof(ControlPkt));

	if (ctrlSendCSVSock >= 0)
	{
		// Now send the Broadcast CSV version
		sprintf(outBuf, "FPP,%d\n", CTRL_PKT_BLANK);
		SendControlCSVPacket(outBuf, strlen(outBuf));
	}
}

