#pragma once
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

#include "PixelOverlayModel.h"

class PixelOverlayModelSub : public PixelOverlayModel {
public:
    PixelOverlayModelSub(const Json::Value& config);
    virtual ~PixelOverlayModelSub();

    bool foundParent();

    virtual void setState(const PixelOverlayState& st) override;

    virtual void doOverlay(uint8_t* channels) override;
    virtual void setData(const uint8_t* data) override;

    virtual bool overlayBufferIsDirty() override;

private:
    PixelOverlayModel* parent = nullptr;

    int xOffset = 0;
    int yOffset = 0;

    volatile bool needRefresh = false;
};
