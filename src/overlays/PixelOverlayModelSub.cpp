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

#include "fpp-json.h"

#include <cstring>

#include "../Sequence.h" // FPPD_OFF_CHANNEL
#include "../log.h"
#include "../common_mini.h"

#include "PixelOverlay.h"
#include "PixelOverlayModelSub.h"

PixelOverlayModelSub::PixelOverlayModelSub(const Json::Value& c) :
    PixelOverlayModel(c) {
    if (config.isMember("XOffset")) {
        xOffset = config["XOffset"].asInt();
    }

    if (config.isMember("YOffset")) {
        yOffset = config["YOffset"].asInt();
    }

    if (config.isMember("SubType")) {
        std::string subType = config["SubType"].asString();
        if (subType == "grid") {
            gridMode = true;
            buildGrid();
        } else if (subType == "channelgrid") {
            gridMode = true;
            channelGridMode = true;
            buildChannelGrid();
        }
    }
}

// Channel-grid (model group) build: each grid cell is a '&'-separated list of
// 1-based absolute start channels (the member pixels binned into that cell). We
// index outputCells by the channelData slot the base assigned to each cell, so
// the scatter reads the right buffer color.
void PixelOverlayModelSub::buildChannelGrid() {
    const int cpn = channelsPerNode;
    outputCells.assign((size_t)width * height, std::vector<uint32_t>());

    std::string grid = config.isMember("Grid") ? config["Grid"].asString() : "";
    std::vector<std::string> rows = split(grid, ';');

    for (int y = 0; y < height; y++) {
        std::vector<std::string> cells;
        if (y < (int)rows.size()) {
            cells = split(rows[y], ',');
        }
        for (int x = 0; x < width; x++) {
            if (x >= (int)cells.size() || cells[x].empty()) {
                continue; // hole
            }
            int buf = (y * width + x) * bytesPerPixel;
            if (buf < 0 || buf >= (int)channelMap.size()) {
                continue;
            }
            uint32_t cd = channelMap[buf];
            if (cd == FPPD_OFF_CHANNEL) {
                continue;
            }
            int slot = cd / cpn;
            if (slot < 0 || slot >= (int)outputCells.size()) {
                continue;
            }
            for (auto& chStr : split(cells[x], '&')) {
                if (chStr.empty()) {
                    continue;
                }
                int ch1 = std::atoi(chStr.c_str()); // 1-based absolute start channel
                if (ch1 > 0) {
                    outputCells[slot].push_back((uint32_t)(ch1 - 1));
                }
            }
        }
    }
}

// Build outputMap: for each populated grid cell, resolve the parent node number
// to the absolute (0-based) output channel base.  The base class has already
// built a dense width*height buffer + channelMap, so we index outputMap by the
// channelData slot the base assigned to each cell, making this robust to the
// base orientation mapping.
void PixelOverlayModelSub::buildGrid() {
    int parentStart0 = 0;
    if (config.isMember("ParentStartChannel")) {
        parentStart0 = config["ParentStartChannel"].asInt() - 1;
    }
    if (parentStart0 < 0) {
        parentStart0 = 0;
    }
    const int cpn = channelsPerNode;

    outputMap.assign((size_t)width * height, FPPD_OFF_CHANNEL);

    std::string grid = config.isMember("Grid") ? config["Grid"].asString() : "";
    std::vector<std::string> rows = split(grid, ';');

    for (int y = 0; y < height; y++) {
        std::vector<std::string> cells;
        if (y < (int)rows.size()) {
            cells = split(rows[y], ',');
        }
        for (int x = 0; x < width; x++) {
            int node = -1;
            if (x < (int)cells.size() && !cells[x].empty()) {
                node = std::atoi(cells[x].c_str());
            }
            if (node <= 0) {
                continue; // hole
            }
            int buf = (y * width + x) * bytesPerPixel;
            if (buf < 0 || buf >= (int)channelMap.size()) {
                continue;
            }
            uint32_t cd = channelMap[buf];
            if (cd == FPPD_OFF_CHANNEL) {
                continue;
            }
            int slot = cd / cpn;
            if (slot >= 0 && slot < (int)outputMap.size()) {
                outputMap[slot] = (uint32_t)(parentStart0 + (node - 1) * cpn);
            }
        }
    }
}

