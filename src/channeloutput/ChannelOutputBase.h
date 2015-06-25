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

#ifndef _CHANNELOUTPUTBASE_H
#define _CHANNELOUTPUTBASE_H

#include <string>
#include <vector>

#include <pthread.h>

#include <jsoncpp/json/json.h>

#include "channeloutput.h"

class ChannelOutputBase {
  public:
	ChannelOutputBase(unsigned int startChannel = 1,
		unsigned int channelCount = 1);
	virtual ~ChannelOutputBase();

	unsigned int  ChannelCount(void) { return m_channelCount; }
	unsigned int  StartChannel(void) { return m_startChannel; }
	int           MaxChannels(void)  { return m_maxChannels; }

	virtual int   Init(Json::Value config);
	virtual int   Init(char *configStr);
	virtual int   Close(void);
	int           SendData(unsigned char *channelData);

	void          OutputThread(void);

  private:
	int           Init(void);

  protected:
	virtual void  DumpConfig(void);
	virtual int   RawSendData(unsigned char *channelData);

	int           StartOutputThread(void);
	int           StopOutputThread(void);
	int           SendOutputBuffer(void);

	std::string      m_outputType;
	unsigned int     m_maxChannels;
	unsigned int     m_startChannel;
	unsigned int     m_channelCount;

	unsigned int     m_useOutputThread;
	unsigned int     m_threadIsRunning;
	unsigned int     m_runThread;
	unsigned int     m_dataWaiting;

	pthread_t        m_threadID;
	pthread_mutex_t  m_bufLock;
	pthread_mutex_t  m_sendLock;
	pthread_cond_t   m_sendCond;

	unsigned char   *m_inBuf;
	unsigned char   *m_outBuf;

};

#endif /* #ifndef _CHANNELOUTPUTBASE_H */
