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

#include "../log.h"

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
    if (foundParent()) {
        parent->setChildState(name, st, xOffset, yOffset, width, height);
    }
}

void PixelOverlayModelSub::doOverlay(uint8_t* channels) {
    if (PixelOverlayModel::overlayBufferIsDirty())
        flushOverlayBuffer();

    if (!foundParent())
        return;

    dirtyBuffer = dirtyBuffer | parent->needRefresh();
    if (!dirtyBuffer)
        return;

    parent->setData(channelData, xOffset, yOffset, width, height, state);
    parent->doOverlay(channels);
    dirtyBuffer = false;
}

void PixelOverlayModelSub::setData(const uint8_t* data) {
    if (!foundParent())
        return;

    PixelOverlayModel::setData(data);

    dirtyBuffer = true;
}
