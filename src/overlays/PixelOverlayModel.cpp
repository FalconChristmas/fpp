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
        LogWarn(VB_CHANNELOUT, "Could not create shared memory block for %s:  %s\n", dataName.c_str(), FPPstrerror(errno));
        // we couldn't create the shared memory block.  Most of the time,
        // we are fine with non-shared memory and this at least prevents a crash
        flags = MAP_ANON;
    } else {
        int rc = ftruncate(f, size);
        if (rc == -1) {
            // if ftruncate fails, we need to completely reset
            close(f);
            shm_unlink(dataName.c_str());
            f = shm_open(dataName.c_str(), O_RDWR | O_CREAT, mode);
            ftruncate(f, size);
        }
    }
    if (size < 8) {
        // cannot mmap 0 size which some models happen to have
        size = 8;
    }
    uint8_t* channelData = (uint8_t*)mmap(0, size, PROT_READ | PROT_WRITE, flags, f, 0);
    if (channelData == MAP_FAILED) {
        // mmap is failing, but we need channelData so just do malloc
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
    startChannel--; // need to be 0 based
    channelCount = config["ChannelCount"].asInt();
    int strings = config["StringCount"].asInt();
    int sps = config["StrandsPerString"].asInt();
    if (sps == 0) {
        sps = 1;
    }

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
    bytesPerPixel = (channelsPerNode >= 4) ? 4 : 3;

    if (config["Type"].asString() != "Channel") {
        if (config.isMember("PixelSize") && config["PixelSize"].asInt() > 1) {            
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
        channelMap.resize(width * height * bytesPerPixel);

        std::swap(width, height);
        for (int x = 0; x < width; x++) {
            int segment = x % sps;
            for (int y = 0; y < height; y++) {
                // pos in the normal overlay buffer
                int ppos = y * width + x;
                // Relative Input Pixel first channel
                int inCh = (ppos * bytesPerPixel);

                // X position in output
                int outX = (LtoR) ? x : width - x - 1;
                // Y position in output
                int outY = (TtoB == (segment % 2)) ? height - y - 1 : y;

                // Relative Mapped Output Pixel first channel
                int mpos = outX * height + outY;
                int outCh = (mpos * channelsPerNode);

                for (int cho = 0; cho < bytesPerPixel; cho++) {
                    if (cho < channelsPerNode) {
                        channelMap[inCh + cho] = outCh + cho;
                    } else {
                        channelMap[inCh + cho] = FPPD_OFF_CHANNEL;
                    }
                }
            }
        }
    } else if (orientation == "C" || orientation == "custom") {
        // Two formats supported:
        //   "data": dense format "n,n,n;n,n,n|n,n,n;n,n,n" (comma=col, semi=row, pipe=layer)
        //   "compressedData": sparse format "node,row,col[,layer];..." (only non-empty cells)
        if (config.isMember("compressedData") && !config["compressedData"].asString().empty()) {
            // Parse compressed format: "node,row,col[,layer];node,row,col[,layer];..."
            std::string compressed = config["compressedData"].asString();
            std::vector<std::string> entries = split(compressed, ';');

            struct NodeEntry {
                int node, row, col, layer;
            };
            std::vector<NodeEntry> nodes;
            nodes.reserve(entries.size());

            int maxRow = 0, maxCol = 0, maxLayer = 0;
            for (auto& entry : entries) {
                std::vector<std::string> parts = split(entry, ',');
                if (parts.size() >= 3) {
                    NodeEntry ne;
                    ne.node = std::atoi(parts[0].c_str());
                    ne.row = std::atoi(parts[1].c_str());
                    ne.col = std::atoi(parts[2].c_str());
                    ne.layer = (parts.size() >= 4) ? std::atoi(parts[3].c_str()) : 0;
                    if (ne.node > 0 && ne.node <= FPPD_MAX_CHANNEL_NUM) {
                        nodes.push_back(ne);
                        maxRow = std::max(maxRow, ne.row);
                        maxCol = std::max(maxCol, ne.col);
                        maxLayer = std::max(maxLayer, ne.layer);
                    }
                }
            }

            width = maxCol + 1;
            height = (maxRow + 1) * (maxLayer + 1);
            if (width < 1) width = 1;
            if (height < 1) height = 1;
            if ((width * height) > (600 * 600)) {
                // Limit to first layer only
                height = maxRow + 1;
                if (height < 1) height = 1;
            }

            channelMap.resize(width * height * bytesPerPixel);
            // Initialize all to FPPD_OFF_CHANNEL
            std::fill(channelMap.begin(), channelMap.end(), FPPD_OFF_CHANNEL);

            int layerHeight = maxRow + 1;
            for (auto& ne : nodes) {
                int y = ne.row + ne.layer * layerHeight;
                if (y >= height || ne.col >= width) {
                    continue;
                }
                for (int cho = 0; cho < bytesPerPixel; cho++) {
                    if (cho < channelsPerNode) {
                        channelMap[(y * width * bytesPerPixel) + (ne.col * bytesPerPixel) + cho] = (ne.node - 1) * channelsPerNode + cho;
                    }
                }
            }
        } else {
            // Parse dense format: "n,n,n;n,n,n|n,n,n;n,n,n"
            std::string customData = ",;,";
            if (config.isMember("data")) {
                customData = config["data"].asString();
            }
            int endOfFirstLayer = 0;
            std::vector<std::string> layers = split(customData, '|');
            std::vector<std::vector<std::string>> allData;
            width = 1;
            for (auto& layer : layers) {
                std::vector<std::string> lines = split(layer, ';');
                for (auto& l : lines) {
                    allData.push_back(split(l, ','));
                    width = std::max(width, (int)allData.back().size());
                    height++;
                }
                if (endOfFirstLayer == 0) {
                    endOfFirstLayer = allData.size();
                }
                lines.clear();
                if ((width * height) > (600 * 600)) {
                    height = endOfFirstLayer;
                    allData.erase(allData.begin() + endOfFirstLayer, allData.end());
                    break;
                }
            }
            channelMap.resize(width * height * bytesPerPixel);
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    int node = -1;
                    if (y < allData.size() && x < allData[y].size()) {
                        std::string& s = allData[y][x];
                        if (s.empty() || s.length() > 10 || s == " ") {
                            node = -1;
                        } else {
                            node = std::atoi(s.c_str());
                            if (node == 0 && s != "0") {
                                node = -1;
                            }
                            if (node > FPPD_MAX_CHANNEL_NUM) {
                                node = -1;
                            }
                        }
                    }

                    for (int cho = 0; cho < bytesPerPixel; cho++) {
                        if (node != -1 && cho < channelsPerNode) {
                            channelMap[(y * width * bytesPerPixel) + (x * bytesPerPixel) + cho] = (node - 1) * channelsPerNode + cho;
                        } else {
                            channelMap[(y * width * bytesPerPixel) + (x * bytesPerPixel) + cho] = FPPD_OFF_CHANNEL;
                        }
                    }
                }
            }
        }
    } else {
        channelMap.resize(width * height * bytesPerPixel);
        for (int y = 0; y < height; y++) {
            int segment = y % sps;
            for (int x = 0; x < width; x++) {
                // pos in the normal overlay buffer
                int ppos = y * width + x;
                // Input Pixel first channel
                int inCh = (ppos * bytesPerPixel);

                // X position in output
                int outX = (LtoR == (segment % 2)) ? width - x - 1 : x;
                // Y position in output
                int outY = (TtoB) ? y : height - y - 1;

                // Relative Mapped Output Pixel first channel
                int mpos = outY * width + outX;
                int outCh = (mpos * channelsPerNode);

                for (int cho = 0; cho < bytesPerPixel; cho++) {
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
        munmap(overlayBufferData, width * height * bytesPerPixel + sizeof(OverlayBufferData));
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
    for (auto& c : children) {
        PixelOverlayManager::INSTANCE.resetChildParent(c.name);
    }
}

bool PixelOverlayModel::isAutoCreated() {
    return config.isMember("autoCreated") && config["autoCreated"].asBool();
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
                it = children.erase(it);
            } else {
                it->state = st;
                it->xoffset = ox;
                it->yoffset = oy;
                it->width = w;
                it->height = h;
                ++it;
            }
        } else {
            ++it;
        }
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
            int yoff = (y + c.yoffset) * width * bytesPerPixel;
            for (int x = 0; x < c.width; x++) {
                int off0 = (x + c.xoffset) * bytesPerPixel + yoff;

                switch (cst) {
                case 1: // Opaque
                    for (int ch = 0; ch < bytesPerPixel; ch++) {
                        if (channelMap[off0 + ch] != FPPD_OFF_CHANNEL) {
                            dst[channelMap[off0 + ch]] = channelData[channelMap[off0 + ch]];
                        }
                    }
                    break;
                case 2: // Transparent
                    for (int ch = 0; ch < bytesPerPixel; ch++) {
                        if (channelMap[off0 + ch] != FPPD_OFF_CHANNEL && channelData[channelMap[off0 + ch]]) {
                            dst[channelMap[off0 + ch]] = channelData[channelMap[off0 + ch]];
                        }
                    }
                    break;
                case 3: { // TransparentRGB
                    bool cp = false;
                    for (int ch = 0; ch < bytesPerPixel; ch++) {
                        cp |= (channelMap[off0 + ch] != FPPD_OFF_CHANNEL && channelData[channelMap[off0 + ch]]);
                    }
                    if (cp) {
                        for (int ch = 0; ch < bytesPerPixel; ch++) {
                            if (channelMap[off0 + ch] != FPPD_OFF_CHANNEL && channelData[channelMap[off0 + ch]]) {
                                dst[channelMap[off0 + ch]] = channelData[channelMap[off0 + ch]];
                            }
                        }
                    }
                    break;
                }
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
        // this model is disable, but we have children that are
        // enabled.  Thus, we need to apply their blending
        // to the channel data.
        flushChildren(dst);
        dirtyBuffer = false;
        return;
    }
    if (((st == 2) || (st == 3)) &&
        (!IsEffectRunning()) &&
        (!sequence->IsSequenceRunning()) &&
        !PluginManager::INSTANCE.hasPlugins()) {
        // there is nothing running that we would be overlaying so do a straight copy
        st = 1;
    }

    uint8_t* src = channelData;
    switch (st) {
    case 1: // Active - Opaque
        memcpy(dst, src, channelCount);
        break;
    case 2: // Active Transparent
        for (int j = 0; j < channelCount; j++, src++, dst++) {
            if (*src) {
                *dst = *src;
            }
        }
        break;
    case 3: // Active Transparent RGB(W)
        for (int j = 0; j < channelCount; j += channelsPerNode, src += channelsPerNode, dst += channelsPerNode) {
            bool any = false;
            for (int k = 0; k < channelsPerNode; k++) {
                any |= src[k];
            }
            if (any) {
                for (int k = 0; k < channelsPerNode; k++) {
                    dst[k] = src[k];
                }
            }
        }
        break;
    }

    dirtyBuffer = false;
}

void PixelOverlayModel::setData(const uint8_t* data) {
    for (int c = 0; c < (width * height * bytesPerPixel); c++) {
        if (channelMap[c] != FPPD_OFF_CHANNEL) {
            channelData[channelMap[c]] = data[c];
        }
    }
    dirtyBuffer = true;
}

void PixelOverlayModel::setData(const uint8_t* data, int xOffset, int yOffset, int w, int h, const PixelOverlayState& st) {
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
    int rowWrap = (width - w) * bytesPerPixel;
    int s = 0;
    int c = (yOffset * width * bytesPerPixel) + (xOffset * bytesPerPixel);
    for (int yPos = 0; yPos < h; yPos++) {
        for (int xPos = 0; xPos < w; xPos++) {
            if (cst == 1 || cst == 0) {
                for (int i = 0; i < bytesPerPixel; i++, c++, s++) {
                    if (channelMap[c] != FPPD_OFF_CHANNEL) {
                        channelData[channelMap[c]] = data[s];
                    }
                }
            } else if (cst == 2) {
                for (int i = 0; i < bytesPerPixel; i++, c++, s++) {
                    if (data[s] && channelMap[c] != FPPD_OFF_CHANNEL) {
                        channelData[channelMap[c]] = data[s];
                    }
                }
            } else if (cst == 3) {
                bool b = false;
                for (int i = 0; i < bytesPerPixel; i++) {
                    b |= data[s + i];
                }
                if (b) {
                    for (int i = 0; i < bytesPerPixel; i++, c++, s++) {
                        if (channelMap[c] != FPPD_OFF_CHANNEL) {
                            channelData[channelMap[c]] = data[s];
                        }
                    }
                } else {
                    s += bytesPerPixel;
                    c += bytesPerPixel;
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
    setPixelValue(x, y, r, g, b, 0);
}
void PixelOverlayModel::setPixelValue(int x, int y, int r, int g, int b, int w) {
    if (y >= height || x >= width || x < 0 || y < 0) {
        return;
    }
    int c = (y * width * bytesPerPixel) + x * bytesPerPixel;
    int mapSize = width * height * bytesPerPixel;
    if (c >= mapSize || c < 0) {
        return;
    }
    if (channelMap[c] != FPPD_OFF_CHANNEL) {
        channelData[channelMap[c]] = r;
    }
    c++;
    if (channelsPerNode > 1 && c < mapSize && channelMap[c] != FPPD_OFF_CHANNEL) {
        channelData[channelMap[c]] = g;
    }
    c++;
    if (channelsPerNode > 2 && c < mapSize && channelMap[c] != FPPD_OFF_CHANNEL) {
        channelData[channelMap[c]] = b;
    }
    c++;
    if (channelsPerNode > 3 && c < mapSize && channelMap[c] != FPPD_OFF_CHANNEL) {
        channelData[channelMap[c]] = w;
    }
    dirtyBuffer = true;
}
void PixelOverlayModel::getPixelValue(int x, int y, int& r, int& g, int& b) {
    int w = 0;
    getPixelValue(x, y, r, g, b, w);
    if (w) {
        r = std::min(r + w, 255);
        g = std::min(g + w, 255);
        b = std::min(b + w, 255);
    }
}
void PixelOverlayModel::getPixelValue(int x, int y, int& r, int& g, int& b, int& w) {
    if (y >= height || x >= width || x < 0 || y < 0) {
        return;
    }
    int c = (y * width * bytesPerPixel) + x * bytesPerPixel;
    int mapSize = width * height * bytesPerPixel;
    if (c >= mapSize || c < 0) {
        return;
    }
    if (channelMap[c] != FPPD_OFF_CHANNEL) {
        r = channelData[channelMap[c]];
    }
    c++;
    if (channelsPerNode > 1 && c < mapSize && channelMap[c] != FPPD_OFF_CHANNEL) {
        g = channelData[channelMap[c]];
    }
    c++;
    if (channelsPerNode > 2 && c < mapSize && channelMap[c] != FPPD_OFF_CHANNEL) {
        b = channelData[channelMap[c]];
    }
    c++;
    if (channelsPerNode > 3 && c < mapSize && channelMap[c] != FPPD_OFF_CHANNEL) {
        w = channelData[channelMap[c]];
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

            int idx = y * width * bytesPerPixel + x * bytesPerPixel;
            int srcidx = srcY * w * bytesPerPixel + srcX * bytesPerPixel;

            for (int ch = 0; ch < bytesPerPixel; ch++) {
                if (ch < channelsPerNode && channelMap[idx] != FPPD_OFF_CHANNEL) {
                    channelData[channelMap[idx]] = data[srcidx];
                }
                idx++;
                srcidx++;
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
    fillData(r, g, b, 0);
}
void PixelOverlayModel::fillData(int r, int g, int b, int w) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            setPixelValue(x, y, r, g, b, w);
        }
    }
}

void PixelOverlayModel::getDataJson(Json::Value& v, bool rle) {
    int bpp = bytesPerPixel;
    if (rle) {
        std::vector<unsigned char> cur(bpp, 0);
        std::vector<unsigned char> last(bpp, 0);
        int count = 0;
        for (int c = 0; c < height * width * bpp; c += bpp) {
            for (int ch = 0; ch < bpp; ch++) {
                cur[ch] = (channelMap[c + ch] != FPPD_OFF_CHANNEL) ? channelData[channelMap[c + ch]] : 0;
            }

            if (cur == last) {
                count++;
            } else {
                if (count) {
                    v.append(count);
                    for (int ch = 0; ch < bpp; ch++) {
                        v.append(last[ch]);
                    }
                }
                count = 1;
                last = cur;
            }
        }

        if (count) {
            v.append(count);
            for (int ch = 0; ch < bpp; ch++) {
                v.append(cur[ch]);
            }
        }
    } else {
        for (int c = 0; c < height * width * bpp; c++) {
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
        int size = width * height * bytesPerPixel + sizeof(OverlayBufferData);
        int flags = MAP_SHARED;
        if (f == -1) {
            LogWarn(VB_CHANNELOUT, "Could not create shared memory for overlay buffer %s: %s\n", name.c_str(), FPPstrerror(errno));
            flags = MAP_ANON;
        } else {
            ftruncate(f, size);
        }
        overlayBufferData = (OverlayBufferData*)mmap(0, size, PROT_READ | PROT_WRITE, flags, f, 0);
        if (overlayBufferData == MAP_FAILED) {
            LogWarn(VB_CHANNELOUT, "Could not mmap overlay buffer for %s, using malloc\n", name.c_str());
            overlayBufferData = (OverlayBufferData*)malloc(size);
        }
        memset(overlayBufferData, 0, size);
        overlayBufferData->width = width;
        overlayBufferData->height = height;
        overlayBufferData->flags = (bytesPerPixel << 8); // store bytesPerPixel in bits 8-15
        if (f != -1) {
            close(f);
        }
    }
    return overlayBufferData->data;
}

void PixelOverlayModel::clearOverlayBuffer() {
    memset(getOverlayBuffer(), 0, width * height * bytesPerPixel);
}
void PixelOverlayModel::fillOverlayBuffer(int r, int g, int b) {
    fillOverlayBuffer(r, g, b, 0);
}
void PixelOverlayModel::fillOverlayBuffer(int r, int g, int b, int w) {
    uint8_t* data = getOverlayBuffer();
    for (int p = 0; p < (width * height); p++) {
        data[0] = r;
        data[1] = g;
        data[2] = b;
        if (bytesPerPixel > 3) {
            data[3] = w;
        }
        data += bytesPerPixel;
    }
}

void PixelOverlayModel::setOverlayPixelValue(int x, int y, int r, int g, int b) {
    setOverlayPixelValue(x, y, r, g, b, 0);
}
void PixelOverlayModel::setOverlayPixelValue(int x, int y, int r, int g, int b, int w) {
    if (y >= height || x >= width || x < 0 || y < 0) {
        return;
    }
    int idx = y * width * bytesPerPixel + x * bytesPerPixel;
    uint8_t* buf = getOverlayBuffer();
    buf[idx++] = r;
    buf[idx++] = g;
    buf[idx++] = b;
    if (bytesPerPixel > 3) {
        buf[idx] = w;
    }
}
void PixelOverlayModel::getOverlayPixelValue(int x, int y, int& r, int& g, int& b) {
    int w = 0;
    getOverlayPixelValue(x, y, r, g, b, w);
    if (w) {
        r = std::min(r + w, 255);
        g = std::min(g + w, 255);
        b = std::min(b + w, 255);
    }
}
void PixelOverlayModel::getOverlayPixelValue(int x, int y, int& r, int& g, int& b, int& w) {
    if (y >= height || x >= width || x < 0 || y < 0) {
        r = g = b = w = 0;
        return;
    }
    int idx = y * width * bytesPerPixel + x * bytesPerPixel;
    uint8_t* buf = getOverlayBuffer();
    r = buf[idx++];
    g = buf[idx++];
    b = buf[idx++];
    w = (bytesPerPixel > 3) ? buf[idx] : 0;
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

            int idx = y * width * bytesPerPixel + x * bytesPerPixel;
            int srcidx = srcY * w * bytesPerPixel + srcX * bytesPerPixel;

            for (int ch = 0; ch < bytesPerPixel; ch++) {
                buf[idx + ch] = data[srcidx + ch];
            }
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

    // Always save as RGB (3 bytes per pixel) for image compatibility
    // For RGBW models, blend the W channel into RGB so white pixels render correctly
    std::vector<uint8_t> data(width * height * 3, 0);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int mapIdx = (y * width + x) * bytesPerPixel;
            int imgIdx = (y * width + x) * 3;
            for (int ch = 0; ch < 3; ch++) {
                if (ch < bytesPerPixel && channelMap[mapIdx + ch] != FPPD_OFF_CHANNEL) {
                    data[imgIdx + ch] = channelData[channelMap[mapIdx + ch]];
                }
            }
            if (bytesPerPixel > 3 && channelMap[mapIdx + 3] != FPPD_OFF_CHANNEL) {
                uint8_t w = channelData[channelMap[mapIdx + 3]];
                if (w) {
                    for (int ch = 0; ch < 3; ch++) {
                        int v = data[imgIdx + ch] + w;
                        data[imgIdx + ch] = (v > 255) ? 255 : v;
                    }
                }
            }
        }
    }
    blob.update(&data[0], width * height * 3);

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
    fill(r, g, b, 0);
}
void PixelOverlayModel::fill(int r, int g, int b, int w) {
    if (overlayBufferData) {
        fillOverlayBuffer(r, g, b, w);
        flushOverlayBuffer();
    } else {
        fillData(r, g, b, w);
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
    std::unique_lock<std::recursive_mutex> l(effectLock);
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
    std::unique_lock<std::recursive_mutex> l(effectLock);
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
