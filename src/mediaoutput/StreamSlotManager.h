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

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#endif

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

    /// Clear a slot when playback finishes.
    void ClearSlot(int slot);

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

private:
    StreamSlotManager();

    struct SlotState {
        MediaOutputStatus status = {};
        GStreamerOutput* activeOutput = nullptr;
        std::string mediaFilename;
        bool isBackground = false;  // background slots don't block playlist
    };

    std::array<SlotState, MAX_SLOTS> m_slots;
    std::recursive_mutex m_mutex;
};