PixelOverlayModelSub::~PixelOverlayModelSub() {
}

bool PixelOverlayModelSub::foundParent() {
    if (parent)
        return true;

    parent = PixelOverlayManager::INSTANCE.getModel(config["Parent"].asString());

    if (parent) {
        return true;
    }

    LogErr(VB_CHANNELOUT, "Unable to find parent\n");
    return false;
}
void PixelOverlayModelSub::setState(const PixelOverlayState& st) {
    PixelOverlayModel::setState(st);
    if (gridMode) {
        // Grid submodels scatter directly to the parent's output channels; they
        // do not register as a rectangular child of the parent.
        return;
    }
    if (foundParent()) {
        parent->setChildState(name, st, xOffset, yOffset, width, height);
    }
}
void PixelOverlayModelSub::resetParent() {
    parent = nullptr;
    PixelOverlayModel::setState(PixelOverlayState(PixelOverlayState::Disabled));
}

void PixelOverlayModelSub::doOverlay(uint8_t* channels) {
    if (PixelOverlayModel::overlayBufferIsDirty())
        flushOverlayBuffer();

    if (gridMode) {
        doGridOverlay(channels);
        return;
    }

    if (!foundParent())
        return;

    dirtyBuffer = dirtyBuffer | parent->needRefresh();
    if (!dirtyBuffer)
        return;

    parent->setData(channelData, xOffset, yOffset, width, height, state);
    parent->doOverlay(channels);
    dirtyBuffer = false;
}

// Scatter each populated grid cell's color from channelData (dense, indexed by
// the base class buffer slot) to the parent's absolute output channel, honoring
// the overlay state.  Unlike a normal model's doOverlay() this never does a
// bulk copy, so sibling pixels that are not part of the submodel are untouched.
// Scatter one node's color (src, cpn bytes) to one output channel base (dst),
// honoring the overlay state.
static inline void scatterNode(uint8_t* dst, const uint8_t* src, int cpn, int st) {
    switch (st) {
    case 1: // Opaque
        memcpy(dst, src, cpn);
        break;
    case 2: // Transparent (per-channel)
        for (int k = 0; k < cpn; k++) {
            if (src[k]) {
                dst[k] = src[k];
            }
        }
        break;
    case 3: { // Transparent RGB(W) (per-node)
        bool any = false;
        for (int k = 0; k < cpn; k++) {
            any |= src[k];
        }
        if (any) {
            for (int k = 0; k < cpn; k++) {
                dst[k] = src[k];
            }
        }
        break;
    }
    }
}

void PixelOverlayModelSub::doGridOverlay(uint8_t* channels) {
    int st = state.getState();
    if (st == 0) {
        dirtyBuffer = false;
        return;
    }
    const int cpn = channelsPerNode;

    if (channelGridMode) {
        // Model group: each cell may map to multiple member channels.
        const int slots = (int)outputCells.size();
        for (int slot = 0; slot < slots; slot++) {
            const std::vector<uint32_t>& chans = outputCells[slot];
            if (chans.empty()) {
                continue;
            }
            uint8_t* src = &channelData[slot * cpn];
            for (uint32_t ob : chans) {
                scatterNode(&channels[ob], src, cpn, st);
            }
        }
        dirtyBuffer = false;
        return;
    }

    const int slots = (int)outputMap.size();
    for (int slot = 0; slot < slots; slot++) {
        uint32_t ob = outputMap[slot];
        if (ob == FPPD_OFF_CHANNEL) {
            continue;
        }
        scatterNode(&channels[ob], &channelData[slot * cpn], cpn, st);
    }
    dirtyBuffer = false;
}

void PixelOverlayModelSub::setData(const uint8_t* data) {
    // Don't call foundParent() here to avoid potential deadlock
    // when called from flushOverlayBuffer() while holding runningEffectMutex.
    // The parent will be resolved in doOverlay() before data is needed.
    PixelOverlayModel::setData(data);

    dirtyBuffer = true;
}
