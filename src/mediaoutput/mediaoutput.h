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

