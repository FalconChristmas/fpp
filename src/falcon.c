/*
 *   Falcon Pi Player config routines for Falcon hardware
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

#include "channeloutput/channeloutput.h"
#include "common.h"
#include "log.h"
#include "Player.h"
#include "playlist/NewPlaylist.h"
#include "settings.h"
#include "Sequence.h"

#ifdef USEWIRINGPI
#   include "wiringPi.h"
#   include "wiringPiSPI.h"
#else
#   define wiringPiSPISetup(a,b)    1
#   define wiringPiSPIDataRW(a,b,c) c
#   define delayMicroseconds(a)     0
#endif

#define FALCON_CFG_FILE_MAX_SIZE      2048

#define FALCON_PASSTHROUGH_DATA_SIZE  1016
#define FALCON_CFG_DATA_SIZE          32768
#define FALCON_CFG_HEADER_SIZE        6
#define FALCON_CFG_BUF_SIZE           (FALCON_CFG_DATA_SIZE+FALCON_CFG_HEADER_SIZE)


/*
 * Read the specified file into the given buffer
 */
int FalconReadConfig(char *filename, char *buf)
{
	LogDebug(VB_SETTING, "FalconReadConfig(%s, %p)\n", filename, buf);
	char  fullFilename[1024];
	FILE *fp = NULL;

	sprintf(fullFilename, "%s/config/%s", getMediaDirectory(), filename);

	fp = fopen(fullFilename, "r");
	if (!fp)
	{
		LogErr(VB_SETTING, "Error opening Falcon hardware config file %s\n",
			fullFilename);
		return -1;
	}

	int bytesRead = fread(buf, 1, FALCON_CFG_FILE_MAX_SIZE, fp);

	fclose(fp);

	return bytesRead;
}

/*
 * Write out the given buffer to the specified filename
 */
int FalconWriteConfig(char *filename, char *buf, int size)
{
	LogDebug(VB_SETTING, "FalconWriteConfig(%s, %p, %d)\n", filename, buf, size);
	char  fullFilename[1024];
	FILE *fp = NULL;

	sprintf(fullFilename, "%s/config/%s", getMediaDirectory(), filename);

	fp = fopen(fullFilename, "w");
	if (!fp)
	{
		LogErr(VB_SETTING, "Error opening Falcon hardware config file %s",
			fullFilename);
		return -1;
	}

	if (size > FALCON_CFG_FILE_MAX_SIZE)
	{
		LogWarn(VB_SETTING, "Tried to write out %d bytes when max is %d\n",
			size, FALCON_CFG_FILE_MAX_SIZE);
		size = FALCON_CFG_FILE_MAX_SIZE;
	}

	int bytesWritten = fwrite(buf, 1, size, fp);

	fclose(fp);

	return bytesWritten;
}

/*
 *
 */
