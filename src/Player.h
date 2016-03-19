/*
 *   Player class for the Falcon Player (FPP) 
 *
 *   Copyright (C) 2013 the Falcon Player Developers
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

#ifndef _PLAYER_H
#define _PLAYER_H

#include <string>
#include <vector>

#include <pthread.h>

#include "playlist/NewPlaylist.h"
#include "Scheduler.h"
#include "Sequence.h"

class Player {
  public:
	Player();
	~Player();

	void MainLoop(void);
	void Shutdown(void);

	int  PlaylistStart(std::string playlistName, int position = 0, int repeat = 0);
	int  PlaylistStopNow(void);
	int  PlaylistStopGracefully(int afterCurrentLoop = 0);

	void DisableChannelOutput(void);
	void EnableChannelOutput(void);

	void RunChannelOutputThread(void);
	int  ChannelOutputThreadIsRunning(void);
	void SetChannelOutputRefreshRate(int rate);
	int  StartChannelOutputThread(void);
	int  StopChannelOutputThread(void);
	void ResetMasterPosition(void);
	void UpdateMasterPosition(int frameNumber);
	void CalculateNewChannelOutputDelay(float mediaPosition);
	void CalculateNewChannelOutputDelayForFrame(int expectedFramesSent);
	void SendBlankingData(void);

	int  GetPlaybackSecondsElapsed(void);
	int  GetPlaybackSecondsRemaining(void);

	int  StartMedia(std::string mediaName, int newMediaOffset = 0);
	int  StopMedia(void);

	long long StartSequence(std::string seqName, int priority = 0, int startSeconds = 0,
			int startChannel = 0, int blockSize = -1, int autoRepeat = 0);
	int  StopSequence(std::string seqName);
	int  StopAllSequences(void);
	int  SeekSequence(std::string seqName, int frameNumber);
	int  SequencesRunning(void);
	int  SequenceIsRunning(long long id);
	int  ToggleSequencePause(void);
	int  SequencesArePaused(void)               { return m_sequencesPaused; }
	void SingleStepSequences(void);
	void SingleStepSequencesBack(void);

	void NextPlaylistItem(void);
	void PrevPlaylistItem(void);

	// Pass-through's to Playlist:: for now
	Json::Value GetCurrentPlaylistInfo(void);
	Json::Value GetCurrentPlaylistEntry(void);

	// Pass-through's to Scheduler:: for now
	void GetNextScheduleStartText(char *txt);
	void GetNextPlaylistText(char *txt);
	void ReLoadCurrentScheduleInfo(void);
	void ReLoadNextScheduleInfo(void);

	Json::Value GetCurrentPlaylist(void);

  private:
	void ProcessChannelData(void);
    char NormalizeControlValue(char in);

	pthread_mutex_t         m_sequenceLock;
	std::vector<Sequence *> m_sequence;
	NewPlaylist            *m_newPlaylist;

	Scheduler    *m_scheduler;
	int           m_runMainLoop;

	long long     m_nextSequenceID;

	int           m_refreshRate;
	int           m_defaultLightDelay;
	int           m_lightDelay;
	int           m_masterFramesPlayed;
	int           m_outputFrames;
	float         m_mediaOffset;
	int           m_sequencesPaused;

	pthread_t     m_outputThreadID;
	int           m_runOutputThread;
	int           m_outputThreadIsRunning;

	char          m_seqData[FPPD_MAX_CHANNELS];
	char          m_seqLastControlMajor;
	char          m_seqLastControlMinor;
};

extern Player *player;

#endif /* _PLAYER_H */

