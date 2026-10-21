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

#include "fpp-pch.h"

#include "fpp-json.h"
#include "log.h"
#include "common_mini.h"
#include "Timers.h"

#include "StreamSlotManager.h"
#include "GStreamerOut.h"
#include "mediaoutput.h"
#include "common.h"
#include "settings.h"

// The volume the user last set for a slot, from pipewire-stream-slots.json.
// Slot volumes apply to a stream that only exists while that slot is playing,
// so they cannot be restored at boot the way group volumes are -- they have to
// be reapplied each time the slot starts instead.
//
// All five slots are treated alike.  Slot 1 used to be excluded because its
// fader was wired to the global master rather than to the stream, which made it
// a duplicate master control instead of a slot of its own.
//
// KEEP IN SYNC with SaveStreamSlotVolume()/GetStreamSlotVolumes() in
// www/api/controllers/pipewire.php.
static int savedStreamSlotVolume(int slot) {
    if (slot < 1 || slot > 5) {
        return -1;
    }
    // The file only exists once a slot fader has been moved, so its absence is
    // the normal case -- don't let LoadJsonFromFile() log it as an error.
    std::string file = FPP_DIR_CONFIG("/pipewire-stream-slots.json");
    Json::Value root;
    // Shape-checked, not trusted: a list root, a non-object "slots" or a
    // volume saved as a string threw from here on every stream start.
    if (!FileExists(file) || !LoadJsonFromFile(file, root, JsonRoot::Object) ||
        !root["slots"].isObject()) {
        return -1;
    }
    std::string key = std::to_string(slot);
    const Json::Value& vol = root["slots"][key];
    if (!vol.isIntegral()) {
        return -1;
    }
    int v = vol.asInt();
    if (v < 0 || v > 100) {
        return -1;
    }
    return v;
}

StreamSlotManager::StreamSlotManager() {
    for (int i = 0; i < MAX_SLOTS; i++) {
        m_slots[i].status = {};
        m_slots[i].status.status = MEDIAOUTPUTSTATUS_IDLE;
        m_slots[i].activeOutput = nullptr;
        m_slots[i].isBackground = false;
    }
}

StreamSlotManager& StreamSlotManager::Instance() {
    static StreamSlotManager instance;
    return instance;
}

MediaOutputStatus* StreamSlotManager::GetStatus(int slot) {
    if (slot < 1 || slot > MAX_SLOTS) {
        LogWarn(VB_MEDIAOUT, "StreamSlotManager::GetStatus: invalid slot %d\n", slot);
        return &mediaOutputStatus;  // fallback to global
    }
    if (slot == 1) {
        // Slot 1 uses the global mediaOutputStatus for backward compatibility
        return &mediaOutputStatus;
    }
    return &m_slots[slot - 1].status;
}

void StreamSlotManager::SetActiveOutput(int slot, GStreamerOutput* output) {
    if (slot < 1 || slot > MAX_SLOTS) return;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_slots[slot - 1].activeOutput = output;
#ifdef HAS_GSTREAMER
    if (output && output->NeedsSlotPump()) {
        StartPumpTimer();
    }
#endif
    // Every start (including a resume) re-arms the settle window.
    m_slots[slot - 1].syncSettleMS = GetMonotonicTimeMS() + SYNC_SETTLE_MS;
#ifdef HAS_GSTREAMER
    if (output) {
        m_slots[slot - 1].mediaFilename = output->m_mediaFilename;
        LogDebug(VB_MEDIAOUT, "StreamSlotManager: slot %d active (%s)\n", slot,
                output->m_mediaFilename.c_str());
        // A volume set while the slot was idle is newer than the saved one
        // (and is all there is when it came from a command, not the mixer).
        int saved = m_slots[slot - 1].pendingVolume;
        m_slots[slot - 1].pendingVolume = -1;
        if (saved < 0) {
            saved = savedStreamSlotVolume(slot);
        }
        if (saved >= 0) {
            output->SetVolume(saved);
            LogDebug(VB_MEDIAOUT, "StreamSlotManager: restored slot %d volume to %d%%\n", slot, saved);
        }
    }
#endif
}

