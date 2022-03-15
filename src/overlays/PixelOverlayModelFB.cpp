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

void PixelOverlayModelFB::doOverlay(uint8_t* channels) {
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