int FalconConfigureHardware(char *filename, int spiPort)
{
	LogDebug(VB_SETTING, "FalconConfigureHardware(%s, %d)\n", filename, spiPort);
	char  fbuf[FALCON_CFG_FILE_MAX_SIZE];
	unsigned char *buf;

	buf = (unsigned char *)malloc(FALCON_CFG_BUF_SIZE);
	if (!buf)
	{
		LogErr(VB_SETTING,
			"Unable to allocate %d byte buffer for writing Falcon config: %s\n",
			FALCON_CFG_BUF_SIZE, strerror(errno));
		return -1;
	}

	int bytesRead = FalconReadConfig(filename, fbuf);

	if (bytesRead != 1024)
	{
		LogErr(VB_SETTING,
			"Invalid data in %s, unable to write Falcon config\n",
			filename);
		free(buf);
		return -1;
	}

	bzero(buf, FALCON_CFG_BUF_SIZE);
	memcpy(buf, fbuf, bytesRead);

	int bytesWritten;

	player->DisableChannelOutput();
	usleep(100000);

	if ((logLevel & LOG_DEBUG) && (logMask & VB_SETTING))
		HexDump("Falcon Hardware Config", buf, bytesRead);

	bytesWritten = wiringPiSPIDataRW (0, (unsigned char *)buf, FALCON_CFG_BUF_SIZE);
	if (bytesWritten != FALCON_CFG_BUF_SIZE)
	{
		LogErr(VB_SETTING,
			"Error: wiringPiSPIDataRW returned %d, expecting %d\n",
			bytesWritten, FALCON_CFG_BUF_SIZE);
	}

	if ((logLevel & LOG_DEBUG) && (logMask & VB_SETTING))
		HexDump("Falcon Hardware Config Response", buf, 8);

	usleep(10000);

	bzero(buf, FALCON_CFG_BUF_SIZE);
	memcpy(buf, fbuf, bytesRead);

	bytesWritten = wiringPiSPIDataRW (0, (unsigned char *)buf, FALCON_CFG_BUF_SIZE);
	if (bytesWritten != FALCON_CFG_BUF_SIZE)
	{
		LogErr(VB_CHANNELOUT,
			"Error: wiringPiSPIDataRW returned %d, expecting %d\n",
			bytesWritten, FALCON_CFG_BUF_SIZE);
		free(buf);
		usleep(100000);
		player->EnableChannelOutput();
		return -1;
	}

	if ((logLevel & LOG_DEBUG) && (logMask & VB_SETTING))
		HexDump("Falcon Hardware Config Response", buf, 8);

	free(buf);
	usleep(100000);
	player->EnableChannelOutput();
}

/*
 *
 */
void PopulatePiConfig(char *ipAddress, char *buf)
{
	char *iface = NULL;
	char addr[16];
	char mask[16];
	char gw[16];

	iface = FindInterfaceForIP(ipAddress);
	if (GetInterfaceAddress(iface, addr, mask, gw))
		return;

	gethostname((char*)buf+7, 31);

	// FIXME, need to fill this in with info on the right interface
	buf[39] = 0x00; // 0 = DHCP, 1 = static

	(*(in_addr_t *)&buf[40]) = inet_addr(addr);
	(*(in_addr_t *)&buf[44]) = inet_addr(gw);
	(*(in_addr_t *)&buf[48]) = inet_addr(mask);
}

/*
 *
 */
int FalconDetectHardware(int spiPort, char *response)
{
	LogDebug(VB_SETTING, "FalconDetectHardware(%p)\n", response);

	bzero(response, FALCON_CFG_BUF_SIZE);

	return wiringPiSPIDataRW(spiPort, (unsigned char *)response, FALCON_CFG_BUF_SIZE);
}

/*
 *
 */
void FalconQueryHardware(int sock, struct sockaddr_in *srcAddr,
	struct in_addr recvAddr, unsigned char *inBuf)
{
	LogDebug(VB_SETTING, "FalconQueryHardware(%p)\n", inBuf);
	// Return config information, Falcon hardware info, network IP info, etc.

	char buf[60];
	bzero(buf, sizeof(buf));

	char query[FALCON_CFG_BUF_SIZE];

	int responseSize = FalconDetectHardware(0, query);

	if (responseSize == FALCON_CFG_BUF_SIZE)
	{
		// Stuff response into our response packet. We could use memcpy, but
		// this helps document what the returned bytes are.
		buf[52] = query[0]; // Hardware ID
		buf[53] = query[1]; // Firmware Major Version
		buf[54] = query[2]; // Firmware Minor Version
		buf[55] = query[3]; // Chip Temperature
		buf[56] = query[4]; // Temperature #1
		buf[57] = query[5]; // Temperature #2
		buf[58] = query[6]; // Voltage #1
		buf[59] = query[7]; // Voltage #2

		if ((logLevel & LOG_DEBUG) && (logMask & VB_SETTING))
			HexDump("Falcon Hardware Query Info Response", query, 8);
	}
	else
	{
		LogErr(VB_SETTING, "Error retrieving hardware data via SPI\n");
	}