void StreamSlotManager::DetachOutput(int slot, GStreamerOutput* owner) {
    if (slot < 1 || slot > MAX_SLOTS || !owner) return;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_slots[slot - 1].activeOutput == owner) {
        m_slots[slot - 1].activeOutput = nullptr;
        m_slots[slot - 1].closingOutput = owner;
    }
}

bool StreamSlotManager::RequestStop(int slot) {
#ifdef HAS_GSTREAMER
    if (slot < 1 || slot > MAX_SLOTS) return false;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_slots[slot - 1].activeOutput) {
        m_slots[slot - 1].activeOutput->RequestStop();
        return true;
    }
#endif
    return false;
}

GStreamerOutput* StreamSlotManager::GetActiveOutput(int slot) {
    if (slot < 1 || slot > MAX_SLOTS) return nullptr;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_slots[slot - 1].activeOutput;
}

void StreamSlotManager::ClearSlot(int slot, GStreamerOutput* owner) {
    if (slot < 1 || slot > MAX_SLOTS) return;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    bool closing = owner && m_slots[slot - 1].closingOutput == owner;
    if (closing) {
        m_slots[slot - 1].closingOutput = nullptr;
    }
    if (owner && m_slots[slot - 1].activeOutput != owner &&
        !(closing && !m_slots[slot - 1].activeOutput)) {
        // Another (newer) stream has already claimed this slot -- our
        // teardown is stale, don't blank out its registration.
        LogDebug(VB_MEDIAOUT, "StreamSlotManager: slot %d clear skipped, already reclaimed by a newer stream\n", slot);
        return;
    }
    m_slots[slot - 1].activeOutput = nullptr;
    m_slots[slot - 1].mediaFilename.clear();
    m_slots[slot - 1].isBackground = false;
    m_slots[slot - 1].syncToMaster = false;
    m_slots[slot - 1].lastSyncMS = 0;
    m_slots[slot - 1].syncSettleMS = 0;
    // Zero the timing fields, not just the state: GetAllSlotsStatus() reports
    // them for any slot holding an active output, so leaving the last track's
    // numbers behind makes a freshly started stream read as though it were
    // already minutes in until its first Process() lands.  Slot 1 aliases the
    // global mediaOutputStatus, which is not ours to wipe.
    std::lock_guard<std::mutex> fieldsLock(MediaOutputStatus::FieldsLock());
    if (slot > 1) {
        m_slots[slot - 1].status = {};
    }
    m_slots[slot - 1].status.status = MEDIAOUTPUTSTATUS_IDLE;
    // #2713: release the KMS connector gate (mediaOutputStatus.output) when the
    // stream tears down.  Otherwise it stays set to the video's connector (e.g.
    // "DSI-1") after playback ends and the KMSFrameBuffer overlay path stays
    // gated off, so images never scan out until fppd is restarted.  Use
    // GetStatus(slot) so slot 1's global mediaOutputStatus is the object cleared.
    GetStatus(slot)->output = "";
    LogDebug(VB_MEDIAOUT, "StreamSlotManager: slot %d cleared\n", slot);
}

std::string StreamSlotManager::GetNodeName(int slot) {
    return "fppd_stream_" + std::to_string(slot);
}

std::string StreamSlotManager::GetNodeDescription(int slot) {
    return "FPP Media Stream " + std::to_string(slot);
}

std::string StreamSlotManager::GetVideoNodeName(int slot) {
    return "fppd_video_stream_" + std::to_string(slot);
}

std::string StreamSlotManager::GetVideoNodeDescription(int slot) {
    return "FPP Video Stream " + std::to_string(slot);
}

