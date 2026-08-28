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

#include "StreamSlotManager.h"
#include "GStreamerOut.h"
#include "mediaoutput.h"

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
    // Every start (including a resume) re-arms the settle window.
    m_slots[slot - 1].syncSettleMS = GetTimeMS() + SYNC_SETTLE_MS;
#ifdef HAS_GSTREAMER
    if (output) {
        m_slots[slot - 1].mediaFilename = output->m_mediaFilename;
        LogInfo(VB_MEDIAOUT, "StreamSlotManager: slot %d active (%s)\n", slot,
                output->m_mediaFilename.c_str());
    }
#endif
}

GStreamerOutput* StreamSlotManager::GetActiveOutput(int slot) {
    if (slot < 1 || slot > MAX_SLOTS) return nullptr;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_slots[slot - 1].activeOutput;
}

void StreamSlotManager::ClearSlot(int slot, GStreamerOutput* owner) {
    if (slot < 1 || slot > MAX_SLOTS) return;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (owner && m_slots[slot - 1].activeOutput != owner) {
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
    LogInfo(VB_MEDIAOUT, "StreamSlotManager: slot %d cleared\n", slot);
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

bool StreamSlotManager::SetSlotVolume(int slot, int volume) {
#ifdef HAS_GSTREAMER
    if (slot < 1 || slot > MAX_SLOTS) return false;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    GStreamerOutput* output = m_slots[slot - 1].activeOutput;
    if (output) {
        output->SetVolume(volume);
        return true;
    }
#endif
    return false;
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

        MediaOutputStatus* st = (i == 0) ? &mediaOutputStatus : &m_slots[i].status;
        if (m_slots[i].activeOutput) {
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
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (int i = 1; i < MAX_SLOTS; i++) {  // skip slot 1 (managed by playlist)
        if (m_slots[i].activeOutput) {
            LogInfo(VB_MEDIAOUT, "StreamSlotManager: stopping slot %d\n", i + 1);
            m_slots[i].activeOutput->Stop();
            m_slots[i].activeOutput->Close();
            // Note: the owning PlaylistEntryMedia will delete the output
            m_slots[i].activeOutput = nullptr;
            m_slots[i].mediaFilename.clear();
            m_slots[i].isBackground = false;
            m_slots[i].syncToMaster = false;
            m_slots[i].lastSyncMS = 0;
            m_slots[i].syncSettleMS = 0;
            m_slots[i].status.status = MEDIAOUTPUTSTATUS_IDLE;
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
    long long now = GetTimeMS();
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
        float drift = m_slots[i].status.mediaSeconds - masterPos;
        if (drift > -MAX_DRIFT_SEC && drift < MAX_DRIFT_SEC)
            continue;
        LogInfo(VB_MEDIAOUT, "StreamSlotManager: slot %d drifted %0.3fs from the primary, seeking to %0.3f\n",
                i + 1, drift, masterPos);
        m_slots[i].activeOutput->SeekTo(masterPos);
    }
#endif
}

void StreamSlotManager::ProcessBackgroundSlots() {
#ifdef HAS_GSTREAMER
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    // Slot 1 is pumped by the playlist; slots 2-5 have no other pump, so
    // without this their status never advances and the UI and status API
    // report a frozen position for the whole track.
    //
    // Teardown is deliberately not done here.  Media started by the "Play
    // Media" command is owned by runningCommandMedia and freed off the
    // GStreamer bus when Stopped() fires, so deleting it here would be a
    // double free.  Close()/ClearSlot() on that path is what clears the slot.
    for (int i = 1; i < MAX_SLOTS; i++) {
        if (m_slots[i].activeOutput) {
            m_slots[i].activeOutput->Process();
        }
    }
#endif
}