	buf[0] = 0x55;
	buf[1] = 0x55;
	buf[2] = 0x55;
	buf[3] = 0x55;
	buf[4] = 0x55;
	buf[5] = inBuf[5];
	buf[6] = 0x00; // Query Info command response

	PopulatePiConfig(inet_ntoa(recvAddr), buf);

	if ((logLevel & LOG_DEBUG) && (logMask & VB_SETTING))
		HexDump("Falcon Query Hardware result", buf, sizeof(buf));

	if(sendto(sock, buf, sizeof(buf), 0, (struct sockaddr*)srcAddr, sizeof(*srcAddr)) < 0)
	{
		LogDebug(VB_SETTING, "ERROR: sentto failed\n");
	}
}

/*
 *
 */
int FalconPassThroughData(int offset, unsigned char *inBuf, int size)
{
	LogDebug(VB_SETTING, "FalconPassThroughData(%p)\n", inBuf);

	if ((logLevel & LOG_DEBUG) && (logMask & VB_SETTING))
		HexDump("Falcon Pass-through data", inBuf, size);

	// Disable channel outputs and let them quiesce before sending config info
	player->DisableChannelOutput();

	usleep(100000);

	if (getSettingInt("FPDEnabled"))
	{
		unsigned char *buf = (unsigned char *)malloc(FALCON_CFG_BUF_SIZE);
		if (!buf)
		{
			LogErr(VB_SETTING,
				"Unable to allocate %d byte buffer for passing through Falcon data: %s\n",
				FALCON_CFG_BUF_SIZE, strerror(errno));
			return -1;
		}

		bzero(buf, FALCON_CFG_BUF_SIZE);

		if(offset < (FALCON_CFG_BUF_SIZE - size))
		{
			LogDebug(VB_SETTING, "Offset = %d\n", offset);
		}
		else
		{
			LogErr(VB_SETTING,"Offset %d is invalid: %s\n",offset, strerror(errno));
		}

		buf[0] = 0xCC;
		buf[1] = 0xCC;
		buf[2] = 0xCC;
		buf[3] = 0xCC;
		buf[4] = 0xCC;
		buf[5] = 0x55;

		memcpy(buf+FALCON_CFG_HEADER_SIZE+offset, inBuf, size);

		int bytesWritten;

		bytesWritten = wiringPiSPIDataRW (0, (unsigned char *)buf, FALCON_CFG_BUF_SIZE);
		if (bytesWritten != FALCON_CFG_BUF_SIZE)
		{
			LogErr(VB_SETTING,
				"Error: wiringPiSPIDataRW returned %d, expecting %d\n",
				bytesWritten, FALCON_CFG_BUF_SIZE);
		}
		free(buf);

		usleep(100000);
	}

	// Pass data on to our regular channel outputs followed by blanking data
	bzero(player->m_seqData + offset, 4096);
	memcpy(player->m_seqData + offset, inBuf, FALCON_PASSTHROUGH_DATA_SIZE);
	player->SendData();
	player->SendBlankingData(); // reset data so we don't keep reprogramming

	// Give changes time to take effect then turn back on channel outputs
	usleep(100000);

	player->EnableChannelOutput();
}

/*
 *
 */
