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

#if __has_include(<xf86drm.h>)
#include "FrameBuffer.h"
#include <xf86drm.h>
#include <xf86drmMode.h>
#define HAS_KMS_FB

#include <map>
#include <string>
#include <vector>

struct DumbBuffer {
    uint32_t handle = 0;
    uint32_t fb_id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    uint32_t format = 0;
    uint64_t size = 0;
    void* mapped = nullptr;
};

struct CardInfo {
    int fd = -1;
    std::string path;
    std::vector<uint32_t> reservedConnectors;
    std::vector<uint32_t> reservedCrtcs;
    std::vector<uint32_t> reservedPlanes;
};

class KMSFrameBuffer : public FrameBuffer {
public:
    KMSFrameBuffer();
    virtual ~KMSFrameBuffer();

    virtual int InitializeFrameBuffer() override;
    virtual void DestroyFrameBuffer() override;
    virtual void SyncLoop() override;
    virtual void SyncDisplay(bool pageChanged = false) override;
    virtual void WaitForPageFree() override;

    virtual void EnableDisplay() override;
    virtual void DisableDisplay() override;

    virtual bool SupportsVariableRefresh() override { return m_variableRefresh; }
    virtual int GetMaxRefreshRate() override;
    virtual bool SetRefreshRate(int fps) override;

    int m_cardFd = -1;
    uint32_t m_connectorId = 0;
    uint32_t m_crtcId = 0;
    int m_crtcPipe = 0; // index of m_crtcId in the card's crtc list (vblank pipe)
    uint32_t m_planeId = 0;
    std::string m_connectorName;
    drmModeModeInfo m_mode = {};
    DumbBuffer m_fb[3] = {};
    bool m_displayEnabled = false;

    static std::atomic_int FRAMEBUFFER_COUNT;
    static std::vector<CardInfo*> CARDS;

private:
    static std::string ConnectorFullName(int fd, drmModeConnectorPtr conn);
    static bool CreateDumbBuffer(int fd, uint32_t width, uint32_t height, uint32_t format, DumbBuffer& buf);
    static void DestroyDumbBuffer(int fd, DumbBuffer& buf);
    uint32_t FindPlaneForCrtc(int fd, uint32_t crtcId, uint32_t format);

    // Rewrites the vertical timing of `mode` for `vactive` scanlines at `fps`,
    // keeping the horizontal timing and pixel clock (and thus the DPI/WS bit
    // timing) unchanged.  Requesting an fps above what the vactive allows is
    // clamped to the minimum vertical blanking (i.e. the maximum rate).
    void ApplyVerticalTiming(drmModeModeInfo& mode, int vactive, int fps);

    // Drain the completion event of an outstanding (m_flipPending) page flip.
    // Blocks in poll() until the flip retires at vblank (or timeoutMs elapses),
    // so it costs no CPU while waiting.
    void WaitForPendingFlip(int timeoutMs);
    static void PageFlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void* data);
    static void VBlankHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void* data);

    // True while a drmModePageFlip has been queued but its vblank completion
    // event has not yet been drained.  Guarded by mediaOutputLock (only touched
    // from SyncDisplay / WaitForPendingFlip, both of which hold that lock).
    bool m_flipPending = false;

    // Flip-path health counters (guarded by mediaOutputLock like m_flipPending).
    // Logged rate-limited at Warn so brief output hitches are visible without
    // debug logging: a stutter on the panel with these counters unchanged means
    // the delay is NOT in the KMS flip path.
    uint32_t m_flipWaitTimeouts = 0;
    uint32_t m_flipRejects = 0;
    uint32_t m_slowSyncs = 0;
    int m_maxSyncMS = 0;
    int m_lastSlowSyncMS = 0;
    long long m_lastFlipWarnMS = 0;
    // Totals as of the last warning, so we only log when something new
    // happened (a warning line then timestamps the event itself).
    uint64_t m_warnedEventTotal = 0;

    // Vblank cadence tracking.  Each flip completion event carries the vblank
    // sequence number; at a steady output rate the delta between consecutive
    // flips is constant (e.g. 2 at 20fps on a 40Hz panel).  If the software
    // frame clock beats against the panel clock, the delta oscillates (1,3,
    // 1,3,...) around each phase crossing - visible as a brief judder on the
    // pixels even though every software metric looks perfect.
    uint32_t m_lastFlipSeq = 0;
    uint32_t m_vblankDeltaCounts[5] = { 0, 0, 0, 0, 0 }; // [0]=unused, 1..3, [4]=4+
    uint32_t m_vblankCadenceBreaks = 0;                  // delta changed vs previous delta
    uint32_t m_prevVblankDelta = 0;
    // Vblank timestamp (monotonic us) of the last completed flip, used to
    // phase-align flip submission away from vblank boundaries.
    uint64_t m_lastFlipTimeUS = 0;
    uint32_t m_phaseNudges = 0;

    CardInfo* m_cardInfo = nullptr;
};

#endif
