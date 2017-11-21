/*
 *   mpg123 player driver for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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

#ifndef _MPG123_H
#define _MPG123_H

#include "MediaOutputBase.h"

#define MAX_BYTES_MP3 1000
#define TIME_STR_MAX  8

#if defined(PLATFORM_BBB) || defined(USE_MPG321)
#   define MPG123_BINARY "/usr/bin/mpg321"
#else
#   define MPG123_BINARY "/usr/bin/mpg123"
#endif

class mpg123Output : public MediaOutputBase {
  public:
	mpg123Output(std::string mediaFilename, MediaOutputStatus *status);
	~mpg123Output();

	int  Start(void);
	int  Stop(void);
	int  Process(void);

  private:
	void PollMusicInfo(void);
	void ProcessMP3Data(int bytesRead);
	void ParseTimes(void);

	char m_mp3Buffer[MAX_BYTES_MP3];
	char m_mpg123_strTime[34];
};

#endif
