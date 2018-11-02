/*
 *   omxplayer driver for Falcon Player (FPP)
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

#ifndef _OMXPLAYER_H
#define _OMXPLAYER_H

#include "MediaOutputBase.h"

class omxplayerOutput : public MediaOutputBase {
  public:
	omxplayerOutput(std::string mediaFilename, MediaOutputStatus *status);
	~omxplayerOutput();

	virtual int  Start(void);
	virtual int  Stop(void);
	virtual int  Process(void);
    virtual int  IsPlaying(void);
    virtual int  Close(void);

	int  AdjustSpeed(int delta);
	void SetVolume(int volume);

  private:
	int  GetVolumeShift(int volume);
	void ProcessPlayerData(char * buffer, int bytesRead);
	void PollPlayerInfo(void);
    
    bool m_allowSpeedAdjust;
	int  m_volumeShift;
    bool m_beforeFirstTick;
    char *m_omxBuffer;
};

#endif
