/*
 *   FrameBuffer Virtual matrix handler for Falcon Player (FPP)
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

#ifndef _FBMATRIX_H
#define _FBMATRIX_H

#include <linux/fb.h>
#include <string>

#include "ThreadedChannelOutputBase.h"

class FBMatrixOutput : public ThreadedChannelOutputBase {
  public:
	FBMatrixOutput(unsigned int startChannel, unsigned int channelCount);
	~FBMatrixOutput();

    virtual int Init(Json::Value config);
	int Init(char *configStr);
	int Close(void);

	int RawSendData(unsigned char *channelData);

	void DumpConfig(void);
	virtual void GetRequiredChannelRange(int &min, int & max);

  private:
	int     m_fbFd;
	int     m_ttyFd;

	std::string  m_layout;
	int          m_width;
	int          m_height;
	int          m_useRGB;
	int          m_inverted;
	int          m_bpp;
	std::string  m_device;

	char   *m_fbp;
	int     m_screenSize;

	unsigned char *m_lastFrame;
	uint16_t ***m_rgb565map;

	struct fb_var_screeninfo m_vInfo;
	struct fb_var_screeninfo m_vInfoOrig;
	struct fb_fix_screeninfo m_fInfo;
};

#endif /* _FBMATRIX_H */
