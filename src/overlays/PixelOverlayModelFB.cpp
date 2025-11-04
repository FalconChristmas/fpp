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

#include "PixelOverlayModelFB.h"

PixelOverlayModelFB::PixelOverlayModelFB(const Json::Value& c) :
    PixelOverlayModel(c) {
    fb = FrameBuffer::createFrameBuffer(config);
    if (fb == nullptr) {
        LogErr(VB_CHANNELOUT, "Error initializing underlying FrameBuffer for Pixel Overlay Model '%s'\n", name.c_str());
        delete fb;
        fb = nullptr;
    }
}

PixelOverlayModelFB::~PixelOverlayModelFB() {
    if (fb)
        delete fb;
}

void PixelOverlayModelFB::doOverlay(uint8_t* channels) {
    if (!fb)
        return;

    // Always enable display on first call, even if no dirty buffer
    if (!displayEnabled) {
        fb->EnableDisplay();
        displayEnabled = true;
    }

    if (children.empty() && !dirtyBuffer)
        return;

    fb->FBCopyData(channelData);
    fb->FBStartDraw();
    dirtyBuffer = false;
}

void PixelOverlayModelFB::setData(const uint8_t* data) {
    memcpy(channelData, data, width * height * 3);
    dirtyBuffer = true;
}
