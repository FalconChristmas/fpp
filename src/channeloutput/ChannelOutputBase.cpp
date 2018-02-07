/*
 *   ChannelOutputBase class for Falcon Player (FPP)
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

#include <vector>

#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "ChannelOutputBase.h"
#include "common.h"
#include "log.h"

ChannelOutputBase::ChannelOutputBase(unsigned int startChannel,
	 unsigned int channelCount)
  : m_outputType(""),
	m_maxChannels(0),
	m_startChannel(startChannel),
	m_channelCount(channelCount),
	m_useOutputThread(1),
	m_threadIsRunning(0),
	m_runThread(0),
	m_dataWaiting(0),
	m_threadID(0),
	m_inBuf(NULL),
	m_outBuf(NULL)
{
	pthread_mutex_init(&m_bufLock, NULL);
	pthread_mutex_init(&m_sendLock, NULL);
	pthread_cond_init(&m_sendCond, NULL);
}

ChannelOutputBase::~ChannelOutputBase()
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::~ChannelOutputBase()\n");

        pthread_cond_destroy(&m_sendCond);
	pthread_mutex_destroy(&m_bufLock);
	pthread_mutex_destroy(&m_sendLock);
}

int ChannelOutputBase::Init(void)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::Init()\n");

	m_inBuf = new unsigned char[m_channelCount];
	m_outBuf = new unsigned char[m_channelCount];

	if (m_useOutputThread)
		StartOutputThread();

	DumpConfig();

	return 1;
}

int ChannelOutputBase::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::Init(JSON)\n");

	m_outputType = config["type"].asString();

	return Init();
}

int ChannelOutputBase::Init(char *configStr)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::Init('%s')\n", configStr);

	std::vector<std::string> configElems = split(configStr, ';');

	for (int i = 0; i < configElems.size(); i++)
	{
		std::vector<std::string> elem = split(configElems[i], '=');
		if (elem.size() < 2)
			continue;

		if (elem[0] == "type")
			m_outputType = elem[1];
	}

	return Init();
}

int ChannelOutputBase::Close(void)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::Close()\n");

	if (m_useOutputThread)
		StopOutputThread();

	delete [] m_inBuf;
	delete [] m_outBuf;

	return 1;
}

int ChannelOutputBase::RawSendData(unsigned char *channelData)
{
	LogErr(VB_CHANNELOUT, "ChannelOutputBase::RawSendData(%p): "
		"Shouldn't be here, derived Channel Output class did not reimpliment "
		"the RawSendData(unsigned char *channelData) method.\n", channelData);
}

void ChannelOutputBase::PrepData(unsigned char *channelData)
{
}

int ChannelOutputBase::SendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "ChannelOutputBase::SendData(%p)\n", channelData);

	if (m_useOutputThread)
	{
		pthread_mutex_lock(&m_bufLock);
		memcpy(m_inBuf, channelData, m_channelCount);
		m_dataWaiting = 1;
		pthread_mutex_unlock(&m_bufLock);
		pthread_cond_signal(&m_sendCond);
	}
	else
	{
		RawSendData(channelData);
	}
}

int ChannelOutputBase::SendOutputBuffer(void)
{
	LogExcess(VB_CHANNELOUT, "ChannelOutputBase::SendOutputBuffer()\n");

	pthread_mutex_lock(&m_bufLock);
	memcpy(m_outBuf, m_inBuf, m_channelCount);
	m_dataWaiting = 0;
	pthread_mutex_unlock(&m_bufLock);

	RawSendData(m_outBuf);
}

void ChannelOutputBase::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    Output Type      : %s\n", m_outputType.c_str());
	LogDebug(VB_CHANNELOUT, "    Max Channels     : %u\n", m_maxChannels);
	LogDebug(VB_CHANNELOUT, "    Start Channel    : %u\n", m_startChannel + 1);
	LogDebug(VB_CHANNELOUT, "    Channel Count    : %u\n", m_channelCount);
	LogDebug(VB_CHANNELOUT, "    Use Output Thread: %u\n", m_useOutputThread);
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

	ChannelOutputBase *output = reinterpret_cast<ChannelOutputBase*>(data);

	output->OutputThread();
}

int ChannelOutputBase::StartOutputThread(void)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::StartOutputThread()\n");

	m_runThread = 1;

	int result = pthread_create(&m_threadID, NULL, &RunChannelOutputBaseThread, this);

	if (result)
	{
		char msg[256];

		m_runThread = 0;
		switch (result)
		{
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

int ChannelOutputBase::StopOutputThread(void)
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

	if (!m_threadID)
	{
		pthread_mutex_unlock(&m_bufLock);
		return -1;
	}

	pthread_join(m_threadID, NULL);
	m_threadID = 0;
	pthread_mutex_unlock(&m_bufLock);

	return 0;
}

void ChannelOutputBase::OutputThread(void)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::OutputThread()\n");

	long long wakeTime = GetTime();
	struct timeval  tv;
	struct timespec ts;

	m_threadIsRunning = 1;
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase thread started\n");

	while (m_runThread)
	{
		// Wait for more data
		pthread_mutex_lock(&m_sendLock);
		LogExcess(VB_CHANNELOUT, "ChannelOutputBase thread: sent: %lld, elapsed: %lld\n",
			GetTime(), GetTime() - wakeTime);

		pthread_mutex_lock(&m_bufLock);
		if (m_dataWaiting)
		{
			pthread_mutex_unlock(&m_bufLock);

			gettimeofday(&tv, NULL);
			ts.tv_sec = tv.tv_sec;
			ts.tv_nsec = (tv.tv_usec + 200000) * 1000;

			if (ts.tv_nsec >= 1000000000)
			{
				ts.tv_sec  += 1;
				ts.tv_nsec -= 1000000000;
			}

			pthread_cond_timedwait(&m_sendCond, &m_sendLock, &ts);
		}
		else
		{
			pthread_mutex_unlock(&m_bufLock);
			pthread_cond_wait(&m_sendCond, &m_sendLock);
		}

		wakeTime = GetTime();
		LogExcess(VB_CHANNELOUT, "ChannelOutputBase thread: woke: %lld\n", GetTime());
		pthread_mutex_unlock(&m_sendLock);

		if (!m_runThread)
			continue;

		// See if there is any data waiting to process or if we timed out
		pthread_mutex_lock(&m_bufLock);
		if (m_dataWaiting)
		{
			pthread_mutex_unlock(&m_bufLock);

			SendOutputBuffer();
		}
		else
		{
			pthread_mutex_unlock(&m_bufLock);
		}
	}

	LogDebug(VB_CHANNELOUT, "ChannelOutputBase thread complete\n");
	m_threadIsRunning = 0;
}

