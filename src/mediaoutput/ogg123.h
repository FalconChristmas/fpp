/*
 *   ogg123 player driver for Falcon Player (FPP)
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

#ifndef _OGG123_H
#define _OGG123_H

#include "MediaOutputBase.h"

#define OGG123_BINARY "/usr/bin/ogg123"

#define MAX_BYTES_OGG 1000
#define TIME_STR_MAX  8

class ogg123Output : public MediaOutputBase {
  public:
	ogg123Output(std::string mediaFilename, MediaOutputStatus *status);
	~ogg123Output();

	int  Start(void);
	int  Stop(void);
	int  Process(void);

  private:
	void PollMusicInfo(void);
	void ProcessOGGData(int bytesRead);
	void ParseTimes(void);

	char m_oggBuffer[MAX_BYTES_OGG];
	char m_ogg123_strTime[34];
};

#endif