void FalconSetData(int sock, struct sockaddr_in *srcAddr, unsigned char *inBuf)
{
	LogDebug(VB_SETTING, "FalconSetData(%p)\n", inBuf);

	char filename[16];
	int  len = 0;
	int  configureHardware = 1;
	char buf[8];

	buf[0] = 0x55;
	buf[1] = 0x55;
	buf[2] = 0x55;
	buf[3] = 0x55;
	buf[4] = 0x55;
	buf[5] = inBuf[5];
	buf[6] = 0x01; // Set Data command response
	buf[7] = 0x00; // 0x00 = success, 0xFF = player busy, 0xFE SPI write failed

	strcpy(filename, "unknown");

	switch ((unsigned char)inBuf[5])
	{
		case 0xCC:  strcpy(filename, "Falcon.FPD");
					len = 1024;
					break;
		case 0xCD:  strcpy(filename, "Falcon.F16V2-alpha");
					len = 1024;
					break;
	}

	FalconWriteConfig(filename, (char *)inBuf, len);

	if (player->SequencesRunning())
	{
		if (inBuf[7] == 0x01)
		{
			newPlaylist->StopNow(); // FIXME: Need to investigate this more
		}
		else
		{
			configureHardware = 0;
			buf[7] = 0xFF;
		}
	}

	if (configureHardware)
	{
		int configResult = FalconConfigureHardware(filename, 0);
		if (configResult != 0)
			buf[7] = 0xFE;
	}

	if ((logLevel & LOG_DEBUG) && (logMask & VB_SETTING))
		HexDump("Falcon Set Data result", buf, sizeof(buf));

	if(sendto(sock, buf, sizeof(buf), 0, (struct sockaddr*)srcAddr, sizeof(*srcAddr)) < 0)
	{
		LogDebug(VB_SETTING, "ERROR: sentto failed\n");
	}
}

/*
 *
 */
void FalconGetData(int sock, struct sockaddr_in *srcAddr, unsigned char *inBuf)
{
	LogDebug(VB_SETTING, "FalconGetData(%p)\n", inBuf);

	char buf[FALCON_CFG_FILE_MAX_SIZE];
	char filename[16];

	switch ((unsigned char)inBuf[5])
	{
		case 0xCC:  strcpy(filename, "Falcon.FPD");
					break;
		case 0xCD:  strcpy(filename, "Falcon.F16V2-alpha");
					break;
	}

	int bytes = FalconReadConfig(filename, buf);

	buf[6] = 0x02; // GetData command response

	if ((logLevel & LOG_DEBUG) && (logMask & VB_SETTING))
		HexDump("Falcon Get Data result", buf, 8);

	if(sendto(sock, buf, bytes, 0, (struct sockaddr*)srcAddr, sizeof(*srcAddr)) < 0)
	{
		LogDebug(VB_SETTING, "ERROR: sentto failed\n");
	}
}

/*
 *
 */
void FalconConfigurePi(int sock, struct sockaddr_in *srcAddr,
	struct in_addr recvAddr, unsigned char *inBuf)
{
	LogDebug(VB_SETTING, "FalconConfigurePi(%p)\n", inBuf);
	// Parse Network/IP info from received data and configure Pi

	char buf[53];
	bzero(buf, sizeof(buf));

	buf[0] = 0x55;
	buf[1] = 0x55;
	buf[2] = 0x55;
	buf[3] = 0x55;
	buf[4] = 0x55;
	buf[5] = inBuf[5];
	buf[6] = 0x03; // Configure Pi command response

	PopulatePiConfig(inet_ntoa(recvAddr), buf);

	if ((logLevel & LOG_DEBUG) && (logMask & VB_SETTING))
		HexDump("Falcon Configure Pi result", buf, sizeof(buf));

	if(sendto(sock, buf, sizeof(buf), 0, (struct sockaddr*)srcAddr, sizeof(*srcAddr)) < 0)
	{
		LogDebug(VB_SETTING, "ERROR: sentto failed\n");
	}
}

/*
 *
 */
void ProcessFalconPacket(int sock, struct sockaddr_in *srcAddr,
	struct in_addr recvAddr, unsigned char *inBuf)
{
	LogDebug(VB_SETTING, "ProcessFalconPacket(%p, %p)\n", srcAddr, inBuf);

