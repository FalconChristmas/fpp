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
#include "mediaoutput/mediaoutput.h"

class PlaylistEntryMedia : public PlaylistEntryBase {
  public:
	PlaylistEntryMedia(PlaylistEntryBase *parent = NULL);
	virtual ~PlaylistEntryMedia();

	virtual int  Init(Json::Value &config) override;

    virtual int  PreparePlay();
	virtual int  StartPlaying(void) override;
	virtual int  Process(void) override;
	virtual int  Stop(void) override;

	virtual int  HandleSigChild(pid_t pid) override;

	virtual void Dump(void) override;

	virtual Json::Value GetConfig(void) override;
	virtual Json::Value GetMqttStatus(void) override;

	std::string GetMediaName(void) { return m_mediaFilename; }

	int   m_status;
	int   m_secondsElapsed;
	int   m_subSecondsElapsed;
	int   m_secondsRemaining;
	int   m_subSecondsRemaining;
	int   m_minutesTotal;
	int   m_secondsTotal;
	float m_mediaSeconds;

    long long m_openTime;
    static int m_openStartDelay;

  private:
	int OpenMediaOutput(void);
	int CloseMediaOutput(void);

    unsigned int  m_fileSeed;
    int           GetFileList(void);
    std::string   GetNextRandomFile(void);

	std::string        m_mediaFilename;
    std::string        m_videoOutput;
	MediaOutputBase   *m_mediaOutput;
	pthread_mutex_t    m_mediaOutputLock;

    std::string              m_fileMode;
    std::vector<std::string> m_files;
};

#endif
