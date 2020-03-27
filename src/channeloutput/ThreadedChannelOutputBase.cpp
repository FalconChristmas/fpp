/*
 *   ChannelOutputBase class for Falcon Player (FPP)
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

#include "ThreadedChannelOutputBase.h"
#include "common.h"
#include "log.h"

ThreadedChannelOutputBase::ThreadedChannelOutputBase(unsigned int startChannel,
	 unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_threadIsRunning(0),
	m_runThread(0),
	m_dataWaiting(0),
	m_useDoubleBuffer(0),
	m_threadID(0),
    m_maxWait(0),
	m_inBuf(NULL),
	m_outBuf(NULL)
{
	pthread_mutex_init(&m_bufLock, NULL);
	pthread_mutex_init(&m_sendLock, NULL);
	pthread_cond_init(&m_sendCond, NULL);
}

ThreadedChannelOutputBase::~ThreadedChannelOutputBase()
{
    pthread_cond_destroy(&m_sendCond);
	pthread_mutex_destroy(&m_bufLock);
	pthread_mutex_destroy(&m_sendLock);
}

int ThreadedChannelOutputBase::Init(void)
{
	LogDebug(VB_CHANNELOUT, "ThreadedChannelOutputBase::Init()\n");

	if (m_useDoubleBuffer){
	 	m_inBuf = new unsigned char[m_channelCount];
		m_outBuf = new unsigned char[m_channelCount];
	}
    StartOutputThread();
	DumpConfig();

	return 1;
}

int ThreadedChannelOutputBase::Init(Json::Value config)
{
	return ChannelOutputBase::Init(config) && Init();
}

int ThreadedChannelOutputBase::Close(void)
{
	LogDebug(VB_CHANNELOUT, "ThreadedChannelOutputBase::Close()\n");

    StopOutputThread();

	if (m_useDoubleBuffer) {
        delete [] m_inBuf;
		delete [] m_outBuf;
	}

	return ChannelOutputBase::Close();
}


int ThreadedChannelOutputBase::SendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "ThreadedChannelOutputBase::SendData(%p)\n", channelData);

    if (m_useDoubleBuffer) {
        pthread_mutex_lock(&m_bufLock);
        memcpy(m_inBuf, channelData, m_channelCount);
        m_dataWaiting = 1;
        pthread_mutex_unlock(&m_bufLock);
    } else {
        m_outBuf = channelData;
        m_dataWaiting = 1;
    }
    
    pthread_cond_signal(&m_sendCond);
    return 0;
}

int ThreadedChannelOutputBase::SendOutputBuffer(void)
{
	LogExcess(VB_CHANNELOUT, "ChannelOutputBase::SendOutputBuffer()\n");

	if (m_useDoubleBuffer) {
		pthread_mutex_lock(&m_bufLock);
		memcpy(m_outBuf, m_inBuf, m_channelCount);
		m_dataWaiting = 0;
		pthread_mutex_unlock(&m_bufLock);
	} else {
		m_dataWaiting = 0;
	}

	RawSendData(m_outBuf);
    return m_channelCount;
}

void ThreadedChannelOutputBase::DumpConfig(void)
{
    ChannelOutputBase::DumpConfig();
	LogDebug(VB_CHANNELOUT, "    Thread Running   : %u\n", m_threadIsRunning);
	LogDebug(VB_CHANNELOUT, "    Run Thread       : %u\n", m_runThread);
	LogDebug(VB_CHANNELOUT, "    Data Waiting     : %u\n", m_dataWaiting);
}

/*
 * pthread wrapper not a member of the class
 */
void *RunChannelOutputBaseThread(void *data)
{
	LogDebug(VB_CHANNELOUT, "RunChannelOutputBaseThread()\n");

	ThreadedChannelOutputBase *output = reinterpret_cast<ThreadedChannelOutputBase*>(data);

	output->OutputThread();
    return nullptr;
}

