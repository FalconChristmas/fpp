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



extern "C" {
    FBMatrixOutput *createFBMatrixOutput(unsigned int startChannel,
                                         unsigned int channelCount) {
        return new FBMatrixOutput(startChannel, channelCount);
    }
}

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
	m_inverted(0),
	m_bpp(24),
	m_device("/dev/fb0"),
	m_fbp(nullptr),
    m_frame(nullptr),
	m_screenSize(0),
	m_rgb565map(nullptr)
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

    if (m_rgb565map) {
		for (int r = 0; r < 32; r++)
		{
			for (int g = 0; g < 64; g++)
			{
				delete[] m_rgb565map[r][g];
			}

			delete[] m_rgb565map[r];
		}

		delete[] m_rgb565map;
	}
    if (m_frame) {
        delete [] m_frame;
    }
}
int FBMatrixOutput::Init(Json::Value config) {
	LogDebug(VB_CHANNELOUT, "FBMatrixOutput::Init()\n");
    

    m_width = config["width"].asInt();
    m_height = config["height"].asInt();
    m_useRGB = config["colorOrder"].asString() == "RGB";
    m_inverted = config["invert"].asInt();
    m_device = "/dev/" + config["device"].asString();
    
	LogDebug(VB_CHANNELOUT, "Using FrameBuffer device %s\n", m_device.c_str());
	m_fbFd = open(m_device.c_str(), O_RDWR);
	if (!m_fbFd) {
		LogErr(VB_CHANNELOUT, "Error opening FrameBuffer device: %s\n", m_device.c_str());
		return 0;
	}

	if (ioctl(m_fbFd, FBIOGET_VSCREENINFO, &m_vInfo)) {
		LogErr(VB_CHANNELOUT, "Error getting FrameBuffer info\n");
		close(m_fbFd);
		return 0;
	}

	memcpy(&m_vInfoOrig, &m_vInfo, sizeof(struct fb_var_screeninfo));

	m_bpp = m_vInfo.bits_per_pixel;
	LogDebug(VB_CHANNELOUT, "FrameBuffer is using %d BPP\n", m_bpp);

	if ((m_bpp != 32) && (m_bpp != 24) && (m_bpp != 16)) {
		LogErr(VB_CHANNELOUT, "Do not know how to handle %d BPP\n", m_bpp);
		close(m_fbFd);
		return 0;
	}

	if (m_bpp == 16) {
		LogExcess(VB_CHANNELOUT, "Current Bitfield offset info:\n");
		LogExcess(VB_CHANNELOUT, " R: %d (%d bits)\n", m_vInfo.red.offset, m_vInfo.red.length);
		LogExcess(VB_CHANNELOUT, " G: %d (%d bits)\n", m_vInfo.green.offset, m_vInfo.green.length);
		LogExcess(VB_CHANNELOUT, " B: %d (%d bits)\n", m_vInfo.blue.offset, m_vInfo.blue.length);

		// RGB565
		m_vInfo.red.offset    = 11;
		m_vInfo.red.length    = 5;
		m_vInfo.green.offset  = 5;
		m_vInfo.green.length  = 6;
		m_vInfo.blue.offset   = 0;
		m_vInfo.blue.length   = 5;
		m_vInfo.transp.offset = 0;
		m_vInfo.transp.length = 0;

		LogExcess(VB_CHANNELOUT, "New Bitfield offset info should be:\n");
		LogExcess(VB_CHANNELOUT, " R: %d (%d bits)\n", m_vInfo.red.offset, m_vInfo.red.length);
		LogExcess(VB_CHANNELOUT, " G: %d (%d bits)\n", m_vInfo.green.offset, m_vInfo.green.length);
		LogExcess(VB_CHANNELOUT, " B: %d (%d bits)\n", m_vInfo.blue.offset, m_vInfo.blue.length);
	}

	m_vInfo.xres_virtual = m_width;
	m_vInfo.yres_virtual = m_height;
    m_vInfo.xres = m_width;
    m_vInfo.yres = m_height;

	// Config to set the screen back to when we are done
	// Once we determine how this interacts with omxplayer, this may change
	m_vInfoOrig.bits_per_pixel = m_bpp;
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

	if (m_screenSize != (m_width * m_height * m_bpp / 8)) {
        LogErr(VB_CHANNELOUT, "Error, screensize incorrect:   %dx%dx%d      %dx%dx%d\n",
               m_vInfo.xres, m_vInfo.yres, m_vInfo.bits_per_pixel,
               m_width, m_height, m_bpp);
		ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfoOrig);
		close(m_fbFd);
		return 0;
	}

	if (m_channelCount != (m_width * m_height * 3)) {
		LogErr(VB_CHANNELOUT, "Error, channel count is incorrect\n");
		ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfoOrig);
		close(m_fbFd);
		return 0;
	}

	if (m_device == "/dev/fb0") {
		m_ttyFd = open("/dev/console", O_RDWR);
		if (!m_ttyFd) {
			LogErr(VB_CHANNELOUT, "Error, unable to open /dev/console\n");
			ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfoOrig);
			close(m_fbFd);
			return 0;
		}

		// Hide the text console
		ioctl(m_ttyFd, KDSETMODE, KD_GRAPHICS);
	}

	m_fbp = (char*)mmap(0, m_screenSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_fbFd, 0);
    m_frame = new char[m_screenSize];

	if ((char *)m_fbp == (char *)-1) {
		LogErr(VB_CHANNELOUT, "Error, unable to map /dev/fb0\n");
		ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfoOrig);
		close(m_fbFd);
		return 0;
	}

	if (m_bpp == 16) {
		LogExcess(VB_CHANNELOUT, "Generating RGB565Map for Bitfield offset info:\n");
		LogExcess(VB_CHANNELOUT, " R: %d (%d bits)\n", m_vInfo.red.offset, m_vInfo.red.length);
		LogExcess(VB_CHANNELOUT, " G: %d (%d bits)\n", m_vInfo.green.offset, m_vInfo.green.length);
		LogExcess(VB_CHANNELOUT, " B: %d (%d bits)\n", m_vInfo.blue.offset, m_vInfo.blue.length);

		unsigned char rMask = (0xFF ^ (0xFF >> m_vInfo.red.length));
		unsigned char gMask = (0xFF ^ (0xFF >> m_vInfo.green.length));
		unsigned char bMask = (0xFF ^ (0xFF >> m_vInfo.blue.length));
		int rShift = m_vInfo.red.offset - (8 + (8-m_vInfo.red.length));
		int gShift = m_vInfo.green.offset - (8 + (8 - m_vInfo.green.length));
		int bShift = m_vInfo.blue.offset - (8 + (8 - m_vInfo.blue.length));;

		//LogDebug(VB_CHANNELOUT, "rM/rS: 0x%02x/%d, gM/gS: 0x%02x/%d, bM/bS: 0x%02x/%d\n", rMask, rShift, gMask, gShift, bMask, bShift);

		uint16_t o;
		m_rgb565map = new uint16_t**[32];

		for (uint16_t b = 0; b < 32; b++) {
			m_rgb565map[b] = new uint16_t*[64];
			for (uint16_t g = 0; g < 64; g++) {
				m_rgb565map[b][g] = new uint16_t[32];
				for (uint16_t r = 0; r < 32; r++) {
					o = 0;

					if (rShift >= 0)
						o |= r >> rShift;
					else
						o |= r << abs(rShift);

					if (gShift >= 0)
						o |= g >> gShift;
					else
						o |= g << abs(gShift);

					if (bShift >= 0)
						o |= b >> bShift;
					else
						o |= b << abs(bShift);

					m_rgb565map[b][g][r] = o;
				}
			}
		}
	}

	return ChannelOutputBase::Init(config);
}

