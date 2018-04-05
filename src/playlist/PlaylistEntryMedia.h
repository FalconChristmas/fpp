/*
 *   Playlist Entry Media Class for Falcon Player (FPP)
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

#ifndef _PLAYLISTENTRYMEDIA_H
#define _PLAYLISTENTRYMEDIA_H

#include <string>

#include "PlaylistEntryBase.h"
#include "mediaoutput.h"

class PlaylistEntryMedia : public PlaylistEntryBase {
  public:
	PlaylistEntryMedia(PlaylistEntryBase *parent = NULL);
	~PlaylistEntryMedia();

	int  Init(Json::Value &config);

	int  StartPlaying(void);
	int  Process(void);
	int  Stop(void);

	int  HandleSigChild(pid_t pid);

	void Dump(void);

	Json::Value GetConfig(void);

	std::string GetMediaName(void) { return m_mediaFilename; }

	int   m_status;
	int   m_secondsElapsed;
	int   m_subSecondsElapsed;
	int   m_secondsRemaining;
	int   m_subSecondsRemaining;
	int   m_minutesTotal;
	int   m_secondsTotal;
	float m_mediaSeconds;
	int   m_speedDelta;

  private:
	int OpenMediaOutput(void);
	int CloseMediaOutput(void);

	std::string        m_mediaFilename;
	MediaOutputBase   *m_mediaOutput;
	pthread_mutex_t    m_mediaOutputLock;
};

#endif
