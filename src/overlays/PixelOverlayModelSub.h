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

#include <atomic>
#include <memory>
#include <mutex>
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
    std::string getParentName() const;
    // Looks the parent up now if it has not been yet (a grid submodel needs
    // none). Not from the channel output thread, as foundParent().
    bool resolveParent() { return gridMode || foundParent(); }
    bool isGridMode() const { return gridMode; }
    // The parent is resolved and its chain ends at a framebuffer, the only
    // kind of model a VirtualMatrix can draw from its own SendData().
    bool drawsToFramebuffer() const { return fbRoot; }
    // For a VirtualMatrix or VirtualDisplay drawing this model from its
    // SendData(): drawsToFramebuffer(), saying once why not when it is not.
    bool readyForPassThrough();

protected:
    // Grid submodels and groups SCATTER: a buffer cell lands on an arbitrary
    // parent/member channel rather than at a fixed offset from a start channel,
    // so the base class's contiguous assumption does not hold. Both scatter
    // tables are indexed by channelData slot, which is what this receives.
    uint32_t outputChannelForData(uint32_t dataOffset) const override;

private:
    bool foundParent();

    // `parent` is what doOverlay() draws through; `parentRef` owns a share
    // of the same model, so setState() can call into it from any thread
    // without modelsLock.  Both are set and cleared together under
    // parentLock (a leaf lock), always with modelsLock held as well.
    PixelOverlayModel* parent = nullptr;
    std::weak_ptr<PixelOverlayModel> parentRef;
    std::mutex parentLock;
    // Held across doOverlay()'s use of the parent, and taken by
    // resetParent(): a VirtualMatrix in pass-through calls doOverlay() outside
    // the active list, so disabling this model does not wait it out.  An
    // output reload closes outputs with the output thread held off anyway;
    // this covers a parent removed any other way (a VirtualMatrix's Init()
    // replacing its model while other outputs keep sending).
    std::mutex useLock;
    std::atomic<bool> fbRoot = false;
    std::atomic<bool> passThroughWarned = false;

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
