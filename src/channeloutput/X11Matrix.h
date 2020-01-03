/*
 *   X11 Matrix handler for Falcon Player (FPP)
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

#ifndef _X11MATRIX_H
#define _X11MATRIX_H

#include <X11/Xlib.h>

#include "ThreadedChannelOutputBase.h"

class X11MatrixOutput : public ThreadedChannelOutputBase {
  public:
	X11MatrixOutput(unsigned int startChannel, unsigned int channelCount);
	virtual ~X11MatrixOutput();

	virtual int Init(Json::Value config) override;
	virtual int Close(void) override;

	virtual int RawSendData(unsigned char *channelData) override;

	virtual void DumpConfig(void) override;

    virtual void  GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) override;

  private:
  	int         m_width;
	int         m_height;
	int         m_scale;
	int         m_scaleWidth;
	int         m_scaleHeight;
	char       *m_imageData;

	Display    *m_display;
	int         m_screen;
	Window      m_window;
	GC          m_gc;
	Pixmap      m_pixmap;
	XImage     *m_image;

	std::string m_title;
};

#endif /* _X11MATRIX_H */
