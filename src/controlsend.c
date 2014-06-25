/*
 *   Falcon Pi Player daemon header file  
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

int ctrlSendSock    = 0;
int remoteCount     = 0;

char               *cLocalAddress;
struct sockaddr_in  cSrcAddr;
struct sockaddr_in  cDestAddr[MAX_SYNC_REMOTES];


/*
 * Send a Control Packet
 */
void SendControlPacket(void *outBuf, int len) {
	if ((logLevel == LOG_EXCESSIVE) &&
		(logMask & VB_SYNC)) {
		HexDump("Sending Control packet with contents:", outBuf, len);
	}

	int i = 0;
	for (i = 0; i < remoteCount; i++) {
		if (sendto(ctrlSendSock, outBuf, len, 0, (struct sockaddr*)&cDestAddr[i], sizeof(struct sockaddr_in)) < 0)
			LogErr(VB_SYNC, "Error: Unable to send packet: %s\n", strerror(errno));
	}
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
int InitSyncMaster(void) {
	LogDebug(VB_SYNC, "InitSyncMaster()\n");

	cLocalAddress = GetInterfaceAddress(getE131interface()); // FIXME

	ctrlSendSock = socket(AF_INET, SOCK_DGRAM, 0);

	if (ctrlSendSock < 0) {
		LogErr(VB_SYNC, "Error opening Master/Remote Sync socket\n");
		exit(1);
	}

	cSrcAddr.sin_family      = AF_INET;
	cSrcAddr.sin_port        = htons(FPP_CTRL_PORT);
	cSrcAddr.sin_addr.s_addr = inet_addr(cLocalAddress);

	if(bind(ctrlSendSock, (struct sockaddr *)&cSrcAddr, sizeof(cSrcAddr)) == -1) {
		LogErr(VB_SYNC, "Error in bind: %s\n", strerror(errno));
		exit(1);
	}

	char loopch = 0;
	if(setsockopt(ctrlSendSock, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
		LogErr(VB_SYNC, "Error setting IP_MULTICAST_LOOP: \n",
			strerror(errno));
		exit(1);
	}

	remoteCount = 0;
	memset((void *)&cDestAddr, 0, sizeof(struct sockaddr_in) * MAX_SYNC_REMOTES);

	char *tmpRemotes = strdup(getSetting("MultiSyncRemotes"));
	char *s = strtok(tmpRemotes, ",");

	while (s) {
		LogDebug(VB_SYNC, "Setting up Remote Sync for %s\n", s);
		cDestAddr[remoteCount].sin_family      = AF_INET;
		cDestAddr[remoteCount].sin_port        = htons(FPP_CTRL_PORT);
		cDestAddr[remoteCount].sin_addr.s_addr = inet_addr(s);
		s = strtok(NULL, ",");
		remoteCount++;
	}
	LogDebug(VB_SYNC, "%d Remote Sync systems configured\n", remoteCount);

	return 1;
}

/*
 *
 */
void ShutdownSync(void) {
	LogDebug(VB_SYNC, "ShutdownMSSync()\n");

	if (ctrlSendSock)
		close(ctrlSendSock);
}

/*
 *
 */
void SendSeqSyncStartPacket(char *filename) {
	LogDebug(VB_SYNC, "SendSeqSyncStartPacket('%s')\n", filename);

	if (!ctrlSendSock) {
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
}

/*
 *
 */
void SendSeqSyncStopPacket(void) {
	LogDebug(VB_SYNC, "SendSeqSyncStopPacket()\n");

	if (!ctrlSendSock) {
		LogErr(VB_SYNC, "ERROR: Tried to send stop packet but sync socket is not open.\n");
		return;
	}

	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPkt(cpkt);

	cpkt->pktType        = CTRL_PKT_SYNC;
	cpkt->extraDataLen   = sizeof(SyncPkt);
	
	spkt->pktType  = SYNC_PKT_STOP;
	spkt->fileType = SYNC_FILE_SEQ;
	spkt->frameNumber = 0;
	spkt->secondsElapsed = 0;

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt));
}

/*
 *
 */
void SendSeqSyncPacket(int frames, float seconds) {
	LogExcess(VB_SYNC, "SendSeqSyncPacket( %d, %.2f)\n", frames, seconds);

	if (!ctrlSendSock) {
		LogErr(VB_SYNC, "ERROR: Tried to send sync packet but sync socket is not open.\n");
		return;
	}

	char           outBuf[2048];
	bzero(outBuf, sizeof(outBuf));

	ControlPkt    *cpkt = (ControlPkt*)outBuf;
	SyncPkt *spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

	InitControlPkt(cpkt);

	cpkt->pktType        = CTRL_PKT_SYNC;
	cpkt->extraDataLen   = sizeof(SyncPkt);
	
	spkt->pktType  = SYNC_PKT_SYNC;
	spkt->fileType = SYNC_FILE_SEQ;
	spkt->frameNumber = frames;
	spkt->secondsElapsed = seconds; // FIXME, does this have endianness issues?

	SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt));
}

