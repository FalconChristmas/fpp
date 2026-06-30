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

#include <vector>

#include "PixelOverlayModel.h"
#include "fpp-json-fwd.h"

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

    // Grid-mode (xLights submodel) support.  When set, the submodel is a sparse
    // subset of its parent's nodes defined by a grid of parent node numbers.
    // The base class builds a dense width*height buffer + channelData; outputMap
    // scatters each buffer cell's color to the parent's absolute output channel.
    // See docs/PixelOverlaySubModels.md.
    bool gridMode = false;
    std::vector<uint32_t> outputMap; // one entry per buffer cell -> absolute output channel base (or FPPD_OFF_CHANNEL)

    // Channel-grid mode (xLights model groups): a combined buffer whose cells
    // each map to one OR MORE absolute output channels (member pixels binned by
    // world position). See docs/PixelOverlaySubModels.md.
    bool channelGridMode = false;
    std::vector<std::vector<uint32_t>> outputCells; // per buffer cell -> list of absolute output channel bases

    void buildGrid();
    void buildChannelGrid();
    void doGridOverlay(uint8_t* channels);
};
