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

#ifndef _THREADEDCHANNELOUTPUTBASE_H
#define _THREADEDCHANNELOUTPUTBASE_H

#include <string>
#include <vector>

#include <pthread.h>

#include <jsoncpp/json/json.h>

#include "ChannelOutputBase.h"

class ThreadedChannelOutputBase : public ChannelOutputBase {
  public:
	ThreadedChannelOutputBase(unsigned int startChannel = 1,
		unsigned int channelCount = 1);
	virtual ~ThreadedChannelOutputBase();


	virtual int   Init(Json::Value config) override;
	virtual int   Close(void) override;

    virtual int   SendData(unsigned char *channelData) override;

	void          OutputThread(void);

  private:
	int           Init(void);

  protected:
	virtual void  DumpConfig(void) override;
	virtual int   RawSendData(unsigned char *channelData) = 0;
    virtual void  WaitTimedOut() {}
	int           StartOutputThread(void);
	int           StopOutputThread(void);
	int           SendOutputBuffer(void);

    unsigned int     m_maxWait;
	unsigned int     m_threadIsRunning;
	unsigned int     m_runThread;
	volatile unsigned int     m_dataWaiting;
	unsigned int     m_useDoubleBuffer;

	pthread_t        m_threadID;
	pthread_mutex_t  m_bufLock;
	pthread_mutex_t  m_sendLock;
	pthread_cond_t   m_sendCond;

	unsigned char   *m_inBuf;
	unsigned char   *m_outBuf;

};

#endif /* #ifndef _CHANNELOUTPUTBASE_H */
