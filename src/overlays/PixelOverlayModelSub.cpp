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

#include <algorithm>
#include <cstring>

#include "../Sequence.h" // FPPD_OFF_CHANNEL
#include "../log.h"
#include "../common_mini.h"

#include "PixelOverlay.h"
#include "PixelOverlayModelSub.h"

PixelOverlayModelSub::PixelOverlayModelSub(const Json::Value& c) :
    PixelOverlayModel(c) {
    if (!isValid()) {
        // The base class rejected the definition; the manager drops it.
        return;
    }
    // Same type tolerance as the base class: these come from the same
    // unchecked files, and asInt()/asString() throw.
    std::string parentName, subType, grid;
    int parentStart = 0;
    if (!configInt(config, "XOffset", 0, xOffset) || !configInt(config, "YOffset", 0, yOffset) ||
        !configInt(config, "ParentStartChannel", 0, parentStart) ||
        !configString(config, "Parent", parentName) || !configString(config, "SubType", subType) ||
        !configString(config, "Grid", grid)) {
        markInvalid("XOffset, YOffset, ParentStartChannel, Parent, SubType or Grid has the wrong type");
        return;
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
                // Same bound as buildGrid(): scattered to on the output thread.
                if (ch1 > 0 && (int64_t)ch1 - 1 + cpn <= FPPD_MAX_CHANNELS) {
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
    configInt(config, "ParentStartChannel", 0, parentStart0); // type checked in the constructor
    parentStart0--;
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
            // doGridOverlay() writes cpn channels at this base on the output
            // thread, so it has to lie inside the channel buffer; a stale or
            // corrupt grid could name any node.
            int64_t ob = (int64_t)parentStart0 + (int64_t)(node - 1) * cpn;
            if (slot >= 0 && slot < (int)outputMap.size() && ob + cpn <= FPPD_MAX_CHANNELS) {
                outputMap[slot] = (uint32_t)ob;
            }
        }
    }
}

PixelOverlayModelSub::~PixelOverlayModelSub() {
}

// Resolve the parent model, caching the pointer.
//
// MUST NOT be called from the channel output thread. The output thread reaches
// doOverlay() from PixelOverlayManager::doOverlays() with activeModelsLock
// already held, and every HTTP handler takes modelsLock before
// activeModelsLock (via setState() -> modelStateChanged()), so a lookup from
// that side would invert them. setState() below is the only caller, and the
// resolution itself uses tryGetModel(): setState() can also run on the effect
// update threads (an effect stopping calls model->setState() under this
// model's effectLock), where BLOCKING on modelsLock is the reverse of the
// HTTP/command paths' modelsLock -> effectLock order and deadlocks outright --
// the same shape 37c8f92a8 fixed one lock over. The try-acquire never blocks:
// from the manager paths (which already hold modelsLock recursively) it always
// succeeds, and a contended effect-side resolve just stays unresolved until
// the next setState().
bool PixelOverlayModelSub::foundParent() {
    if (parent)
        return true;

    PixelOverlayModel* p = nullptr;
    if (!PixelOverlayManager::INSTANCE.tryGetModel(config["Parent"].asString(), p)) {
        // modelsLock contended and not ours; do not log -- the parent may be
        // perfectly valid, we just can't look it up without risking deadlock.
        return false;
    }

    // Refuse a parent chain that leads back here: a submodel naming itself
    // (the Parent picker lists every model, this one included) or a loop of
    // them. doOverlay() calls parent->setData() then parent->doOverlay(), and
    // with this model somewhere up the chain that never returned - the channel
    // output thread recursed until its stack overflowed. Grid submodels end
    // the walk: they scatter to output channels and never call their parent.
    constexpr int MAX_SUBMODEL_DEPTH = 16;
    PixelOverlayModel* a = p;
    for (int depth = 0; a; depth++) {
        if (a == this || depth >= MAX_SUBMODEL_DEPTH) {
            LogErr(VB_CHANNELOUT, "Submodel \"%s\" cannot use \"%s\" as its parent: the parent chain leads back to it\n",
                   name.c_str(), config["Parent"].asString().c_str());
            return false;
        }
        PixelOverlayModelSub* s = dynamic_cast<PixelOverlayModelSub*>(a);
        if (!s || s->gridMode) {
            break;
        }
        PixelOverlayModel* next = nullptr;
        if (!PixelOverlayManager::INSTANCE.tryGetModel(s->config["Parent"].asString(), next)) {
            return false;
        }
        a = next;
    }
    parent = p;

    if (parent) {
        return true;
    }

    LogErr(VB_CHANNELOUT, "Unable to find parent \"%s\" for submodel \"%s\"\n",
           config["Parent"].asString().c_str(), name.c_str());
    return false;
}
void PixelOverlayModelSub::setState(const PixelOverlayState& st) {
    if (gridMode) {
        // Grid submodels scatter directly to the parent's output channels; they
        // never dereference `parent`, so they need no resolution at all, and
        // they do not register as a rectangular child of the parent.
        PixelOverlayModel::setState(st);
        return;
    }

    // Resolve BEFORE the base class publishes this model into the manager's
    // active list. From the moment it lands there the channel output thread can
    // call doOverlay() on it, and resolving from that side takes modelsLock
    // while doOverlays() holds activeModelsLock -- the inversion described on
    // foundParent(). Doing it here is free: setState() already runs under
    // modelsLock on every path that reaches it, so this is a recursive
    // re-acquire rather than a new one.
    bool haveParent = foundParent();

    PixelOverlayModel::setState(st);

    if (haveParent) {
        parent->setChildState(name, st, xOffset, yOffset, width, height);
    }
}
void PixelOverlayModelSub::resetParent() {
    // Disable first: that takes this model out of the active list under
    // activeModelsLock, which waits out a doOverlay() already reading
    // `parent` on the channel output thread. Only then is it safe to clear.
    PixelOverlayModel::setState(PixelOverlayState(PixelOverlayState::Disabled));
    parent = nullptr;
}
std::string PixelOverlayModelSub::getParentName() const {
    return config["Parent"].asString();
}

