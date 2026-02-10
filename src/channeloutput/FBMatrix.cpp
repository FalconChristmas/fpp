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

#include "../Warnings.h"
#include "../common.h"
#include "../log.h"

#include "FBMatrix.h"
#include "overlays/PixelOverlay.h"

#include "../Plugin.h"

class FBMatrixPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    FBMatrixPlugin() :
        FPPPlugins::Plugin("FBMatrix") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new FBMatrixOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new FBMatrixPlugin();
}
}

/*
 *
 */
FBMatrixOutput::FBMatrixOutput(unsigned int startChannel,
                               unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount) {
    LogDebug(VB_CHANNELOUT, "FBMatrixOutput::FBMatrixOutput(%u, %u)\n",
             startChannel, channelCount);
}

/*
 *
 */
FBMatrixOutput::~FBMatrixOutput() {
    LogDebug(VB_CHANNELOUT, "FBMatrixOutput::~FBMatrixOutput()\n");

    if (buffer)
        delete[] buffer;
}

int FBMatrixOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "FBMatrixOutput::Init()\n");

    if (config.isMember("modelName"))
        modelName = config["modelName"].asString();

    printf("FBMatrix::Init: modelName='%s', empty=%d\n", modelName.c_str(), modelName == "");
    fflush(stdout);

    if (modelName == "") {
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
            modelName = "FB - ";
            modelName += device;

            printf("FBMatrix: Checking for existing model '%s', hasModel=%d\n", 
                   modelName.c_str(), PixelOverlayManager::INSTANCE.getModel(modelName) != nullptr);
            fflush(stdout);

            // Delete existing model if it exists, so we can recreate with PixelSize
            if (PixelOverlayManager::INSTANCE.getModel(modelName)) {
                printf("FBMatrix: Removing existing model '%s' to recreate with PixelSize\n", modelName.c_str());
                fflush(stdout);
                PixelOverlayManager::INSTANCE.removeAutoOverlayModel(modelName);
            }

            if (width && height && (device != "")) {
                Json::Value val;
                val["Name"] = modelName;
                val["Type"] = "FB";
                val["Width"] = width;
                val["Height"] = height;
                val["Device"] = device;
                val["autoCreated"] = true;
                
                // Pass through pixelSize if specified in config
                if (config.isMember("pixelSize")) {
                    int ps = config["pixelSize"].asInt();
                    val["PixelSize"] = ps;
                    printf("FBMatrix: Setting PixelSize=%d for model '%s'\n", ps, modelName.c_str());
                    fflush(stdout);
                } else {
                    printf("FBMatrix: No pixelSize in config for model '%s'\n", modelName.c_str());
                    fflush(stdout);
                }

                PixelOverlayManager::INSTANCE.addModel(val);
                m_autoCreatedFBModelName = modelName;
            } else {
                LogErr(VB_CHANNELOUT, "Empty Pixel Overlay Model name\n");
                WarningHolder::AddWarning("VirtualMatrix: Empty Pixel Overlay Model name");
                return 0;
            }
        } else {
            LogErr(VB_CHANNELOUT, "Empty Pixel Overlay Model name\n");
            WarningHolder::AddWarning("VirtualMatrix: Empty Pixel Overlay Model name");
            return 0;
        }
    }

    model = PixelOverlayManager::INSTANCE.getModel(modelName);

    if (!model) {
        LogErr(VB_CHANNELOUT, "Invalid Pixel Overlay Model: '%s'\n", modelName.c_str());
        WarningHolder::AddWarning("VirtualMatrix: Invalid Pixel Overlay Model: " + modelName);
        return 0;
    }

    width = model->getWidth();
    height = model->getHeight();

    // Calculate channel count from the model size
    m_channelCount = width * height * 3;

    inverted = config["invert"].asInt();

    if (m_channelCount != (width * height * 3)) {
        LogErr(VB_CHANNELOUT, "Error, channel count is incorrect\n");
        WarningHolder::AddWarning("VirtualMatrix: Invalid Pixel Overlay Model: " + modelName);
        return 0;
    }

    buffer = new unsigned char[width * height * 3];

    if (PixelOverlayManager::INSTANCE.isAutoCreatePixelOverlayModels()) {
        std::string dd = "VirtualMatrix";
        if (config.isMember("description")) {
            dd = config["description"].asString();
        }
        std::string desc = dd;
        int count = 0;
        while (PixelOverlayManager::INSTANCE.getModel(desc) != nullptr) {
            count++;
            desc = dd + "-" + std::to_string(count);
        }
        PixelOverlayManager::INSTANCE.addAutoOverlayModel(desc,
                                                          m_startChannel, m_channelCount, 3,
                                                          "H", "TL",
                                                          height, 1);
        m_autoCreatedModelName = desc;
    }
    return ChannelOutput::Init(config);
}

/*
 *
 */
int FBMatrixOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "FBMatrixOutput::Close()\n");
    if (!m_autoCreatedModelName.empty()) {
        PixelOverlayManager::INSTANCE.removeAutoOverlayModel(m_autoCreatedModelName);
    }
    if (!m_autoCreatedFBModelName.empty()) {
        PixelOverlayManager::INSTANCE.removeAutoOverlayModel(m_autoCreatedFBModelName);
    }
    return ChannelOutput::Close();
}

void FBMatrixOutput::PrepData(unsigned char* channelData) {
    channelData += m_startChannel;

    LogExcess(VB_CHANNELOUT, "FBMatrixOutput::SendData(%p)\n",
              channelData);

    int stride = width * 3;
    int drow = inverted ? height - 1 : 0;
    unsigned char* src = channelData;
    unsigned char* dst = buffer;

    if (inverted) {
        dst = (unsigned char*)buffer + (stride * (height - 1));
    }
    for (int y = 0; y < height; y++) {
        memcpy(dst, src, stride);
        src += stride;
        if (inverted) {
            dst -= stride;
        } else {
            dst += stride;
        }
    }
}

int FBMatrixOutput::SendData(unsigned char* channelData) {
    model->setData(buffer);
    if (model->getState() == 0) {
        model->doOverlay(channelData);
    }
    return m_channelCount;
}

void FBMatrixOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + (width * height * 3) - 1);
}

/*
 *
 */
void FBMatrixOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "VirtualMatrixOutput::DumpConfig()\n");
    LogDebug(VB_CHANNELOUT, "    model  : %s\n", modelName.c_str());
    LogDebug(VB_CHANNELOUT, "    width  : %d\n", width);
    LogDebug(VB_CHANNELOUT, "    height : %d\n", height);
}
