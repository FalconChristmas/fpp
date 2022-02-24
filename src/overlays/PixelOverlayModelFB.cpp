/*
*   Pixel Overlay Model on a FrameBuffer for Falcon Player (FPP)
*
*   The Falcon Player (FPP) is free software; you can redistribute it
*   and/or modify it under the terms of the GNU General Public License
*   as published by the Free Software Foundation; either version 2 of
*   the License, or (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include "fpp-pch.h"

#include "PixelOverlayModelFB.h"

PixelOverlayModelFB::PixelOverlayModelFB(const Json::Value& c) :
    PixelOverlayModel(c) {

    fb = new FrameBuffer();

    fb->FBInit(config);
}

PixelOverlayModelFB::~PixelOverlayModelFB() {
    if (fb)
        delete fb;
}

void PixelOverlayModelFB::setState(const PixelOverlayState& st) {
    if (st != state) {
        if (!st.getState() && overlayBufferIsDirty()) {
            // Give the buffer a little time to flush before disabling
            int x = 0;
            while (overlayBufferIsDirty() && (x++ < 10)) {
                usleep(10000);
            }
        }

        PixelOverlayModel::setState(st);
    }
}

void PixelOverlayModelFB::doOverlay(uint8_t* channels) {
    if (PixelOverlayModel::overlayBufferIsDirty()) {
        flushOverlayBuffer();
        needRefresh = true;
    }

    if (!needRefresh)
        return;

    fb->FBCopyData(channelData);
    fb->FBStartDraw();
    needRefresh = false;
}

void PixelOverlayModelFB::setData(const uint8_t* data) {
    memcpy(channelData, data, width * height * 3);
    needRefresh = true;
}

bool PixelOverlayModelFB::overlayBufferIsDirty() {
    if (needRefresh)
        return true;

    return PixelOverlayModel::overlayBufferIsDirty();
}

