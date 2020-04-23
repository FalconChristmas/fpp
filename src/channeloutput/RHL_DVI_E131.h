#pragma once
/*
 *   Ron's Holiday Lights DVI to E1.31 Channel Output for Falcon Player (FPP)
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
#include <linux/fb.h>

#include "ChannelOutputBase.h"

class RHLDVIE131Output : public ChannelOutputBase {
  public:
	RHLDVIE131Output(unsigned int startChannel, unsigned int channelCount);
	virtual ~RHLDVIE131Output();

	virtual int Init(Json::Value config) override;
	virtual int Close(void) override;

	virtual int SendData(unsigned char *channelData) override;

	virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) override;

  private:
	int     m_fbFd;
	int     m_ttyFd;

	int     m_width;
	int     m_height;
	int     m_bytesPerPixel;
	int     m_screenSize;
	char   *m_data;

	struct fb_var_screeninfo m_vInfo;
	struct fb_var_screeninfo m_vInfoOrig;
	struct fb_fix_screeninfo m_fInfo;
};