bool StreamSlotManager::SetSlotVolume(int slot, int volume, bool* deferred) {
#ifdef HAS_GSTREAMER
    if (slot < 1 || slot > MAX_SLOTS) return false;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    GStreamerOutput* output = m_slots[slot - 1].activeOutput;
    if (output) {
        output->SetVolume(volume);
    } else {
        // Nothing to apply it to yet: the slot's next stream starts at it,
        // as it does at the level the mixer saves.
        m_slots[slot - 1].pendingVolume = volume;
    }
    if (deferred) {
        *deferred = !output;
    }
    return true;
#else
    return false;
#endif
}

bool StreamSlotManager::SeekSlot(int slot, float seconds) {
#ifdef HAS_GSTREAMER
    if (slot < 1 || slot > MAX_SLOTS) return false;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    GStreamerOutput* output = m_slots[slot - 1].activeOutput;
    if (output) {
        return output->SeekTo(seconds);
    }
#endif
    return false;
}

Json::Value StreamSlotManager::GetAllSlotsStatus() {
    Json::Value result(Json::arrayValue);
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (int i = 0; i < MAX_SLOTS; i++) {
        Json::Value slotJson;
        slotJson["slot"] = i + 1;
        slotJson["nodeName"] = GetNodeName(i + 1);
        slotJson["nodeDescription"] = GetNodeDescription(i + 1);

        // The output's own status, which is not always the slot's: Play Media
        // on slot 1 keeps a private one so it doesn't overwrite the playlist's
        // (the global mediaOutputStatus), and reading the global reported 0
        // for its whole track. Close() takes an output out of the table
        // before freeing it, so under m_mutex the pointer is live.
        GStreamerOutput* out = m_slots[i].activeOutput;
        MediaOutputStatus* st = (out && out->m_mediaOutputStatus) ? out->m_mediaOutputStatus
                                : (i == 0)                         ? &mediaOutputStatus
                                                                   : &m_slots[i].status;
        if (out) {
            std::lock_guard<std::mutex> fieldsLock(MediaOutputStatus::FieldsLock());
            slotJson["status"] = "playing";
            slotJson["mediaFilename"] = m_slots[i].mediaFilename;
            slotJson["secondsElapsed"] = st->secondsElapsed;
            slotJson["subSecondsElapsed"] = st->subSecondsElapsed;
            slotJson["secondsRemaining"] = st->secondsRemaining;
            slotJson["subSecondsRemaining"] = st->subSecondsRemaining;
            slotJson["minutesTotal"] = st->minutesTotal;
            slotJson["secondsTotal"] = st->secondsTotal;
            slotJson["isBackground"] = m_slots[i].isBackground;
        } else {
            slotJson["status"] = "idle";
            slotJson["mediaFilename"] = "";
            slotJson["isBackground"] = false;
        }
        result.append(slotJson);
    }
    return result;
}

int StreamSlotManager::ActiveSlotCount() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    int count = 0;
    for (int i = 0; i < MAX_SLOTS; i++) {
        if (m_slots[i].activeOutput) count++;
    }
    return count;
}

void StreamSlotManager::StopBackgroundSlots() {
#ifdef HAS_GSTREAMER
    // These outputs belong to someone else -- Play Media's registry, or a
    // playlist entry playing on a background slot -- which stops, closes and
    // frees them on its own thread.  Stopping and closing them here, from
    // whatever thread is stopping the playlist, raced the owner: a second
    // Close() alongside the deferred delete's, and Stop() under this lock
    // while the main loop waited on it.  Each output is asked to stop, and
    // its own Close() clears the slot.
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (int i = 1; i < MAX_SLOTS; i++) {  // skip slot 1 (managed by playlist)
        if (m_slots[i].activeOutput) {
            LogInfo(VB_MEDIAOUT, "StreamSlotManager: stopping slot %d\n", i + 1);
            m_slots[i].activeOutput->RequestStop();
        }
    }
#endif
}

void StreamSlotManager::SetSyncToMaster(int slot, bool sync) {
    if (slot < 1 || slot > MAX_SLOTS)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_slots[slot - 1].syncToMaster = sync;
    if (sync) {
        LogInfo(VB_MEDIAOUT, "StreamSlotManager: slot %d will follow the primary media clock\n", slot);
    }
}

