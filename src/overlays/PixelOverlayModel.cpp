/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#if __has_include(<sys/posix_shm.h>)
#include <sys/posix_shm.h>
#else
#include <limits.h>
#define PSHMNAMLEN NAME_MAX
#endif

#include <Magick++/Blob.h>
#include <Magick++/Color.h>
#include <Magick++/Geometry.h>
#include <Magick++/Image.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <algorithm>
#include <cstdint>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <list>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "../Plugins.h"
#include "../Sequence.h"
#include "../commands/Commands.h"
#include "../common.h"
#include "../effects.h"
#include "../log.h"
#include "../settings.h"

#include "PixelOverlay.h"
#include "PixelOverlayEffects.h"
#include "PixelOverlayModel.h"

static uint8_t* createChannelDataMemory(const std::string& dataName, uint32_t size) {
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    int f = shm_open(dataName.c_str(), O_RDWR | O_CREAT, mode);
    int flags = MAP_SHARED;
    if (f == -1) {
        LogWarn(VB_CHANNELOUT, "Could not create shared memory block for %s:  %s\n", dataName.c_str(), strerror(errno));
        //we couldn't create the shared memory block.  Most of the time,
        //we are fine with non-shared memory and this at least prevents a crash
        flags = MAP_ANON;
    } else {
        int rc = ftruncate(f, size);
        if (rc == -1) {
            //if ftruncate fails, we need to completely reset
            close(f);
            shm_unlink(dataName.c_str());
            f = shm_open(dataName.c_str(), O_RDWR | O_CREAT, mode);
            ftruncate(f, size);
        }
    }
    uint8_t* channelData = (uint8_t*)mmap(0, size, PROT_READ | PROT_WRITE, flags, f, 0);
    if (channelData == MAP_FAILED) {
        //mmap is failing, but we need channelData so just do malloc
        channelData = (uint8_t*)malloc(size);
    }
    if (f != -1) {
        close(f);
    }
    return channelData;
}

