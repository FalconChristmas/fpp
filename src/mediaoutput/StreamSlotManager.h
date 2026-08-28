#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2026 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <array>
#include <mutex>
#include <string>

#include "fpp-json-fwd.h"

#include "MediaOutputStatus.h"

class GStreamerOutput;

/**
 * StreamSlotManager — manages up to MAX_SLOTS simultaneous fppd media streams.
 *
 * Each slot has:
 * - A unique PipeWire node name: fppd_stream_1 .. fppd_stream_5
 * - Its own MediaOutputStatus for independent playback tracking
 * - A pointer to the active GStreamerOutput (nullptr when idle)
 *
 * Slot 1 is the "primary" slot whose status mirrors the global
 * mediaOutputStatus for backward compatibility.
 */
class StreamSlotManager {
public:
    static constexpr int MAX_SLOTS = 5;

    static StreamSlotManager& Instance();

    /// Get the MediaOutputStatus for a 1-based slot number.
    /// Slot 1 returns a reference to the global mediaOutputStatus.
    MediaOutputStatus* GetStatus(int slot);

    /// Register an active GStreamerOutput on a slot.
    void SetActiveOutput(int slot, GStreamerOutput* output);

    /// Get the active output for a slot (nullptr if idle).
    GStreamerOutput* GetActiveOutput(int slot);

    /// Clear a slot when playback finishes.  If `owner` is non-null, the slot
    /// is only cleared when it is still registered to that instance -- a
    /// stream whose teardown was already superseded by a newer stream on the
    /// same slot (e.g. a second "Play Media" on slot 1 before the first one
    /// finished tearing down) must not blank out the newer stream's tracking.
    void ClearSlot(int slot, GStreamerOutput* owner = nullptr);

    /// Get the PipeWire node name for a slot (e.g. "fppd_stream_1").
    static std::string GetNodeName(int slot);

    /// Get the PipeWire node description for a slot (e.g. "FPP Media Stream 1").
    static std::string GetNodeDescription(int slot);

    /// Get the PipeWire video node name for a slot (e.g. "fppd_video_stream_1").
    static std::string GetVideoNodeName(int slot);

    /// Get the PipeWire video node description for a slot.
    static std::string GetVideoNodeDescription(int slot);

    /// Set volume on a specific slot (0-100). Returns true on success.
    bool SetSlotVolume(int slot, int volume);

    /// Seek a slot to an absolute position in seconds.  Returns false if the
    /// slot is idle or the seek failed.
    bool SeekSlot(int slot, float seconds);

    /// Get status JSON for all slots (for /api/fppd/status extension).
    Json::Value GetAllSlotsStatus();

    /// Count of currently active (playing) slots.
    int ActiveSlotCount();

    /// Stop active background slots (2-5). Called during playlist stop.
    /// Slot 1 is owned by the playlist entry, which must stop it itself so
    /// the MultiSync media stop packet is sent before pipeline teardown.
    void StopBackgroundSlots();

    /// Process all active background slots (call from Playlist::Process).
    void ProcessBackgroundSlots();

    /// Mark a slot as following the primary media's clock.  Off by default:
    /// two unrelated videos on two displays should each run at their own pace,
    /// and only opting in avoids rate-nudging media that has no relationship
    /// to the show.
    void SetSyncToMaster(int slot, bool sync);

    /// Nudge every sync-enabled slot toward masterPos (seconds).  Reuses the
    /// same rate-convergence AdjustSpeed() that remote-mode MultiSync uses, so
    /// a slot drifting from the show is pulled back rather than hard-seeked.
    /// Safe to call every media tick — the rate nudge is throttled internally.
    void SyncSlotsToMaster(float masterPos);

private:
    StreamSlotManager();

    struct SlotState {
        MediaOutputStatus status = {};
        GStreamerOutput* activeOutput = nullptr;
        std::string mediaFilename;
        bool isBackground = false;  // background slots don't block playlist
        bool syncToMaster = false;  // follow the primary media's clock
        long long lastSyncMS = 0;   // last drift check (see SyncSlotsToMaster)
        long long syncSettleMS = 0; // no drift checks until this time
    };

    /// How often a sync-enabled slot's position is checked against the primary.
    static constexpr long long SYNC_INTERVAL_MS = 500;
    /// How far a sync-enabled slot may drift before it is seeked back into
    /// line.  Well above anything seen in practice (same-box slots measure
    /// 0.000s), so in normal operation nothing is ever seeked.
    static constexpr float MAX_DRIFT_SEC = 0.5f;
    /// Quiet period after a slot starts, before drift is trusted.  A freshly
    /// built pipeline reports a settling position for about a second, and so
    /// does the primary it is compared against -- on resume the primary was
    /// seen reporting 0.486s having just been handed Start(6040).  Correcting
    /// against either transient produced a pair of pointless, audible seeks.
    static constexpr long long SYNC_SETTLE_MS = 3000;

    std::array<SlotState, MAX_SLOTS> m_slots;
    std::recursive_mutex m_mutex;
};
