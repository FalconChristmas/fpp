/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "fpp-pch.h"

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cmath>
#include <fcntl.h>

#include "../common.h"
#include "../log.h"

#include "VirtualDisplay.h"
#include "overlays/PixelOverlay.h"

#include "Plugin.h"
class VirtualDisplayPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    VirtualDisplayPlugin() :
        FPPPlugins::Plugin("VirtualDisplay") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new VirtualDisplayOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new VirtualDisplayPlugin();
}
}

/*
 *
 */
VirtualDisplayOutput::VirtualDisplayOutput(unsigned int startChannel,
                                           unsigned int channelCount) :
    VirtualDisplayBaseOutput(startChannel, channelCount) {
    LogDebug(VB_CHANNELOUT, "VirtualDisplayOutput::VirtualDisplayOutput(%u, %u)\n",
             startChannel, channelCount);
}

/*
 *
 */
VirtualDisplayOutput::~VirtualDisplayOutput() {
    LogDebug(VB_CHANNELOUT, "VirtualDisplayOutput::~VirtualDisplayOutput()\n");

    Close();
}

/*
 *
 */
int VirtualDisplayOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "VirtualDisplayOutput::Init()\n");

    // Check for old config names
    if (config.isMember("ModelName"))
        m_modelName = config["ModelName"].asString();
    if (config.isMember("PixelSize"))
        m_pixelSize = config["PixelSize"].asInt();

    // Check for new config names
    if (config.isMember("modelName"))
        m_modelName = config["modelName"].asString();
    if (config.isMember("pixelSize"))
        m_pixelSize = config["pixelSize"].asInt();

    if (m_modelName == "") {
        int width = 0;
        int height = 0;
        std::string device;

        if ((config.isMember("width")) &&
            (config.isMember("height")) &&
            (config.isMember("device"))) {
            width = config["width"].asInt();
            height = config["height"].asInt();
            device = config["device"].asString();

            replaceStart(device, "/dev/");
            m_modelName = "FB - ";
            m_modelName += device;

            if (width && height && (device != "") && !PixelOverlayManager::INSTANCE.getModel(m_modelName)) {
                Json::Value val;
                val["Name"] = m_modelName;
                val["Type"] = "FB";
                val["Width"] = width;
                val["Height"] = height;
                val["PixelSize"] = 1;
                val["Device"] = device;
                val["autoCreated"] = true;

                PixelOverlayManager::INSTANCE.addModel(val);
                modelCreated = true;
            } else {
                LogErr(VB_CHANNELOUT, "Empty Pixel Overlay Model name\n");
                return 0;
            }
        } else {
            LogErr(VB_CHANNELOUT, "Empty Pixel Overlay Model name\n");
            return 0;
        }
    }

    if (m_modelName == "") {
        LogErr(VB_CHANNELOUT, "Empty Pixel Overlay Model name\n");
        return 0;
    }

    m_model = PixelOverlayManager::INSTANCE.getModel(m_modelName);

    if (!m_model) {
        LogErr(VB_CHANNELOUT, "Invalid Pixel Overlay Model: '%s'\n", m_modelName.c_str());
        return 0;
    }

    m_width = m_model->getWidth();
    m_height = m_model->getHeight();

    config["width"] = m_width;
    config["height"] = m_height;

    if (!VirtualDisplayBaseOutput::Init(config))
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

    // Put the background image onto the model and always call doOverlay
    // to ensure display is enabled (even if model state is not Disabled)
    m_model->setData(m_virtualDisplay);
    m_model->doOverlay(m_virtualDisplay);
    return 1;
}

/*
 *
 */
int VirtualDisplayOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "VirtualDisplayOutput::Close()\n");
    if (modelCreated) {
        PixelOverlayManager::INSTANCE.removeAutoOverlayModel(m_modelName);
    }
    if (m_virtualDisplay) {
        free(m_virtualDisplay);
        m_virtualDisplay = nullptr;
    }

    return VirtualDisplayBaseOutput::Close();
}

int VirtualDisplayOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "VirtualDisplayOutput::SendData(%p)\n",
              channelData);
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    int stride = m_width * m_bytesPerPixel;
    VirtualDisplayPixel pixel;

    int tx = 0;
    int ty = 0;
    int radius = m_pixelSize >= 2 ? m_pixelSize / 2 : 1;

    for (int i = 0; i < m_pixels.size(); i++) {
        pixel = m_pixels[i];

        GetPixelRGB(pixel, channelData, r, g, b);

        for (int xo = 0 - radius; xo < radius; xo++) {
            for (int yo = 0 - radius; yo < radius; yo++) {
                if (std::sqrt(xo * xo + yo * yo) > radius)
                    continue;

                tx = pixel.xs + xo;
                ty = pixel.ys + yo;

                if ((tx < 0) || (ty < 0) || (tx > (m_width - 1)) || (ty > (m_height - 1)))
                    continue;

                m_model->setPixelValue(tx, ty, r, g, b);
            }
        }
    }
    m_model->setBufferIsDirty(true);
    if (m_model->getState() == 0) {
        m_model->doOverlay(m_model->getOverlayBuffer());
    }
    return m_channelCount;
}