	// Falcon hardware config packet
	if ((inBuf[0] == 0x55) &&
		(inBuf[1] == 0x55) &&
		(inBuf[2] == 0x55) &&
		(inBuf[3] == 0x55) &&
		(inBuf[4] == 0x55) &&
		((inBuf[5] == 0xCC) || // FPD
		 (inBuf[5] == 0xCD)))  // F16V2-alpha
	{
		char command = inBuf[6];
	
		switch (command) {
			case 0: // Query
					FalconQueryHardware(sock, srcAddr, recvAddr, inBuf);
					break;
			case 1: // Set Data
					FalconSetData(sock, srcAddr, inBuf);
					break;
			case 2: // Get Data
					FalconGetData(sock, srcAddr, inBuf);
					break;
			case 3: // Set Pi Info
					FalconConfigurePi(sock, srcAddr, recvAddr, inBuf);
					break;
		}
	}
	// Data pass-through packet to configure SSC or uSC
	else if ((inBuf[0] == 0xCC) &&
			 (inBuf[1] == 0xCC) &&
			 (inBuf[2] == 0xCC) &&
			 (inBuf[3] == 0xCC) &&
			 (inBuf[4] == 0xCC) &&
			 (inBuf[5] == 0x55))
	{
		int offset = inBuf[6] + inBuf[7]*256;
		FalconPassThroughData(offset, &inBuf[8], FALCON_PASSTHROUGH_DATA_SIZE);
	}
}

/*
 *
 */
int DetectFalconHardware(int configureHardware)
{
	int  spiPort = 0;
	char query[FALCON_CFG_BUF_SIZE];
	if (wiringPiSPISetup(0, 8000000) < 0)
	{
		LogErr(VB_CHANNELOUT, "Unable to set SPI speed to detect hardware\n");
		return 0;
	}

	int responseSize = FalconDetectHardware(spiPort, query);

	if ((logLevel & LOG_DEBUG) && (logMask & VB_SETTING))
		HexDump("Falcon Detect Hardware Response", query, 8);

	if ((responseSize == FALCON_CFG_BUF_SIZE) && (query[0] > 0)) 
	{
		int spiSpeed = 8000000;
		char model[32];
		char cfgFile[32];

		strcpy(model, "UNKNOWN");
		strcpy(cfgFile, "Falcon.FPD");

		switch ((unsigned char)query[0])
		{
			// 0x01-0x7F == Falcon Controllers
			case 0x01:	strcpy(model, "F16 v2.x");
						strcpy(cfgFile, "Falcon.F16V2-alpha");
						spiSpeed = 16000000;
						break;
			case 0x02:	strcpy(model, "FPD v1.x");
						strcpy(cfgFile, "Falcon.FPDV1");
						spiSpeed = 8000000;
						break;
			case 0x03:	strcpy(model, "FPD v2.x");
						strcpy(cfgFile, "Falcon.FPDV2");
						spiSpeed = 8000000;
						break;

			// 0x81-0xFF == Non-Falcon Controllers
			case 0x81:	strcpy(model, "Ron's Holiday Lights - FPGA Pi Pixel");
						strcpy(cfgFile, "");
						spiSpeed = 16000000;
						break;

		}

		LogInfo(VB_SETTING, "Falcon Hardware Detected on SPI port %d\n", spiPort);
		LogInfo(VB_SETTING, "    Model           : %s\n", model);
		LogInfo(VB_SETTING, "    Firmware Version: %d.%d\n", query[1], query[2]);

#if 0
		if (query[0] == 0x01) // F16 v2.x
		{
			LogInfo(VB_SETTING, "    Chip Temperature: %d\n", query[3]);
			LogInfo(VB_SETTING, "    Temperature #1  : %d\n", query[4]);
			LogInfo(VB_SETTING, "    Temperature #2  : %d\n", query[5]);
			LogInfo(VB_SETTING, "    Voltage #1      : %d\n", query[6]);
			LogInfo(VB_SETTING, "    Voltage #2      : %d\n", query[7]);
		}
#endif

		if (configureHardware)
		{
			LogInfo(VB_SETTING, "Setting SPI speed to %d for %s hardware.\n",
				 spiSpeed, model);

			if (wiringPiSPISetup(0, spiSpeed) < 0)
			{
				LogErr(VB_CHANNELOUT, "Unable to set SPI speed to %d for %s\n",
					 spiSpeed, model);
				return 0;
			}

			if (strlen(cfgFile))
				FalconConfigureHardware(cfgFile, 0);
		}

		return 1;
	}

	return 0;
}