PixelOverlayModel::PixelOverlayModel(const Json::Value& c) :
    config(c),
    overlayBufferData(nullptr),
    channelData(nullptr),
    runningEffect(nullptr) {
    name = config["Name"].asString();
    type = config["Type"].asString();
    replaceAll(name, "/", "_");
    startChannel = config["StartChannel"].asInt();
    startChannel--; //need to be 0 based
    channelCount = config["ChannelCount"].asInt();
    int strings = config["StringCount"].asInt();
    int sps = config["StrandsPerString"].asInt();

    std::string orientation = config["Orientation"].asString();
    std::string startCorner = config["StartCorner"].asString();

    bool TtoB = (startCorner[0] == 'T');
    bool LtoR = (startCorner[1] == 'L');

    channelsPerNode = 3;
    if (config.isMember("ChannelCountPerNode")) {
        channelsPerNode = config["ChannelCountPerNode"].asInt();
        if (channelsPerNode == 0) {
            channelsPerNode = 1;
        }
    }

    if (config["Type"].asString() != "Channel") {
        if (config.isMember("PixelSize")) {
            width = config["Width"].asInt() / config["PixelSize"].asInt();
            height = config["Height"].asInt() / config["PixelSize"].asInt();
        } else {
            width = config["Width"].asInt();
            height = config["Height"].asInt();
        }
        config["ChannelCount"] = channelCount = width * height * channelsPerNode;
        config["StringCount"] = strings = height;
        config["StrandsPerString"] = sps = 1;
        config["StartChannel"] = startChannel = 0;
        config["Orientation"] = orientation = "horizontal";
        config["StartCorner"] = startCorner = "TL";
        TtoB = true;
        LtoR = true;
    } else {
        height = strings * sps;
        if (height == 0) {
            height = 1;
        }
        width = channelCount / channelsPerNode;
        width /= height;
        if (width == 0) {
            width = 1;
        }
    }

    std::string dataName = "/FPP-Model-Data-" + name;
    if (PSHMNAMLEN <= 48) {
        // system doesn't allow very long shared memory names, we'll use a shortened form
        dataName = "/FPPMD-" + name;
        if (dataName.size() > PSHMNAMLEN) {
            dataName = dataName.substr(0, PSHMNAMLEN);
        }
    }
    channelData = createChannelDataMemory(dataName, channelCount);

    if (orientation == "V" || orientation == "vertical") {
        channelMap.resize(width * height * 3);

        std::swap(width, height);
        for (int x = 0; x < width; x++) {
            int segment = x % sps;
            for (int y = 0; y < height; y++) {
                //pos in the normal overlay buffer
                int ppos = y * width + x;
                // Relative Input Pixel 'R' channel
                int inCh = (ppos * 3);

                // X position in output
                int outX = (LtoR) ? x : width - x - 1;
                // Y position in output
                int outY = (TtoB == (segment % 2)) ? height - y - 1 : y;

                // Relative Mapped Output Pixel 'R' channel
                int mpos = outX * height + outY;
                int outCh = (mpos * channelsPerNode);

                // Map the pixel's triplet
                for (int cho = 0; cho < 3; cho++) {
                    if (cho < channelsPerNode) {
                        channelMap[inCh + cho] = outCh + cho;
                    } else {
                        channelMap[inCh + cho] = FPPD_OFF_CHANNEL;
                    }
                }
            }
        }
    } else if (orientation == "C" || orientation == "custom") {
        std::string customData = ",;,";
        if (config.isMember("data")) {
            customData = config["data"].asString();
        }
        std::vector<std::string> layers = split(customData, '|');
        std::vector<std::vector<std::string>> allData;
        for (auto& layer : layers) {
            std::vector<std::string> lines = split(layer, ';');
            for (auto& l : lines) {
                allData.push_back(split(l, ','));
            }
            lines.clear();
        }
        width = 1;
        height = allData.size();
        for (auto& l : allData) {
            width = std::max(width, (int)l.size());
        }
        if (height < 1) {
            height = 1;
        }
        channelMap.resize(width * height * 3);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int node = -1;
                if (y < allData.size() && x < allData[y].size()) {
                    std::string& s = allData[y][x];
                    if (s == "" || s.length() > 10) {
                        node = -1;
                    } else {
                        node = std::stoi(s);
                        if (node > FPPD_MAX_CHANNEL_NUM) {
                            node = -1;
                        }
                    }
                }
                int inChan = node * channelsPerNode;

                for (int cho = 0; cho < 3; cho++) {
                    if (node != -1 && cho < channelsPerNode) {
                        channelMap[y * 3 + x + cho] = node * channelsPerNode + cho;
                    } else {
                        channelMap[y * 3 + x + cho] = FPPD_OFF_CHANNEL;
                    }
                }
            }
        }
    } else {
        channelMap.resize(width * height * 3);
        for (int y = 0; y < height; y++) {
            int segment = y % sps;
            for (int x = 0; x < width; x++) {
                //pos in the normal overlay buffer
                int ppos = y * width + x;
                // Input Pixel 'R' channel
                int inCh = (ppos * 3);

                // X position in output
                int outX = (LtoR == (segment % 2)) ? width - x - 1 : x;
                // Y position in output
                int outY = (TtoB) ? y : height - y - 1;

                // Relative Mapped Output Pixel 'R' channel
                int mpos = outY * width + outX;
                int outCh = (mpos * channelsPerNode);

                // Map the pixel's triplet
                for (int cho = 0; cho < 3; cho++) {
                    if (cho < channelsPerNode) {
                        channelMap[inCh + cho] = outCh + cho;
                    } else {
                        channelMap[inCh + cho] = FPPD_OFF_CHANNEL;
                    }
                }
            }
        }
    }
}
PixelOverlayModel::~PixelOverlayModel() {
    if (channelData) {
        munmap(channelData, channelCount);
        std::string dataName = "/FPP-Model-Data-" + name;
        if (PSHMNAMLEN <= 48) {
            // system doesn't allow very long shared memory names, we'll use a shortened form
            dataName = "/FPPMD-" + name;
            if (dataName.size() > PSHMNAMLEN) {
                dataName = dataName.substr(0, PSHMNAMLEN);
            }
        }
        shm_unlink(dataName.c_str());
    }
    if (overlayBufferData) {
        munmap(overlayBufferData, width * height * 3 + sizeof(OverlayBufferData));
        std::string overlayBufferName = "/FPP-Model-Overlay-Buffer-" + name;
        if (PSHMNAMLEN <= 48) {
            // system doesn't allow very long shared memory names, we'll use a shortened form
            overlayBufferName = "/FPPMB-" + name;
            if (overlayBufferName.size() > PSHMNAMLEN) {
                overlayBufferName = overlayBufferName.substr(0, PSHMNAMLEN);
            }
        }
        shm_unlink(overlayBufferName.c_str());
    }
}