uint32_t PixelOverlayModelSub::outputChannelForData(uint32_t dataOffset) const {
    if (!gridMode) {
        // A rectangular submodel draws through its parent, so its cells land
        // wherever the parent's do at the offset cell; the base class's
        // startChannel arithmetic is meaningless here (startChannel is forced
        // to 0 for a submodel), and joined the polar layout against channels
        // 0..N. channelData is node-major at channelsPerNode.
        if (!parent || channelsPerNode <= 0) {
            return FPPD_OFF_CHANNEL;
        }
        int node = dataOffset / channelsPerNode;
        int k = dataOffset % channelsPerNode;
        uint32_t ch = parent->outputChannelForCell(node % width + xOffset, node / width + yOffset);
        return ch == FPPD_OFF_CHANNEL ? ch : ch + k;
    }
    int slot = dataOffset / channelsPerNode;
    if (channelGridMode) {
        // A cell may feed several member pixels; they are one buffer cell and
        // therefore one position, so the first is the representative one.
        if (slot < 0 || slot >= (int)outputCells.size() || outputCells[slot].empty()) {
            return FPPD_OFF_CHANNEL;
        }
        return outputCells[slot][0];
    }
    if (slot < 0 || slot >= (int)outputMap.size()) {
        return FPPD_OFF_CHANNEL;
    }
    return outputMap[slot];
}

void PixelOverlayModelSub::doOverlay(uint8_t* channels) {
    if (PixelOverlayModel::overlayBufferIsDirty())
        flushOverlayBuffer();

    if (gridMode) {
        doGridOverlay(channels);
        return;
    }

    // Deliberately the cached pointer, NOT foundParent(): this runs on the
    // channel output thread with activeModelsLock held, where taking modelsLock
    // deadlocks (see foundParent()). setState() has already resolved it by the
    // time we can be reached. Null here means the parent genuinely does not
    // exist -- a submodel left pointing at a renamed or deleted model -- and
    // retrying every frame would both spam the log and hold that deadlock
    // window open permanently rather than for a single lookup. setState()
    // reports the failure once instead.
    if (!parent)
        return;

    dirtyBuffer = dirtyBuffer | parent->needRefresh();
    if (!dirtyBuffer)
        return;

    // channelData is packed at this model's node width, but the parent reads
    // the region at its own bytes per pixel. A UI-made submodel is 3 channels
    // per node, so on an RGBW parent that read ran a third past the end of
    // channelData (and shifted every colour). Repack when they differ; this
    // runs only on the channel output thread, hence the thread_local.
    const uint8_t* data = channelData;
    const int pb = parent->getBytesPerPixel();
    if (pb != channelsPerNode) {
        static thread_local std::vector<uint8_t> repacked;
        const size_t px = (size_t)width * height;
        const int n = std::min(pb, channelsPerNode);
        repacked.assign(px * pb, 0);
        for (size_t p = 0; p < px; p++) {
            memcpy(&repacked[p * pb], &channelData[p * channelsPerNode], n);
        }
        data = repacked.data();
    }
    parent->setData(data, xOffset, yOffset, width, height, state);
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
