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

    LogDebug(VB_CHANNELOUT, "FBMatrix::Init: modelName='%s', empty=%d\n", modelName.c_str(), modelName == "");

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

            LogDebug(VB_CHANNELOUT, "FBMatrix: Checking for existing model '%s', hasModel=%d\n",
                     modelName.c_str(), PixelOverlayManager::INSTANCE.getModel(modelName) != nullptr);

            // Delete existing model if it exists, so we can recreate with PixelSize
            if (PixelOverlayManager::INSTANCE.getModel(modelName)) {
                LogDebug(VB_CHANNELOUT, "FBMatrix: Removing existing model '%s' to recreate with PixelSize\n", modelName.c_str());
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
                    LogDebug(VB_CHANNELOUT, "FBMatrix: Setting PixelSize=%d for model '%s'\n", ps, modelName.c_str());
                } else {
                    LogDebug(VB_CHANNELOUT, "FBMatrix: No pixelSize in config for model '%s'\n", modelName.c_str());
                }

                PixelOverlayManager::INSTANCE.addModel(val);
                m_autoCreatedFBModelName = modelName;
            } else {
                LogErr(VB_CHANNELOUT, "Empty Pixel Overlay Model name\n");
                WarningHolder::AddWarning(35, "VirtualMatrix: Empty Pixel Overlay Model name");
                return 0;
            }
        } else {
            LogErr(VB_CHANNELOUT, "Empty Pixel Overlay Model name\n");
            WarningHolder::AddWarning(35, "VirtualMatrix: Empty Pixel Overlay Model name");
            return 0;
        }
    }

    model = PixelOverlayManager::INSTANCE.getModel(modelName);

    if (!model) {
        LogErr(VB_CHANNELOUT, "Invalid Pixel Overlay Model: '%s'\n", modelName.c_str());
        WarningHolder::AddWarning(35, "VirtualMatrix: Invalid Pixel Overlay Model: " + modelName);
        return 0;
    }

    width = model->getWidth();
    height = model->getHeight();

    // Calculate channel count from the model size
    m_channelCount = width * height * 3;

    inverted = config["invert"].asInt();
    flipHorizontal = config["flipHorizontal"].asInt();

    if (m_channelCount != (width * height * 3)) {
        LogErr(VB_CHANNELOUT, "Error, channel count is incorrect\n");
        WarningHolder::AddWarning(35, "VirtualMatrix: Invalid Pixel Overlay Model: " + modelName);
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

    for (int y = 0; y < height; y++) {
        unsigned char* src = channelData + (y * stride);
        // Vertical flip: write source row y into the mirrored destination row
        int dstRow = inverted ? (height - 1 - y) : y;
        unsigned char* dst = buffer + (dstRow * stride);

        if (flipHorizontal) {
            // Horizontal flip: reverse the pixel order within the row
            unsigned char* sp = src;
            unsigned char* dp = dst + ((width - 1) * 3);
            for (int x = 0; x < width; x++) {
                dp[0] = sp[0];
                dp[1] = sp[1];
                dp[2] = sp[2];
                sp += 3;
                dp -= 3;
            }
        } else {
            memcpy(dst, src, stride);
        }
    }
}

int FBMatrixOutput::SendData(unsigned char* channelData) {
    if (model->getState() == 0) {
        // Pass-through mode: the model isn't being driven by an active
        // overlay/effect, so push the current sequence channels into it
        // and draw to the framebuffer right here.
        model->setData(buffer);
        model->doOverlay(channelData);
    }
    // else: the model is acting as an overlay (e.g., a Pixel Overlay
    // effect is writing into channelData via flushOverlayBuffer). Don't
    // call setData here — that would overwrite the effect's data with
    // m_seqData every frame and leave the framebuffer mostly black.
    // doOverlays() in ProcessSequenceData() drives the framebuffer in
    // that case via PixelOverlayModelFB::doOverlay().
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
    LogDebug(VB_CHANNELOUT, "    flipV  : %d\n", inverted);
    LogDebug(VB_CHANNELOUT, "    flipH  : %d\n", flipHorizontal);
}
