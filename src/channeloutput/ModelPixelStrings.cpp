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

#include <fcntl.h>
#include <signal.h>
#include <stdint.h>

#include "overlays/PixelOverlay.h"
#include "ModelPixelStrings.h"

/////////////////////////////////////////////////////////////////////////////

extern "C" {
ModelPixelStringsOutput* createModelPixelStringsOutput(unsigned int startChannel,
                                                   unsigned int channelCount) {
    return new ModelPixelStringsOutput(startChannel, channelCount);
}
}

/*
 *
 */
ModelPixelStringsOutput::ModelPixelStringsOutput(unsigned int startChannel, unsigned int channelCount) :
    ThreadedChannelOutputBase(startChannel, channelCount) {
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
        return 0;
    }

    model = PixelOverlayManager::INSTANCE.getModel(modelName);

    if (!model) {
        LogErr(VB_CHANNELOUT, "Invalid Pixel Overlay Model: '%s'\n", modelName.c_str());
        return 0;
    }

    model->setState(PixelOverlayState(PixelOverlayState::PixelState::Enabled));

    for (int i = 0; i < config["outputs"].size(); i++) {
        Json::Value s = config["outputs"][i];
        PixelString* newString = new PixelString();

        if (!newString->Init(s))
            return 0;

        if ((newString->m_outputChannels / 3) <= model->getWidth()) {
            strings.push_back(newString);
        } else {
            delete newString;

            LogErr(VB_CHANNELOUT, "ERROR, string length of %d is too long for model width of %d\n",
                   (newString->m_outputChannels / 3), model->getWidth());
        }
    }

    LogDebug(VB_CHANNELOUT, "   Found %d strings of pixels\n", strings.size());

    buffer = (unsigned char*)malloc(model->getWidth() * model->getHeight() * 3);

    return ThreadedChannelOutputBase::Init(config);
}

/*
 *
 */
int ModelPixelStringsOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "ModelPixelStringsOutput::Close()\n");

    return ThreadedChannelOutputBase::Close();
}

void ModelPixelStringsOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    int min = FPPD_MAX_CHANNELS;
    int max = 0;

    PixelString* ps = NULL;
    for (int s = 0; s < strings.size(); s++) {
        ps = strings[s];
        int inCh = 0;
        for (int p = 0; p < ps->m_outputChannels; p++) {
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
    LogExcess(VB_CHANNELOUT, "ModelPixelStringsOutput::RawSendData(%p)\n", channelData);

    unsigned char* c = channelData;
    unsigned char* d = buffer;
    int stride = model->getWidth() * 3; // RGB

    PixelString* ps = NULL;
    int inCh = 0;

    for (int s = 0; s < strings.size(); s++) {
        ps = strings[s];
        d = buffer + (s * stride);
        inCh = 0;

        for (int p = 0, pix = 0; p < ps->m_outputChannels; pix++) {
            *(d++) = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
            *(d++) = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
            *(d++) = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
        }
    }
}

int ModelPixelStringsOutput::RawSendData(unsigned char* channelData) {
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

    ThreadedChannelOutputBase::DumpConfig();
}
