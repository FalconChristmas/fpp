/*
 *   MediaOutputBase class for Falcon Player (FPP)
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

#ifndef _MEDIAOUTPUTBASE_H
#define _MEDIAOUTPUTBASE_H

#include <string>
#include <vector>

#include <pthread.h>
#include <sys/select.h>
#include <unistd.h>

#include <jsoncpp/json/json.h>

#include "MediaOutputStatus.h"

#define MEDIAOUTPUTPIPE_READ       0
#define MEDIAOUTPUTPIPE_WRITE      1

#define MEDIAOUTPUTSTATUS_IDLE     0
#define MEDIAOUTPUTSTATUS_PLAYING  1

#define MediaOutputSpeedDecrease -1
#define MediaOutputSpeedNormal    0
#define MediaOutputSpeedIncrease +1

class MediaOutputBase {
  public:
	MediaOutputBase();
	virtual ~MediaOutputBase();

	virtual int   Start(void);
	virtual int   Stop(void);
	virtual int   Process(void);
	virtual int   AdjustSpeed(float masterPos);
	virtual void  SetVolume(int volume);
	virtual int   Close(void);

	virtual int   IsPlaying(void);

	std::string        m_mediaFilename;
	MediaOutputStatus *m_mediaOutputStatus;
	pid_t              m_childPID;

  protected:
    bool isChildRunning();
    
	unsigned int       m_isPlaying;
	int                m_childPipe[2];
	fd_set             m_activeFDSet;
	fd_set             m_readFDSet;

	pthread_mutex_t    m_outputLock;
};

#endif /* #ifndef _MEDIAOUTPUTBASE_H */
