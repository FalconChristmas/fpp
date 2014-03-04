#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "../log.h"
#include "mediaoutput.h"
#include "mpg123.h"
#include "ogg123.h"
#include "omxplayer.h"

/////////////////////////////////////////////////////////////////////////////
MediaOutput     *mediaOutput = 0;
pthread_mutex_t  mediaOutputLock;


MediaOutputStatus mediaOutputStatus = {
	.status = MEDIAOUTPUTSTATUS_IDLE
	};

void MediaOutput_sigchld_handler(int signal)
{
    int status;
    pid_t p = waitpid(-1, &status, WNOHANG);

	pthread_mutex_lock(&mediaOutputLock);
	if (!mediaOutput) {
		pthread_mutex_unlock(&mediaOutputLock);
		return;
	}

	LogInfo(VB_MEDIAOUT,
		"MediaOutput_sigchld_handler(): pid: %d, waiting for %d\n",
		p, mediaOutput->childPID);

    if (p == mediaOutput->childPID)
    {
        mediaOutput->childPID =0;

		if (mediaOutput->childPipe[MEDIAOUTPUTPIPE_READ]) {
	        close(mediaOutput->childPipe[MEDIAOUTPUTPIPE_READ]);
			mediaOutput->childPipe[MEDIAOUTPUTPIPE_READ] = 0;
		}

		if (mediaOutput->childPipe[MEDIAOUTPUTPIPE_WRITE]) {
	        close(mediaOutput->childPipe[MEDIAOUTPUTPIPE_WRITE]);
			mediaOutput->childPipe[MEDIAOUTPUTPIPE_WRITE] = 0;
		}

		pthread_mutex_unlock(&mediaOutputLock);

        mediaOutputStatus.status = MEDIAOUTPUTSTATUS_IDLE;
        CloseSequenceFile();
		CloseMediaOutput();
        usleep(1000000);
    } else {
		pthread_mutex_unlock(&mediaOutputLock);
	}

}

/*
 *
 */
void InitMediaOutput(void)
{
	if (pthread_mutex_init(&mediaOutputLock, NULL) != 0) {
		LogDebug(VB_MEDIAOUT, "ERROR: Media Output mutex init failed!\n");
	}

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = MediaOutput_sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);
}

/*
 *
 */
void CleanupMediaOutput(void)
{
	CloseMediaOutput();

	pthread_mutex_destroy(&mediaOutputLock);
}

/*
 *
 */
int OpenMediaOutput(char *filename) {
	LogDebug(VB_MEDIAOUT, "OpenMediaOutput(%s)\n", filename);

	pthread_mutex_lock(&mediaOutputLock);
	if (!mediaOutput) {
		pthread_mutex_unlock(&mediaOutputLock);
		CloseMediaOutput();
	}
	pthread_mutex_unlock(&mediaOutputLock);

	pthread_mutex_lock(&mediaOutputLock);
	mediaOutput = NULL;

	if (mpg123Output.canHandle(filename)) {
		mediaOutput = &mpg123Output;
	} else if (ogg123Output.canHandle(filename)) {
		mediaOutput = &ogg123Output;
	} else if (omxplayerOutput.canHandle(filename)) {
		mediaOutput = &omxplayerOutput;
	} else {
		pthread_mutex_unlock(&mediaOutputLock);
		LogDebug(VB_MEDIAOUT, "ERROR: No Media Output handler for %s\n", filename);
		return 0;
	}

	if ((!mediaOutput) || (!mediaOutput->startPlaying(filename))) {
		mediaOutput = 0;
		pthread_mutex_unlock(&mediaOutputLock);
		return 0;
	}

	mediaOutput->filename = strdup(filename);
	mediaOutputStatus.status = MEDIAOUTPUTSTATUS_PLAYING;

	pthread_mutex_unlock(&mediaOutputLock);

	return 1;
}

void CloseMediaOutput(void) {
	LogDebug(VB_MEDIAOUT, "CloseMediaOutput()\n");

	mediaOutputStatus.status = MEDIAOUTPUTSTATUS_IDLE;

	pthread_mutex_lock(&mediaOutputLock);
	if (!mediaOutput) {
		pthread_mutex_unlock(&mediaOutputLock);
		return;
	}

	if (mediaOutput->childPID) {
		pthread_mutex_unlock(&mediaOutputLock);
		mediaOutput->stopPlaying();
		pthread_mutex_lock(&mediaOutputLock);
	}


	free(mediaOutput->filename);

	mediaOutput = 0;
	pthread_mutex_unlock(&mediaOutputLock);
}

