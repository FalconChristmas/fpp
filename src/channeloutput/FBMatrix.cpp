/*
 *   FrameBuffer Virtual Matrix handler for Falcon Player (FPP)
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

#include <fcntl.h>
#include <linux/kd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "common.h"
#include "log.h"
#include "FBMatrix.h"
#include "settings.h"

/////////////////////////////////////////////////////////////////////////////
// To disable interpolated scaling on the GPU, add this to /boot/config.txt:
// scaling_kernel=8

/*
 *
 */
FBMatrixOutput::FBMatrixOutput(unsigned int startChannel,
	unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_fbFd(0),
	m_ttyFd(0),
	m_width(0),
	m_height(0),
	m_useRGB(0),
	m_fbp(NULL),
	m_screenSize(0)
{
	LogDebug(VB_CHANNELOUT, "FBMatrixOutput::FBMatrixOutput(%u, %u)\n",
		startChannel, channelCount);
}

/*
 *
 */
FBMatrixOutput::~FBMatrixOutput()
{
	LogDebug(VB_CHANNELOUT, "FBMatrixOutput::~FBMatrixOutput()\n");
}

/*
 *
 */
int FBMatrixOutput::Init(char *configStr)
{
	LogDebug(VB_CHANNELOUT, "FBMatrixOutput::Init('%s')\n", configStr);

	std::vector<std::string> configElems = split(configStr, ';');

	for (int i = 0; i < configElems.size(); i++)
	{
		std::vector<std::string> elem = split(configElems[i], '=');
		if (elem.size() < 2)
			continue;

		if (elem[0] == "layout")
		{
			m_layout = elem[1];

			std::vector<std::string> parts = split(m_layout, 'x');
			m_width  = atoi(parts[0].c_str());
			m_height = atoi(parts[1].c_str());
		}
		else if (elem[0] == "colorOrder")
		{
			if (elem[1] == "RGB")
				m_useRGB = 1;
		}
	}

	m_fbFd = open("/dev/fb0", O_RDWR);
	if (!m_fbFd)
	{
		LogErr(VB_CHANNELOUT, "Error opening FrameBuffer device\n");
		return 0;
	}

	if (ioctl(m_fbFd, FBIOGET_VSCREENINFO, &m_vInfo))
	{
		LogErr(VB_CHANNELOUT, "Error getting FrameBuffer info\n");
		close(m_fbFd);
		return 0;
	}

	memcpy(&m_vInfoOrig, &m_vInfo, sizeof(struct fb_var_screeninfo));

	m_vInfo.bits_per_pixel = 24;
	m_vInfo.xres = m_vInfo.xres_virtual = m_width;
	m_vInfo.yres = m_vInfo.yres_virtual = m_height;

	// Config to set the screen back to when we are done
	// Once we determine how this interacts with omxplayer, this may change
	m_vInfoOrig.bits_per_pixel = 16;
	m_vInfoOrig.xres = m_vInfoOrig.xres_virtual = 640;
	m_vInfoOrig.yres = m_vInfoOrig.yres_virtual = 480;

	if (ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfo))
	{
		LogErr(VB_CHANNELOUT, "Error setting FrameBuffer info\n");
		close(m_fbFd);
		return 0;
	}

	if (ioctl(m_fbFd, FBIOGET_FSCREENINFO, &m_fInfo))
	{
		LogErr(VB_CHANNELOUT, "Error getting fixed FrameBuffer info\n");
		close(m_fbFd);
		return 0;
	}

	m_screenSize = m_vInfo.xres * m_vInfo.yres * m_vInfo.bits_per_pixel / 8;

	if ((m_screenSize != (m_width * m_height * 3)) ||
		(m_screenSize != m_channelCount))
	{
		LogErr(VB_CHANNELOUT, "Error, screensize incorrect\n");
		ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfoOrig);
		close(m_fbFd);
		return 0;
	}

	m_ttyFd = open("/dev/console", O_RDWR);
	if (!m_ttyFd)
	{
		LogErr(VB_CHANNELOUT, "Error, unable to open /dev/console\n");
		ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfoOrig);
		close(m_fbFd);
		return 0;
	}

	// Hide the text console
	ioctl(m_ttyFd, KDSETMODE, KD_GRAPHICS);

	m_fbp = (char*)mmap(0, m_screenSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_fbFd, 0);

	if ((char *)m_fbp == (char *)-1)
	{
		LogErr(VB_CHANNELOUT, "Error, unable to map /dev/fb0\n");
		ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfoOrig);
		close(m_fbFd);
		return 0;
	}

	return ChannelOutputBase::Init(configStr);
}

/*
 *
 */
int FBMatrixOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "FBMatrixOutput::Close()\n");

	munmap(m_fbp, m_screenSize);

	if (ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfoOrig))
		LogErr(VB_CHANNELOUT, "Error resetting variable info\n");

	close(m_fbFd);

	// Re-enable the text console
	ioctl(m_ttyFd, KDSETMODE, KD_TEXT);
	close(m_ttyFd);

	return ChannelOutputBase::Close();
}

/*
 *
 */
int FBMatrixOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "FBMatrixOutput::RawSendData(%p)\n",
		channelData);

	// FIXME, enhance this Channel Output to support wrapping, etc. to
	// allow it to be used for duplicating a physical matrix for testing
	if (m_useRGB)
	{
		// Slower method but allows sequence data to be in RGB order
		unsigned char *sR = channelData + 0;
		unsigned char *sG = channelData + 1;
		unsigned char *sB = channelData + 2;
		unsigned char *dR = (unsigned char *)m_fbp + 2;
		unsigned char *dG = (unsigned char *)m_fbp + 1;
		unsigned char *dB = (unsigned char *)m_fbp + 0;
		for (int y = 0; y < m_height; y++)
		{
			for (int x = 0; x < m_width; x++)
			{
				*dR = *sR;
				*dG = *sG;
				*dB = *sB;
				sR += 3;
				sG += 3;
				sB += 3;
				dR += 3;
				dG += 3;
				dB += 3;
			}
		}
	}
	else
		memcpy(m_fbp, channelData, m_screenSize);

	return m_channelCount;
}

/*
 *
 */
void FBMatrixOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "FBMatrixOutput::DumpConfig()\n");
	LogDebug(VB_CHANNELOUT, "    layout : %s\n", m_layout.c_str());
	LogDebug(VB_CHANNELOUT, "    width  : %d\n", m_width);
	LogDebug(VB_CHANNELOUT, "    height : %d\n", m_height);
}

