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

#include "AudioLevelMonitor.h"

#ifdef HAS_AUDIO_LEVEL_MONITOR

#include <algorithm>
#include <chrono>
#include <cmath>

#include "common_mini.h"
#include "log.h"

AudioLevelMonitor AudioLevelMonitor::INSTANCE;

// How often the level element reports, and therefore the ceiling on how fast a
// meter can move. 100ms is well below what reads as laggy and keeps the message
// traffic trivial.
static constexpr int LEVEL_INTERVAL_NS = 100000000;

// Floor of the meter scale. Programme material sits well above this; anything
// quieter is indistinguishable from silence on a bar a few pixels tall.
static constexpr double METER_FLOOR_DB = -60.0;

AudioLevelMonitor::~AudioLevelMonitor() {
    Shutdown();
}

void AudioLevelMonitor::StartMeter(const std::string& node) {
    // Node names are validated by the callers that accept them from outside
    // (the HTTP endpoint), but this builds a pipeline description string, so
    // anything unexpected is a reason to refuse rather than to quote around.
    if (node.empty() ||
        node.find_first_not_of("abcdefghijklmnopqrstuvwxyz"
                               "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               "0123456789_.-") != std::string::npos) {
        LogWarn(VB_MEDIAOUT, "AudioLevelMonitor: refusing node with unexpected name: %s\n", node.c_str());
        return;
    }

    std::string desc = "pipewiresrc target-object=" + node + ".monitor ! " +
                       "audioconvert ! audio/x-raw,channels=1,rate=8000 ! " +
                       "level interval=" + std::to_string(LEVEL_INTERVAL_NS) +
                       " post-messages=true ! fakesink sync=false";

    GError* err = nullptr;
    GstElement* pipeline = gst_parse_launch(desc.c_str(), &err);
    if (!pipeline || err) {
        LogWarn(VB_MEDIAOUT, "AudioLevelMonitor: could not build meter for %s: %s\n",
                node.c_str(), err ? err->message : "unknown error");
        if (err) {
            g_error_free(err);
        }
        if (pipeline) {
            gst_object_unref(pipeline);
        }
        return;
    }

    Meter m;
    m.pipeline = pipeline;
    m.bus = gst_element_get_bus(pipeline);
    m.updatedMS = GetTimeMS();
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    m_meters[node] = m;
    LogDebug(VB_MEDIAOUT, "AudioLevelMonitor: metering %s\n", node.c_str());
}

void AudioLevelMonitor::StopMeter(const std::string& node) {
    auto it = m_meters.find(node);
    if (it == m_meters.end()) {
        return;
    }
    if (it->second.bus) {
        gst_object_unref(it->second.bus);
    }
    if (it->second.pipeline) {
        gst_element_set_state(it->second.pipeline, GST_STATE_NULL);
        gst_object_unref(it->second.pipeline);
    }
    m_meters.erase(it);
    LogDebug(VB_MEDIAOUT, "AudioLevelMonitor: stopped metering %s\n", node.c_str());
}

void AudioLevelMonitor::Subscribe(const std::vector<std::string>& nodes, int ttlMs) {
    std::unique_lock<std::mutex> lk(m_lock);

    // Bounded: this is reachable from the web UI, and each entry is a real
    // pipeline against a real audio device.
    std::set<std::string> wanted(nodes.begin(), nodes.end());
    while (wanted.size() > 12) {
        wanted.erase(std::prev(wanted.end()));
    }

    for (auto it = m_meters.begin(); it != m_meters.end();) {
        if (wanted.find(it->first) == wanted.end()) {
            std::string dead = it->first;
            ++it;
            StopMeter(dead);
        } else {
            ++it;
        }
    }
    // Already-running nodes are left as they are: a keepalive must not restart
    // a pipeline and blank the meter every few seconds.
    for (const auto& n : wanted) {
        if (m_meters.find(n) == m_meters.end()) {
            StartMeter(n);
        }
    }

    m_expiresMS = m_meters.empty() ? 0 : GetTimeMS() + std::max(1000, ttlMs);
    m_active.store(!m_meters.empty());
    bool needThread = !m_meters.empty();
    lk.unlock();

    // Thread lifecycle is handled outside m_lock, and the previous thread is
    // joined before a new one is assigned: the poll thread retires itself once
    // the last meter goes away, and assigning over a still-joinable thread
    // calls std::terminate() -- which aborted fppd the first time metering was
    // switched on, off, and on again.
    if (needThread) {
        std::lock_guard<std::mutex> tlk(m_threadLock);
        if (!m_running.load()) {
            if (m_thread.joinable()) {
                m_thread.join();
            }
            m_running.store(true);
            m_thread = std::thread(&AudioLevelMonitor::PollThread, this);
        }
    }
}

// Drains each pipeline's bus for level messages. A thread rather than bus
// callbacks so this owns no GLib main loop -- fppd has no shared one to attach
// to, and adding a second is a bigger commitment than a 20ms poll.
void AudioLevelMonitor::PollThread() {
    while (m_running.load()) {
        {
            std::lock_guard<std::mutex> lk(m_lock);

            // Nobody has re-subscribed: tear everything down. This is what
            // keeps a closed browser tab from metering forever.
            if (!m_meters.empty() && GetTimeMS() > m_expiresMS) {
                std::vector<std::string> all;
                for (const auto& kv : m_meters) {
                    all.push_back(kv.first);
                }
                for (const auto& n : all) {
                    StopMeter(n);
                }
                m_active.store(false);
            }

            for (auto& kv : m_meters) {
                GstMessage* msg = nullptr;
                while ((msg = gst_bus_pop_filtered(kv.second.bus,
                                                   (GstMessageType)(GST_MESSAGE_ELEMENT | GST_MESSAGE_ERROR))) != nullptr) {
                    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
                        GError* err = nullptr;
                        gst_message_parse_error(msg, &err, nullptr);
                        LogWarn(VB_MEDIAOUT, "AudioLevelMonitor: %s: %s\n", kv.first.c_str(),
                                err ? err->message : "pipeline error");
                        if (err) {
                            g_error_free(err);
                        }
                        gst_message_unref(msg);
                        continue;
                    }

                    const GstStructure* st = gst_message_get_structure(msg);
                    if (st && gst_structure_has_name(st, "level")) {
                        // The level element reports per-channel values in a
                        // GValueArray, not a GstValueArray -- they are
                        // different types, and testing for the GStreamer one
                        // silently matched nothing and left every meter at
                        // zero. Both are accepted here so this keeps working
                        // if that ever changes.
                        const GValue* arr = gst_structure_get_value(st, "rms");
                        const GValue* v = nullptr;
                        if (arr && G_VALUE_HOLDS(arr, G_TYPE_VALUE_ARRAY)) {
                            GValueArray* va = (GValueArray*)g_value_get_boxed(arr);
                            if (va && va->n_values > 0) {
                                v = &va->values[0];
                            }
                        } else if (arr && GST_VALUE_HOLDS_ARRAY(arr) && gst_value_array_get_size(arr) > 0) {
                            v = gst_value_array_get_value(arr, 0);
                        }
                        if (v && G_VALUE_HOLDS_DOUBLE(v)) {
                            kv.second.rmsDb = g_value_get_double(v);
                            kv.second.updatedMS = GetTimeMS();
                        }
                    }
                    gst_message_unref(msg);
                }
            }

            if (m_meters.empty()) {
                m_running.store(false);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

Json::Value AudioLevelMonitor::GetLevels() {
    Json::Value out(Json::objectValue);
    std::lock_guard<std::mutex> lk(m_lock);
    long long now = GetTimeMS();
    for (const auto& kv : m_meters) {
        // A pipeline that has stopped reporting (device gone, node removed)
        // reads as silent rather than freezing at its last value.
        double db = (now - kv.second.updatedMS) > 1000 ? METER_FLOOR_DB : kv.second.rmsDb;
        double pct = ((db - METER_FLOOR_DB) / -METER_FLOOR_DB) * 100.0;
        out[kv.first] = (int)std::lround(std::clamp(pct, 0.0, 100.0));
    }
    return out;
}

void AudioLevelMonitor::Shutdown() {
    m_running.store(false);
    {
        std::lock_guard<std::mutex> tlk(m_threadLock);
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
    std::lock_guard<std::mutex> lk(m_lock);
    std::vector<std::string> all;
    for (const auto& kv : m_meters) {
        all.push_back(kv.first);
    }
    for (const auto& n : all) {
        StopMeter(n);
    }
    m_active.store(false);
}

#endif
