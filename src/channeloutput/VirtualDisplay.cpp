/*
 *   FBVirtualDisplay Channel Output for Falcon Player (FPP)
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

#include "fpp-pch.h"

#include <linux/kd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "FBVirtualDisplay.h"
#include "overlays/PixelOverlay.h"

extern "C" {
FBVirtualDisplayOutput* createOutputFBVirtualDisplay(unsigned int startChannel,
                                                     unsigned int channelCount) {
    return new FBVirtualDisplayOutput(startChannel, channelCount);
}
}

/////////////////////////////////////////////////////////////////////////////
// To disable interpolated scaling on the GPU, add this to /boot/config.txt:
// scaling_kernel=8

/*
 *
 */
FBVirtualDisplayOutput::FBVirtualDisplayOutput(unsigned int startChannel,
                                               unsigned int channelCount) :
    VirtualDisplayOutput(startChannel, channelCount) {
    LogDebug(VB_CHANNELOUT, "FBVirtualDisplayOutput::FBVirtualDisplayOutput(%u, %u)\n",
             startChannel, channelCount);
}

/*
 *
 */
FBVirtualDisplayOutput::~FBVirtualDisplayOutput() {
    LogDebug(VB_CHANNELOUT, "FBVirtualDisplayOutput::~FBVirtualDisplayOutput()\n");

    Close();
}

/*
 *
 */
int FBVirtualDisplayOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "FBVirtualDisplayOutput::Init()\n");

    if (config.isMember("ModelName"))
        m_modelName = config["ModelName"].asString();

    if (m_modelName == "") {
        LogErr(VB_PLAYLIST, "Empty Pixel Overlay Model name\n");
        return 0;
    }

    m_model = PixelOverlayManager::INSTANCE.getModel(m_modelName);

    if (!m_model) {
        LogErr(VB_PLAYLIST, "Invalid Pixel Overlay Model: '%s'\n", m_modelName.c_str());
        return 0;
    }

    m_model->setState(PixelOverlayState(PixelOverlayState::PixelState::Enabled));
    m_width = m_model->getWidth();
    m_height = m_model->getHeight();

    config["width"] = m_width;
    config["height"] = m_height;

    if (!VirtualDisplayOutput::Init(config))
        return 0;

    m_virtualDisplay = (unsigned char*)malloc(m_width * m_height * m_bytesPerPixel);
    if (!m_virtualDisplay) {
        LogErr(VB_CHANNELOUT, "Unable to malloc buffer\n");
        return 0;
    }

    memset(m_virtualDisplay, 0, m_width * m_height * m_bytesPerPixel);

    int result = InitializePixelMap();
    if (!result)
        return 0;

    // Put the background image onto the model.
    m_model->setData(m_virtualDisplay);

    return 1;
}

/*
 *
 */
int FBVirtualDisplayOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "FBVirtualDisplayOutput::Close()\n");

    if (m_virtualDisplay) {
        free(m_virtualDisplay);
        m_virtualDisplay = nullptr;
    }

    return VirtualDisplayOutput::Close();
}

int FBVirtualDisplayOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "FBVirtualDisplayOutput::SendData(%p)\n",
              channelData);
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    int stride = m_width * m_bytesPerPixel;
    VirtualDisplayPixel pixel;

    for (int i = 0; i < m_pixels.size(); i++) {
        pixel = m_pixels[i];

        GetPixelRGB(pixel, channelData, r, g, b);

        m_model->setPixelValue(pixel.xs, pixel.ys, r, g, b);

        if (m_pixelSize >= 2) {
            if (m_pixelSize == 2) {
                r /= 4;
                g /= 4;
                b /= 4;
                r *= 3;
                g *= 3;
                b *= 3;
            }

            if (pixel.y < (m_width - 1))
                m_model->setPixelValue(pixel.xs, pixel.ys + 1, r, g, b);

            if (pixel.y > 0)
                m_model->setPixelValue(pixel.xs, pixel.ys - 1, r, g, b);

            if (pixel.x > 0)
                m_model->setPixelValue(pixel.xs - 1, pixel.ys, r, g, b);

            if (pixel.x < (m_height - 1))
                m_model->setPixelValue(pixel.xs + 1, pixel.ys, r, g, b);
        }
    }

    m_model->setBufferIsDirty(true);

    return m_channelCount;
}
