/*
*   Pixel Overlay Model on Sub Model for Falcon Player (FPP)
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

    if (!dirtyBuffer)
        return;

    if (!foundParent())
        return;

    parent->setData(channelData, xOffset, yOffset, width, height, state);
    dirtyBuffer = false;
}

void PixelOverlayModelSub::setData(const uint8_t* data) {
    if (!foundParent())
        return;

    PixelOverlayModel::setData(data);

    dirtyBuffer = true;
}

