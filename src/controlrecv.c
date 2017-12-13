/*
 *   Falcon Pi Player control socket recieve code
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
#include <unistd.h>

#include "channeloutputthread.h"
#include "command.h"
#include "common.h"
#include "control.h"
#include "events.h"
#include "falcon.h"
#include "log.h"
#include "mediaoutput.h"
#include "Plugins.h"
#include "Sequence.h"
#include "settings.h"

struct sockaddr_in  crSrcAddr;

int ctrlRecvSock = 0;
extern PluginCallbackManager pluginCallbackManager;

/*
 *
 */
int InitControlSocket(void) {
	LogDebug(VB_CONTROL, "InitControlSocket()\n");

	int            UniverseOctet[2];
	int            i;
	char           strMulticastGroup[16];

	/* set up socket */
	ctrlRecvSock = socket(AF_INET, SOCK_DGRAM, 0);
	if (ctrlRecvSock < 0) {
		perror("control socket");
		exit(1);
	}

	bzero((char *)&crSrcAddr, sizeof(crSrcAddr));
	crSrcAddr.sin_family = AF_INET;
	crSrcAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	crSrcAddr.sin_port = htons(FPP_CTRL_PORT);

	int optval = 1;
#ifdef SO_REUSEPORT
	if (setsockopt(ctrlRecvSock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0)
	{
		perror("control setsockopt SO_REUSEPORT");
		exit(1);
	}
#endif

	// Bind the socket to address/port
	if (bind(ctrlRecvSock, (struct sockaddr *) &crSrcAddr, sizeof(crSrcAddr)) < 0) 
	{
		perror("control bind");
		exit(1);
	}

	if (setsockopt(ctrlRecvSock, IPPROTO_IP, IP_PKTINFO, &optval, sizeof(optval)) < 0)
	{
		perror("control setsockopt pktinfo");
		exit(1);
	}

	return ctrlRecvSock;
}

/*
 *
 */
void ShutdownControlSocket(void) {
	LogDebug(VB_CONTROL, "ShutdownControlSocket()\n");

	if (ctrlRecvSock)
		close(ctrlRecvSock);
}

/*
 *
 */
void StartSyncedSequence(char *filename) {
	LogDebug(VB_SYNC, "StartSyncedSequence(%s)\n", filename);

	sequence->OpenSequenceFile(filename);
	ResetMasterPosition();
}

/*
 *
 */
void StopSyncedSequence(char *filename) {
	LogDebug(VB_SYNC, "StopSyncedSequence(%s)\n", filename);

	sequence->CloseIfOpen(filename);
}

/*
 *
 */
void SyncSyncedSequence(char *filename, int frameNumber, float secondsElapsed) {
	LogExcess(VB_SYNC, "SyncSyncedSequence('%s', %d, %.2f)\n",
		filename, frameNumber, secondsElapsed);

	if (!sequence->IsSequenceRunning(filename))
	{
		if (sequence->OpenSequenceFile(filename))
			sequence->SeekSequenceFile(frameNumber);
	}

	if (sequence->IsSequenceRunning(filename))
		UpdateMasterPosition(frameNumber);
}

/*
 *
 */
void StopSyncedMedia(char *filename) {
	LogDebug(VB_SYNC, "StopSyncedMedia(%s)\n", filename);

	if (!mediaOutput)
		return;

	int stopSyncedMedia = 0;

	if (!strcmp(mediaOutput->m_mediaFilename.c_str(), filename))
	{
		stopSyncedMedia = 1;
	}
	else
	{
		char tmpFile[1024];
		strcpy(tmpFile, filename);

		int filenameLen = strlen(filename);
		if (filenameLen > 4)
		{
			if ((!strcmp(&tmpFile[filenameLen - 4], ".mp3")) ||
				(!strcmp(&tmpFile[filenameLen - 4], ".ogg")))
			{
				strcpy(&tmpFile[filenameLen - 4], ".mp4");

				if (!strcmp(mediaOutput->m_mediaFilename.c_str(), tmpFile))
					stopSyncedMedia = 1;
			}
		}
	}

	if (stopSyncedMedia)
	{
		LogDebug(VB_SYNC, "Stopping synced media: %s\n", mediaOutput->m_mediaFilename.c_str());
		CloseMediaOutput();
	}
}

/*
 *
 */
void StartSyncedMedia(char *filename) {
	LogDebug(VB_SYNC, "StartSyncedMedia(%s)\n", filename);

	if (mediaOutput)
	{
		LogDebug(VB_SYNC, "Start media %s received while playing media %s\n",
			filename, mediaOutput->m_mediaFilename.c_str());
		CloseMediaOutput();
	}

	OpenMediaOutput(filename);
	ResetMasterPosition();
}

/*
 *
 */
void SyncSyncedMedia(char *filename, int frameNumber, float secondsElapsed) {
	LogExcess(VB_SYNC, "SyncSyncedMedia('%s', %d, %.2f)\n",
		filename, frameNumber, secondsElapsed);

	if (!mediaOutput)
	{
		LogExcess(VB_SYNC, "Received sync for media %s but no media playing\n",
			filename);
		return;
	}

	if (!strcmp(mediaOutput->m_mediaFilename.c_str(), filename))
	{
		UpdateMasterMediaPosition(secondsElapsed);
	}
}

/*
 *
 */
void ProcessCommandPacket(ControlPkt *pkt, int len) {
	LogDebug(VB_CONTROL, "ProcessCommandPacket()\n");

	if (pkt->extraDataLen < sizeof(CommandPkt)) {
		LogErr(VB_CONTROL, "Error: Invalid length of received command packet\n");
		HexDump("Received data:", (void*)&pkt, len);
		return;
	}

	CommandPkt *cpkt = (CommandPkt*)(((char*)pkt) + sizeof(ControlPkt));

	ProcessCommand(cpkt->command);
}

/*
 *
 */
void ProcessEventPacket(ControlPkt *pkt, int len) {
	LogDebug(VB_CONTROL, "ProcessEventPacket()\n");

	if (pkt->extraDataLen < sizeof(EventPkt)) {
		LogErr(VB_CONTROL, "Error: Invalid length of received Event packet\n");
		HexDump("Received data:", (void*)&pkt, len);
		return;
	}

	EventPkt *epkt = (EventPkt*)(((char*)pkt) + sizeof(ControlPkt));

	pluginCallbackManager.eventCallback(epkt->eventID, "remote");
	TriggerEventByID(epkt->eventID);
}

/*
 *
 */
void ProcessSyncPacket(ControlPkt *pkt, int len) {
	LogDebug(VB_CONTROL, "ProcessSyncPacket()\n");

	if (pkt->extraDataLen < sizeof(SyncPkt)) {
		LogErr(VB_CONTROL, "Error: Invalid length of received sync packet\n");
		HexDump("Received data:", (void*)&pkt, len);
		return;
	}

	SyncPkt *spkt = (SyncPkt*)(((char*)pkt) + sizeof(ControlPkt));

	spkt->pktType     = spkt->pktType;
	spkt->frameNumber = spkt->frameNumber;

	if (spkt->fileType == SYNC_FILE_SEQ)
	{
		switch (spkt->pktType) {
			case SYNC_PKT_START: StartSyncedSequence(spkt->filename);
								 break;
			case SYNC_PKT_STOP:  StopSyncedSequence(spkt->filename);
								 break;
			case SYNC_PKT_SYNC:  SyncSyncedSequence(spkt->filename,
									spkt->frameNumber, spkt->secondsElapsed);
								 break;
		}
	}
	else if (spkt->fileType == SYNC_FILE_MEDIA)
	{
		switch (spkt->pktType) {
			case SYNC_PKT_START: StartSyncedMedia(spkt->filename);
								 break;
			case SYNC_PKT_STOP:  StopSyncedMedia(spkt->filename);
								 break;
			case SYNC_PKT_SYNC:  SyncSyncedMedia(spkt->filename,
									spkt->frameNumber, spkt->secondsElapsed);
								 break;
		}
	}
}

/*
 *
 */
void ProcessControlPacket(void) {
	LogExcess(VB_CONTROL, "ProcessControlPacket()\n");

	unsigned char inBuf[2048];
	ControlPkt *pkt;
	int         addrlen = sizeof(crSrcAddr);
	int         len = 0;

	struct iovec iov[1];
	iov[0].iov_base = inBuf;
	iov[0].iov_len  = sizeof(inBuf);

	char                     cmbuf[0x100];
	struct sockaddr_storage  mSrcAddr;
	struct msghdr            msg;

	msg.msg_name       = &mSrcAddr;
	msg.msg_namelen    = sizeof(mSrcAddr);
	msg.msg_iov        = iov;
	msg.msg_iovlen     = 1;
	msg.msg_control    = cmbuf;
	msg.msg_controllen = sizeof(cmbuf);

	bzero(inBuf, sizeof(inBuf));

	len = recvmsg(ctrlRecvSock, &msg, 0);
	if (len == -1) {
		LogErr(VB_CONTROL, "Error: recvmsg failed: %s\n", strerror(errno));
		return;
	} else if (msg.msg_flags & MSG_TRUNC) {
		LogErr(VB_CONTROL, "Error: Received control packet too large\n");
		HexDump("Received data:", (void*)inBuf, iov[0].iov_len);
		return;
	}

	if (inBuf[0] == 0x55 || inBuf[0] == 0xCC) {
		struct in_addr  recvAddr;
		struct cmsghdr *cmsg;

		for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg))
		{
			if (cmsg->cmsg_level != IPPROTO_IP || cmsg->cmsg_type != IP_PKTINFO)
			{
				continue;
			}

			struct in_pktinfo *pi = (struct in_pktinfo *)CMSG_DATA(cmsg);
			recvAddr = pi->ipi_addr;
			recvAddr = pi->ipi_spec_dst;
		}

		ProcessFalconPacket(ctrlRecvSock, (struct sockaddr_in *)&mSrcAddr, recvAddr, inBuf);
		return;
	}

	if (len < sizeof(ControlPkt)) {
		LogErr(VB_CONTROL, "Error: Received control packet too short\n");
		HexDump("Received data:", (void*)inBuf, len);
		return;
	}

	pkt = (ControlPkt*)inBuf;

	if ((pkt->fppd[0] != 'F') ||
		(pkt->fppd[1] != 'P') ||
		(pkt->fppd[2] != 'P') ||
		(pkt->fppd[3] != 'D')) {
		LogErr(VB_CONTROL, "Error: Invalid Received Control Packet, missing 'FPPD' header\n");
		HexDump("Received data:", (void*)inBuf, len);
		return;
	}

	if (len != (sizeof(ControlPkt) + pkt->extraDataLen)) {
		LogErr(VB_CONTROL, "Error: Expected %d data bytes, received %d\n",
			pkt->extraDataLen, len - sizeof(ControlPkt));
		HexDump("Received data:", (void*)inBuf, len);
		return;
	}

	if ((logLevel == LOG_EXCESSIVE) &&
		(logMask & VB_CONTROL)) {
		HexDump("Received Master/Remote sync packet with contents:", (void*)inBuf, len);
	}

	switch (pkt->pktType) {
		case CTRL_PKT_CMD:	ProcessCommandPacket(pkt, len);
							break;
		case CTRL_PKT_SYNC: if (getFPPmode() == REMOTE_MODE)
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
	}
}