PixelOverlayState PixelOverlayModel::getState() const {
    return state;
}

void PixelOverlayModel::setState(const PixelOverlayState& st) {
    if (st == state)
        return;

    if (!st.getState() && needRefresh()) {
        // Give the buffer a little time to flush before disabling
        int x = 0;
        while (needRefresh() && (x++ < 10)) {
            usleep(10000);
        }
    }

    PixelOverlayState old = state;
    state = st;
    PixelOverlayManager::INSTANCE.modelStateChanged(this, old, state);
}
void PixelOverlayModel::setChildState(const std::string& n, const PixelOverlayState& st, int ox, int oy, int w, int h) {
    bool hadChildren = !children.empty();

    auto it = children.begin();
    bool found = false;
    while (it != children.end()) {
        if (it->name == n) {
            found = true;
            if (st.getState() == 0) {
                children.erase(it);
                it = children.begin();
            } else {
                it->name = n;
                it->state = st;
                it->xoffset = ox;
                it->yoffset = oy;
                it->width = w;
                it->height = h;
            }
        }
        it++;
    }

    if (!found && st.getState()) {
        ChildModelState cms;
        cms.name = n;
        cms.state = st;
        cms.xoffset = ox;
        cms.yoffset = oy;
        cms.width = w;
        cms.height = h;
        children.push_back(cms);
    }
    bool hasChildren = !children.empty();
    if (hadChildren && !hasChildren) {
        PixelOverlayManager::INSTANCE.modelStateChanged(this, PixelOverlayState::Enabled, state);
    } else if (!hadChildren && hasChildren) {
        PixelOverlayManager::INSTANCE.modelStateChanged(this, state, PixelOverlayState::Enabled);
    }
}

bool PixelOverlayModel::flushChildren(uint8_t* dst) {
    if (children.empty()) {
        return false;
    }
    for (auto& c : children) {
        int cst = c.state.getState();
        for (int y = 0; y < c.height; y++) {
            int yoff = (y + c.yoffset) * width * 3;
            for (int x = 0; x < c.width; x++) {
                int offr = (x + c.xoffset) * 3 + yoff;
                int offg = offr + 1;
                int offb = offr + 2;

                switch (cst) {
                case 1:
                    if (channelMap[offr] != FPPD_OFF_CHANNEL) {
                        dst[channelMap[offr]] = channelData[channelMap[offr]];
                    }
                    if (channelMap[offg] != FPPD_OFF_CHANNEL) {
                        dst[channelMap[offg]] = channelData[channelMap[offg]];
                    }
                    if (channelMap[offb] != FPPD_OFF_CHANNEL) {
                        dst[channelMap[offb]] = channelData[channelMap[offb]];
                    }
                    break;
                case 2:
                    if (channelMap[offr] != FPPD_OFF_CHANNEL && channelData[channelMap[offr]]) {
                        dst[channelMap[offr]] = channelData[channelMap[offr]];
                    }
                    if (channelMap[offg] != FPPD_OFF_CHANNEL && channelData[channelMap[offg]]) {
                        dst[channelMap[offg]] = channelData[channelMap[offg]];
                    }
                    if (channelMap[offb] != FPPD_OFF_CHANNEL && channelData[channelMap[offb]]) {
                        dst[channelMap[offb]] = channelData[channelMap[offb]];
                    }
                    break;
                case 3:
                    bool cp = channelMap[offr] != FPPD_OFF_CHANNEL && channelData[channelMap[offr]];
                    cp |= channelMap[offg] != FPPD_OFF_CHANNEL && channelData[channelMap[offg]];
                    cp |= channelMap[offb] != FPPD_OFF_CHANNEL && channelData[channelMap[offb]];
                    if (cp) {
                        if (channelMap[offr] != FPPD_OFF_CHANNEL && channelData[channelMap[offr]]) {
                            dst[channelMap[offr]] = channelData[offr];
                        }
                        if (channelMap[offg] != FPPD_OFF_CHANNEL && channelData[offg]) {
                            dst[channelMap[offg]] = channelData[offg];
                        }
                        if (channelMap[offb] != FPPD_OFF_CHANNEL && channelData[offb]) {
                            dst[channelMap[offb]] = channelData[offb];
                        }
                    }
                    break;
                }
            }
        }
    }
    return true;
}