int ThreadedChannelOutputBase::StartOutputThread(void)
{
	LogDebug(VB_CHANNELOUT, "ThreadedChannelOutputBase::StartOutputThread()\n");

	m_runThread = 1;

	int result = pthread_create(&m_threadID, NULL, &RunChannelOutputBaseThread, this);

	if (result) {
		char msg[256];

		m_runThread = 0;
		switch (result) {
			case EAGAIN: strcpy(msg, "Insufficient Resources");
				break;
			case EINVAL: strcpy(msg, "Invalid settings");
				break;
			case EPERM : strcpy(msg, "Invalid Permissions");
				break;
		}
		LogErr(VB_CHANNELOUT, "ERROR creating ChannelOutputBase thread: %s\n", msg );
	}

	while (!m_threadIsRunning)
		usleep(10000);

	return 0;
}

int ThreadedChannelOutputBase::StopOutputThread(void)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::StopOutputThread()\n");

	if (!m_threadID)
		return -1;

	m_runThread = 0;

	pthread_cond_signal(&m_sendCond);

	int loops = 0;
	// Wait up to 110ms for data to be sent
	while ((m_dataWaiting) &&
	       (m_threadIsRunning) &&
	       (loops++ < 11))
		usleep(10000);

	pthread_mutex_lock(&m_bufLock);

	if (!m_threadID) {
		pthread_mutex_unlock(&m_bufLock);
		return -1;
	}

	pthread_join(m_threadID, NULL);
	m_threadID = 0;
	pthread_mutex_unlock(&m_bufLock);

	return 0;
}

void ThreadedChannelOutputBase::OutputThread(void)
{
	LogDebug(VB_CHANNELOUT, "ThreadedChannelOutputBase::OutputThread()\n");

	long long wakeTime = GetTime();
	struct timeval  tv;
	struct timespec ts;

	m_threadIsRunning = 1;
	LogDebug(VB_CHANNELOUT, "ThreadedChannelOutputBase thread started\n");

	while (m_runThread) {
		// Wait for more data
		pthread_mutex_lock(&m_sendLock);
        long long nowTime = GetTime();
		LogExcess(VB_CHANNELOUT, "ThreadedChannelOutputBase thread: sent: %lld, elapsed: %lld\n",
			nowTime, nowTime - wakeTime);

		if (m_useDoubleBuffer)
			pthread_mutex_lock(&m_bufLock);

		if (m_dataWaiting || m_maxWait) {
			if (m_useDoubleBuffer)
				pthread_mutex_unlock(&m_bufLock);

			gettimeofday(&tv, NULL);
			ts.tv_sec = tv.tv_sec;
            if (m_maxWait) {
                ts.tv_nsec = (tv.tv_usec*1000) + (m_maxWait*1000000);
            } else {
                ts.tv_nsec = (tv.tv_usec + 200000) * 1000;
            }

			if (ts.tv_nsec >= 1000000000) {
				ts.tv_sec  += 1;
				ts.tv_nsec -= 1000000000;
			}

			pthread_cond_timedwait(&m_sendCond, &m_sendLock, &ts);
		} else {
			if (m_useDoubleBuffer)
				pthread_mutex_unlock(&m_bufLock);

			pthread_cond_wait(&m_sendCond, &m_sendLock);
		}

		pthread_mutex_unlock(&m_sendLock);

		if (!m_runThread)
			continue;

        wakeTime = GetTime();
        LogExcess(VB_CHANNELOUT, "ThreadedChannelOutputBase thread: woke: %lld\n", wakeTime);

		// See if there is any data waiting to process or if we timed out
		if (m_useDoubleBuffer)
			pthread_mutex_lock(&m_bufLock);

		if (m_dataWaiting) {
			if (m_useDoubleBuffer)
				pthread_mutex_unlock(&m_bufLock);

			SendOutputBuffer();
		} else {
			if (m_useDoubleBuffer)
				pthread_mutex_unlock(&m_bufLock);
            WaitTimedOut();
		}
	}

	LogDebug(VB_CHANNELOUT, "ThreadedChannelOutputBase thread complete\n");
	m_threadIsRunning = 0;
}