/*
 *
 */
int FBMatrixOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "FBMatrixOutput::Close()\n");

	munmap(m_fbp, m_screenSize);

	if (m_device == "/dev/fb0") {
		if (ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfoOrig))
			LogErr(VB_CHANNELOUT, "Error resetting variable info\n");
	}

	close(m_fbFd);

	if (m_device == "/dev/fb0") {
		// Re-enable the text console
		ioctl(m_ttyFd, KDSETMODE, KD_TEXT);
		close(m_ttyFd);
	}

    if (m_frame) {
        delete [] m_frame;
        m_frame = nullptr;
    }
	return ChannelOutputBase::Close();
}


void FBMatrixOutput::PrepData(unsigned char *channelData) {
    channelData += m_startChannel;


	LogExcess(VB_CHANNELOUT, "FBMatrixOutput::SendData(%p)\n",
		channelData);

	int ostride = m_width * m_bpp / 8;
	int srow = 0;
	int drow = m_inverted ? m_height - 1 : 0;
	unsigned char *s = channelData;
	unsigned char *d;
	unsigned char *sR = channelData;
	unsigned char *sG = channelData + 1;
	unsigned char *sB = channelData + 2;

	if (m_bpp == 16) {
		for (int y = 0; y < m_height; y++) {
			d = (unsigned char *)m_frame + (drow * ostride);
			for (int x = 0; x < m_width; x++) {
                if (m_useRGB) // RGB data to BGR framebuffer
                    *((uint16_t*)d) = m_rgb565map[*sR >> 3][*sG >> 2][*sB >> 3];
                else // BGR data to BGR framebuffer
                    *((uint16_t*)d) = m_rgb565map[*sB >> 3][*sG >> 2][*sR >> 3];

                sG += 3;
                sB += 3;
                sR += 3;
                d += 2;
			}

			srow++;
			drow += m_inverted ? -1 : 1;
		}
	} else if (m_useRGB || m_bpp == 32) {
		unsigned char *dR;
		unsigned char *dG;
		unsigned char *dB;
        unsigned char *dA;
        int add = m_bpp / 8;

		for (int y = 0; y < m_height; y++) {
			// RGB data to BGR framebuffer
            if (m_useRGB) {
                dR = (unsigned char *)m_frame + (drow * ostride) + 2;
                dG = (unsigned char *)m_frame + (drow * ostride) + 1;
                dB = (unsigned char *)m_frame + (drow * ostride) + 0;
            } else {
                dR = (unsigned char *)m_frame + (drow * ostride) + 0;
                dG = (unsigned char *)m_frame + (drow * ostride) + 1;
                dB = (unsigned char *)m_frame + (drow * ostride) + 2;
            }

			for (int x = 0; x < m_width; x++) {
                *dR = *sR;
                *dG = *sG;
                *dB = *sB;

				sR += 3;
				sG += 3;
				sB += 3;
				dR += add;
				dG += add;
				dB += add;
                dA += add;
			}

			srow++;
			drow += m_inverted ? -1 : 1;
		}
	} else {
		if (m_inverted) {
			int istride = m_width * 3;
			unsigned char *src = channelData;
			unsigned char *dst = (unsigned char *)m_frame + (ostride * (m_height-1));

			for (int y = 0; y < m_height; y++) {
				memcpy(dst, src, istride);
				src += istride;
				dst -= ostride;
			}
		} else {
			memcpy(m_frame, channelData, m_screenSize);
		}
	}
}

int FBMatrixOutput::SendData(unsigned char *channelData) {
    memcpy(m_fbp, m_frame, m_screenSize);
    return m_channelCount;
}


void FBMatrixOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    addRange(m_startChannel, m_startChannel + (m_width * m_height * 3) - 1);
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