void StreamSlotManager::SyncSlotsToMaster(float masterPos) {
#ifdef HAS_GSTREAMER
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    long long now = GetMonotonicTimeMS();
    // Slot 1 IS the master (it is the playlist's own media), so start at 2.
    for (int i = 1; i < MAX_SLOTS; i++) {
        if (!m_slots[i].activeOutput || !m_slots[i].syncToMaster)
            continue;

        // Process() first: nothing else drives a non-background slot's status,
        // so without this its mediaSeconds stays 0 and AdjustSpeed bails out
        // ("can't adjust speed if not playing yet") having never seen the slot
        // actually move.
        m_slots[i].activeOutput->Process();

        if ((now - m_slots[i].lastSyncMS) < SYNC_INTERVAL_MS)
            continue;
        m_slots[i].lastSyncMS = now;
        if (now < m_slots[i].syncSettleMS)
            continue;

        // Slots started together on this box share the pipeline clock and stay
        // sample-locked by themselves -- measured 0.000s offset across a whole
        // track with no correction at all.  So this is a repair path, not a
        // control loop: it only fires when something has genuinely knocked a
        // slot off the show timeline (the primary was seeked or restarted, or
        // MultiSync is rate-adjusting the primary on a remote).
        //
        // Correcting by rate is not an option.  These pipelines refuse
        // GST_SEEK_FLAG_INSTANT_RATE_CHANGE, so ApplyRate() falls back to a
        // flush seek, which moves the position enough to feed the next
        // correction -- measured 0.9-2.5s oscillating, never converging, on
        // streams that were exactly locked before the "sync" was applied.
        float slotPos;
        {
            std::lock_guard<std::mutex> fieldsLock(MediaOutputStatus::FieldsLock());
            slotPos = m_slots[i].status.mediaSeconds;
        }
        float drift = slotPos - masterPos;
        if (drift > -MAX_DRIFT_SEC && drift < MAX_DRIFT_SEC)
            continue;
        LogInfo(VB_MEDIAOUT, "StreamSlotManager: slot %d drifted %0.3fs from the primary, seeking to %0.3f\n",
                i + 1, drift, masterPos);
        m_slots[i].activeOutput->SeekTo(masterPos);
    }
#endif
}

void StreamSlotManager::ProcessBackgroundSlots() {
    PumpSlotOutputs();
}

// Calls Process() for every output whose owner doesn't (see
// GStreamerOutput::NeedsSlotPump()) -- Play Media's, on any slot.  Without it
// their status never advances, so the UI and status API report a frozen
// position, and the preroll and stall watchdogs never run.  This used to
// happen only from a playing playlist (ProcessBackgroundSlots()), and only for
// slots 2-5: Play Media with the player idle, or on slot 1, got nothing.  The
// pump timer covers those; the playlist's tick still pumps while it runs, and
// the two share one cadence.
//
// Teardown is deliberately not done here.  Media started by the "Play Media"
// command is owned by runningCommandMedia and freed off the main loop when
// Stopped() fires, so deleting it here would be a double free.
// Close()/ClearSlot() on that path is what clears the slot.
void StreamSlotManager::PumpSlotOutputs() {
#ifdef HAS_GSTREAMER
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    long long now = GetMonotonicTimeMS();
    if (now - m_lastPumpMS < PUMP_INTERVAL_MS / 2) {
        return;
    }
    m_lastPumpMS = now;
    for (int i = 0; i < MAX_SLOTS; i++) {
        if (m_slots[i].activeOutput && m_slots[i].activeOutput->NeedsSlotPump()) {
            m_slots[i].activeOutput->Process();
        }
    }
#endif
}

void StreamSlotManager::StartPumpTimer() {
    if (m_pumpTimerStarted.exchange(true)) {
        return;
    }
    Timers::INSTANCE.addPeriodicTimer("StreamSlotPump", PUMP_INTERVAL_MS, [this]() {
        PumpSlotOutputs();
    });
}