void PixelOverlayModel::doOverlay(uint8_t* channels) {
    int st = state.getState();
    uint8_t* dst = &channels[startChannel];
    if (st == 0 && !children.empty()) {
        //this model is disable, but we have children that are
        //enabled.  Thus, we need to apply their blending
        //to the channel data.
        flushChildren(dst);
        dirtyBuffer = false;
        return;
    }
    if (((st == 2) || (st == 3)) &&
        (!IsEffectRunning()) &&
        (!sequence->IsSequenceRunning()) &&
        !PluginManager::INSTANCE.hasPlugins()) {
        //there is nothing running that we would be overlaying so do a straight copy
        st = 1;
    }

    uint8_t* src = channelData;
    switch (st) {
    case 1: //Active - Opaque
        memcpy(dst, src, channelCount);
        break;
    case 2: //Active Transparent
        for (int j = 0; j < channelCount; j++, src++, dst++) {
            if (*src) {
                *dst = *src;
            }
        }
        break;
    case 3: //Active Transparent RGB {
        for (int j = 0; j < channelCount; j += 3, src += 3, dst += 3) {
            if (src[0] || src[1] || src[2]) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
            }
        }
        break;
    }

    dirtyBuffer = false;
}

void PixelOverlayModel::setData(const uint8_t* data) {
    for (int c = 0; c < (width * height * 3); c++) {
        if (channelMap[c] != FPPD_OFF_CHANNEL) {
            channelData[channelMap[c]] = data[c];
        }
    }
    dirtyBuffer = true;
}

void PixelOverlayModel::setData(const uint8_t* data, int xOffset, int yOffset, int w, int h, const PixelOverlayState& st) {
    if (st.getState() == 0) {
        return;
    }
    if (((w + xOffset) > width) ||
        ((h + yOffset) > height) ||
        (xOffset < 0) ||
        (yOffset < 0)) {
        LogErr(VB_CHANNELOUT, "Error: region is out of bounds, model is %dx%d, region is %dx%d @ %d,%d\n",
               width, height, w, h, xOffset, yOffset);
        return;
    }

    if ((w == width) &&
        (h == height) &&
        (xOffset == 0) &&
        (yOffset == 0) &&
        (st.getState() == 1)) {
        setData(data);
        return;
    }

    int cst = st.getState();
    int rowWrap = (width - w) * 3;
    int s = 0;
    int c = (yOffset * width * 3) + (xOffset * 3);
    for (int yPos = 0; yPos < h; yPos++) {
        for (int xPos = 0; xPos < w; xPos++) {
            if (cst == 1) {
                for (int i = 0; i < 3; i++, c++, s++) {
                    if (channelMap[c] != FPPD_OFF_CHANNEL) {
                        channelData[channelMap[c]] = data[s];
                    }
                }
            } else if (cst == 2) {
                for (int i = 0; i < 3; i++, c++, s++) {
                    if (data[s] && channelMap[c] != FPPD_OFF_CHANNEL) {
                        channelData[channelMap[c]] = data[s];
                    }
                }
            } else if (cst == 3) {
                bool b = data[s] | data[s + 1] | data[s + 2];
                if (b) {
                    for (int i = 0; i < 3; i++, c++, s++) {
                        if (channelMap[c] != FPPD_OFF_CHANNEL) {
                            channelData[channelMap[c]] = data[s];
                        }
                    }
                } else {
                    s += 3;
                    c += 3;
                }
            }
        }
        c += rowWrap;
    }
    dirtyBuffer = true;
}

