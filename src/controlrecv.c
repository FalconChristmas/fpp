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
#include "falcon.h"
#include "log.h"
#include "sequence.h"
#include "settings.h"

struct sockaddr_in  cSrcAddr;

int ctrlRecvSock = 0;

/*
 *
 */
int InitControlSocket(void) {
	LogDebug(VB_CONTROL, "InitControlSocket()\n");

	int            UniverseOctet[2];
	int            i;
	struct ip_mreq mreq;
	char           strMulticastGroup[16];

	/* set up socket */
	ctrlRecvSock = socket(AF_INET, SOCK_DGRAM, 0);
	if (ctrlRecvSock < 0) {
		perror("socket");
		exit(1);
	}

	bzero((char *)&cSrcAddr, sizeof(cSrcAddr));
	cSrcAddr.sin_family = AF_INET;
	cSrcAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	cSrcAddr.sin_port = htons(FPP_CTRL_PORT);

	// Bind the socket to address/port
	if (bind(ctrlRecvSock, (struct sockaddr *) &cSrcAddr, sizeof(cSrcAddr)) < 0) 
	{
		perror("bind");
		exit(1);
	}

	// Receive multicast from anywhere		
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	mreq.imr_multiaddr.s_addr = inet_addr(FPP_CTRL_ADDR);

	// Add group to groups to listen for
	if (setsockopt(ctrlRecvSock, IPPROTO_IP, IP_ADD_MEMBERSHIP,&mreq, sizeof(mreq)) < 0) 
	{
		perror("setsockopt mreq");
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
	LogDebug(VB_SYNC, "StartSyncedSequenceSync(%s)\n", filename);
	OpenSequenceFile(filename);
}

/*
 *
 */
void StopSyncedSequence(void) {
	LogDebug(VB_SYNC, "StopSyncedSequenceSync()\n");
	CloseSequenceFile();
}

/*
 *
 */
void SyncSyncedSequence(int frameNumber, float secondsElapsed) {
//	LogDebug(VB_SYNC, "SyncSyncedSequence(%d, %.2f)\n",
//		frameNumber, secondsElapsed);

	UpdateMasterPosition(frameNumber);
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

	CommandPkt *cpkt = (CommandPkt*)(((void*)pkt) + sizeof(ControlPkt));

	ProcessCommand(cpkt->command);
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

	SyncPkt *spkt = (SyncPkt*)(((void*)pkt) + sizeof(ControlPkt));

	spkt->pktType     = spkt->pktType;
	spkt->frameNumber = spkt->frameNumber;

	switch (spkt->pktType) {
		case SYNC_PKT_START: StartSyncedSequence(spkt->filename);
							 break;
		case SYNC_PKT_STOP:  StopSyncedSequence();
							 break;
		case SYNC_PKT_SYNC:  SyncSyncedSequence(spkt->frameNumber, spkt->secondsElapsed);
							 break;
	}
}

/*
 *
 */
void ProcessControlPacket(void) {
	LogExcess(VB_CONTROL, "ProcessControlPacket()\n");

	char        inBuf[2048];
	ControlPkt *pkt;
	int         addrlen = sizeof(cSrcAddr);
	int         len = 0;

	len = recvfrom(ctrlRecvSock, inBuf, sizeof(inBuf), 0, (struct sockaddr *) &cSrcAddr, &addrlen);

	if (inBuf[0] == 0x55) {
		ProcessFalconPacket(&cSrcAddr, inBuf);
		return;
	}

	if (len <= sizeof(ControlPkt)) {
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
		case CTRL_PKT_SYNC: ProcessSyncPacket(pkt, len);
							break;
	}
}

