/*
 *   (Old) Playlist Class for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
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
#include "command.h"
#include "events.h"
#include "log.h"
#include "mediadetails.h"
#include "Playlist.h"
#include "Plugins.h"
#include "Scheduler.h"
#include "settings.h"
#include "Sequence.h"
#include "channeloutput/E131.h"
#include "mediaoutput/mediaoutput.h"

#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

extern PluginCallbackManager pluginCallbackManager;

OldPlaylist *oldPlaylist = NULL;

OldPlaylist::OldPlaylist()
  : m_playlistAction(PL_ACTION_NOOP),
	m_numberOfSecondsPaused(0),
	m_pauseStatus(PAUSE_STATUS_IDLE)
{
}

OldPlaylist::~OldPlaylist()
{
}

void OldPlaylist::IncrementPlayListEntry(void)
{
	int maxEntryIndex;
	int firstEntryIndex;
	int lastEntry;

	if (!m_playlistDetails.playlistStarting)
	{
		//calculate next
		if(FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY)
		{
			lastEntry = m_playlistDetails.last?m_playlistDetails.playListCount-1:PLAYLIST_STOP_INDEX;
			m_playlistDetails.currentPlaylistEntry = m_playlistDetails.currentPlaylistEntry == m_playlistDetails.playListCount-1 ? PLAYLIST_STOP_INDEX:lastEntry;
		}
		else
		{
			if (m_playlistDetails.repeat)
				maxEntryIndex = m_playlistDetails.last?m_playlistDetails.playListCount-1:m_playlistDetails.playListCount;
			else
				maxEntryIndex = m_playlistDetails.playListCount;

			m_playlistDetails.currentPlaylistEntry++;
			LogDebug(VB_PLAYLIST, "currentPlaylistEntry = %d Last=%d maxEntryIndex=%d repeat=%d \n",m_playlistDetails.currentPlaylistEntry, m_playlistDetails.last,maxEntryIndex,m_playlistDetails.repeat);
			if((m_playlistDetails.currentPlaylistEntry == maxEntryIndex) && !m_playlistDetails.repeat)
			{
				LogInfo(VB_PLAYLIST, "Stopping Gracefully\n");
				m_playlistDetails.currentPlaylistEntry = PLAYLIST_STOP_INDEX;
			}
			else if(m_playlistDetails.currentPlaylistEntry >= maxEntryIndex)
			{
				// Calculate where start index is.
				firstEntryIndex = m_playlistDetails.first?1:0;
				m_playlistDetails.currentPlaylistEntry = firstEntryIndex;
			}
		}
	}
}

void OldPlaylist::DecrementPlayListEntry(void)
{
	int maxEntryIndex;
	int firstEntryIndex;
	int lastEntry;

	if (!m_playlistDetails.playlistStarting)
	{
		firstEntryIndex = m_playlistDetails.first ? 1 : 0;
		maxEntryIndex   = m_playlistDetails.last ? m_playlistDetails.playListCount - 2 : m_playlistDetails.playListCount - 1;

		m_playlistDetails.currentPlaylistEntry--;

		LogDebug(VB_PLAYLIST, "currentPlaylistEntry = %d Last=%d maxEntryIndex=%d firstEntryIndex=%d repeat=%d \n",m_playlistDetails.currentPlaylistEntry, m_playlistDetails.last,maxEntryIndex,firstEntryIndex,m_playlistDetails.repeat);

		if (m_playlistDetails.currentPlaylistEntry < firstEntryIndex)
		{
			m_playlistDetails.currentPlaylistEntry = maxEntryIndex;
		}
	}
}

void OldPlaylist::CalculateNextPlayListEntry(void)
{
	IncrementPlayListEntry();

	while ( m_playlistDetails.playList[m_playlistDetails.currentPlaylistEntry].cType == 'P' )
	{
		if ( !m_playlistDetails.playlistStarting )
		{
			//callback & populate
			if ( pluginCallbackManager.nextPlaylistEntryCallback(&(m_playlistDetails.playList[m_playlistDetails.currentPlaylistEntry].data[0]),
										m_playlistDetails.currentPlaylistEntry,
										getFPPmode(),
										m_playlistDetails.repeat,
										&(m_playlistDetails.playList[m_playlistDetails.currentPlaylistEntry])) == -1)
			{
				if ( m_playlistDetails.playListCount == 1 )
				{
					m_playlistDetails.currentPlaylistEntry = PLAYLIST_STOP_INDEX;
					break;
				}
				IncrementPlayListEntry();
			}
			else
				break;
		}
		else
			break;
	}

	if(m_playlistDetails.playlistStarting)
	{
		// Do not change "playlistDetails.currentPlaylistEntry"
		m_playlistDetails.playlistStarting=0;
		LogInfo(VB_PLAYLIST, "Playlist Starting\n");
		return;
	}

}

int OldPlaylist::ParsePlaylistEntry(char *buf, OldPlaylistEntry *pe)
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

int OldPlaylist::ReadPlaylist(char const * file)
{
	FILE *fp;
	int listIndex=0;
	char buf[512];
	char *s;
	// Put together playlist file with default folder
	strncpy((char *)m_playlistDetails.currentPlaylist,
			(const char *)getPlaylistDirectory(),
			sizeof(m_playlistDetails.currentPlaylist));
	strncat((char*)m_playlistDetails.currentPlaylist, "/",
			sizeof(m_playlistDetails.currentPlaylist)
				- strlen(m_playlistDetails.currentPlaylist)-1);
	strncat((char*)m_playlistDetails.currentPlaylist, file,
			sizeof(m_playlistDetails.currentPlaylist)
				- strlen(m_playlistDetails.currentPlaylist)-1);

	LogInfo(VB_PLAYLIST, "Opening File Now %s\n",m_playlistDetails.currentPlaylist);
	fp = fopen((const char*)m_playlistDetails.currentPlaylist, "r");
	if (fp == NULL)
	{
		LogErr(VB_PLAYLIST, "Could not open playlist file %s\n",file);
		return 0;
	}
	// Parse Playlist settings (First, Last)
	fgets(buf, 512, fp);

	s=strtok(buf,",");
	if (s == NULL)
	{
		LogErr(VB_PLAYLIST, "File might be corrupt or empty, no data to parse from %s\n", file);
		return 0;
	}
	m_playlistDetails.first = atoi(s);
	s = strtok(NULL,",");
	if (s == NULL)
	{
		LogErr(VB_PLAYLIST, "File might be corrupt or empty, no data to parse from %s\n", file);
		return 0;
	}
	m_playlistDetails.last = atoi(s);

	// Parse Playlists
	while((fgets(buf, 512, fp) != NULL) && (listIndex < PL_MAX_ENTRIES))
	{
		LogExcess(VB_PLAYLIST, "Loading playlist entry %d/%d\n", listIndex + 1, PL_MAX_ENTRIES);
		if (!ParsePlaylistEntry(buf, &m_playlistDetails.playList[listIndex]))
			listIndex++;
		else
			break;
	}
	fclose(fp);

	if (listIndex == PL_MAX_ENTRIES)
		LogErr(VB_PLAYLIST, "Limit of max number of playlist entries (%d) reached.\n",
			   PL_MAX_ENTRIES);

	m_playlistDetails.playListCount = listIndex;
	pluginCallbackManager.playlistCallback(&m_playlistDetails, PLAYLIST_STARTING);

	return listIndex;
}

void OldPlaylist::PlayListPlayingInit(void)
{
	LogDebug(VB_PLAYLIST, "PlayListPlayingInit() playing %s %s.\n",
		m_playlistDetails.currentPlaylistFile,
		m_playlistDetails.repeat ? "repeating" : "once");

	m_playlistDetails.StopPlaylist = 0;
	m_playlistDetails.ForceStop = 0;
	m_playlistDetails.playListCount = ReadPlaylist(m_playlistDetails.currentPlaylistFile);

	if(m_playlistDetails.playListCount == 0)
	{
		LogInfo(VB_PLAYLIST, "PlaylistCount = 0. Exiting PlayListPlayingLoop\n");
		FPPstatus = FPP_STATUS_IDLE;
		return;
	}

	if(m_playlistDetails.currentPlaylistEntry < 0 || m_playlistDetails.currentPlaylistEntry >= m_playlistDetails.playListCount)
	{
		LogErr(VB_PLAYLIST, "currentPlaylistEntry is not valid\n");
		FPPstatus = FPP_STATUS_IDLE;
		return;
	}
}

void OldPlaylist::PlaylistProcessMediaData(void)
{
	sigset_t blockset;

	sigemptyset(&blockset);
	sigaddset(&blockset, SIGCHLD);
	sigprocmask(SIG_BLOCK, &blockset, NULL);

	pthread_mutex_lock(&mediaOutputLock);
	if (mediaOutput)
		mediaOutput->Process();
	pthread_mutex_unlock(&mediaOutputLock);

	sigprocmask(SIG_UNBLOCK, &blockset, NULL);
}

void OldPlaylist::PlayListPlayingProcess(void)
{
	bool calculateNext = true;

	while ( m_playlistDetails.playList[m_playlistDetails.currentPlaylistEntry].cType == 'P' )
	{
		if (m_playlistDetails.playlistStarting)
		{
			//callback & populate
			if ( pluginCallbackManager.nextPlaylistEntryCallback(&(m_playlistDetails.playList[m_playlistDetails.currentPlaylistEntry].data[0]),
										m_playlistDetails.currentPlaylistEntry,
										getFPPmode(),
										m_playlistDetails.repeat,
										&(m_playlistDetails.playList[m_playlistDetails.currentPlaylistEntry])) == -1)
			{
				if ( m_playlistDetails.playListCount == 1 )
				{
					m_playlistDetails.currentPlaylistEntry = PLAYLIST_STOP_INDEX;
					break;
				}
				m_playlistDetails.playlistStarting = 0;
				IncrementPlayListEntry();
			}
			else
				break;
		}
		else
			break;
	}

	int playlistEntryType = m_playlistDetails.playList[m_playlistDetails.currentPlaylistEntry].type;

	if ((FPPstatus == FPP_STATUS_PLAYLIST_PLAYING) &&
		(m_playlistAction != PL_ACTION_NOOP))
	{
		switch (m_playlistAction)
		{
			case PL_ACTION_NEXT_ITEM:
			case PL_ACTION_PREV_ITEM:
					if ((sequence->IsSequenceRunning()) &&
						((playlistEntryType == PL_TYPE_BOTH) ||
						 (playlistEntryType == PL_TYPE_SEQUENCE)))
					{
						sequence->CloseSequenceFile();
					}

					if ((mediaOutputStatus.status == MEDIAOUTPUTSTATUS_PLAYING) &&
						((playlistEntryType == PL_TYPE_BOTH) ||
						 (playlistEntryType == PL_TYPE_MEDIA)))
					{
						CloseMediaOutput();
					}

					if (playlistEntryType == PL_TYPE_PAUSE)
					{
						m_pauseStatus = PAUSE_STATUS_ENDED;
					}

					if (m_playlistDetails.playlistStarting)
						m_playlistDetails.playlistStarting = 0;

					if (m_playlistAction == PL_ACTION_NEXT_ITEM)
					{
						IncrementPlayListEntry();
					}
					else
					{
						DecrementPlayListEntry();
					}
					calculateNext = false;

					break;
		}
		m_playlistAction = PL_ACTION_NOOP;
	}

	switch(m_playlistDetails.playList[m_playlistDetails.currentPlaylistEntry].type)
	{
		case PL_TYPE_BOTH:
			// TODO: Do we need to lock around this check?
			if(mediaOutputStatus.status == MEDIAOUTPUTSTATUS_IDLE)
			{
				LogInfo(VB_PLAYLIST, "Play File Now \n");
				PlayPlaylistEntry(calculateNext);
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
				PlayPlaylistEntry(calculateNext);
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
			if(!sequence->IsSequenceRunning())
			{
				PlayPlaylistEntry(calculateNext);
			}
			break;
		case PL_TYPE_PAUSE:
			PauseProcess();
			if(m_pauseStatus==PAUSE_STATUS_ENDED)
			{
				PlayPlaylistEntry(calculateNext);
				m_pauseStatus = PAUSE_STATUS_IDLE;
			}
			break;
		case PL_TYPE_VIDEO:
			PlayPlaylistEntry(calculateNext);
			break;
		case PL_TYPE_EVENT:
			PlayPlaylistEntry(calculateNext);
			break;
		default:
			break;
	}

	if (m_playlistDetails.StopPlaylist)
	{
		FPPstatus = FPP_STATUS_IDLE;
	}
}

void OldPlaylist::PlayListPlayingCleanup(void)
{
	LogDebug(VB_PLAYLIST, "PlayListPlayingCleanup()\n");

	pluginCallbackManager.playlistCallback(&m_playlistDetails, PLAYLIST_STOPPING);

	FPPstatus = FPP_STATUS_IDLE;
	sequence->SendBlankingData();
	scheduler->ReLoadCurrentScheduleInfo();

	if(!m_playlistDetails.ForceStop)
	{
		scheduler->CheckIfShouldBePlayingNow();
	}
}

void OldPlaylist::PauseProcess(void)
{
	switch(m_pauseStatus)
	{
		case PAUSE_STATUS_IDLE:
			m_pauseStatus = PAUSE_STATUS_STARTED;
			gettimeofday(&m_pauseStartTime,NULL);
			break;
		case PAUSE_STATUS_STARTED:
			gettimeofday(&m_nowTime,NULL);
			m_numberOfSecondsPaused = m_nowTime.tv_sec - m_pauseStartTime.tv_sec;
			if ( m_numberOfSecondsPaused
				> (int)m_playlistDetails.playList[m_playlistDetails.currentPlaylistEntry].pauselength)
			{
				m_pauseStatus = PAUSE_STATUS_ENDED;
			}
			break;
		default:
			break;
	}
}


void OldPlaylist::PlayPlaylistEntry(bool calculateNext)
{
	OldPlaylistEntry *plEntry = NULL;

	if (calculateNext)
		CalculateNextPlayListEntry();

	if( m_playlistDetails.currentPlaylistEntry==PLAYLIST_STOP_INDEX)
	{
		LogInfo(VB_PLAYLIST, "Stopping Playlist\n");
		m_playlistDetails.StopPlaylist = 1;
		return;
	}

	LogDebug(VB_PLAYLIST, "playListCount=%d CurrentPlaylistEntry = %d\n", m_playlistDetails.playListCount,m_playlistDetails.currentPlaylistEntry);

	plEntry = &m_playlistDetails.playList[m_playlistDetails.currentPlaylistEntry];

	ParseMedia(NULL);
	pluginCallbackManager.mediaCallback();

	switch(plEntry->type)
	{
		case PL_TYPE_BOTH:
			if (sequence->OpenSequenceFile(plEntry->seqName, 0) > 0)
			{
				OpenMediaOutput(m_playlistDetails.playList[m_playlistDetails.currentPlaylistEntry].songName);
			}
			else
			{
				CalculateNextPlayListEntry();
			}
			break;
		case PL_TYPE_MEDIA:
			OpenMediaOutput(m_playlistDetails.playList[m_playlistDetails.currentPlaylistEntry].songName);
			break;
		case PL_TYPE_SEQUENCE:
			sequence->OpenSequenceFile(plEntry->seqName, 0);
			break;
		case PL_TYPE_PAUSE:
			break;
		case PL_TYPE_EVENT:
			pluginCallbackManager.eventCallback(plEntry->eventID, "playlist");
			TriggerEventByID(plEntry->eventID);
			break;
		default:
			LogErr(VB_PLAYLIST, "Should never be here!\n");
			break;
	}
}

void OldPlaylist::PlaylistPrint(void)
{
	int i=0;
	LogDebug(VB_PLAYLIST, "playListCount=%d\n",m_playlistDetails.playListCount);
	for(i=0;i<m_playlistDetails.playListCount;i++)
	{
		LogDebug(VB_PLAYLIST, "type=%d\n",m_playlistDetails.playList[i].type);
		LogDebug(VB_PLAYLIST, "seqName=%s\n",m_playlistDetails.playList[i].seqName);
		LogDebug(VB_PLAYLIST, "SongName=%s\n",m_playlistDetails.playList[i].songName);
		LogDebug(VB_PLAYLIST, "pauselength=%d\n",m_playlistDetails.playList[i].pauselength);
	}
}

void OldPlaylist::StopPlaylistGracefully(void)
{
	FPPstatus = FPP_STATUS_STOPPING_GRACEFULLY;
}

void OldPlaylist::StopPlaylistNow(void)
{
	LogInfo(VB_PLAYLIST, "StopPlaylistNow()\n");
	FPPstatus = FPP_STATUS_IDLE;
	sequence->CloseSequenceFile();
	CloseMediaOutput();
	m_playlistDetails.StopPlaylist = 1;
}

