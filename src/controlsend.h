/*
 *   Falcon Pi Player control socket send header file  
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
 
#ifndef _CONTROLSEND_H
#define _CONTROLSEND_H

/* Functions for controlling other FPP instances */
int  InitSyncMaster(void);
void SendSeqSyncStartPacket(const char *filename);
void SendSeqSyncStopPacket(const char *filename);
void SendSeqSyncPacket(const char *filename, int frames, float seconds);
void ShutdownSync(void);

void SendMediaSyncStartPacket(const char *filename);
void SendMediaSyncStopPacket(const char *filename);
void SendMediaSyncPacket(const char *filename, int frames, float seconds);

void SendEventPacket(const char *eventID);
void SendBlankingDataPacket(void);

#endif /* _CONTROL_H */
