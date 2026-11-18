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

#include "VideoOutputManager.h"

#if defined(HAS_GSTREAMER_VIDEO_OUTPUT) && !defined(PLATFORM_OSX)
#define HAS_VIDEO_DISPLAY

#include <gst/gst.h>

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

// A display connector's kmssink, kept running across the media played on it.
//
// When each track's pipeline owned its kmssink, every track change released
// the display: the video plane went dark and the console underneath showed
// until the next track's first frame, about a third of a second on a Pi 5 --
// at every pass of a looping video.  Most of that is fixed cost: a DRM
// release delay, building the next pipeline, and decoding its first frame.
//
// Now a track's pipeline ends in an appsink (AttachFeed()), which keeps the
// track's timing -- it syncs on the track's own clock and drops late frames as
// a video sink does -- and hands each frame here as it falls due.  This
// pipeline (appsrc -> crop/scale -> kmssink) shows it at once.  Between tracks
// the last frame stays on screen, and the next track's frames replace it.
//
// It closes as soon as its last user releases it, and the console -- or a
// Virtual Matrix on the same connector, which stands aside while media holds
// it -- comes back at once.  The next video in a playlist keeps it up by
// holding it before the current one lets go (it is prepared ahead).
class VideoDisplay : public std::enable_shared_from_this<VideoDisplay> {
public:
    struct Config {
        std::string cardPath;       // "/dev/dri/card1"
        int connectorId = -1;
        std::string connectorName;  // "HDMI-A-1": for the log and warnings
        int width = 0;              // the display's mode
        int height = 0;
        VideoCropRect crop;
        std::string scaling = "fit";

        bool SameOutput(const Config& o) const;
    };

    // The running display for cfg's connector, built if there is none.  Every
    // display returned needs a Release().  nullptr if it could not be built.
    static std::shared_ptr<VideoDisplay> Acquire(const Config& cfg);
    // The same, but only a display already up with these settings; nullptr
    // rather than build one.  For a track prepared while another plays: it
    // must not take the connector from what is on it now (a Virtual Matrix
    // stands aside for as long as a display holds it).
    static std::shared_ptr<VideoDisplay> AcquireRunning(const Config& cfg);
    // A user is done.  The display closes when the last one is.
    void Release();

    // Make `appsink` a feed of this display: the caps it must deliver, its
    // timing, and the callback that hands its frames over.  With activate,
    // it becomes the feed shown, and frames from a feed attached earlier are
    // dropped from then on, so a track still tearing down can't flash up
    // between the next one's frames.  Without, its frames are dropped until
    // ActivateFeed() (a track prepared while another plays).  Returns the
    // feed's token.
    uint64_t AttachFeed(GstElement* appsink, bool activate = true);
    void ActivateFeed(uint64_t token);
    // That feed has stopped: its frames are no longer shown (the last one
    // stays on screen).  No-op if another feed has been attached since.
    void DetachFeed(uint64_t token);

    // Close every display now (fppd shutdown).
    static void ShutdownAll();

    ~VideoDisplay();

private:
    explicit VideoDisplay(const Config& cfg);

    bool Build();
    // Takes the pipeline down and gives back the lease and the connector.
    // Called with m_lock not held.
    void Teardown(const char* why);
    void Push(uint64_t token, GstSample* sample);

    static GstFlowReturn OnFeedSample(GstElement* appsink, gpointer userData);
    static GstPadProbeReturn OnFeedQuery(GstPad* pad, GstPadProbeInfo* info, gpointer userData);
    // Answer a feed's ALLOCATION query as the kmssink would.
    bool AnswerAllocation(GstQuery* query);
    static GstBusSyncReply OnBusMessage(GstBus* bus, GstMessage* msg, gpointer userData);
    static void WorkerLoop();

    const Config m_cfg;
    std::mutex m_lock;
    int m_users = 0;
    bool m_failed = false;           // the sink errored: rebuild on the next Acquire()
    bool m_closed = false;           // Teardown() has run
    // Build() is still running: a second Acquire() of the display waits for
    // it (m_builtCv) rather than attach a feed with no caps to keep to.
    bool m_building = true;
    std::condition_variable m_builtCv;
    // Torn down (it failed) while tracks still held it: the lease, which a
    // decoder's buffers from the display's pool may still be on, is closed
    // when the last of them lets go (Release()).
    bool m_releaseLeaseWhenUnused = false;
    void ReleaseLeaseAndConnector();
    uint64_t m_feedToken = 0;        // the feed allowed to show frames, 0 = none
    uint64_t m_lastToken = 0;

    GstElement* m_pipeline = nullptr;
    GstElement* m_src = nullptr;     // appsrc (ref held)
    GstCaps* m_feedCaps = nullptr;   // what the sink accepts, for each feed
    GstCaps* m_srcCaps = nullptr;    // caps last set on m_src
    DrmCards::Lease m_lease;
    bool m_claimedConnector = false;
    uint64_t m_framesShown = 0;
};

#endif
