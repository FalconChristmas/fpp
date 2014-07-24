/*
 *   Media handler for Falcon Pi Player (FPP)
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

#ifndef _MEDIAOUTPUT_H
#define _MEDIAOUTPUT_H

#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>

typedef struct mediaOutput {
	char  *filename;
	pid_t  childPID;
	int   *childPipe;
	int  (*canHandle) (const char *filename);
	int  (*startPlaying) (const char *filename);
	void (*stopPlaying) (void);
	int  (*processData) (void);
	int  (*isPlaying) (void);
} MediaOutput;

typedef struct mediaOutputStatus {
	int status; 
	int secondsElapsed;
	int subSecondsElapsed;
	int secondsRemaining;
	int subSecondsRemaining;
	int minutesTotal;
	int secondsTotal;
} MediaOutputStatus; 

#define MEDIAOUTPUTPIPE_READ       0
#define MEDIAOUTPUTPIPE_WRITE      1

#define MEDIAOUTPUTSTATUS_IDLE     0
#define MEDIAOUTPUTSTATUS_PLAYING  1

extern MediaOutput       *mediaOutput;
extern pthread_mutex_t    mediaOutputLock;
extern MediaOutputStatus  mediaOutputStatus;

void InitMediaOutput(void);
void CleanupMediaOutput(void);
int  OpenMediaOutput(char *filename);
void CloseMediaOutput(void);

#endif /* _MEDIAOUTPUT_H */

