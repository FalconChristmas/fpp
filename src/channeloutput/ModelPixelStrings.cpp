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

#include "fpp-json.h"

#include <fcntl.h>
#include <signal.h>
#include <stdint.h>

#include "../Sequence.h"
#include "../Warnings.h"
#include "../log.h"

#include "ModelPixelStrings.h"
#include "overlays/PixelOverlay.h"

/////////////////////////////////////////////////////////////////////////////

#include "Plugin.h"
class ModelPixelStringsPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    ModelPixelStringsPlugin() :
        FPPPlugins::Plugin("ModelPixelStrings") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new ModelPixelStringsOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new ModelPixelStringsPlugin();
}
}
/*
 *
 */
ModelPixelStringsOutput::ModelPixelStringsOutput(unsigned int startChannel, unsigned int channelCount) :
    ThreadedChannelOutput(startChannel, channelCount) {
    LogDebug(VB_CHANNELOUT, "ModelPixelStringsOutput::ModelPixelStringsOutput(%u, %u)\n",
             startChannel, channelCount);
}

/*
 *
 */
ModelPixelStringsOutput::~ModelPixelStringsOutput() {
    LogDebug(VB_CHANNELOUT, "ModelPixelStringsOutput::~ModelPixelStringsOutput()\n");

    for (int s = 0; s < strings.size(); s++)
        delete strings[s];

    if (buffer)
        free(buffer);

    if (prepBuffer)
        free(prepBuffer);
}

/*
 *
 */
int ModelPixelStringsOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "ModelPixelStringsOutput::Init(JSON)\n");

    if (config.isMember("modelName"))
        modelName = config["modelName"].asString();

    if (modelName == "") {
        LogErr(VB_CHANNELOUT, "Empty Pixel Overlay Model name\n");
        WarningHolder::AddWarning(35, "ModelPixelStringsOutput: Empty Pixel Overlay Model name");
        return 0;
    }

    model = PixelOverlayManager::INSTANCE.getModel(modelName);

    if (!model) {
        LogErr(VB_CHANNELOUT, "Invalid Pixel Overlay Model: '%s'\n", modelName.c_str());
        WarningHolder::AddWarning(35, "ModelPixelStringsOutput: Invalid Pixel Overlay Model: " + modelName);
        return 0;
    }

    model->setState(PixelOverlayState(PixelOverlayState::PixelState::Enabled));

    for (int i = 0; i < config["outputs"].size(); i++) {
        Json::Value s = config["outputs"][i];
        PixelString* newString = new PixelString();

        if (!newString->Init(s)) {
            delete newString;
            return 0;
        }

        if ((newString->m_outputBytes / 3) > model->getWidth()) {
            LogErr(VB_CHANNELOUT, "ERROR, string length of %d is too long for model width of %d\n",
                   (newString->m_outputBytes / 3), model->getWidth());
            delete newString;
        } else if (strings.size() >= (size_t)model->getHeight()) {
            // PrepData lays one string per model row, so a string past the last
            // row would write off the end of the buffer.
            LogErr(VB_CHANNELOUT, "ERROR, more strings configured than the model height of %d\n",
                   model->getHeight());
            WarningHolder::AddWarning(35, "ModelPixelStringsOutput: model " + modelName + " has fewer rows than the configured strings");
            delete newString;
        } else {
            strings.push_back(newString);
        }
    }

    LogDebug(VB_CHANNELOUT, "   Found %d strings of pixels\n", strings.size());

    // setData() reads width*height*bytesPerPixel while PrepData writes 3 bytes
    // per pixel, so the allocation has to cover whichever is larger.  Zeroed
    // because only the rows that have a string are ever written.
    size_t bufSize = (size_t)model->getWidth() * model->getHeight() *
                     std::max(3, model->getBytesPerPixel());
    buffer = (unsigned char*)calloc(1, bufSize);
    prepBuffer = (unsigned char*)calloc(1, bufSize);
    if (!buffer || !prepBuffer) {
        LogErr(VB_CHANNELOUT, "ERROR, could not allocate %zu bytes for model '%s'\n",
               bufSize, modelName.c_str());
        WarningHolder::AddWarning(35, "ModelPixelStringsOutput: could not allocate buffers for model " + modelName);
        return 0;
    }

    return ThreadedChannelOutput::Init(config);
}

/*
 *
 */
int ModelPixelStringsOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "ModelPixelStringsOutput::Close()\n");

    return ThreadedChannelOutput::Close();
}

void ModelPixelStringsOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    int min = FPPD_MAX_CHANNELS;
    int max = 0;

    PixelString* ps = NULL;
    for (int s = 0; s < strings.size(); s++) {
        ps = strings[s];
        int inCh = 0;
        for (int p = 0; p < ps->m_outputBytes; p++) {
            int ch = ps->m_outputMap[inCh++];
            if (ch < (FPPD_MAX_CHANNELS - 3)) {
                min = std::min(min, ch);
                max = std::max(max, ch);
            }
        }
    }

    if (min < max)
        addRange(min, max);
}

/*
 *
 */
void ModelPixelStringsOutput::PrepData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "ModelPixelStringsOutput::PrepData(%p)\n", channelData);

    unsigned char* d = prepBuffer;
    int stride = model->getWidth() * 3; // RGB

    PixelString* ps = NULL;
    int inCh = 0;

    for (int s = 0; s < strings.size(); s++) {
        ps = strings[s];
        d = prepBuffer + (s * stride);
        inCh = 0;

        for (int p = 0, pix = 0; p < ps->m_outputBytes; pix++) {
            *(d++) = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
            *(d++) = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
            *(d++) = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
        }
    }

    // Publish the finished frame.  The worker touches `buffer` only under this
    // lock, so what it hands to the model is never the frame being filled.
    std::lock_guard<std::mutex> lock(bufferLock);
    std::swap(buffer, prepBuffer);
}

int ModelPixelStringsOutput::RawSendData(unsigned char* channelData) {
    std::lock_guard<std::mutex> lock(bufferLock);
    model->setData(buffer);

    return m_channelCount;
}

/*
 *
 */
void ModelPixelStringsOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "ModelPixelStringsOutput::DumpConfig()\n");

    for (int i = 0; i < strings.size(); i++) {
        LogDebug(VB_CHANNELOUT, "    String #%d\n", i);
        strings[i]->DumpConfig();
    }

    ThreadedChannelOutput::DumpConfig();
}
