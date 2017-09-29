/*
 *   Falcon Pi Player control protocol header file  
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
 
#ifndef _CONTROL_H
#define _CONTROL_H

#include <sys/types.h>

#define FPP_CTRL_PORT 32320
#define FPP_CTRL_CSV_PORT 32321

#define CTRL_PKT_CMD    0
#define CTRL_PKT_SYNC   1
#define CTRL_PKT_EVENT  2
#define CTRL_PKT_BLANK  3
#define CTRL_PKT_DISCRQ 4 // Discovery Request
#define CTRL_PKT_DISCRP 5 // Discovery Reply

typedef struct __attribute__((packed)) {
	char     fppd[4];        // 'FPPD'
	uint8_t  pktType;        // Control Packet Type
	uint16_t extraDataLen;   // Length of extra data following Control Packet
} ControlPkt;

typedef struct {
	char   command[1];       // Null-terminated Command to process
	                         // (data may continue past this header)
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

#endif /* _CONTROL_H */
