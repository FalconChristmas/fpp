#pragma once
/*
 *   libav/SDL player driver for Falcon Player (FPP)
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

#include "MediaOutputBase.h"

class SDLInternalData;

class SDLOutput : public MediaOutputBase {
  public:
    SDLOutput(const std::string &mediaFilename, MediaOutputStatus *status, const std::string &videoOut);
	virtual ~SDLOutput();

	virtual int  Start(int msTime = 0) override;
	virtual int  Stop(void) override;
	virtual int  Process(void) override;
    virtual int  Close(void) override;
    virtual int  IsPlaying(void) override;

    
    static bool IsOverlayingVideo();
    static bool ProcessVideoOverlay(unsigned int msTimestamp);
  private:
    SDLInternalData *data;
};
