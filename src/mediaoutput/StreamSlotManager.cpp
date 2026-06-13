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
#include "log.h"

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

void StreamSlotManager::ClearSlot(int slot) {
    if (slot < 1 || slot > MAX_SLOTS) return;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_slots[slot - 1].activeOutput = nullptr;
    m_slots[slot - 1].mediaFilename.clear();
    m_slots[slot - 1].isBackground = false;
    m_slots[slot - 1].status.status = MEDIAOUTPUTSTATUS_IDLE;
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
            m_slots[i].status.status = MEDIAOUTPUTSTATUS_IDLE;
        }
    }
#endif
}

void StreamSlotManager::ProcessBackgroundSlots() {
#ifdef HAS_GSTREAMER
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (int i = 1; i < MAX_SLOTS; i++) {  // skip slot 1 (managed by playlist)
        if (m_slots[i].activeOutput && m_slots[i].isBackground) {
            m_slots[i].activeOutput->Process();
            if (!m_slots[i].activeOutput->IsPlaying()) {
                LogInfo(VB_MEDIAOUT, "StreamSlotManager: background slot %d finished\n", i + 1);
                m_slots[i].activeOutput->Close();
                delete m_slots[i].activeOutput;
                m_slots[i].activeOutput = nullptr;
                m_slots[i].mediaFilename.clear();
                m_slots[i].isBackground = false;
                m_slots[i].status.status = MEDIAOUTPUTSTATUS_IDLE;
            }
        }
    }
#endif
}
