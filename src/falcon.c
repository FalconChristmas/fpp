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
#include <unistd.h>

/*
 *
 */
int FalconReadConfig(char *filename)
{
}

/*
 *
 */
int FalconWriteConfig(char *filename, char *inBuf)
{
}

/*
 *
 */
int FalconConfigureHardware(char *filename, int spiPort)
{
}

/*
 *
 */
void FalconQueryHardware(char *inBuf)
{
	// Return config information, Falcon hardware info, network IP info, etc.
}

/*
 *
 */
void FalconSetData(char *inBuf)
{
	// Save the recieved packet as /home/pi/media/config/falcon.f16v2, etc..
	// Call Falcon config routine to load the config file and write out to the SPI port
}

/*
 *
 */
void FalconGetData(char *inBuf)
{
	// Read the config data from the file using generic falcon read routine and return data to caller
}

/*
 *
 */
void FalconConfigurePi(char *inBuf)
{
	// Parse Network/IP info from received data and configure Pi
}

/*
 *
 */
void ProcessFalconPacket(struct sockaddr_in *srcAddr, char *inBuf)
{
	if ((inBuf[0] != 0x55) ||
		(inBuf[1] != 0x55) ||
		(inBuf[2] != 0x55) ||
		(inBuf[3] != 0x55) ||
		(inBuf[4] != 0x55) ||
		(inBuf[5] != 0xCD))
		return;

	int port = ntohs(srcAddr->sin_port);

	// We have a Falcon config packet
	char command = inBuf[6];

	switch (command) {
		case 0: // Query
				FalconQueryHardware(inBuf);
				break;
		case 1: // Set Data
				FalconSetData(inBuf);
				break;
		case 2: // Get Data
				FalconGetData(inBuf);
				break;
		case 3: // Set Pi Info
				FalconConfigurePi(inBuf);
				break;
	}
}