void PixelOverlayModel::setValue(uint8_t value, int startChannel, int endChannel) {
    int start;
    int end;

    start = startChannel - this->startChannel;
    if (startChannel == -1) {
        start = 0;
    }
    if (start > channelCount) {
        return;
    }

    end = endChannel - startChannel + 1;
    if (end >= channelCount) {
        end = channelCount - 1;
    }
    for (int c = start; c <= end; c++) {
        if (channelMap[c] != FPPD_OFF_CHANNEL) {
            channelData[channelMap[c]] = value;
        }
    }
    dirtyBuffer = true;
}
void PixelOverlayModel::setPixelValue(int x, int y, int r, int g, int b) {
    int c = (y * width * 3) + x * 3;
    if (channelMap[c] != FPPD_OFF_CHANNEL) {
        channelData[channelMap[c++]] = r;
    }
    if (channelsPerNode > 1 && channelMap[c] != FPPD_OFF_CHANNEL) {
        channelData[channelMap[c++]] = g;
    }
    if (channelsPerNode > 2 && channelMap[c] != FPPD_OFF_CHANNEL) {
        channelData[channelMap[c++]] = b;
    }
    dirtyBuffer = true;
}
void PixelOverlayModel::getPixelValue(int x, int y, int& r, int& g, int& b) {
    int c = (y * width * 3) + x * 3;
    if (channelMap[c] != FPPD_OFF_CHANNEL) {
        r = channelData[channelMap[c++]];
    }
    if (channelsPerNode > 1 && channelMap[c] != FPPD_OFF_CHANNEL) {
        g = channelData[channelMap[c++]];
    }
    if (channelsPerNode > 2 && channelMap[c] != FPPD_OFF_CHANNEL) {
        b = channelData[channelMap[c++]];
    }
}

void PixelOverlayModel::setScaledData(uint8_t* data, int w, int h) {
    float ydiff = (float)h / (float)height;
    float xdiff = (float)w / (float)width;

    float newy = 0.0f;
    float newx = 0.0f;
    int x, y;

    for (y = 0, newy = 0.0f; y < height; y++, newy += ydiff) {
        int srcY = newy;
        for (x = 0, newx = 0.0f; x < width; x++, newx += xdiff) {
            int srcX = newx;

            int idx = y * width * 3 + x * 3;
            int srcidx = srcY * w * 3 + srcX * 3;

            if (channelMap[idx] != FPPD_OFF_CHANNEL) {
                channelData[channelMap[idx++]] = data[srcidx++];
            }
            if (channelsPerNode > 1 && channelMap[idx] != FPPD_OFF_CHANNEL) {
                channelData[channelMap[idx++]] = data[srcidx++];
            }
            if (channelsPerNode > 2 && channelMap[idx] != FPPD_OFF_CHANNEL) {
                channelData[channelMap[idx++]] = data[srcidx++];
            }
        }
    }
    dirtyBuffer = true;
}
void PixelOverlayModel::clearData() {
    memset(channelData, 0, channelCount);
    dirtyBuffer = true;
}
void PixelOverlayModel::fillData(int r, int g, int b) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            setPixelValue(x, y, r, g, b);
        }
    }
}

