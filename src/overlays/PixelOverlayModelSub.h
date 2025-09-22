#pragma once
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

#include "PixelOverlayModel.h"

class PixelOverlayModelSub : public PixelOverlayModel {
public:
    PixelOverlayModelSub(const Json::Value& config);
    virtual ~PixelOverlayModelSub();

    virtual void doOverlay(uint8_t* channels) override;
    virtual void setData(const uint8_t* data) override;

    virtual void setState(const PixelOverlayState& st) override;

    void resetParent();

private:
    bool foundParent();

    PixelOverlayModel* parent = nullptr;

    int xOffset = 0;
    int yOffset = 0;

    volatile bool needRefresh = false;
};
