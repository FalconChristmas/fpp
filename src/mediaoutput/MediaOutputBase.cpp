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

#include <vector>

#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "MediaOutputBase.h"
#include "common.h"
#include "log.h"

MediaOutputBase::MediaOutputBase(void)
  : m_isPlaying(0),
	m_childPID(0)
{
	LogDebug(VB_MEDIAOUT, "MediaOutputBase::MediaOutputBase()\n");

	pthread_mutex_init(&m_outputLock, NULL);

	m_childPipe[0] = 0;
	m_childPipe[1] = 0;
}

MediaOutputBase::~MediaOutputBase()
{
	LogDebug(VB_MEDIAOUT, "MediaOutputBase::~MediaOutputBase()\n");

	Close();

	pthread_mutex_destroy(&m_outputLock);
}

/*
 *
 */
int MediaOutputBase::Start(void)
{
	return 0;
}

/*
 *
 */
int MediaOutputBase::Stop(void)
{
	return 0;
}

/*
 *
 */
int MediaOutputBase::Process(void)
{
	return 0;
}

/*
 *
 */
int MediaOutputBase::Close(void)
{
	LogDebug(VB_MEDIAOUT, "MediaOutputBase::Close\n");

	pthread_mutex_lock(&m_outputLock);

	for (int i = 0; i < 2; i++)
	{
		if (m_childPipe[i])
		{
			close(m_childPipe[i]);
			m_childPipe[i] = 0;
		}
	}

	pthread_mutex_unlock(&m_outputLock);

	return 1;
}

/*
 *
 */
int MediaOutputBase::AdjustSpeed(float masterPos) {
	return 1;
}

/*
 *
 */
void MediaOutputBase::SetVolume(int volume) {
}

/*
 *
 */
int MediaOutputBase::IsPlaying(void)
{
	int result = 0;

	pthread_mutex_lock(&m_outputLock);

    if (m_childPID > 0) {
        result = 1;
    }

	pthread_mutex_unlock(&m_outputLock);

	return result;
}

bool MediaOutputBase::isChildRunning() {
    if (m_childPID > 0) {
        int status = 0;
        if (waitpid(m_childPID, &status, WNOHANG)) {
            return false;
        } else {
            return true;
        }
    }
    return false;
}

