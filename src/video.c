#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "settings.h"
#include "video.h"

pid_t videoPID = 0;

/*
 * Play a Video
 */
int PlayVideo(char *videoName, unsigned int videoDelay)
{
	pid_t pid = 0;
	char  fullVideoPath[1024];

	if (snprintf(fullVideoPath, 1024, "%s/%s", getVideoDirectory(), videoName)
		>= 1024)
	{
		LogWrite("Unable to play %s, full path name too long\n", videoName);
		return 0;
	}

	pid = fork();
	if (pid == 0) // Event Script process
	{
		sleep(videoDelay);
		execlp("/usr/bin/omxplayer", fullVideoPath, NULL);

		LogWrite("PlayVideo(), ERROR, we shouldn't be here, this means "
			"that execvl() failed\n");
		exit(EXIT_FAILURE);
	}

	videoPID = pid;

	return 1;
}

/*
 * Stop a running Video
 */
int StopVideo(void)
{
	if (videoPID == 0)
		return 0;

	kill(videoPID, SIGKILL);
	videoPID = 0;

	return 1;
}