void PixelOverlayModel::getDataJson(Json::Value& v, bool rle) {
    if (rle) {
        unsigned char r = 0;
        unsigned char g = 0;
        unsigned char b = 0;
        unsigned char lr = 0;
        unsigned char lg = 0;
        unsigned char lb = 0;
        int count = 0;
        for (int c = 0; c < height * width * 3; c += 3) {
            if (channelMap[c] != FPPD_OFF_CHANNEL) {
                r = channelData[channelMap[c]];
            }
            if (channelMap[c + 1] != FPPD_OFF_CHANNEL) {
                g = channelData[channelMap[c + 1]];
            }
            if (channelMap[c + 2] != FPPD_OFF_CHANNEL) {
                b = channelData[channelMap[c + 2]];
            }

            if ((r == lr) && (g == lg) && (b == lb)) {
                count++;
            } else {
                if (count) {
                    v.append(count);
                    v.append(lr);
                    v.append(lg);
                    v.append(lb);
                }

                count = 1;
                lr = r;
                lg = g;
                lb = b;
            }
        }

        if (count) {
            v.append(count);
            v.append(r);
            v.append(g);
            v.append(b);
        }
    } else {
        for (int c = 0; c < height * width * 3; c++) {
            unsigned char i = 0;
            if (channelMap[c] != FPPD_OFF_CHANNEL) {
                i = channelData[channelMap[c]];
            }
            v.append(i);
        }
    }
}

bool PixelOverlayModel::needRefresh() {
    return (dirtyBuffer || overlayBufferIsDirty());
}

void PixelOverlayModel::setBufferIsDirty(bool dirty) {
    dirtyBuffer = dirty;
}

uint8_t* PixelOverlayModel::getOverlayBuffer() {
    if (!overlayBufferData) {
        std::string overlayBufferName = "/FPP-Model-Overlay-Buffer-" + name;
        if (PSHMNAMLEN <= 48) {
            // system doesn't allow very long shared memory names, we'll use a shortened form
            overlayBufferName = "/FPPMB-" + name;
            if (overlayBufferName.size() > PSHMNAMLEN) {
                overlayBufferName = overlayBufferName.substr(0, PSHMNAMLEN);
            }
        }

        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
        int f = shm_open(overlayBufferName.c_str(), O_RDWR | O_CREAT, mode);
        int size = width * height * 3 + sizeof(OverlayBufferData);
        ftruncate(f, size);
        overlayBufferData = (OverlayBufferData*)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, f, 0);
        memset(overlayBufferData, 0, size);
        overlayBufferData->width = width;
        overlayBufferData->height = height;
        close(f);
    }
    return overlayBufferData->data;
}

void PixelOverlayModel::clearOverlayBuffer() {
    memset(getOverlayBuffer(), 0, width * height * 3);
}
void PixelOverlayModel::fillOverlayBuffer(int r, int g, int b) {
    uint8_t* data = getOverlayBuffer();
    for (int w = 0; w < (width * height); w++) {
        data[0] = r;
        data[1] = g;
        data[2] = b;
        data += 3;
    }
}

void PixelOverlayModel::setOverlayPixelValue(int x, int y, int r, int g, int b) {
    if (y >= height || x >= width || x < 0 || y < 0) {
        return;
    }
    int idx = y * width * 3 + x * 3;
    uint8_t* buf = getOverlayBuffer();
    buf[idx++] = r;
    buf[idx++] = g;
    buf[idx] = b;
}
void PixelOverlayModel::getOverlayPixelValue(int x, int y, int& r, int& g, int& b) {
    if (y >= height || x >= width || x < 0 || y < 0) {
        r = g = b = 0;
        return;
    }
    int idx = y * width * 3 + x * 3;
    uint8_t* buf = getOverlayBuffer();
    r = buf[idx++];
    g = buf[idx++];
    b = buf[idx];
}

void PixelOverlayModel::flushOverlayBuffer() {
    setData(getOverlayBuffer());
    setOverlayBufferDirty(false);
}

bool PixelOverlayModel::overlayBufferIsDirty() {
    return (overlayBufferData && (overlayBufferData->flags & 0x1));
}

void PixelOverlayModel::setOverlayBufferDirty(bool dirty) {
    getOverlayBuffer();

    if (dirty)
        overlayBufferData->flags |= 0x1;
    else
        overlayBufferData->flags &= ~0x1;
}

