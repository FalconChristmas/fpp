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

#include <cstddef>
#include <cstdint>
#include <deque>
#include <utility>

// A remote's audio error readings over the last two seconds, for
// GStreamerOutput::AdjustFeedSpeed().
//
// Each media sync packet gives one reading: where this remote's sound is
// against the position the master put in the packet (> 0: this remote is
// ahead).  A packet can arrive late -- the master busy, the network, Wi-Fi
// holding it -- but never early, and a late one carries a position the
// master has already moved past, so it can only make this remote look
// further ahead than it is.  The lowest reading of the last two seconds is
// the least late packet's, and the honest one: the same rule
// MasterFrame0() steers a remote's lights by (channeloutputthread.cpp).
// The master's own position steps (a whole audio period at a time) inflate
// readings the same way, so the lowest also sees through those.
//
// Steered by each packet alone, one packet more than the seek threshold
// late reads as this remote that far ahead: the seek pulls the sound behind
// by the delay, the landing teaches the next seek to aim further ahead, and
// one late packet costs two or three audible seeks.
//
// Readings are ms of monotonic time and seconds of error.
class FeedSyncWindow {
public:
    static constexpr uint64_t kWindowMs = 2000;  // as MasterFrame0()'s window
    static constexpr std::size_t kAgreeing = 2;  // readings before an error is acted on

    // A seek (or a new song) starts a new timeline: earlier readings are of
    // the old one.
    void Clear() {
        m_errs.clear();
    }

    // Add this packet's reading, returning the lowest of the window.  The
    // master going back -- a new song, a seek back -- starts over, as
    // MasterFrame0() does when the master's frame goes back.
    double Add(uint64_t nowMs, double err, double masterPos) {
        if (m_lastMasterPos >= 0.0 && masterPos + 0.5 < m_lastMasterPos) {
            m_errs.clear();
        }
        m_lastMasterPos = masterPos;
        while (!m_errs.empty() && nowMs - m_errs.front().first > kWindowMs) {
            m_errs.pop_front();
        }
        m_errs.emplace_back(nowMs, err);
        return Lowest();
    }

    double Lowest() const {
        double best = m_errs.empty() ? 0.0 : m_errs.front().second;
        for (const auto& e : m_errs) {
            if (e.second < best) {
                best = e.second;
            }
        }
        return best;
    }

    // Enough readings that one late packet can't be the whole story.
    bool Agreed() const {
        return m_errs.size() >= kAgreeing;
    }

    std::size_t Size() const {
        return m_errs.size();
    }

private:
    std::deque<std::pair<uint64_t, double>> m_errs;
    double m_lastMasterPos = -1.0;
};
