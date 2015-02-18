/*
 *   Play list handler for Falcon Pi Player (FPP)
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

#include "fpp.h"
#include "E131.h"
#include "command.h"
#include "events.h"
#include "log.h"
#include "mediadetails.h"
#include "mediaoutput.h"
#include "playList.h"
#include "plugins.h"
#include "schedule.h"
#include "settings.h"
#include "sequence.h"

#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

PlaylistDetails playlistDetails;

struct timeval pauseStartTime;
int numberOfSecondsPaused;
struct timeval nowTime;
int pauseStatus = PAUSE_STATUS_IDLE;
int playlistAction = PL_ACTION_NOOP;

extern char logText[256];

void IncrimentPlayListEntry()
{
	int maxEntryIndex;
	int firstEntryIndex;
	int lastEntry;

	if (!playlistDetails.playlistStarting)
	{
		//calculate next
		if(FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY)
		{
			lastEntry = playlistDetails.last?playlistDetails.playListCount-1:PLAYLIST_STOP_INDEX;
			playlistDetails.currentPlaylistEntry = playlistDetails.currentPlaylistEntry == playlistDetails.playListCount-1 ? PLAYLIST_STOP_INDEX:lastEntry;
		}
		else
		{
			maxEntryIndex = playlistDetails.last?playlistDetails.playListCount-1:playlistDetails.playListCount;
			playlistDetails.currentPlaylistEntry++;
			LogDebug(VB_PLAYLIST, "currentPlaylistEntry = %d Last=%d maxEntryIndex=%d repeat=%d \n",playlistDetails.currentPlaylistEntry, playlistDetails.last,maxEntryIndex,playlistDetails.repeat);
			if((playlistDetails.currentPlaylistEntry == maxEntryIndex) && !playlistDetails.repeat)
			{
				LogInfo(VB_PLAYLIST, "Stopping Gracefully\n");
				playlistDetails.currentPlaylistEntry = PLAYLIST_STOP_INDEX;
			}
			else if(playlistDetails.currentPlaylistEntry >= maxEntryIndex)
			{
				// Calculate where start index is.
				firstEntryIndex = playlistDetails.first?1:0;
				playlistDetails.currentPlaylistEntry = firstEntryIndex;
			}
		}
	}
}

void DecrementPlayListEntry()
{
	int maxEntryIndex;
	int firstEntryIndex;
	int lastEntry;

	if (!playlistDetails.playlistStarting)
	{
		firstEntryIndex = playlistDetails.first ? 1 : 0;
		maxEntryIndex   = playlistDetails.last ? playlistDetails.playListCount - 2 : playlistDetails.playListCount - 1;

		playlistDetails.currentPlaylistEntry--;

		LogDebug(VB_PLAYLIST, "currentPlaylistEntry = %d Last=%d maxEntryIndex=%d firstEntryIndex=%d repeat=%d \n",playlistDetails.currentPlaylistEntry, playlistDetails.last,maxEntryIndex,firstEntryIndex,playlistDetails.repeat);

		if (playlistDetails.currentPlaylistEntry < firstEntryIndex)
		{
			playlistDetails.currentPlaylistEntry = maxEntryIndex;
		}
	}
}

void CalculateNextPlayListEntry()
{
	IncrimentPlayListEntry();

	while ( playlistDetails.playList[playlistDetails.currentPlaylistEntry].cType == 'P' )
	{
		if ( !playlistDetails.playlistStarting )
		{
			//callback & populate
			if ( NextPlaylistEntryCallback(&(playlistDetails.playList[playlistDetails.currentPlaylistEntry].data[0]),
										playlistDetails.currentPlaylistEntry,
										getFPPmode(),
										playlistDetails.repeat,
										&(playlistDetails.playList[playlistDetails.currentPlaylistEntry])) == -1)
			{
				if ( playlistDetails.playListCount == 1 )
				{
					playlistDetails.currentPlaylistEntry = PLAYLIST_STOP_INDEX;
					break;
				}
				IncrimentPlayListEntry();
			}
			else
				break;
		}
		else
			break;
	}

	if(playlistDetails.playlistStarting)
	{
		// Do not change "playlistDetails.currentPlaylistEntry"
		playlistDetails.playlistStarting=0;
		LogInfo(VB_PLAYLIST, "Playlist Starting\n");
		return;
	}

}

int ParsePlaylistEntry(char *buf, PlaylistEntry *pe)
{
	char *s;
	s=strtok(buf,",");
	pe->cType = s[0];
	switch(s[0])
	{
		case 'b':
			s = strtok(NULL,",");
			strncpy(pe->seqName,s,sizeof(pe->seqName));
			s = strtok(NULL,",");
			strncpy(pe->songName,s,sizeof(pe->songName));
			pe->type = PL_TYPE_BOTH;
			break;
		case 's':
			s = strtok(NULL,",");
			strncpy(pe->seqName,s,sizeof(pe->seqName));
			pe->type = PL_TYPE_SEQUENCE;
			break;
		case 'm':
		case 'v':
			s = strtok(NULL,",");
			strncpy(pe->songName,s,sizeof(pe->songName));
			pe->type = PL_TYPE_MEDIA;
			break;
		case 'p':
			s = strtok(NULL,",");
			pe->pauselength = atoi(s);
			pe->type = PL_TYPE_PAUSE;
			break;
		case 'e':
			s = strtok(NULL,",");
			strncpy(pe->eventID,s,sizeof(pe->eventID));
			s = strtok(NULL,",");
			pe->pauselength = atoi(s);
			pe->type = PL_TYPE_EVENT;
			break;
		case 'P':
			s = strtok(NULL,",");
			strncpy(pe->data,s,sizeof(pe->data));
			// This means the user entered no data for the plugin and strtok
			// skipped the next comma and gave us the newline... don't care!
			if ( s[0] == '\n' )
				bzero(&pe->data[0], sizeof(pe->data));
			pe->type=PL_TYPE_PLUGIN_NEXT;
			break;
		default:
			LogErr(VB_PLAYLIST, "Invalid entry in sequence file!\n");
			return -1;
			break;
	}
	return 0;
}

int ReadPlaylist(char const * file)
{
	FILE *fp;
	int listIndex=0;
	char buf[512];
	char *s;
	// Put together playlist file with default folder
	strncpy((char *)playlistDetails.currentPlaylist,
			(const char *)getPlaylistDirectory(),
			sizeof(playlistDetails.currentPlaylist));
	strncat((char*)playlistDetails.currentPlaylist, "/",
			sizeof(playlistDetails.currentPlaylist)
				- strlen(playlistDetails.currentPlaylist)-1);
	strncat((char*)playlistDetails.currentPlaylist, file,
			sizeof(playlistDetails.currentPlaylist)
				- strlen(playlistDetails.currentPlaylist)-1);

	LogInfo(VB_PLAYLIST, "Opening File Now %s\n",playlistDetails.currentPlaylist);
	fp = fopen((const char*)playlistDetails.currentPlaylist, "r");
	if (fp == NULL)
	{
		LogErr(VB_PLAYLIST, "Could not open playlist file %s\n",file);
		return 0;
	}
	// Parse Playlist settings (First, Last)
	fgets(buf, 512, fp);
	s=strtok(buf,",");
	playlistDetails.first = atoi(s);
	s = strtok(NULL,",");
	playlistDetails.last = atoi(s);

	// Parse Playlists
	while((fgets(buf, 512, fp) != NULL) && (listIndex < PL_MAX_ENTRIES))
	{
		LogExcess(VB_PLAYLIST, "Loading playlist entry %d/%d\n", listIndex + 1, PL_MAX_ENTRIES);
		if (!ParsePlaylistEntry(buf, &playlistDetails.playList[listIndex]))
			listIndex++;
		else
			break;
	}
	fclose(fp);

	if (listIndex == PL_MAX_ENTRIES)
		LogErr(VB_PLAYLIST, "Limit of max number of playlist entries (%d) reached.\n",
			   PL_MAX_ENTRIES);

	playlistDetails.playListCount = listIndex;
	PlaylistCallback(&playlistDetails, PLAYLIST_STARTING);

	return listIndex;
}

void PlayListPlayingInit(void)
{
	LogDebug(VB_PLAYLIST, "PlayListPlayingInit() playing %s %s.\n",
		playlistDetails.currentPlaylistFile,
		playlistDetails.repeat ? "repeating" : "once");

	playlistDetails.StopPlaylist = 0;
	playlistDetails.ForceStop = 0;
	playlistDetails.playListCount = ReadPlaylist(playlistDetails.currentPlaylistFile);

	if(playlistDetails.playListCount == 0)
	{
		LogInfo(VB_PLAYLIST, "PlaylistCount = 0. Exiting PlayListPlayingLoop\n");
		FPPstatus = FPP_STATUS_IDLE;
		return;
	}

	if(playlistDetails.currentPlaylistEntry < 0 || playlistDetails.currentPlaylistEntry >= playlistDetails.playListCount)
	{
		LogErr(VB_PLAYLIST, "currentPlaylistEntry is not valid\n");
		FPPstatus = FPP_STATUS_IDLE;
		return;
	}
}

void PlaylistProcessMediaData(void)
{
	sigset_t blockset;

	sigemptyset(&blockset);
	sigaddset(&blockset, SIGCHLD);
	sigprocmask(SIG_BLOCK, &blockset, NULL);

	pthread_mutex_lock(&mediaOutputLock);
	if (mediaOutput)
		mediaOutput->processData();
	pthread_mutex_unlock(&mediaOutputLock);

	sigprocmask(SIG_UNBLOCK, &blockset, NULL);
}

void PlayListPlayingProcess(void)
{
	bool calculateNext = true;

	while ( playlistDetails.playList[playlistDetails.currentPlaylistEntry].cType == 'P' )
	{
		if (playlistDetails.playlistStarting)
		{
			//callback & populate
			if ( NextPlaylistEntryCallback(&(playlistDetails.playList[playlistDetails.currentPlaylistEntry].data[0]),
										playlistDetails.currentPlaylistEntry,
										getFPPmode(),
										playlistDetails.repeat,
										&(playlistDetails.playList[playlistDetails.currentPlaylistEntry])) == -1)
			{
				if ( playlistDetails.playListCount == 1 )
				{
					playlistDetails.currentPlaylistEntry = PLAYLIST_STOP_INDEX;
					break;
				}
				playlistDetails.playlistStarting = 0;
				IncrimentPlayListEntry();
			}
			else
				break;
		}
		else
			break;
	}

	int playlistEntryType = playlistDetails.playList[playlistDetails.currentPlaylistEntry].type;

	if ((FPPstatus == FPP_STATUS_PLAYLIST_PLAYING) &&
		(playlistAction != PL_ACTION_NOOP))
	{
		switch (playlistAction)
		{
			case PL_ACTION_NEXT_ITEM:
			case PL_ACTION_PREV_ITEM:
					if ((IsSequenceRunning()) &&
						((playlistEntryType == PL_TYPE_BOTH) ||
						 (playlistEntryType == PL_TYPE_SEQUENCE)))
					{
						CloseSequenceFile();
					}

					if ((mediaOutputStatus.status == MEDIAOUTPUTSTATUS_PLAYING) &&
						((playlistEntryType == PL_TYPE_BOTH) ||
						 (playlistEntryType == PL_TYPE_MEDIA)))
					{
						CloseMediaOutput();
					}

					if (playlistEntryType == PL_TYPE_PAUSE)
					{
						pauseStatus = PAUSE_STATUS_ENDED;
					}

					if (playlistDetails.playlistStarting)
						playlistDetails.playlistStarting = 0;

					if (playlistAction == PL_ACTION_NEXT_ITEM)
					{
						IncrimentPlayListEntry();
					}
					else
					{
						DecrementPlayListEntry();
					}
					calculateNext = false;

					break;
		}
		playlistAction = PL_ACTION_NOOP;
	}

	switch(playlistDetails.playList[playlistDetails.currentPlaylistEntry].type)
	{
		case PL_TYPE_BOTH:
			// TODO: Do we need to lock around this check?
			if(mediaOutputStatus.status == MEDIAOUTPUTSTATUS_IDLE)
			{
				LogInfo(VB_PLAYLIST, "Play File Now \n");
				Play_PlaylistEntry(calculateNext);
			}
			else
			{
				if(mediaOutputStatus.status == MEDIAOUTPUTSTATUS_PLAYING)
				{
					PlaylistProcessMediaData();
				}
			}
			break;
		case PL_TYPE_MEDIA:
			if(mediaOutputStatus.status == MEDIAOUTPUTSTATUS_IDLE)
			{
				Play_PlaylistEntry(calculateNext);
			}
			else
			{
				if(mediaOutputStatus.status == MEDIAOUTPUTSTATUS_PLAYING)
				{
					PlaylistProcessMediaData();
				}
			}
			break;
		case PL_TYPE_SEQUENCE:
			if(!IsSequenceRunning())
			{
				Play_PlaylistEntry(calculateNext);
			}
			break;
		case PL_TYPE_PAUSE:
			PauseProcess();
			if(pauseStatus==PAUSE_STATUS_ENDED)
			{
				Play_PlaylistEntry(calculateNext);
				pauseStatus = PAUSE_STATUS_IDLE;
			}
			break;
		case PL_TYPE_VIDEO:
			Play_PlaylistEntry(calculateNext);
			break;
		case PL_TYPE_EVENT:
			Play_PlaylistEntry(calculateNext);
			break;
		default:
			break;
	}

	if (playlistDetails.StopPlaylist)
	{
		FPPstatus = FPP_STATUS_IDLE;
	}
}

void PlayListPlayingCleanup(void)
{
	LogDebug(VB_PLAYLIST, "PlayListPlayingCleanup()\n");

	PlaylistCallback(&playlistDetails, PLAYLIST_STOPPING);

	FPPstatus = FPP_STATUS_IDLE;
	SendBlankingData();
	ReLoadCurrentScheduleInfo();

	if(!playlistDetails.ForceStop)
	{
		CheckIfShouldBePlayingNow();
	}
}

void PauseProcess(void)
{
	switch(pauseStatus)
	{
		case PAUSE_STATUS_IDLE:
			pauseStatus = PAUSE_STATUS_STARTED;
			gettimeofday(&pauseStartTime,NULL);
			break;
		case PAUSE_STATUS_STARTED:
			gettimeofday(&nowTime,NULL);
			numberOfSecondsPaused = nowTime.tv_sec - pauseStartTime.tv_sec;
			if ( numberOfSecondsPaused
				> (int)playlistDetails.playList[playlistDetails.currentPlaylistEntry].pauselength)
			{
				pauseStatus = PAUSE_STATUS_ENDED;
			}
			break;
		default:
			break;
	}
}


void Play_PlaylistEntry(bool calculateNext)
{
	PlaylistEntry *plEntry = NULL;

	if (calculateNext)
		CalculateNextPlayListEntry();

	if( playlistDetails.currentPlaylistEntry==PLAYLIST_STOP_INDEX)
	{
		LogInfo(VB_PLAYLIST, "Stopping Playlist\n");
		playlistDetails.StopPlaylist = 1;
		return;
	}

	LogDebug(VB_PLAYLIST, "playListCount=%d CurrentPlaylistEntry = %d\n", playlistDetails.playListCount,playlistDetails.currentPlaylistEntry);

	plEntry = &playlistDetails.playList[playlistDetails.currentPlaylistEntry];

	ParseMedia();
	MediaCallback();

	switch(plEntry->type)
	{
		case PL_TYPE_BOTH:
			if (OpenSequenceFile(plEntry->seqName, 0) > 0)
			{
				OpenMediaOutput(playlistDetails.playList[playlistDetails.currentPlaylistEntry].songName);
			}
			else
			{
				CalculateNextPlayListEntry();
			}
			break;
		case PL_TYPE_MEDIA:
			OpenMediaOutput(playlistDetails.playList[playlistDetails.currentPlaylistEntry].songName);
			break;
		case PL_TYPE_SEQUENCE:
			OpenSequenceFile(plEntry->seqName, 0);
			break;
		case PL_TYPE_PAUSE:
			break;
		case PL_TYPE_EVENT:
			EventCallback(plEntry->eventID, "playlist");
			TriggerEventByID(plEntry->eventID);
			break;
		default:
			LogErr(VB_PLAYLIST, "Should never be here!\n");
			break;
	}
}

void PlaylistPrint()
{
	int i=0;
	LogDebug(VB_PLAYLIST, "playListCount=%d\n",playlistDetails.playListCount);
	for(i=0;i<playlistDetails.playListCount;i++)
	{
		LogDebug(VB_PLAYLIST, "type=%d\n",playlistDetails.playList[i].type);
		LogDebug(VB_PLAYLIST, "seqName=%s\n",playlistDetails.playList[i].seqName);
		LogDebug(VB_PLAYLIST, "SongName=%s\n",playlistDetails.playList[i].songName);
		LogDebug(VB_PLAYLIST, "pauselength=%d\n",playlistDetails.playList[i].pauselength);
	}
}

void StopPlaylistGracefully(void)
{
	FPPstatus = FPP_STATUS_STOPPING_GRACEFULLY;
}

void StopPlaylistNow(void)
{
	LogInfo(VB_PLAYLIST, "StopPlaylistNow()\n");
	FPPstatus = FPP_STATUS_IDLE;
	CloseSequenceFile();
	CloseMediaOutput();
	playlistDetails.StopPlaylist = 1;
}

void JumpToPlaylistEntry(int entryIndex)
{
}