void PixelOverlayModel::setOverlayBufferScaledData(uint8_t* data, int w, int h) {
    float ydiff = (float)h / (float)height;
    float xdiff = (float)w / (float)width;
    uint8_t* buf = getOverlayBuffer();

    float newy = 0.0f;
    float newx = 0.0f;
    int x, y;

    for (y = 0, newy = 0.0f; y < height; y++, newy += ydiff) {
        int srcY = newy;
        for (x = 0, newx = 0.0f; x < width; x++, newx += xdiff) {
            int srcX = newx;

            int idx = y * width * 3 + x * 3;
            int srcidx = srcY * w * 3 + srcX * 3;

            buf[idx++] = data[srcidx++];
            buf[idx++] = data[srcidx++];
            buf[idx] = data[srcidx];
        }
    }
}

void PixelOverlayModel::saveOverlayAsImage(std::string filename) {
    Magick::Blob blob;
    Magick::Image image(Magick::Geometry(width, height), Magick::Color("black"));

    if (filename == "") {
        struct timeval tv;
        struct tm tm;
        char timeStr[32];

        gettimeofday(&tv, nullptr);
        localtime_r(&tv.tv_sec, &tm);

        snprintf(timeStr, sizeof(timeStr),
                 "%4d%02d%02d_%02d%02d%02d",
                 1900 + tm.tm_year, tm.tm_mon + 1,
                 tm.tm_mday, tm.tm_hour, tm.tm_min,
                 tm.tm_sec);

        filename = name + "-" + timeStr + ".png";
    }

    filename = FPP_DIR_IMAGE("/") + filename;

    LogDebug(VB_CHANNELOUT, "Saving %dx%d PixelOverlayModel image to: %s\n",
             width, height, filename.c_str());

    uint8_t data[width * height * 3];
    memset(data, 0, width * height * 3);
    for (int c = 0; c < height * width * 3; c++) {
        if (channelMap[c] != FPPD_OFF_CHANNEL) {
            data[c] = channelData[channelMap[c]];
        }
    }
    blob.update(data, width * height * 3);

    image.magick("RGB");
    image.depth(8);
    image.quiet(true);
    image.read(blob, Magick::Geometry(width, height));

    image.write(filename.c_str());

    SetFilePerms(filename);
}

void PixelOverlayModel::clear() {
    if (overlayBufferData) {
        clearOverlayBuffer();
        flushOverlayBuffer();
    } else {
        clearData();
    }
}
void PixelOverlayModel::fill(int r, int g, int b) {
    if (overlayBufferData) {
        fillOverlayBuffer(r, g, b);
        flushOverlayBuffer();
    } else {
        fillData(r, g, b);
    }
}

int PixelOverlayModel::getStartChannel() const {
    return startChannel;
}
int PixelOverlayModel::getChannelCount() const {
    return channelCount;
}
void PixelOverlayModel::toJson(Json::Value& result) {
    result = config;
}

int32_t PixelOverlayModel::updateRunningEffects() {
    std::unique_lock<std::mutex> l(effectLock);
    if (runningEffect) {
        int32_t v = runningEffect->update();
        if (v == 0) {
            delete runningEffect;
            runningEffect = nullptr;
        }
        return v;
    }
    return 0;
}

void PixelOverlayModel::setRunningEffect(RunningEffect* ef, int32_t firstUpdateMS) {
    std::unique_lock<std::mutex> l(effectLock);
    if (runningEffect) {
        if (runningEffect != ef) {
            delete runningEffect;
        }
        PixelOverlayManager::INSTANCE.removePeriodicUpdate(this);
    }
    runningEffect = ef;
    PixelOverlayManager::INSTANCE.addPeriodicUpdate(firstUpdateMS, this);
}

bool PixelOverlayModel::applyEffect(const std::string& autoState, const std::string& effect, const std::vector<std::string>& args) {
    PixelOverlayEffect* pe = PixelOverlayEffect::GetPixelOverlayEffect(effect);
    if (pe) {
        return pe->apply(this, autoState, args);
    }
    return false;
}
