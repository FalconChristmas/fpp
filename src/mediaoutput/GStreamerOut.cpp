/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2025 by the Falcon Player Developers.
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

#include "Warnings.h" // WarningHolder -- needed directly for NOPCH builds
#include <cmath>

#include "GStreamerOut.h"

#ifdef HAS_GSTREAMER

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <condition_variable>
#include <thread>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "common.h"
#include "log.h"
#include "StreamSlotManager.h"
#include "VideoOutputManager.h"
#include "mediadetails.h"
#include "settings.h"
#include "channeloutput/channeloutputthread.h"
#include "../MultiSync.h"
#include "overlays/PixelOverlay.h"
#include "overlays/PixelOverlayModel.h"

// Static instance pointer for callbacks
GStreamerOutput* GStreamerOutput::m_currentInstance = nullptr;
std::mutex GStreamerOutput::m_overlayOutputsLock;
std::vector<GStreamerOutput*> GStreamerOutput::m_overlayOutputs;

// ──────────────────────────────────────────────────────────────────────────────
// Wedged-decoder recovery (issue #2695).
//
// The Pi's V4L2 hardware decoder can wedge after ~24h of continuous use even
// with CMA healthy (confirmed on the 2026-07-03 soak: preroll failure with
// 402MB CmaFree).  Once wedged, two things go wrong in-process:
//   1. gst_element_set_state(NULL) on the wedged pipeline blocks FOREVER —
//      the soak's playlist thread hung 2h25m in that call and then died with
//      SIGBUS.  Teardown is therefore supervised: the state change runs on a
//      reaper thread with a deadline, and on timeout the pipeline is
//      deliberately ABANDONED (leaked, ~40 fds + threads) so the playlist
//      thread survives.  One leaked pipeline is recoverable; a hung playlist
//      thread is not.
//   2. The decoder stays wedged, so subsequent videos fail preroll too.
//      Consecutive wedge events (preroll-watchdog fires, abandoned
//      teardowns) are counted here; at the limit fppd restarts itself via
//      RestartFPPDResumingPlaylist() — a fresh process re-opens the V4L2
//      device (and picks the show back up where it left off), which
//      bounds the abandoned-pipeline fd leakage and recovers the decoder in
//      the cases we can recover.  The count resets the moment any pipeline
//      reports a valid playback position (decoder demonstrably healthy).
// ──────────────────────────────────────────────────────────────────────────────
static constexpr int PIPELINE_TEARDOWN_TIMEOUT_MS = 10000;
static std::atomic<int> s_consecutiveWedgeEvents{ 0 };
// Ensures the escalation decision (restart vs reboot) is made exactly once
// per process — additional wedge events racing in while shutdown is already
// underway must not re-record restarts or re-trigger anything.
static std::atomic<bool> s_wedgeShutdownTriggered{ false };

// Wedge-triggered restart history, kept in tmpfs: survives fppd restarts but
// is cleared by a reboot — exactly the lifetime of the VideoCore firmware
// state we're trying to recover.  On platforms without /run/fppd (macOS,
// Docker) the file is unusable and the count stays at 1, which keeps the
// reboot rung disengaged there by construction.
static constexpr const char* WEDGE_RESTART_STATE_FILE = "/run/fppd/gstreamer_wedge_restarts";
static constexpr long long WEDGE_RESTART_WINDOW_MS = 2LL * 60 * 60 * 1000; // 2 hours

// Record that a wedge-triggered fppd restart is happening now and return how
// many such restarts (including this one) occurred within the window.
static int RecordWedgeRestartAndCount() {
    long long now = GetTimeMS();
    std::vector<long long> stamps;
    {
        std::ifstream in(WEDGE_RESTART_STATE_FILE);
        std::string line;
        while (in && std::getline(in, line)) {
            long long v = atoll(line.c_str());
            if (v > 0 && v <= now && (now - v) < WEDGE_RESTART_WINDOW_MS) {
                stamps.push_back(v);
            }
        }
    }
    stamps.push_back(now);
    std::ofstream out(WEDGE_RESTART_STATE_FILE, std::ios::trunc);
    if (!out) {
        return 1;
    }
    for (long long s : stamps) {
        out << s << "\n";
    }
    return (int)stamps.size();
}

static void RecordWedgeEventAndMaybeRestart(const char* what) {
    int fails = s_consecutiveWedgeEvents.fetch_add(1) + 1;
    // 0 disables the self-restart escalation (teardown abandonment still applies)
    int limit = getSettingInt("GStreamerWedgeRestartLimit", 3);
    LogErr(VB_MEDIAOUT, "GStreamer: %s (consecutive decoder-wedge events: %d, restart limit: %d)\n",
           what, fails, limit);
    if (limit <= 0 || fails < limit) {
        return;
    }
    if (s_wedgeShutdownTriggered.exchange(true)) {
        return; // restart/reboot already in flight
    }

    // A process restart re-opens the V4L2 device, which recovers the decoder
    // when the wedge is driver-side.  But the dmesg evidence from issue #2727
    // shows the wedge can live in the VideoCore FIRMWARE (VCHIQ stops
    // answering, "invalid message context") — no userspace restart resets
    // that; only a reboot reinitializes the firmware.  So: restart first,
    // and if wedge-triggered restarts keep happening within the window, the
    // restart rung clearly isn't working — escalate to a system reboot.
    // GStreamerWedgeRebootLimit is the number of wedge-triggered restarts
    // within 2 hours that triggers the reboot (default 2: one restart gets a
    // chance to fix it, the second escalates).  0 disables the reboot rung.
    int rebootLimit = getSettingInt("GStreamerWedgeRebootLimit", 2);
    int recentRestarts = RecordWedgeRestartAndCount();
    if (rebootLimit > 0 && recentRestarts >= rebootLimit) {
        LogErr(VB_MEDIAOUT, "GStreamer: %d wedge-triggered fppd restarts within %d minutes did not recover the decoder — "
                            "VideoCore firmware is wedged (issues #2695/#2727), REBOOTING the system\n",
               recentRestarts, (int)(WEDGE_RESTART_WINDOW_MS / 60000));
        WarningHolder::AddWarning(37, "Rebooting: hardware video decoder firmware wedged; fppd restart did not recover it");
        sync();
        if (system("/usr/sbin/reboot") != 0) {
            // Reboot unavailable (container, dev box) — fall back to the
            // restart rung rather than doing nothing.
            LogErr(VB_MEDIAOUT, "GStreamer: reboot command failed, falling back to fppd restart\n");
            RestartFPPDResumingPlaylist();
            return;
        }
        // Exit cleanly while the system goes down.
        ShutdownFPPD(false);
        return;
    }

    LogErr(VB_MEDIAOUT, "GStreamer: %d consecutive pipeline wedges — hardware decoder is not recoverable in-process, restarting fppd (restart %d/%d before reboot escalation)\n",
           fails, recentRestarts, rebootLimit);
    WarningHolder::AddWarningTimeout(300, 37, "Restarting fppd: hardware media decoder wedged (repeated pipeline hangs, issue #2695)");
    // Resume whatever was playing: the restart exists to keep the show (and
    // the remotes syncing to it) alive, not just to reset the decoder.
    RestartFPPDResumingPlaylist();
}

// Drive a pipeline to GST_STATE_NULL on a reaper thread with a deadline.
// Returns true when the pipeline reached NULL in time.  Returns false when
// the transition wedged: the reaper thread keeps its own ref and either
// finishes late (logged) or blocks forever, in which case the pipeline and
// its resources are intentionally leaked.  After a false return the caller
// must NOT release the pipeline's kernel-side resources (DRM planes, shared
// DRM fd refs) — the wedged pipeline still owns them.
static bool SetPipelineToNullSupervised(GstElement* pipeline, int timeoutMs) {
    struct TeardownCtx {
        std::mutex m;
        std::condition_variable cv;
        bool done = false;
    };
    auto ctx = std::make_shared<TeardownCtx>();
    gst_object_ref(pipeline);
    std::thread([pipeline, ctx, timeoutMs]() {
        SetThreadName("FPP-GstReaper");
        uint64_t start = GetTimeMS();
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        uint64_t took = GetTimeMS() - start;
        if (took > (uint64_t)timeoutMs) {
            // The caller already gave up and abandoned this pipeline —
            // record that the kernel/GStreamer eventually let go so a
            // "late but not never" wedge is distinguishable in soak logs.
            LogWarn(VB_MEDIAOUT, "GStreamer: abandoned pipeline teardown eventually completed after %llu ms\n",
                    (unsigned long long)took);
        }
        {
            std::lock_guard<std::mutex> l(ctx->m);
            ctx->done = true;
        }
        ctx->cv.notify_all();
    }).detach();

    std::unique_lock<std::mutex> l(ctx->m);
    return ctx->cv.wait_for(l, std::chrono::milliseconds(timeoutMs), [&] { return ctx->done; });
}

// Static audio sample buffer for WLED audio-reactive effects
std::array<float, GStreamerOutput::SAMPLE_BUFFER_SIZE> GStreamerOutput::s_sampleBuffer = {};
int GStreamerOutput::s_sampleWritePos = 0;
int GStreamerOutput::s_sampleRate = 0;
std::mutex GStreamerOutput::s_sampleMutex;

// One-time GStreamer initialization
static bool gst_initialized = false;
void GStreamerOutput::EnsureGStreamerInit() {
    if (!gst_initialized) {
        LogWarn(VB_MEDIAOUT, "GStreamer: EnsureGStreamerInit() entered\n");
        // Set PipeWire env vars so pipewiresink can find the FPP PipeWire runtime.
        // Both Simple PipeWire and PipeWire Advanced share the same runtime stack.
        if (isPipeWireBackend()) {
            setenv("PIPEWIRE_RUNTIME_DIR", "/run/pipewire-fpp", 1);
            setenv("XDG_RUNTIME_DIR", "/run/pipewire-fpp", 1);
            setenv("PULSE_RUNTIME_PATH", "/run/pipewire-fpp/pulse", 1);
            LogWarn(VB_MEDIAOUT, "GStreamer: Set PipeWire env (PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp)\n");
        } else {
            std::string mediaBackend = getSetting("MediaBackend");
            LogWarn(VB_MEDIAOUT, "GStreamer: MediaBackend='%s', not setting PipeWire env\n", mediaBackend.c_str());
        }
        LogWarn(VB_MEDIAOUT, "GStreamer: Calling gst_init()...\n");
        gst_init(nullptr, nullptr);
        gst_initialized = true;
        LogWarn(VB_MEDIAOUT, "GStreamer initialized: %s\n", gst_version_string());
    }
}

// Pick the best available ALSA-direct audio sink element.
//
// We prefer alsasink (avoids autoaudiosink's slow pulsesink probe in
// ALSA-only mode), but the gstreamer1.0-alsa plugin is not present on
// every install — older/upgraded systems may lack it.  If alsasink is
// not registered, gst_parse_launch() / gst_element_factory_make() fail
// with "no element alsasink" and audio never starts.  Fall back to
// autoaudiosink so playback always works.  Result is cached; must be
// called after gst_init().
static const char* GetAlsaDirectSinkName() {
    static const char* cached = nullptr;
    if (!cached) {
        GstElementFactory* f = gst_element_factory_find("alsasink");
        if (f) {
            gst_object_unref(f);
            cached = "alsasink";
        } else {
            LogWarn(VB_MEDIAOUT, "GStreamer: alsasink not available "
                                 "(gstreamer1.0-alsa not installed) — "
                                 "falling back to autoaudiosink\n");
            cached = "autoaudiosink";
        }
    }
    return cached;
}

// Resolve a DRM connector name (e.g., "HDMI-A-1") to its card path, connector ID,
// connection status, and current display resolution by scanning sysfs.
// Works on all Pi models (Pi 3/4/5) and x86 — no libdrm ioctls needed.
GStreamerOutput::DrmConnectorInfo GStreamerOutput::ResolveDrmConnector(const std::string& connectorName) {
    DrmConnectorInfo info;

    // Scan /sys/class/drm/ for cardN-<connectorName>
    for (int cardNum = 0; cardNum < 8; cardNum++) {
        std::string sysBase = "/sys/class/drm/card" + std::to_string(cardNum) + "-" + connectorName;
        std::string statusPath = sysBase + "/status";
        if (!FileExists(statusPath))
            continue;

        info.cardPath = "/dev/dri/card" + std::to_string(cardNum);

        // Read connection status
        std::string status = GetFileContents(statusPath);
        info.connected = (status.find("connected") != std::string::npos &&
                          status.find("disconnected") == std::string::npos);

        // Read connector ID (available since Linux 5.x)
        std::string cidPath = sysBase + "/connector_id";
        if (FileExists(cidPath)) {
            std::string cidStr = GetFileContents(cidPath);
            info.connectorId = atoi(cidStr.c_str());
        }

        // Read display resolution from first available mode
        std::string modesPath = sysBase + "/modes";
        if (FileExists(modesPath)) {
            std::ifstream mf(modesPath);
            std::string firstMode;
            if (std::getline(mf, firstMode) && !firstMode.empty()) {
                // Format: "1920x1080" or "1280x720"
                size_t xpos = firstMode.find('x');
                if (xpos != std::string::npos) {
                    info.displayWidth = atoi(firstMode.substr(0, xpos).c_str());
                    info.displayHeight = atoi(firstMode.substr(xpos + 1).c_str());
                }
            }
        }

        LogDebug(VB_MEDIAOUT, "GStreamer DRM: %s on card%d connector_id=%d connected=%d display=%dx%d\n",
                connectorName.c_str(), cardNum, info.connectorId, info.connected,
                info.displayWidth, info.displayHeight);
        break;
    }

    return info;
}

// ──────────────────────────────────────────────────────────────────────────────
// Shared DRM master fd — opened once per card, shared by all kmssink elements,
// refcounted so it closes (and drops DRM master) once nothing references it
// any more.  This avoids DRM master contention between multiple pipelines
// driving different HDMI connectors on the same card, AND — critically for
// the 24h soak case — closing the fd is the kernel's cleanup backstop that
// frees any GEM handles/framebuffers a kmssink/vc4 driver failed to release
// explicitly.  Keeping the fd open forever (the old behavior) meant that
// backstop never ran, so any per-track leak in gstkmssink/vc4 accumulated in
// CMA without bound across ~300-500 build/teardown cycles.
// ──────────────────────────────────────────────────────────────────────────────
static std::mutex s_drmFdMutex;
struct DrmFdEntry {
    int fd = -1;
    int refCount = 0;
};
static std::map<std::string, DrmFdEntry> s_drmFds;  // cardPath → {fd, refCount}

int GStreamerOutput::AcquireSharedDrmFd(const std::string& cardPath) {
    std::lock_guard<std::mutex> lock(s_drmFdMutex);
    auto it = s_drmFds.find(cardPath);
    if (it != s_drmFds.end()) {
        it->second.refCount++;
        return it->second.fd;
    }

    int fd = open(cardPath.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        LogErr(VB_MEDIAOUT, "GStreamer: Failed to open DRM device %s: %s\n",
               cardPath.c_str(), FPPstrerror(errno));
        WarningHolder::AddWarning(31, "Video output: could not open DRM device " + cardPath);
        return -1;
    }

    // Become DRM master — required for modesetting (kmssink needs this)
    if (ioctl(fd, DRM_IOCTL_SET_MASTER, 0) < 0) {
        LogWarn(VB_MEDIAOUT, "GStreamer: DRM_IOCTL_SET_MASTER failed for %s: %s (another master may exist)\n",
                cardPath.c_str(), FPPstrerror(errno));
        // Continue anyway — kmssink may still work if we're root
    }

    s_drmFds[cardPath] = DrmFdEntry{fd, 1};
    LogDebug(VB_MEDIAOUT, "GStreamer: Opened shared DRM fd=%d for %s (refcount=1)\n", fd, cardPath.c_str());
    return fd;
}

void GStreamerOutput::ReleaseSharedDrmFd(const std::string& cardPath) {
    std::lock_guard<std::mutex> lock(s_drmFdMutex);
    auto it = s_drmFds.find(cardPath);
    if (it == s_drmFds.end()) {
        LogWarn(VB_MEDIAOUT, "GStreamer: ReleaseSharedDrmFd(%s) called with no matching acquire\n", cardPath.c_str());
        return;
    }
    it->second.refCount--;
    if (it->second.refCount <= 0) {
        LogDebug(VB_MEDIAOUT, "GStreamer: Closing shared DRM fd=%d for %s (refcount 0)\n",
                it->second.fd, cardPath.c_str());
        close(it->second.fd);
        s_drmFds.erase(it);
    } else {
        LogDebug(VB_MEDIAOUT, "GStreamer: Released one ref on DRM fd for %s (refcount=%d)\n",
                cardPath.c_str(), it->second.refCount);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Find a DRM OVERLAY plane for the CRTC bound to a given connector.
// Primary planes are often held by fbcon and cannot be replaced by kmssink.
// Overlay planes sit on top of the primary plane and are freely available.
// Each call returns a unique plane — tracked via s_allocatedPlanes so that
// multiple kmssink elements sharing a DRM fd don't collide.
// ──────────────────────────────────────────────────────────────────────────────
static std::set<uint32_t> s_allocatedPlanes;
// FindPrimaryPlaneForConnector / ReleasePlane are called both from the main
// media thread (GStreamerOutput pipelines) and from VideoOutputManager's
// consumer threads, so the shared set needs its own lock.
static std::mutex s_allocatedPlanesMutex;

int GStreamerOutput::FindPrimaryPlaneForConnector(int drmFd, int connectorId) {
    if (drmFd < 0 || connectorId < 0)
        return -1;

    // Enable universal planes so we see Primary/Overlay/Cursor types
    drmSetClientCap(drmFd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

    // Get connector → encoder → CRTC
    drmModeConnectorPtr conn = drmModeGetConnector(drmFd, connectorId);
    if (!conn) return -1;

    uint32_t crtcId = 0;
    if (conn->encoder_id) {
        drmModeEncoderPtr enc = drmModeGetEncoder(drmFd, conn->encoder_id);
        if (enc) {
            crtcId = enc->crtc_id;
            drmModeFreeEncoder(enc);
        }
    }
    drmModeFreeConnector(conn);

    if (!crtcId) return -1;

    // Find CRTC index in the resources list
    drmModeResPtr res = drmModeGetResources(drmFd);
    if (!res) return -1;

    int crtcIndex = -1;
    for (int i = 0; i < res->count_crtcs; i++) {
        if (res->crtcs[i] == crtcId) {
            crtcIndex = i;
            break;
        }
    }
    drmModeFreeResources(res);
    if (crtcIndex < 0) return -1;

    // Scan planes for an OVERLAY one compatible with this CRTC
    drmModePlaneResPtr planeRes = drmModeGetPlaneResources(drmFd);
    if (!planeRes) return -1;

    // Hold the lock across the scan-and-claim so two concurrent callers can't
    // both select the same free plane.
    std::lock_guard<std::mutex> planeLock(s_allocatedPlanesMutex);

    int foundPlane = -1;
    for (uint32_t i = 0; i < planeRes->count_planes && foundPlane < 0; i++) {
        uint32_t pid = planeRes->planes[i];
        if (s_allocatedPlanes.count(pid))
            continue;  // already assigned to another consumer

        drmModePlanePtr plane = drmModeGetPlane(drmFd, pid);
        if (!plane) continue;

        if (plane->possible_crtcs & (1u << crtcIndex)) {
            // Check plane type property — want OVERLAY
            drmModeObjectPropertiesPtr props =
                drmModeObjectGetProperties(drmFd, pid, DRM_MODE_OBJECT_PLANE);
            if (props) {
                for (uint32_t j = 0; j < props->count_props; j++) {
                    drmModePropertyPtr prop = drmModeGetProperty(drmFd, props->props[j]);
                    if (prop) {
                        if (strcmp(prop->name, "type") == 0 &&
                            props->prop_values[j] == DRM_PLANE_TYPE_OVERLAY) {
                            foundPlane = pid;
                        }
                        drmModeFreeProperty(prop);
                    }
                    if (foundPlane >= 0) break;
                }
                drmModeFreeObjectProperties(props);
            }
        }
        drmModeFreePlane(plane);
    }
    drmModeFreePlaneResources(planeRes);

    if (foundPlane >= 0) {
        s_allocatedPlanes.insert(foundPlane);
    }

    LogDebug(VB_MEDIAOUT, "GStreamer DRM: connector %d → CRTC %u (index %d) → overlay plane %d\n",
            connectorId, crtcId, crtcIndex, foundPlane);
    return foundPlane;
}

void GStreamerOutput::ReleasePlane(int planeId) {
    if (planeId < 0)
        return;
    std::lock_guard<std::mutex> lock(s_allocatedPlanesMutex);
    s_allocatedPlanes.erase((uint32_t)planeId);
}

// ──────────────────────────────────────────────────────────────────────────────
// Read the maximum per-member delay (ms) across all enabled PipeWire audio
// output groups.  This compensates the GStreamer ts-offset so that audio
// through the longest-delayed PipeWire path still lines up with video.
// ──────────────────────────────────────────────────────────────────────────────
static int GetMaxPipeWireGroupDelayMs() {
    std::string configPath = FPP_DIR_CONFIG("/pipewire-audio-groups.json");
    if (!FileExists(configPath))
        return 0;

    Json::Value root;
    if (!LoadJsonFromFile(configPath, root, JsonRoot::Object) || !root.isMember("groups"))
        return 0;

    int maxDelay = 0;
    for (const auto& group : root["groups"]) {
        if (!group.get("enabled", false).asBool())
            continue;
        for (const auto& member : group["members"]) {
            int delayMs = member.get("delayMs", 0).asInt();
            if (delayMs > maxDelay)
                maxDelay = delayMs;
        }
    }
    return maxDelay;
}

// ──────────────────────────────────────────────────────────────────────────────
// Sample rate the PipeWire graph clock is running at, or 0 if it can't be
// determined.  Every stream that arrives at a different rate is resampled by
// its client-side adapter for as long as it plays, so this is what the decode
// chain wants to line up with.
//
// The live daemon is the authority (a card can refine the configured rate
// upward, and force-rate can override it outright), so ask it; the generated
// conf is only a fallback for when the query fails.  Cached for the life of
// the process: the graph rate only changes when the PipeWire daemon restarts,
// and fppd restarts with it.
// ──────────────────────────────────────────────────────────────────────────────
static int GetPipeWireGraphRate() {
    static int cachedRate = -1;
    if (cachedRate >= 0)
        return cachedRate;

    // pw-metadata prints lines of the form
    //   update: id:0 key:'clock.rate' value:'44100' type:''
    auto metadataValue = [](const std::string& out, const std::string& key) -> int {
        std::string needle = "key:'" + key + "' value:'";
        size_t p = out.find(needle);
        if (p == std::string::npos)
            return 0;
        p += needle.size();
        size_t e = out.find('\'', p);
        if (e == std::string::npos)
            return 0;
        return atoi(out.substr(p, e - p).c_str());
    };

    std::string dump;
    if (FILE* p = popen("/usr/bin/pw-metadata -n settings 2>/dev/null", "r")) {
        char buf[1024];
        size_t r;
        while ((r = fread(buf, 1, sizeof(buf), p)) > 0)
            dump.append(buf, r);
        pclose(p);
    }
    // force-rate pins the graph outright; clock.rate is the default it would
    // otherwise settle on.
    int rate = metadataValue(dump, "clock.force-rate");
    if (rate <= 0)
        rate = metadataValue(dump, "clock.rate");

    if (rate <= 0) {
        // Daemon unreachable — fall back to what FPP told it to run at.  The
        // per-card file is written from the rate the hardware actually clocks
        // at and sorts after the defaults, so it wins where both exist.
        static const char* confs[] = {
            "/etc/pipewire/pipewire.conf.d/95-fpp-alsa-sink.conf",
            "/etc/pipewire/pipewire.conf.d/90-fpp.conf"
        };
        for (const char* conf : confs) {
            std::string contents = GetFileContents(conf);
            size_t p = contents.find("default.clock.rate");
            if (p == std::string::npos)
                continue;
            p = contents.find('=', p);
            if (p == std::string::npos)
                continue;
            rate = atoi(contents.c_str() + p + 1);
            if (rate > 0)
                break;
        }
    }

    cachedRate = rate > 0 ? rate : 0;
    LogDebug(VB_MEDIAOUT, "GStreamer: PipeWire graph clock rate: %d\n", cachedRate);
    return cachedRate;
}

// ──────────────────────────────────────────────────────────────────────────────
// Rate to pin the decode chain to before the tee, or 0 to leave it
// unconstrained and let the source's own rate flow through.
//
// A file whose rate differs from the graph's gets resampled by somebody; the
// only question is who.  Measured on a single-core AM335x feeding a 44100 Hz
// graph, 30 s of stereo audio, summing gstreamer + pipewire + wireplumber as a
// percentage of the one core:
//
//   source    pinned 48000     unpinned    pinned to graph rate
//   44.1 kHz     25.2%           18.0%           18.0%
//   48 kHz       20.9%           20.6%           24.0%
//   88.2 kHz       --            41.0%           34.5%
//   96 kHz       42.8%           41.2%           37.0%
//
// PipeWire's resampler is roughly three times cheaper than audioresample here,
// so leaving the rate alone wins whenever the two are close — including the
// 48 kHz-into-44.1 kHz case, where GStreamer converting costs more than the
// 9% extra samples it would save.  That flips once the source runs at twice
// the graph rate or more: carrying that many extra samples through volume, the
// tee, the sample tap and the sink outweighs the slower resampler, so drop the
// rate early.
//
// The old unconditional 48000 was the worst of both worlds on a 44.1 kHz
// graph, which is what an I2S cape or a 44.1 kHz USB card gives you:
// audioresample converted 44100 up to 48000 and PipeWire converted it straight
// back down.
// ──────────────────────────────────────────────────────────────────────────────
static int ChooseDecodeRate(int mediaRate, bool usePipeWire) {
    // Only PipeWire has a fixed graph rate to line up with.  With alsasink the
    // sink negotiates against the device directly and audioresample fills in
    // whatever conversion that needs.
    if (!usePipeWire || mediaRate <= 0)
        return 0;
    // No graph FPP configures runs below 44100, so nothing at or under 48000
    // can reach the 2x threshold — skip the query entirely for the rates
    // essentially all show media uses.
    if (mediaRate <= 48000)
        return 0;
    int graphRate = GetPipeWireGraphRate();
    if (graphRate > 0 && mediaRate >= 2 * graphRate)
        return graphRate;
    return 0;
}

// ---------------------------------------------------------------------------
// gst-pipewire channel-order quirk detection (FalconChristmas/fpp#2620)
//
// pipewiresink's caps→SPA conversion (set_default_channels() in
// gstpipewireformat.c, present through at least pipewire 1.4.2) ignores the
// GStreamer channel-mask and stamps a hard-coded PulseAudio-order position
// table onto the stream — [FL FR RL RR FC LFE SL SR] for 7.1 — while the
// samples stay in GStreamer/WAV canonical order [FL FR FC LFE RL RR SL SR].
// Every label-matched PipeWire link downstream then swaps FC/LFE with RL/RR
// (5.0/5.1 layouts are affected the same way; mono/stereo/quad tables happen
// to match; 3ch/7ch get no position table at all).
//
// Rather than keying a workaround off a version number (a distro backport
// would silently break it in either direction), probe the installed plugin
// once: connect a silent unrouted 7.1 stream and read back the channel
// positions it declares.  The verdict is cached keyed to the plugin binary's
// identity, so a plugin upgrade re-probes and a fixed plugin automatically
// disables the workaround.
static bool IsGstPipeWireChannelOrderQuirky() {
    static int cachedVerdict = -1; // -1 unknown, 0 clean, 1 quirky (per fppd run)
    if (cachedVerdict >= 0)
        return cachedVerdict == 1;
    cachedVerdict = 0; // default: no workaround unless proven needed

    // Identity of the installed gst-pipewire plugin binary
    std::string pluginSig;
    GstPlugin* plugin = gst_registry_find_plugin(gst_registry_get(), "pipewire");
    if (plugin) {
        const gchar* fn = gst_plugin_get_filename(plugin);
        if (fn) {
            struct stat st;
            if (stat(fn, &st) == 0) {
                pluginSig = std::string(fn) + ":" + std::to_string(st.st_mtime) + ":" + std::to_string(st.st_size);
            }
        }
        gst_object_unref(plugin);
    }
    if (pluginSig.empty()) {
        LogWarn(VB_MEDIAOUT, "GStreamer: cannot identify pipewire plugin; channel-order workaround disabled\n");
        return false;
    }

    // Cached verdict from a previous run for this exact plugin binary?
    const std::string cachePath = getFPPMediaDir() + "/config/gst-pipewire-quirks.json";
    if (FileExists(cachePath)) {
        Json::Value cache;
        if (LoadJsonFromFile(cachePath, cache, JsonRoot::Object)
            && cache.get("pluginSig", "").asString() == pluginSig) {
            cachedVerdict = cache.get("channelOrderQuirky", false).asBool() ? 1 : 0;
            LogInfo(VB_MEDIAOUT, "GStreamer: pipewire channel-order quirk (cached): %s\n",
                    cachedVerdict ? "PRESENT (workaround active)" : "absent");
            return cachedVerdict == 1;
        }
    }

    // Probe: silent 7.1 stream, not linked to anything (node.autoconnect=false),
    // then read the position array the plugin declared from pw-dump.
    LogInfo(VB_MEDIAOUT, "GStreamer: probing pipewire plugin for channel-order quirk...\n");
    GError* perr = nullptr;
    GstElement* probe = gst_parse_launch(
        "audiotestsrc wave=silence is-live=true ! "
        "audio/x-raw,format=S16LE,rate=48000,channels=8,channel-mask=(bitmask)0xc3f ! "
        "pipewiresink sync=false "
        "stream-properties=\"props,node.name=(string)fpp_chorder_probe,node.autoconnect=(boolean)false\"",
        &perr);
    if (!probe || perr) {
        LogWarn(VB_MEDIAOUT, "GStreamer: quirk probe pipeline failed: %s\n",
                perr ? perr->message : "unknown");
        if (perr)
            g_error_free(perr);
        if (probe)
            gst_object_unref(probe);
        return false;
    }
    gst_element_set_state(probe, GST_STATE_PLAYING);

    // Poll pw-dump for the probe node's declared EnumFormat positions
    std::vector<std::string> positions;
    for (int attempt = 0; attempt < 10 && positions.empty(); attempt++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        FILE* p = popen("/usr/bin/pw-dump 2>/dev/null", "r");
        if (!p)
            break;
        std::string out;
        char buf[8192];
        size_t r;
        while ((r = fread(buf, 1, sizeof(buf), p)) > 0)
            out.append(buf, r);
        pclose(p);
        Json::Value dump;
        if (!LoadJsonFromString(out, dump) || !dump.isArray())
            continue;
        for (const auto& obj : dump) {
            if (!obj.isObject())
                continue;
            if (obj.get("type", "").asString() != "PipeWire:Interface:Node")
                continue;
            if (obj["info"]["props"].get("node.name", "").asString() != "fpp_chorder_probe")
                continue;
            for (const auto& fmt : obj["info"]["params"]["EnumFormat"]) {
                const Json::Value& pos = fmt["position"];
                if (pos.isArray() && pos.size() == 8) {
                    for (const auto& ch : pos)
                        positions.push_back(ch.asString());
                    break;
                }
            }
        }
    }
    gst_element_set_state(probe, GST_STATE_NULL);
    gst_object_unref(probe);

    if (positions.size() != 8) {
        // PipeWire down or dump unparsable — leave workaround off, don't cache
        LogWarn(VB_MEDIAOUT, "GStreamer: channel-order quirk probe inconclusive; workaround disabled this run\n");
        return false;
    }
    int idxRL = -1, idxFC = -1;
    for (int i = 0; i < 8; i++) {
        if (positions[i] == "RL")
            idxRL = i;
        else if (positions[i] == "FC")
            idxFC = i;
    }
    bool quirky = (idxRL >= 0 && idxFC >= 0 && idxRL < idxFC);
    cachedVerdict = quirky ? 1 : 0;
    LogWarn(VB_MEDIAOUT, "GStreamer: pipewire declares 7.1 positions [%s %s %s %s %s %s %s %s] — channel-order quirk %s\n",
            positions[0].c_str(), positions[1].c_str(), positions[2].c_str(), positions[3].c_str(),
            positions[4].c_str(), positions[5].c_str(), positions[6].c_str(), positions[7].c_str(),
            quirky ? "PRESENT (pre-permute workaround active)" : "absent");

    Json::Value cache;
    cache["pluginSig"] = pluginSig;
    cache["channelOrderQuirky"] = quirky;
    cache["declaredOrder"] = Json::arrayValue;
    for (const auto& s : positions)
        cache["declaredOrder"].append(s);
    SaveJsonToFile(cache, cachePath);
    return quirky;
}

// Build the pre-permute fragment that reorders GStreamer/WAV canonical
// channel order into the PulseAudio order the quirky plugin will declare,
// so PipeWire's label-matched links route the right content.  Returns the
// extra caps to append to the rate capsfilter and the permute elements.
static void BuildChannelOrderFix(int channels, std::string& extraCaps, std::string& permuteChain) {
    struct Layout {
        int channels;
        const char* mask;
        std::vector<int> srcIdx; // pulse-order slot -> wav-order source channel
    };
    static const Layout layouts[] = {
        { 5, "0x37", { 0, 1, 3, 4, 2 } },          // [FL FR RL RR FC] <- [FL FR FC RL RR]
        { 6, "0x3f", { 0, 1, 4, 5, 2, 3 } },       // [FL FR RL RR FC LFE] <- [FL FR FC LFE RL RR]
        { 8, "0xc3f", { 0, 1, 4, 5, 2, 3, 6, 7 } } // [FL FR RL RR FC LFE SL SR] <- [FL FR FC LFE RL RR SL SR]
    };
    for (const auto& l : layouts) {
        if (l.channels != channels)
            continue;
        extraCaps = ",channels=" + std::to_string(channels) + ",channel-mask=(bitmask)" + l.mask;
        std::string matrix = "<";
        for (int row = 0; row < channels; row++) {
            matrix += (row ? ",<" : "<");
            for (int col = 0; col < channels; col++) {
                matrix += (col ? "," : "");
                matrix += (l.srcIdx[row] == col) ? "(float)1.0" : "(float)0.0";
            }
            matrix += ">";
        }
        matrix += ">";
        permuteChain = "audioconvert mix-matrix=\"" + matrix + "\" ! audio/x-raw" + extraCaps + " ! ";
        return;
    }
    extraCaps.clear();
    permuteChain.clear();
}

GStreamerOutput::GStreamerOutput(const std::string& mediaFilename, MediaOutputStatus* status, const std::string& videoOut, int streamSlot)
    : m_videoOut(videoOut), m_streamSlot(streamSlot) {
    LogWarn(VB_MEDIAOUT, "GStreamer: CTOR enter (%s, videoOut=%s, slot=%d)\n", mediaFilename.c_str(), videoOut.c_str(), streamSlot);
    m_mediaFilename = mediaFilename;
    m_mediaOutputStatus = status;
    m_allowSpeedAdjust = (getSettingInt("remoteIgnoreSync") == 0);
    EnsureGStreamerInit();
    LogWarn(VB_MEDIAOUT, "GStreamer: CTOR done (%s)\n", mediaFilename.c_str());
}

GStreamerOutput::~GStreamerOutput() {
    Close();
}

int GStreamerOutput::Start(int msTime) {
    LogWarn(VB_MEDIAOUT, "GStreamer: Start(%d) enter - %s\n", msTime, m_mediaFilename.c_str());

    // Flush PipeWire filter-chain delay ring-buffers EARLY in Start().
    // This runs as a fire-and-forget thread (pw-cli calls take ~200ms+).
    // By calling it here instead of right before set_state(PLAYING),
    // the flush has the entire pipeline-build time (~50-100ms) to complete
    // before audio actually starts flowing.
    FlushPipeWireDelayBuffers();

    // Fresh pipeline — allow Stop()'s teardown to run for this track
    m_teardownComplete = false;

    // Fresh lifetime guard for this pipeline's decodebin pad callbacks.  Any
    // connection left over from a previous pipeline holds the previous guard,
    // which Close() already cleared, so it can never reach this object again.
    m_cbGuard = std::make_shared<CallbackGuard>();
    m_cbGuard->self = this;

    // Reset MultiSync rate-matching state for the new track
    m_currentRate = 1.0f;
    m_diffsSize = 0;
    m_diffIdx = 0;
    m_diffSum = 0;
    m_rateSum = 0.0f;
    m_lastDiff = -1;
    m_rateDiff = 0;
    m_lastRates.clear();
    m_lastRatesSum = 0.0f;

    // Build full path — check music dir, then video dir (mirrors SDLOutput)
    std::string fullPath = m_mediaFilename;
    if (!FileExists(fullPath)) {
        fullPath = FPP_DIR_MUSIC("/" + m_mediaFilename);
    }
    if (!FileExists(fullPath)) {
        fullPath = FPP_DIR_VIDEO("/" + m_mediaFilename);
    }
    if (!FileExists(fullPath)) {
        LogErr(VB_MEDIAOUT, "GStreamer: media file not found: %s\n", m_mediaFilename.c_str());
        WarningHolder::AddWarningTimeout(60, 30, "Media file not found: " + m_mediaFilename);
        return 0;
    }

    // Pre-populate duration from file metadata so that
    // PlaylistEntryMedia::GetLengthInMS() returns a valid value immediately.
    // Without this, the playlist status polls GetElapsedMS() before GStreamer
    // has queried the duration, causing m_duration to track elapsed time
    // and seconds_remaining to always report 0.
    int mediaChannels = 0; // used by the pipewire channel-order workaround below
    int mediaRate = 0;     // used by the decode-rate choice below
    {
        MediaDetails details;
        details.ParseMedia(fullPath.c_str());
        mediaChannels = details.channels;
        mediaRate = details.sampleRate;
        if (details.lengthMS > 0) {
            int totalSecs = details.lengthMS / 1000;
            m_mediaOutputStatus->minutesTotal = totalSecs / 60;
            m_mediaOutputStatus->secondsTotal = totalSecs % 60;
            m_maxDuration = (gint64)details.lengthMS * GST_MSECOND;
            LogDebug(VB_MEDIAOUT, "GStreamer: pre-set duration from metadata: %d:%02d (%d ms)\n",
                    m_mediaOutputStatus->minutesTotal, m_mediaOutputStatus->secondsTotal, details.lengthMS);
        }
    }

    // Read PipeWire video routing setting early — it affects whether we
    // still need a video pipeline even when the primary HDMI is disconnected.
    // Only honour it when PipeWire is actually the audio backend; in ALSA-only
    // mode PipeWire isn't running so pipewiresink would fail to connect and
    // block the pipeline (causing audio stall / playback abort).
    std::string mediaBackend = toLowerCopy(getSetting("MediaBackend"));
    bool usePipeWireBackendLocal = (mediaBackend == "pipewire" || mediaBackend == "pipewire-simple");
    if (usePipeWireBackendLocal) {
        m_pwVideoSinkName = getSetting("PipeWireVideoSinkName");
        if (m_streamSlot > 1) {
            std::string slotSetting = "PipeWireVideoSinkName_" + std::to_string(m_streamSlot);
            std::string slotVideoSinkName = getSetting(slotSetting.c_str());
            if (!slotVideoSinkName.empty()) {
                m_pwVideoSinkName = slotVideoSinkName;
            }
        }
    } else {
        m_pwVideoSinkName.clear();
    }
    if (!m_pwVideoSinkName.empty()) {
        LogDebug(VB_MEDIAOUT, "GStreamer: PipeWireVideoSinkName='%s' (slot %d) — video will route through PipeWire\n",
                m_pwVideoSinkName.c_str(), m_streamSlot);
    }

    // Determine if we need a video overlay branch or HDMI output
    bool wantVideo = false;  // PixelOverlay mode
    bool wantHDMI = false;   // DRM/KMS HDMI mode

    if (m_videoOut != "--Disabled--" && !m_videoOut.empty()) {
        // Check if this is an HDMI/DRM output connector
        if (m_videoOut.starts_with("HDMI-") || m_videoOut.starts_with("DSI-") ||
            m_videoOut.starts_with("Composite-") ||
            m_videoOut == "--HDMI--" || m_videoOut == "--hdmi--" || m_videoOut == "HDMI") {
            // Resolve the connector name
            std::string connectorName = m_videoOut;
            if (connectorName == "--HDMI--" || connectorName == "--hdmi--" || connectorName == "HDMI") {
                connectorName = "HDMI-A-1";
            }
            DrmConnectorInfo drmInfo = ResolveDrmConnector(connectorName);
            if (drmInfo.connected && drmInfo.connectorId >= 0) {
                if (!m_pwVideoSinkName.empty()) {
                    // PipeWire video routing is active — VideoOutput is a legacy
                    // setting that shouldn't drive a direct kmssink when PipeWire
                    // groups define all outputs.  Build the video pipeline (so the
                    // pipewiresink receives decoded frames) but don't create a
                    // primary kmssink; consumer kmssinks in VideoOutputManager
                    // (one per group member) own every HDMI connector.
                    wantHDMI = true;
                    LogDebug(VB_MEDIAOUT, "GStreamer: PipeWire video routing active — ignoring VideoOutput=%s for direct kmssink, consumers handle HDMI\n",
                            connectorName.c_str());
                } else {
                    m_wantHDMI = true;
                    wantHDMI = true;
                    m_hdmiConnectorId = drmInfo.connectorId;
                    m_hdmiCardPath = drmInfo.cardPath;
                    m_hdmiDisplayWidth = drmInfo.displayWidth;
                    m_hdmiDisplayHeight = drmInfo.displayHeight;
                    LogDebug(VB_MEDIAOUT, "GStreamer HDMI output: connector=%s id=%d card=%s resolution=%dx%d\n",
                            connectorName.c_str(), m_hdmiConnectorId, m_hdmiCardPath.c_str(),
                            m_hdmiDisplayWidth, m_hdmiDisplayHeight);
                }
            } else if (!drmInfo.connected) {
                // Configured connector not connected — do NOT fall back to
                // another connector.  Each HDMI output is independently
                // configured; stealing a different connector would conflict
                // with whatever that connector is supposed to display.
                if (!m_pwVideoSinkName.empty()) {
                    // PipeWire video routing is active — still build the
                    // video pipeline (without kmssink) so consumers on
                    // other connectors can display the stream.
                    // Don't set m_hdmiDisplayWidth/Height — without a local
                    // kmssink there's no need to software-scale in the main
                    // pipeline.  Consumer kmssinks do DRM hardware scaling.
                    wantHDMI = true;
                    LogWarn(VB_MEDIAOUT, "GStreamer: %s is not connected — building video pipeline without kmssink (PipeWire consumers will handle output)\n", connectorName.c_str());
                } else {
                    LogWarn(VB_MEDIAOUT, "GStreamer: %s is not connected, disabling video\n", connectorName.c_str());
                }
            } else {
                LogWarn(VB_MEDIAOUT, "GStreamer: could not resolve connector ID for %s\n", connectorName.c_str());
            }
        } else {
            // PixelOverlay model name
            wantVideo = true;
        }
    }

    if (wantVideo) {
        // Register listener and get model
        PixelOverlayManager::INSTANCE.addModelListener(m_videoOut, "GStreamerOut",
            [this](PixelOverlayModel* m) {
                std::lock_guard<std::mutex> lock(m_videoOverlayModelLock);
                m_videoOverlayModel = m;
            });
        m_videoOverlayModel = PixelOverlayManager::INSTANCE.getModel(m_videoOut);
        if (m_videoOverlayModel) {
            m_videoOverlayModel->getSize(m_videoOverlayWidth, m_videoOverlayHeight);
            LogDebug(VB_MEDIAOUT, "GStreamer video overlay: model=%s size=%dx%d\n",
                    m_videoOut.c_str(), m_videoOverlayWidth, m_videoOverlayHeight);
            {
                std::lock_guard<std::mutex> lock(m_overlayOutputsLock);
                if (std::find(m_overlayOutputs.begin(), m_overlayOutputs.end(), this) == m_overlayOutputs.end()) {
                    m_overlayOutputs.push_back(this);
                }
            }
        } else {
            LogWarn(VB_MEDIAOUT, "GStreamer: PixelOverlay model '%s' not found, skipping video\n",
                    m_videoOut.c_str());
            wantVideo = false;
        }
    }

    // Build the pipeline
    // Audio: filesrc ! decodebin ! audioconvert ! audioresample ! tee name=t
    //   t. ! queue ! volume ! pipewiresink
    //   t. ! queue ! audioconvert ! F32LE,1ch ! appsink (WLED tap)
    // Video (when overlay model available):
    //   decodebin pad-added -> videoconvert ! videoscale ! capsfilter(RGB,WxH) ! appsink
    //
    // When video is needed, we must use decodebin's pad-added signal for dynamic linking,
    // since decodebin creates pads on-the-fly for each stream type.
    // We still use gst_parse_launch for the audio chain and manually add the video chain.

    LogWarn(VB_MEDIAOUT, "GStreamer: Start() building pipeline...");

    bool usePipeWire = usePipeWireBackendLocal;

    // Rate to hand the sink, or 0 to pass the file's own rate straight through
    // (see ChooseDecodeRate).  Shared by all three pipeline shapes below.
    int decodeRate = ChooseDecodeRate(mediaRate, usePipeWire);
    if (decodeRate) {
        LogDebug(VB_MEDIAOUT, "GStreamer: media rate %d Hz, resampling to the %d Hz graph rate\n",
                 mediaRate, decodeRate);
    } else {
        LogDebug(VB_MEDIAOUT, "GStreamer: media rate %d Hz passed through unconstrained\n", mediaRate);
    }

    std::string pipelineSinkName;
    if (usePipeWire) {
        pipelineSinkName = getSetting("PipeWireSinkName");

        // For multi-stream slots > 1, check for a per-slot PipeWire sink setting.
        // Format: PipeWireSinkName_2, PipeWireSinkName_3, etc.
        // If not set, falls back to the global PipeWireSinkName.
        if (m_streamSlot > 1) {
            std::string slotSetting = "PipeWireSinkName_" + std::to_string(m_streamSlot);
            std::string slotSinkName = getSetting(slotSetting.c_str());
            if (!slotSinkName.empty()) {
                pipelineSinkName = slotSinkName;
            }
        }
    }
    LogWarn(VB_MEDIAOUT, "GStreamer: PipeWireSinkName='%s' (slot %d, backend=%s)\n",
            pipelineSinkName.c_str(), m_streamSlot, mediaBackend.c_str());

    // Log PipeWire group delay for reference (handled natively by PipeWire
    // filter-chain delay nodes, not by GStreamer ts-offset).
    int maxGroupDelayMs = GetMaxPipeWireGroupDelayMs();
    LogDebug(VB_MEDIAOUT, "GStreamer: PipeWire maxGroupDelay=%dms (handled by PipeWire filter-chain)\n",
            maxGroupDelayMs);

    // PipeWireVideoSinkName was already read above (before connector check)

    // Stream identity: use slot number for PipeWire node naming
    std::string streamNodeName = StreamSlotManager::GetNodeName(m_streamSlot);
    std::string streamNodeDesc = StreamSlotManager::GetNodeDescription(m_streamSlot);

    std::string sinkStr;
    if (!pipelineSinkName.empty()) {
        // stream-properties cannot be set inline in gst_parse_launch (GstStructure
        // values with spaces break the parser).  Set it post-launch instead.
        sinkStr = "pipewiresink name=pwsink sync=true target-object=" + pipelineSinkName;
    } else if (usePipeWire) {
        // PipeWire backend with no specific sink configured — use pipewiresink
        // and let PipeWire route to the default output.
        sinkStr = "pipewiresink name=pwsink sync=true";
    } else {
        // Prefer alsasink directly — autoaudiosink probes pulsesink first
        // which fails slowly in ALSA-only mode, causing startup delay.
        // buffer-time=500000 (500ms) gives USB audio devices adequate ring
        // buffer headroom to avoid underruns during initial isochronous
        // scheduling.  Falls back to autoaudiosink if the gstreamer1.0-alsa
        // plugin is not installed (otherwise the pipeline fails to build).
        const char* alsaSink = GetAlsaDirectSinkName();
        if (strcmp(alsaSink, "alsasink") == 0) {
            sinkStr = "alsasink buffer-time=500000";
        } else {
            sinkStr = alsaSink;
        }
    }

    GError* error = nullptr;

    if (wantVideo) {
        // Build pipeline with decodebin pad-added for dynamic audio+video linking
        std::string pipelineStr =
            "filesrc location=\"" + fullPath + "\" ! decodebin name=decoder";

        LogDebug(VB_MEDIAOUT, "GStreamer pipeline (video): %s\n", pipelineStr.c_str());
        m_pipeline = gst_parse_launch(pipelineStr.c_str(), &error);
        if (error) {
            LogErr(VB_MEDIAOUT, "GStreamer pipeline error: %s\n", error->message);
            g_error_free(error);
            return 0;
        }

        // Build audio sub-chain: audioconvert ! audioresample ! tee ! ...
        GstElement* audioconvert = gst_element_factory_make("audioconvert", "aconv");
        GstElement* audioresample = gst_element_factory_make("audioresample", "aresample");
        GstElement* tee = gst_element_factory_make("tee", "t");
        GstElement* queue1 = gst_element_factory_make("queue", "q1");
        // Generous buffer for PipeWire connection startup.
        g_object_set(queue1, "max-size-time", (guint64)(5 * GST_SECOND),
                     "max-size-buffers", 0, "max-size-bytes", 0, NULL);
        m_volume = gst_element_factory_make("volume", "vol");
        GstElement* sink = nullptr;
        if (!pipelineSinkName.empty()) {
            sink = gst_element_factory_make("pipewiresink", "pwsink");
            g_object_set(sink, "target-object", pipelineSinkName.c_str(), NULL);
            // sync=TRUE: gate audio buffer delivery by PTS against
            // GstSystemClock (the pipeline clock we force above).
            // Both pipewiresink and kmssink sync against the same system
            // clock, so A/V stay aligned.  No ts-offset needed — PipeWire
            // filter-chain delay nodes handle inter-member alignment,
            // and PipeWire quantum latency (~21ms) ≈ DRM vsync (~16ms).
            g_object_set(sink, "sync", TRUE, NULL);
            // Set stream identity and disable channel remixing so PipeWire
            // preserves the source file's channel order (prevents random
            // FL-FR / RL-RR / SL-SR swaps on multi-channel devices).
            GstStructure* props = gst_structure_new("props",
                "node.name", G_TYPE_STRING, streamNodeName.c_str(),
                "node.description", G_TYPE_STRING, streamNodeDesc.c_str(),
                "stream.dont-remix", G_TYPE_BOOLEAN, TRUE,
                "channelmix.disable", G_TYPE_BOOLEAN, TRUE,
                NULL);
            g_object_set(sink, "stream-properties", props, NULL);
            gst_structure_free(props);
        } else {
            // Prefer alsasink directly — avoids autoaudiosink probe delay.
            // Fall back to autoaudiosink if gstreamer1.0-alsa is missing.
            const char* alsaSinkName = GetAlsaDirectSinkName();
            sink = gst_element_factory_make(alsaSinkName, "audiosink");
            if (sink && strcmp(alsaSinkName, "alsasink") == 0) {
                g_object_set(sink, "buffer-time", (gint64)500000, NULL);
            }
        }
        GstElement* queue2 = gst_element_factory_make("queue", "q2");
        GstElement* audioconvert2 = gst_element_factory_make("audioconvert", "aconv2");
        GstElement* capsfilterAudio = gst_element_factory_make("capsfilter", "acapsf");
        m_appsink = gst_element_factory_make("appsink", "sampletap");

        // Set audio tap caps: F32LE mono
        GstCaps* audioCaps = gst_caps_new_simple("audio/x-raw",
            "format", G_TYPE_STRING, "F32LE",
            "channels", G_TYPE_INT, 1, NULL);
        g_object_set(capsfilterAudio, "caps", audioCaps, NULL);
        gst_caps_unref(audioCaps);

        // Configure queues
        g_object_set(queue2, "max-size-buffers", 3, "leaky", 2 /* downstream */, NULL);

        // Configure audio appsink
        g_object_set(m_appsink, "emit-signals", TRUE, "sync", FALSE,
                     "max-buffers", 3, "drop", TRUE, NULL);

        GstElement* rateCapsfilter = gst_element_factory_make("capsfilter", "ratecaps");
        GstCaps* rateCaps = decodeRate
                                ? gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, decodeRate, NULL)
                                : gst_caps_new_empty_simple("audio/x-raw");
        g_object_set(rateCapsfilter, "caps", rateCaps, NULL);
        gst_caps_unref(rateCaps);

        // Add audio elements to pipeline
        gst_bin_add_many(GST_BIN(m_pipeline), audioconvert, audioresample, rateCapsfilter, tee,
                         queue1, m_volume, sink,
                         queue2, audioconvert2, capsfilterAudio, m_appsink, NULL);

        // Take our own refs on elements we keep pointers to.
        // gst_bin_add sinks the floating ref; we need our own ref so Close() can safely unref.
        gst_object_ref(m_volume);
        gst_object_ref(m_appsink);

        // Link audio chain
        if (!gst_element_link_many(audioconvert, audioresample, rateCapsfilter, tee, NULL)) {
            LogErr(VB_MEDIAOUT, "GStreamer: Failed to link audioconvert->audioresample->ratecaps->tee\n");
        }
        if (!gst_element_link_many(queue1, m_volume, sink, NULL)) {
            LogErr(VB_MEDIAOUT, "GStreamer: Failed to link queue1->volume->sink\n");
        }
        if (!gst_element_link_many(queue2, audioconvert2, capsfilterAudio, m_appsink, NULL)) {
            LogErr(VB_MEDIAOUT, "GStreamer: Failed to link queue2->audioconvert2->capsfilter->appsink\n");
        }

        // Link tee to both queues
        GstPad* teeSrc1 = gst_element_request_pad_simple(tee, "src_%u");
        GstPad* q1Sink = gst_element_get_static_pad(queue1, "sink");
        gst_pad_link(teeSrc1, q1Sink);
        gst_object_unref(teeSrc1);
        gst_object_unref(q1Sink);

        GstPad* teeSrc2 = gst_element_request_pad_simple(tee, "src_%u");
        GstPad* q2Sink = gst_element_get_static_pad(queue2, "sink");
        gst_pad_link(teeSrc2, q2Sink);
        gst_object_unref(teeSrc2);
        gst_object_unref(q2Sink);

        // Remember the audio chain entry point for pad-added linkage
        m_audioChain = audioconvert;

        // Attach AES67 zero-hop RTP branches if any send instances are active
#ifdef HAS_AES67_GSTREAMER
        AttachAES67Branches(tee);
#endif

        // Build video sub-chain: videoconvert ! videoscale ! capsfilter(RGB,WxH) ! appsink
        GstElement* videoconvert = gst_element_factory_make("videoconvert", "vconv");
        GstElement* videoscale = gst_element_factory_make("videoscale", "vscale");
        GstElement* capsfilterVideo = gst_element_factory_make("capsfilter", "vcapsf");
        GstElement* videoQueue = gst_element_factory_make("queue", "vq");
        m_videoAppsink = gst_element_factory_make("appsink", "videosink");

        // Set video caps: RGB at overlay model dimensions
        GstCaps* videoCaps = gst_caps_new_simple("video/x-raw",
            "format", G_TYPE_STRING, "RGB",
            "width", G_TYPE_INT, m_videoOverlayWidth,
            "height", G_TYPE_INT, m_videoOverlayHeight, NULL);
        g_object_set(capsfilterVideo, "caps", videoCaps, NULL);
        gst_caps_unref(videoCaps);

        // Configure video appsink: emit signals, sync=TRUE to pace delivery at real-time speed.
        // Without sync=TRUE, all frames are consumed immediately and position jumps to EOF,
        // falsely triggering stall detection (especially for video-only files).
        // drop=TRUE ensures old frames are discarded if ProcessVideoOverlay can't keep up.
        g_object_set(m_videoAppsink, "emit-signals", TRUE, "sync", TRUE,
                     "max-buffers", 2, "drop", TRUE, NULL);

        // Add video elements to pipeline
        gst_bin_add_many(GST_BIN(m_pipeline), videoQueue, videoconvert, videoscale,
                         capsfilterVideo, m_videoAppsink, NULL);

        // Take our own ref on the video appsink pointer
        gst_object_ref(m_videoAppsink);

        // Link video chain
        if (!gst_element_link_many(videoQueue, videoconvert, videoscale, capsfilterVideo,
                              m_videoAppsink, NULL)) {
            LogErr(VB_MEDIAOUT, "GStreamer: Failed to link video chain\n");
        }

        // Remember the video chain entry point for pad-added linkage
        m_videoChain = videoQueue;

        // Connect decodebin pad-added signal for dynamic linking
        GstElement* decoder = gst_bin_get_by_name(GST_BIN(m_pipeline), "decoder");
        ConnectPadSignals(decoder, true);
        gst_object_unref(decoder);

    } else if (wantHDMI) {
        // ---------------------------------------------------------------
        // HDMI/DRM video output via kmssink (Phase 4)
        // Pipeline: filesrc ! decodebin name=decoder
        //   Audio: pad-added -> audioconvert ! audioresample ! tee
        //     tee -> queue ! volume ! pipewiresink
        //     tee -> queue ! audioconvert ! F32LE,1ch ! appsink (WLED tap)
        //   Video: pad-added -> queue ! videoconvert ! videoscale ! capsfilter ! kmssink
        //
        // decodebin auto-selects the best available decoder:
        //   Pi 5: v4l2slh265dec for H.265, avdec_h264 for H.264
        //   Pi 4: v4l2 stateless H.264 if kernel supports, else avdec_h264
        //   Pi 3/Zero2: avdec_h264 software decode
        //   All platforms: avdec_h264 from gstreamer1.0-libav as universal fallback
        // ---------------------------------------------------------------
        std::string pipelineStr =
            "filesrc location=\"" + fullPath + "\" ! decodebin name=decoder";

        LogDebug(VB_MEDIAOUT, "GStreamer pipeline (HDMI): %s  connector-id=%d card=%s\n",
                 pipelineStr.c_str(), m_hdmiConnectorId, m_hdmiCardPath.c_str());
        m_pipeline = gst_parse_launch(pipelineStr.c_str(), &error);
        if (error) {
            LogErr(VB_MEDIAOUT, "GStreamer HDMI pipeline error: %s\n", error->message);
            g_error_free(error);
            return 0;
        }

        // Build audio sub-chain (same as overlay mode)
        GstElement* audioconvert = gst_element_factory_make("audioconvert", "aconv");
        GstElement* audioresample = gst_element_factory_make("audioresample", "aresample");
        GstElement* tee = gst_element_factory_make("tee", "t");
        GstElement* queue1 = gst_element_factory_make("queue", "q1");
        // Generous buffer for PipeWire connection startup.
        g_object_set(queue1, "max-size-time", (guint64)(5 * GST_SECOND),
                     "max-size-buffers", 0, "max-size-bytes", 0, NULL);
        m_volume = gst_element_factory_make("volume", "vol");
        GstElement* sink = nullptr;
        if (!pipelineSinkName.empty()) {
            sink = gst_element_factory_make("pipewiresink", "pwsink");
            g_object_set(sink, "target-object", pipelineSinkName.c_str(), NULL);
            // sync=TRUE: same rationale as the wantVideo branch above.
            g_object_set(sink, "sync", TRUE, NULL);
            // Set stream identity and disable channel remixing so PipeWire
            // preserves the source file's channel order.
            GstStructure* props = gst_structure_new("props",
                "node.name", G_TYPE_STRING, streamNodeName.c_str(),
                "node.description", G_TYPE_STRING, streamNodeDesc.c_str(),
                "stream.dont-remix", G_TYPE_BOOLEAN, TRUE,
                "channelmix.disable", G_TYPE_BOOLEAN, TRUE,
                NULL);
            g_object_set(sink, "stream-properties", props, NULL);
            gst_structure_free(props);
        } else {
            // Prefer alsasink directly — avoids autoaudiosink probe delay.
            // Fall back to autoaudiosink if gstreamer1.0-alsa is missing.
            const char* alsaSinkName = GetAlsaDirectSinkName();
            sink = gst_element_factory_make(alsaSinkName, "audiosink");
            if (sink && strcmp(alsaSinkName, "alsasink") == 0) {
                g_object_set(sink, "buffer-time", (gint64)500000, NULL);
            }
        }
        GstElement* queue2 = gst_element_factory_make("queue", "q2");
        GstElement* audioconvert2 = gst_element_factory_make("audioconvert", "aconv2");
        GstElement* capsfilterAudio = gst_element_factory_make("capsfilter", "acapsf");
        m_appsink = gst_element_factory_make("appsink", "sampletap");

        // Set audio tap caps: F32LE mono
        GstCaps* audioCaps = gst_caps_new_simple("audio/x-raw",
            "format", G_TYPE_STRING, "F32LE",
            "channels", G_TYPE_INT, 1, NULL);
        g_object_set(capsfilterAudio, "caps", audioCaps, NULL);
        gst_caps_unref(audioCaps);

        g_object_set(queue2, "max-size-buffers", 3, "leaky", 2 /* downstream */, NULL);
        g_object_set(m_appsink, "emit-signals", TRUE, "sync", FALSE,
                     "max-buffers", 3, "drop", TRUE, NULL);

        GstElement* rateCapsfilter = gst_element_factory_make("capsfilter", "ratecaps");
        GstCaps* rateCaps = decodeRate
                                ? gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, decodeRate, NULL)
                                : gst_caps_new_empty_simple("audio/x-raw");
        g_object_set(rateCapsfilter, "caps", rateCaps, NULL);
        gst_caps_unref(rateCaps);

        gst_bin_add_many(GST_BIN(m_pipeline), audioconvert, audioresample, rateCapsfilter, tee,
                         queue1, m_volume, sink,
                         queue2, audioconvert2, capsfilterAudio, m_appsink, NULL);

        gst_object_ref(m_volume);
        gst_object_ref(m_appsink);

        if (!gst_element_link_many(audioconvert, audioresample, rateCapsfilter, tee, NULL)) {
            LogErr(VB_MEDIAOUT, "GStreamer HDMI: Failed to link audioconvert->audioresample->ratecaps->tee\n");
        }
        if (!gst_element_link_many(queue1, m_volume, sink, NULL)) {
            LogErr(VB_MEDIAOUT, "GStreamer HDMI: Failed to link queue1->volume->sink\n");
        }
        if (!gst_element_link_many(queue2, audioconvert2, capsfilterAudio, m_appsink, NULL)) {
            LogErr(VB_MEDIAOUT, "GStreamer HDMI: Failed to link audio appsink chain\n");
        }

        GstPad* teeSrc1 = gst_element_request_pad_simple(tee, "src_%u");
        GstPad* q1Sink = gst_element_get_static_pad(queue1, "sink");
        gst_pad_link(teeSrc1, q1Sink);
        gst_object_unref(teeSrc1);
        gst_object_unref(q1Sink);

        GstPad* teeSrc2 = gst_element_request_pad_simple(tee, "src_%u");
        GstPad* q2Sink = gst_element_get_static_pad(queue2, "sink");
        gst_pad_link(teeSrc2, q2Sink);
        gst_object_unref(teeSrc2);
        gst_object_unref(q2Sink);

        m_audioChain = audioconvert;

        // Attach AES67 zero-hop RTP branches if any send instances are active
#ifdef HAS_AES67_GSTREAMER
        AttachAES67Branches(tee);
#endif

        // Build video sub-chain: queue ! videoconvert ! videoscale ! capsfilter ! kmssink
        // When PipeWire video routing is active, a tee is inserted before kmssink
        // so a deferred pipewiresink can expose the stream in the PipeWire graph.
        // All kmssink elements share a single DRM master fd via AcquireSharedDrmFd()
        // so multiple HDMI outputs on the same card work simultaneously.
        bool haveHdmiConnector = (m_hdmiConnectorId >= 0);
        GstElement* videoQueue = gst_element_factory_make("queue", "vq");

        if (haveHdmiConnector) {
            m_kmssink = gst_element_factory_make("kmssink", "kmsvideosink");
            if (!m_kmssink) {
                LogErr(VB_MEDIAOUT, "GStreamer: kmssink element not available — is gstreamer1.0-plugins-bad installed?\n");
                WarningHolder::AddWarning(31, "Video output unavailable: kmssink element missing (install gstreamer1.0-plugins-bad)");
                // Release the extra refs taken above (~827-828) before discarding
                // the pipeline.  Close() only unrefs m_volume/m_appsink when
                // m_pipeline is still set, and we're about to null it out below,
                // so without this those two refs leak every time kmssink is
                // unavailable.
                if (m_volume) {
                    gst_object_unref(m_volume);
                    m_volume = nullptr;
                }
                if (m_appsink) {
                    gst_object_unref(m_appsink);
                    m_appsink = nullptr;
                }
                gst_object_unref(m_pipeline);
                m_pipeline = nullptr;
                return 0;
            }
            int sharedFd = AcquireSharedDrmFd(m_hdmiCardPath);
            if (sharedFd >= 0) {
                m_acquiredDrmCards.push_back(m_hdmiCardPath);
                g_object_set(m_kmssink,
                             "fd", sharedFd,
                             "connector-id", m_hdmiConnectorId,
                             "restore-crtc", TRUE,
                             "skip-vsync", TRUE,
                             NULL);
                int planeId = FindPrimaryPlaneForConnector(sharedFd, m_hdmiConnectorId);
                if (planeId >= 0) {
                    g_object_set(m_kmssink, "plane-id", planeId, NULL);
                    m_allocatedPlanes.push_back(planeId);
                }
            } else {
                g_object_set(m_kmssink,
                             "driver-name", "vc4",
                             "connector-id", m_hdmiConnectorId,
                             "restore-crtc", TRUE,
                             "skip-vsync", TRUE,
                             NULL);
            }
        }

        // kmssink handles format conversion + scaling in hardware.
        // No capsfilter needed — let sinks negotiate with the decoder directly.

        if (!m_pwVideoSinkName.empty()) {
            // ── PipeWire video routing (tee + deferred pipewiresink) ──
            // When HDMI is connected, the primary pipeline drives it via kmssink.
            // When HDMI is not connected, vtee routes only to pipewiresink.
            // In both cases a tee exposes the stream in the PipeWire graph for
            // overlay consumers and secondary HDMI outputs.
            m_videoPipeWireRouting = true;

            GstElement* vtee = gst_element_factory_make("tee", "vtee");
            g_object_set(vtee, "allow-not-linked", TRUE, NULL);

            // Link videoQueue directly to vtee — skip videoconvert/videoscale.
            // kmssink accepts the HW decoder's native output (NV12 DMA buffers)
            // and handles format conversion + scaling in hardware.  Putting
            // videoconvert/videoscale before the tee forces CPU-side conversion
            // of every frame (~100% CPU on Pi4).  Paths that need specific
            // formats (e.g., pipewiresink) include their own converter.
            gst_bin_add_many(GST_BIN(m_pipeline), videoQueue, vtee, NULL);
            if (!gst_element_link_many(videoQueue, vtee, NULL)) {
                LogErr(VB_MEDIAOUT, "GStreamer video: Failed to link vq → vtee\n");
            }

            if (haveHdmiConnector) {
                // Link tee → kmsQueue → kmssink via request pad
                GstElement* kmsQueue = gst_element_factory_make("queue", "vkmsq");
                // Passthrough for HW-decoded NV12/DMA; required for software
                // decode (I420 won't negotiate against the DRM plane).
                GstElement* teeKmsConvert = gst_element_factory_make("videoconvert", "vteekmsconv");
                // The primary display can be one half of a split too, so it
                // takes a crop from the same config the consumers use.
                GstElement* kmsCrop = VideoOutputManager::CreateCropElement(
                    VideoOutputManager::Instance().GetHdmiCropForConnector(m_streamSlot, m_hdmiConnectorId),
                    "vkmscrop");
                gst_bin_add_many(GST_BIN(m_pipeline), kmsQueue, teeKmsConvert, m_kmssink, NULL);
                GstPad* teeSrc = gst_element_request_pad_simple(vtee, "src_%u");
                GstPad* kmsQSink = gst_element_get_static_pad(kmsQueue, "sink");
                gst_pad_link(teeSrc, kmsQSink);
                gst_object_unref(teeSrc);
                gst_object_unref(kmsQSink);
                if (kmsCrop) {
                    gst_bin_add(GST_BIN(m_pipeline), kmsCrop);
                    gst_element_link_many(kmsQueue, teeKmsConvert, kmsCrop, m_kmssink, NULL);
                } else {
                    gst_element_link_many(kmsQueue, teeKmsConvert, m_kmssink, NULL);
                }
                LogDebug(VB_MEDIAOUT, "GStreamer: video tee active — kmssink direct, pipewiresink deferred\n");
            } else {
                // No primary HDMI — vtee routes to deferred pipewiresink
                // and direct consumer kmssinks linked below.
                LogDebug(VB_MEDIAOUT, "GStreamer: video tee active — no primary kmssink (HDMI disconnected)\n");
            }

            // ── Consumer direct kmssink branches ──
            // Added to pipeline BEFORE PLAYING so video renders from frame 0.
            // Previously these were in the deferred thread, but that caused
            // a ~500ms-1s gap where vtee dropped all frames (decoder raced
            // ahead and the first rendered frame was already ~1 second in).
            {
                std::set<int> excludeConnectors;
                if (m_hdmiConnectorId >= 0)
                    excludeConnectors.insert(m_hdmiConnectorId);

                auto hdmiConsumers = VideoOutputManager::Instance().GetHdmiConsumers(
                    m_streamSlot, excludeConnectors);

                for (const auto& hc : hdmiConsumers) {
                    // Resolve the DRM connector live from sysfs — the config
                    // may have a stale cardPath (e.g. card1 when HDMI is card0).
                    std::string resolvedCard;
                    int resolvedConnId = hc.connectorId;
                    if (!hc.connector.empty()) {
                        auto drmCheck = ResolveDrmConnector(hc.connector);
                        if (!drmCheck.connected) {
                            LogDebug(VB_MEDIAOUT, "GStreamer: skipping direct kmssink for connector %d (%s) — not connected\n",
                                    hc.connectorId, hc.connector.c_str());
                            continue;
                        }
                        resolvedCard = drmCheck.cardPath;
                        if (drmCheck.connectorId > 0)
                            resolvedConnId = drmCheck.connectorId;
                    }

                    std::string sinkName = "dkms_" + std::to_string(resolvedConnId);
                    GstElement* dkmsSink = gst_element_factory_make("kmssink", sinkName.c_str());
                    if (!dkmsSink) {
                        LogWarn(VB_MEDIAOUT, "GStreamer: kmssink not available for connector %d\n", resolvedConnId);
                        continue;
                    }

                    // sync=TRUE: pace video rendering to PTS timestamps via
                    // the PipeWire pipeline clock, keeping A/V in sync.
                    // max-lateness=-1: NEVER drop frames as "too late".
                    // On cold start the PipeWire clock can have a startup
                    // offset (0.91x ratio in logs) causing frames to appear
                    // late to kmssink.  The default max-lateness (5ms) would
                    // silently drop them → blank screen.  With -1, late frames
                    // are rendered immediately instead of dropped, while early
                    // frames still wait for their PTS — correct pacing with
                    // no blanking.
                    // skip-vsync=TRUE: required for vc4 atomic modesetting
                    // to avoid double-vsync wait (kmssink + kernel).
                    // Prefer the live-resolved card path over the config value.
                    std::string cardForDkms = !resolvedCard.empty() ? resolvedCard
                                            : !hc.cardPath.empty() ? hc.cardPath
                                            : m_hdmiCardPath;
                    int drmFd = cardForDkms.empty() ? -1 : AcquireSharedDrmFd(cardForDkms);
                    LogDebug(VB_MEDIAOUT, "GStreamer: dkms_%d cardPath='%s' sharedFd=%d\n",
                            resolvedConnId, cardForDkms.c_str(), drmFd);
                    if (drmFd >= 0) {
                        m_acquiredDrmCards.push_back(cardForDkms);
                        g_object_set(dkmsSink,
                                     "fd", drmFd,
                                     "connector-id", resolvedConnId,
                                     "sync", TRUE,
                                     "max-lateness", (gint64)-1,
                                     "skip-vsync", TRUE,
                                     NULL);
                        int planeId = (hc.primaryPlaneId >= 0) ? hc.primaryPlaneId
                                      : FindPrimaryPlaneForConnector(drmFd, resolvedConnId);
                        if (planeId >= 0) {
                            g_object_set(dkmsSink, "plane-id", planeId, NULL);
                            // hc.primaryPlaneId is always -1 today (GetHdmiConsumers
                            // never sets it), so this plane came from our own
                            // allocation and must be released in Close().
                            if (hc.primaryPlaneId < 0)
                                m_allocatedPlanes.push_back(planeId);
                        }
                        // Verify the fd was accepted by kmssink
                        gint readbackFd = -1;
                        g_object_get(dkmsSink, "fd", &readbackFd, NULL);
                        LogDebug(VB_MEDIAOUT, "GStreamer: dkms_%d fd=%d plane=%d\n",
                                resolvedConnId, readbackFd, planeId);
                    } else {
                        g_object_set(dkmsSink,
                                     "driver-name", "vc4",
                                     "connector-id", resolvedConnId,
                                     "sync", TRUE,
                                     "max-lateness", (gint64)-1,
                                     "skip-vsync", TRUE,
                                     NULL);
                    }

                    GstElement* dQueue = gst_element_factory_make("queue", nullptr);
                    g_object_set(dQueue,
                                 "max-size-buffers", 3,
                                 "max-size-bytes", 0,
                                 "max-size-time", (guint64)0,
                                 NULL);

                    // videoconvert negotiates to passthrough for a HW decoder's
                    // native NV12/DMA (no CPU cost), but is required when the
                    // decode happened in software -- I420 will not negotiate
                    // against the DRM plane and the pipeline would fail with
                    // "not-negotiated".  See the direct-kmssink path below.
                    GstElement* dConvert = gst_element_factory_make("videoconvert", nullptr);

                    // Optional crop, last before the sink so kmssink takes it
                    // as the DRM plane's source rectangle (metadata only, no
                    // per-frame copy).  This is what lets one decode of a
                    // double-wide file feed a different half to each display.
                    GstElement* dCrop = VideoOutputManager::CreateCropElement(
                        hc.crop, "dcrop_" + std::to_string(resolvedConnId));

                    gst_bin_add_many(GST_BIN(m_pipeline), dQueue, dConvert, dkmsSink, NULL);
                    if (dCrop) {
                        gst_bin_add(GST_BIN(m_pipeline), dCrop);
                        gst_element_link_many(dQueue, dConvert, dCrop, dkmsSink, NULL);
                    } else {
                        gst_element_link_many(dQueue, dConvert, dkmsSink, NULL);
                    }

                    GstPad* teeSrc = gst_element_request_pad_simple(vtee, "src_%u");
                    GstPad* dQSink = gst_element_get_static_pad(dQueue, "sink");
                    GstPadLinkReturn lr = gst_pad_link(teeSrc, dQSink);
                    gst_object_unref(teeSrc);
                    gst_object_unref(dQSink);

                    if (lr == GST_PAD_LINK_OK) {
                        m_directConnectorIds.insert(resolvedConnId);
                        LogDebug(VB_MEDIAOUT, "GStreamer: direct kmssink for connector %d (%dx%d) linked to vtee (pre-PLAYING)\n",
                                resolvedConnId, hc.width, hc.height);
                    } else {
                        LogWarn(VB_MEDIAOUT, "GStreamer: failed to link vtee → kmssink for connector %d (ret=%d)\n",
                                resolvedConnId, lr);
                    }
                }
            }
        } else if (haveHdmiConnector) {
            // ── Direct kmssink (no PipeWire video routing) ──
            // videoQueue → videoconvert → kmssink.  A hardware decoder hands us
            // NV12/DMA that kmssink takes directly, and in that case videoconvert
            // negotiates to passthrough and costs nothing -- so this keeps the
            // "no CPU-side conversion" property that matters on a Pi4.  But when
            // the decode is in SOFTWARE the format is typically I420, which the
            // DRM plane will not accept, and without a converter the whole
            // pipeline dies with "not-negotiated".  That is not hypothetical: a
            // Pi5 has no hardware H.264 decoder at all, so every H.264 video
            // takes the software path and HDMI output failed outright.
            GstElement* kmsConvert = gst_element_factory_make("videoconvert", "vkmsconv");
            // A crop configured for this display still applies with no PipeWire
            // routing -- a single-output split (one half shown, other discarded)
            // is legitimate, and the setting shouldn't silently do nothing.
            GstElement* kmsCrop = VideoOutputManager::CreateCropElement(
                VideoOutputManager::Instance().GetHdmiCropForConnector(m_streamSlot, m_hdmiConnectorId),
                "vkmscrop");
            gst_bin_add_many(GST_BIN(m_pipeline), videoQueue, kmsConvert, m_kmssink, NULL);
            bool linked = false;
            if (kmsCrop) {
                gst_bin_add(GST_BIN(m_pipeline), kmsCrop);
                linked = gst_element_link_many(videoQueue, kmsConvert, kmsCrop, m_kmssink, NULL);
            } else {
                linked = gst_element_link_many(videoQueue, kmsConvert, m_kmssink, NULL);
            }
            if (!linked) {
                LogErr(VB_MEDIAOUT, "GStreamer HDMI: Failed to link video chain\n");
            }
        }

        m_videoChain = videoQueue;
        m_hasVideoStream = true;  // expect video stream from decodebin

        // Connect decodebin pad-added signal for dynamic linking
        GstElement* decoder = gst_bin_get_by_name(GST_BIN(m_pipeline), "decoder");
        ConnectPadSignals(decoder, true);
        gst_object_unref(decoder);

    } else {
        // Audio-only pipeline (original gst_parse_launch approach)
        LogWarn(VB_MEDIAOUT, "GStreamer: Building audio-only pipeline\n");
        // expose-all-streams=false + audio caps makes decodebin discard any
        // video stream instead of auto-plugging a decoder for it.  Without
        // this, playing an mp4 audio-only still spins up the bcm2835 hardware
        // H.264 decoder for a video branch nothing consumes; tearing that down
        // leaves vb2 buffers active and sprays kernel WARN stack dumps
        // (videobuf2-core "driver bug: stop_streaming...") on every stop.
        // Multichannel + quirky gst-pipewire plugin: pin the layout and
        // pre-permute the samples into the (wrong) PulseAudio channel order
        // the plugin will declare, so the labels PipeWire routes by actually
        // match the content.  Self-disabling: once a fixed plugin is
        // installed the probe returns false and no permute is inserted.
        std::string chOrderCaps, chOrderPermute;
        if (usePipeWire && (mediaChannels == 5 || mediaChannels == 6 || mediaChannels == 8)
            && IsGstPipeWireChannelOrderQuirky()) {
            BuildChannelOrderFix(mediaChannels, chOrderCaps, chOrderPermute);
            LogWarn(VB_MEDIAOUT, "GStreamer: applying %dch channel-order workaround for quirky gst-pipewire plugin\n",
                    mediaChannels);
        }
        std::string rateCaps = decodeRate ? ",rate=" + std::to_string(decodeRate) : "";
        std::string pipelineStr =
            "filesrc location=\"" + fullPath + "\" ! decodebin name=decoder expose-all-streams=false caps=\"audio/x-raw\" ! audioconvert ! audioresample ! "
            "audio/x-raw" + rateCaps + chOrderCaps + " ! " + chOrderPermute +
            "tee name=t "
            "t. ! queue ! volume name=vol ! " + sinkStr + " "
            "t. ! queue max-size-buffers=3 leaky=downstream ! "
            "audioconvert ! audio/x-raw,format=F32LE,channels=1 ! "
            "appsink name=sampletap emit-signals=true sync=false max-buffers=3 drop=true";

        LogWarn(VB_MEDIAOUT, "GStreamer pipeline: %s\n", pipelineStr.c_str());

        LogWarn(VB_MEDIAOUT, "GStreamer: Calling gst_parse_launch()...\n");
        m_pipeline = gst_parse_launch(pipelineStr.c_str(), &error);
        LogWarn(VB_MEDIAOUT, "GStreamer: gst_parse_launch() returned (pipeline=%p, error=%p)\n", m_pipeline, error);
        if (error) {
            LogErr(VB_MEDIAOUT, "GStreamer pipeline error: %s\n", error->message);
            g_error_free(error);
            return 0;
        }

        // This pipeline's only sink is audio, so a file with nothing decodable
        // to feed it cannot play at all -- and the caps filter above turns that
        // into an autoplug failure decodebin reports as a missing plug-in, which
        // sends people off installing packages that were never the problem.
        // Watch the pads so the real reason can be given instead; the linking
        // itself stays with gst_parse_launch's own delayed-link handler.
        m_audioOnlyPipeline = true;
        if (GstElement* fbDecoder = gst_bin_get_by_name(GST_BIN(m_pipeline), "decoder")) {
            ConnectPadSignals(fbDecoder, false);
            gst_object_unref(fbDecoder);
        }

        // Get the volume element for later control
        m_volume = gst_bin_get_by_name(GST_BIN(m_pipeline), "vol");

        // Get the appsink
        m_appsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "sampletap");

        // Set stream-properties on pipewiresink (must be done post-launch;
        // gst_parse_launch can't deserialize GstStructure with spaced values).
        // Always set when using PipeWire backend to disable channel remixing.
        if (usePipeWire) {
            GstElement* pwsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "pwsink");
            if (pwsink) {
                GstStructure* props = gst_structure_new("props",
                    "node.name", G_TYPE_STRING, streamNodeName.c_str(),
                    "node.description", G_TYPE_STRING, streamNodeDesc.c_str(),
                    "stream.dont-remix", G_TYPE_BOOLEAN, TRUE,
                    "channelmix.disable", G_TYPE_BOOLEAN, TRUE,
                    NULL);
                g_object_set(pwsink, "stream-properties", props, NULL);
                gst_structure_free(props);
                gst_object_unref(pwsink);
            }
        }

        // Attach AES67 zero-hop RTP branches to the audio tee
#ifdef HAS_AES67_GSTREAMER
        {
            GstElement* tee = gst_bin_get_by_name(GST_BIN(m_pipeline), "t");
            if (tee) {
                AttachAES67Branches(tee);
                gst_object_unref(tee);
            }
        }
#endif
    }

    if (!m_pipeline) {
        LogErr(VB_MEDIAOUT, "Failed to create GStreamer pipeline\n");
        WarningHolder::AddWarning(31, "Failed to create GStreamer media pipeline");
        return 0;
    }

    // Connect audio appsink callback
    m_shutdownFlag.store(false);
    if (m_appsink) {
        m_appsinkSignalId = g_signal_connect(m_appsink, "new-sample", G_CALLBACK(OnNewSample), this);
        LogDebug(VB_MEDIAOUT, "GStreamer audio sample tap connected\n");
    } else {
        m_appsinkSignalId = 0;
        LogWarn(VB_MEDIAOUT, "GStreamer: could not find sampletap appsink element\n");
    }

    // Connect video appsink callback
    if (m_videoAppsink) {
        m_videoAppsinkSignalId = g_signal_connect(m_videoAppsink, "new-sample",
                                                   G_CALLBACK(OnNewVideoSample), this);
        m_hasVideoStream = true;
        LogDebug(VB_MEDIAOUT, "GStreamer video appsink connected\n");
    } else {
        m_videoAppsinkSignalId = 0;
    }

    // Clear the sample buffer for fresh playback
    {
        std::lock_guard<std::mutex> lock(s_sampleMutex);
        s_sampleBuffer.fill(0.0f);
        s_sampleWritePos = 0;
        s_sampleRate = 0;
    }

    // Compute the target volume (1.0 unless dB adjustment is set)
    double targetVolume = 1.0;
    if (m_volumeAdjust != 0) {
        targetVolume = pow(10.0, m_volumeAdjust / 2000.0); // volAdj is in 0.01dB units
    }

    // Start muted — the background thread will ramp up to targetVolume
    // after the pipeline reaches PLAYING.  This eliminates audible clicks
    // caused by USB/ALSA/PipeWire sink initialisation transients.
    if (m_volume) {
        g_object_set(m_volume, "volume", 0.0, NULL);
    }

    // Get the bus for message handling
    LogWarn(VB_MEDIAOUT, "GStreamer: Getting bus and setting sync handler...\n");
    m_bus = gst_element_get_bus(m_pipeline);

    // Install sync handler for autonomous bus message processing
    // This allows GStreamer playback to work without external Process() calls
    gst_bus_set_sync_handler(m_bus, BusSyncHandler, this, nullptr);

    // Force the pipeline to use GstSystemClock instead of auto-selecting
    // the PipeWire clock.  The PipeWire clock (provided by the audio
    // pipewiresink) is frozen at 0 on cold start — it does not begin
    // ticking until PipeWire's graph processes the first audio quantum,
    // which can be delayed hundreds of milliseconds.  During that window
    // kmssink's sync=TRUE blocks forever in gst_clock_id_wait() because
    // every frame PTS is "in the future" relative to clock time 0.
    // Diagnostics confirmed: rendered=3 (preroll only) across 160 s of
    // playback, clock_time=0 at first Process(), dkms_43 state=PAUSED.
    //
    // With GstSystemClock the system monotonic clock drives sync:
    //  - Video kmssink paces frames correctly from the first frame.
    //  - Audio pipewiresink (sync=TRUE) delivers buffers at wall-clock
    //    rate; PipeWire's internal quantum scheduling handles actual HW
    //    playout timing, so audio stays correct.
    //  - Any drift between the system clock and PipeWire's graph clock
    //    is negligible over typical media durations (sub-ms per minute).
    //
    // Audio-only pipelines are excluded, because there the trade is all cost
    // and no benefit: with no kmssink there is nothing to wedge, and a running
    // wall clock costs the head of every file.  base_time is latched when the
    // pipeline reaches PLAYING, but pipewiresink prerolls before its PipeWire
    // stream is actually streaming, so everything the sink renders during that
    // gap is already late and never reaches the graph.  Measured on a BBB with
    // an I2S cape, playing a 10 s chirp and capturing the sink monitor: 240 ms
    // to 760 ms of the start missing, run to run.  Leaving pipewiresink's own
    // clock in place -- it does not advance until the stream runs, so no buffer
    // is ever late -- brings that down to one quantum (~20 ms).
    if (wantVideo || wantHDMI) {
        GstClock* sysClock = gst_system_clock_obtain();
        gst_pipeline_use_clock(GST_PIPELINE(m_pipeline), sysClock);
        gst_object_unref(sysClock);
        LogDebug(VB_MEDIAOUT, "GStreamer: forced pipeline clock to GstSystemClock\n");
    }

    // Flush AES67 send pipelines just before PLAYING so the drop probe
    // catches any PipeWire graph transition artifacts (combine-stream
    // gaining a new source).  The Close()-time flush is consumed by
    // silence buffers during idle, so this Start()-time flush is the
    // one that actually protects the AES67 receivers.
#ifdef HAS_AES67_GSTREAMER
    if (AES67Manager::INSTANCE.IsActive()) {
        AES67Manager::INSTANCE.FlushSendPipelines();
    }
#endif

    // Move the potentially-blocking state transition to a background thread.
    // pipewiresink's READY→PAUSED blocks in pw_thread_loop_wait if its
    // target PipeWire node doesn't exist yet, which would hang the calling
    // HTTP handler thread and make the entire fppd API unresponsive.
    m_playing = true;
    // Anchor the preroll watchdog (Process()): if the pipeline never reaches a
    // playing position, this is the reference point for declaring it wedged.
    m_playStartMs = GetTimeMS();

    {
        int seekMs = msTime;
        bool videoPW = m_videoPipeWireRouting;
        int streamSlot = m_streamSlot;
        int hdmiConnectorId = m_hdmiConnectorId;
        // Whether a kmssink (primary or consumer) paces the video clock.
        bool kmsPaces = m_kmssink || !m_directConnectorIds.empty();
        // Final consumer connector set (primary HDMI included), captured by value.
        std::set<int> directConnectorIds = m_directConnectorIds;
        if (hdmiConnectorId >= 0)
            directConnectorIds.insert(hdmiConnectorId);

        // The thread must NOT capture `this`: it may still be running (blocked in
        // gst_element_set_state / pipewiresink) when this GStreamerOutput is torn
        // down and freed, and any `this->member` access would be a use-after-free.
        // Capture owning refs on the GStreamer objects we touch (pipeline already
        // ref'd above; volume ref'd here) plus a shared cancellation token; Stop()
        // sets the token so a fast stop aborts the ramp/attach early.
        m_startThreadCancel = std::make_shared<std::atomic<bool>>(false);
        std::shared_ptr<std::atomic<bool>> cancel = m_startThreadCancel;
        GstElement* pipeline = m_pipeline;
        gst_object_ref(pipeline);
        GstElement* volume = m_volume ? GST_ELEMENT(gst_object_ref(m_volume)) : nullptr;
        std::thread([pipeline, volume, cancel, seekMs, videoPW, targetVolume,
                     streamSlot, hdmiConnectorId, kmsPaces, directConnectorIds]() {
            // Releases our owning refs on exit no matter which path we return on.
            struct RefGuard {
                GstElement* pipeline;
                GstElement* volume;
                ~RefGuard() {
                    if (volume) gst_object_unref(volume);
                    gst_object_unref(pipeline);
                }
            } refGuard{pipeline, volume};

            LogWarn(VB_MEDIAOUT, "GStreamer: Setting pipeline to PLAYING...\n");
            GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
            LogWarn(VB_MEDIAOUT, "GStreamer: set_state returned %d\n", ret);
            if (ret == GST_STATE_CHANGE_FAILURE) {
                // Don't touch the (possibly freed) object — failure is reported
                // to the rest of FPP via the GStreamer bus ERROR message
                // (ProcessMessages clears m_playing) and the stall watchdog.
                LogErr(VB_MEDIAOUT, "Failed to set GStreamer pipeline to PLAYING\n");
                WarningHolder::AddWarningTimeout(60, 30, "Could not start media playback (pipeline failed to start)");
                return;
            }

            // If starting at a non-zero position, seek after state change
            if (seekMs > 0) {
                gst_element_seek_simple(pipeline, GST_FORMAT_TIME,
                                        (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                        (gint64)seekMs * GST_MSECOND);
            }

            // Fade volume from 0 to target over ~50ms to eliminate startup
            // clicks from sink initialisation transients.
            if (volume && !cancel->load()) {
                constexpr int kRampSteps = 10;
                constexpr int kRampStepUs = 5000; // 5ms per step = 50ms total
                for (int i = 1; i <= kRampSteps && !cancel->load(); i++) {
                    double v = targetVolume * ((double)i / kRampSteps);
                    g_object_set(volume, "volume", v, NULL);
                    std::this_thread::sleep_for(std::chrono::microseconds(kRampStepUs));
                }
                if (!cancel->load())
                    g_object_set(volume, "volume", targetVolume, NULL);
            }

            // Confirm the async transition actually completed.  ASYNC only means
            // "in progress": if a sink never prerolls, the pipeline sits in
            // PAUSED forever, produces silence, and posts NOTHING on the bus, so
            // nothing above notices.  That is exactly what a PipeWire daemon
            // restart under a live fppd leaves behind -- the GStreamer PipeWire
            // plugin caches its daemon connection process-wide, so every later
            // pipewiresink reuses a dead one.  Process()'s preroll watchdog
            // catches this for playlist media, but a standalone Play Media never
            // runs Process() (only PlaylistEntryMedia calls it), so without this
            // the failure is completely invisible.
            if (!cancel->load()) {
                GstState startedState = GST_STATE_NULL;
                GstStateChangeReturn scr = gst_element_get_state(pipeline, &startedState, nullptr,
                                                                 (GstClockTime)PREROLL_TIMEOUT_MS * GST_MSECOND);
                if (scr != GST_STATE_CHANGE_SUCCESS || startedState != GST_STATE_PLAYING) {
                    if (!cancel->load()) {
                        // FAILURE and "still not there after the timeout" are
                        // different faults and want different advice, but this
                        // reported both as the stale-PipeWire-connection case.
                        // An element that errors out returns FAILURE straight
                        // away, so the message claimed a 15-second timeout it
                        // had not waited for and blamed a PipeWire restart that
                        // had not happened -- for any failure at all, including
                        // a media file the pipeline simply could not play.  The
                        // bus error handler has already logged the real cause in
                        // that case, so say nothing more than the outcome here.
                        if (scr == GST_STATE_CHANGE_FAILURE) {
                            LogErr(VB_MEDIAOUT,
                                   "GStreamer: pipeline failed to start (state=%s) — see the error logged above "
                                   "for the cause.\n",
                                   gst_element_state_get_name(startedState));
                        } else {
                            LogErr(VB_MEDIAOUT,
                                   "GStreamer: pipeline never reached PLAYING within %dms (state=%s) — output will "
                                   "be silent. A PipeWire restart under a running fppd does this; fppd must be "
                                   "restarted to reconnect.\n",
                                   PREROLL_TIMEOUT_MS, gst_element_state_get_name(startedState));
                            WarningHolder::AddWarningTimeout(60, 30,
                                                            "Media playback did not start (audio backend connection lost — restart FPPD)");
                        }
                    }
                }
            }

            // Deferred: attach pipewiresink to video tee and start consumer
            // pipelines.  Pipewiresink must be added AFTER the pipeline has
            // fully reached PLAYING because gst_element_sync_state_with_parent
            // on a new element during a pending state transition can stall.
            if (videoPW) {
                GstState state;
                gst_element_get_state(pipeline, &state, nullptr, 10 * GST_SECOND);
                if (state < GST_STATE_PLAYING || cancel->load()) {
                    LogWarn(VB_MEDIAOUT, "GStreamer: Pipeline not PLAYING (state=%d), skipping pipewiresink\n", state);
                    return;
                }

                GstElement* vtee = gst_bin_get_by_name(GST_BIN(pipeline), "vtee");
                if (!vtee) {
                    LogWarn(VB_MEDIAOUT, "GStreamer: vtee not found, cannot attach pipewiresink\n");
                    return;
                }

                GstElement* pwvideosink = gst_element_factory_make("pipewiresink", "pwvideosink");
                if (!pwvideosink) {
                    LogWarn(VB_MEDIAOUT, "GStreamer: pipewiresink not available\n");
                    gst_object_unref(vtee);
                    VideoOutputManager::Instance().StartConsumers("", hdmiConnectorId, directConnectorIds);
                    return;
                }

                std::string videoNodeName = StreamSlotManager::GetVideoNodeName(streamSlot);
                std::string videoNodeDesc = StreamSlotManager::GetVideoNodeDescription(streamSlot);
                GstStructure* vprops = gst_structure_new("props",
                    "media.class", G_TYPE_STRING, "Stream/Output/Video",
                    "node.name", G_TYPE_STRING, videoNodeName.c_str(),
                    "node.description", G_TYPE_STRING, videoNodeDesc.c_str(),
                    "node.autoconnect", G_TYPE_BOOLEAN, FALSE,
                    "node.always-process", G_TYPE_BOOLEAN, TRUE,
                    NULL);
                g_object_set(pwvideosink, "stream-properties", vprops, NULL);
                gst_structure_free(vprops);

                if (kmsPaces) {
                    g_object_set(pwvideosink, "async", FALSE, "sync", FALSE, NULL);
                    LogDebug(VB_MEDIAOUT, "GStreamer: pipewiresink sync=FALSE (kmssink paces)\n");
                } else {
                    g_object_set(pwvideosink, "sync", TRUE, NULL);
                    LogDebug(VB_MEDIAOUT, "GStreamer: pipewiresink sync=TRUE (no kmssink, pipewiresink paces)\n");
                }

                GstElement* pwQueue = gst_element_factory_make("queue", "vpwq");
                if (kmsPaces) {
                    g_object_set(pwQueue,
                                 "leaky", 2,
                                 "max-size-buffers", 2,
                                 "max-size-bytes", 0,
                                 "max-size-time", (guint64)0,
                                 NULL);
                } else {
                    g_object_set(pwQueue,
                                 "max-size-buffers", 3,
                                 "max-size-bytes", 0,
                                 "max-size-time", (guint64)0,
                                 NULL);
                }

                gst_bin_add_many(GST_BIN(pipeline), pwQueue, pwvideosink, NULL);
                if (!gst_element_link(pwQueue, pwvideosink)) {
                    LogWarn(VB_MEDIAOUT, "GStreamer: failed to link vpwq → pwvideosink\n");
                }

                gboolean qSync = gst_element_sync_state_with_parent(pwQueue);
                gboolean pwSync = gst_element_sync_state_with_parent(pwvideosink);
                LogDebug(VB_MEDIAOUT, "GStreamer: deferred state sync — queue=%d pipewiresink=%d\n",
                        qSync, pwSync);

                GstPad* teeSrc = gst_element_request_pad_simple(vtee, "src_%u");
                GstPad* pwQSink = gst_element_get_static_pad(pwQueue, "sink");
                GstPadLinkReturn linkRet = gst_pad_link(teeSrc, pwQSink);
                if (linkRet != GST_PAD_LINK_OK) {
                    LogWarn(VB_MEDIAOUT, "GStreamer: vtee→vpwq pad link failed: %d\n", linkRet);
                }
                gst_object_unref(teeSrc);
                gst_object_unref(pwQSink);

                GstState pwState, pwPending;
                gst_element_get_state(pwvideosink, &pwState, &pwPending, 2 * GST_SECOND);
                LogDebug(VB_MEDIAOUT, "GStreamer: pipewiresink state=%d pending=%d after attach\n",
                        pwState, pwPending);

                LogDebug(VB_MEDIAOUT, "GStreamer: pipewiresink attached to vtee (node=%s)\n",
                        videoNodeName.c_str());

                std::this_thread::sleep_for(std::chrono::milliseconds(200));

                gst_object_unref(vtee);

                // Stop() may have run during the 200ms wait above and set the
                // cancellation token — if so the producer pipeline is already
                // being (or has been) torn down, and starting consumers now
                // would strand their DRM planes/CMA against a dead producer
                // that nothing will ever stop them against.  Bail out.
                if (cancel->load()) {
                    LogDebug(VB_MEDIAOUT, "GStreamer: Start() cancelled during deferred attach, skipping StartConsumers\n");
                    return;
                }

                VideoOutputManager::Instance().StartConsumers(videoNodeName, hdmiConnectorId, directConnectorIds);
            }
            // Owning refs on pipeline/volume released by refGuard on scope exit.
        }).detach();
    }

    // m_currentInstance is used by WLED audio-reactive tap and video overlay.
    // Slot 1 (primary) always takes priority; secondary slots only claim it
    // if no other instance is active.
    if (m_streamSlot == 1 || m_currentInstance == nullptr) {
        m_currentInstance = this;
    }

    // Register with StreamSlotManager
    StreamSlotManager::Instance().SetActiveOutput(m_streamSlot, this);

#ifdef HAS_AES67_GSTREAMER
    // AES67 send pipelines run continuously (always sending to multicast).
    // No resume needed — filter-chain outputs silence when idle.
#endif

    if (m_mediaOutputStatus) {
        m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_PLAYING;
    }

    // Ensure channel output thread is running so ProcessVideoOverlay gets called
    // (same as SDLOutput behavior)
    if (m_videoOverlayModel) {
        StartChannelOutputThread();
    }

    Starting();
    LogInfo(VB_MEDIAOUT, "GStreamer started playing: %s\n", m_mediaFilename.c_str());
    return 1;
}

int GStreamerOutput::Stop(void) {
    LogDebug(VB_MEDIAOUT, "GStreamerOutput::Stop()\n");
    // Idempotency guard: Stop() is invoked both directly (e.g. from
    // CloseMediaOutput() when the media is still playing) and again via the
    // destructor's Close().  The teardown below includes a 250ms audio-silence
    // flush and a 150ms DRM-release sleep; running it twice adds ~400ms of
    // dead time to every clip transition, which on a multisync remote pushes
    // playback progressively behind the master.  Run the teardown once only.
    if (m_pipeline && !m_teardownComplete) {
        // Detach AES67 zero-hop RTP branches BEFORE pipeline goes NULL —
        // this resumes the standalone send pipeline so AES67 keeps working
        // between tracks.
#ifdef HAS_AES67_GSTREAMER
        DetachAES67Branches();
#endif

        // Set shutdown flag to prevent appsink callbacks from doing work
        // during teardown — without this, the streaming thread can deadlock
        // with gst_element_set_state(NULL) due to malloc arena locks.
        m_shutdownFlag.store(true);

        // Tell the detached Start() PLAYING-transition thread (if still running)
        // to abort its volume ramp / deferred pipewiresink attach.  It captures
        // only this token plus owning GStreamer refs, so it is safe even after
        // this object is freed.
        if (m_startThreadCancel) {
            m_startThreadCancel->store(true);
        }

        // Disconnect appsink signals BEFORE state change to prevent
        // callbacks firing during pipeline teardown.
        if (m_appsink) {
            if (m_appsinkSignalId > 0) {
                g_signal_handler_disconnect(m_appsink, m_appsinkSignalId);
                m_appsinkSignalId = 0;
            }
            g_object_set(m_appsink, "emit-signals", FALSE, NULL);
        }
        if (m_videoAppsink) {
            if (m_videoAppsinkSignalId > 0) {
                g_signal_handler_disconnect(m_videoAppsink, m_videoAppsinkSignalId);
                m_videoAppsinkSignalId = 0;
            }
            g_object_set(m_videoAppsink, "emit-signals", FALSE, NULL);
        }

        // Remove bus sync handler before state change
        if (m_bus) {
            gst_bus_set_sync_handler(m_bus, nullptr, nullptr, nullptr);
        }

        // Stop video output consumers BEFORE tearing down the producer pipeline
        // to avoid pipewiresrc crash from disappearing PipeWire producer node
        if (m_videoPipeWireRouting) {
            VideoOutputManager::Instance().StopConsumers();
        }

        // NOTE: Do NOT pre-disconnect pipewiresink elements here.
        // Any operation that triggers gst_pipewire_sink_change_state or
        // gst_pipewire_sink_event will call pw_thread_loop_lock, which
        // deadlocks if the PipeWire callback thread is inside
        // on_param_changed.  The 200ms delay before StartConsumers in
        // the deferred-attach thread ensures the buffer pool is active so
        // on_param_changed never blocks, and the pipeline NULL below can
        // proceed normally.

        // For pipelines with no DRM/KMS involvement (audio-only), the rest of
        // the teardown - the 250ms silence flush and the pipeline state
        // change to NULL - has no ordering dependency on anything the next
        // track needs: PipeWire mixes concurrent streams, and the next
        // pipeline uses its own stream node.  Run that slow tail on a
        // detached thread so a track transition (or a looping playlist wrap)
        // doesn't spend ~0.5s of lights-frozen time on it.  Pipelines that
        // touch DRM (kmssink, direct connectors, reserved planes, shared DRM
        // fds) keep the fully synchronous path below: their kernel resources
        // must be released before the next pipeline may claim them.
        bool asyncTeardownEligible = !m_hasVideoStream && !m_kmssink &&
                                     m_directConnectorIds.empty() &&
                                     !m_videoPipeWireRouting &&
                                     m_allocatedPlanes.empty() &&
                                     m_acquiredDrmCards.empty();
        if (asyncTeardownEligible) {
            // These are what Close() would run inside its m_pipeline guard;
            // that guard will be skipped because we null the members here.
            FlushPipeWireDelayBuffers();
#ifdef HAS_AES67_GSTREAMER
            if (AES67Manager::INSTANCE.IsActive()) {
                AES67Manager::INSTANCE.FlushSendPipelines();
            }
#endif
            Stopping();

            // The detached thread takes over this object's refs; it must not
            // touch `this` (the object is typically destroyed right after).
            std::thread([pipeline = m_pipeline, volume = m_volume, bus = m_bus,
                         appsink = m_appsink, videoAppsink = m_videoAppsink]() {
                SetThreadName("FPP-GstTeardown");
                if (volume) {
                    // Silence flush: push zeros through the PipeWire chain so
                    // stale buffer contents can't replay as a click when the
                    // next track starts (see comment on the synchronous path).
                    g_object_set(volume, "volume", 0.0, NULL);
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                }
                if (!SetPipelineToNullSupervised(pipeline, PIPELINE_TEARDOWN_TIMEOUT_MS)) {
                    // Reaper keeps its own refs to a wedged pipeline; drop
                    // ours below either way (mirrors Close()'s behavior).
                    RecordWedgeEventAndMaybeRestart("async pipeline teardown wedged, abandoning pipeline");
                }
                if (appsink)
                    gst_object_unref(appsink);
                if (videoAppsink)
                    gst_object_unref(videoAppsink);
                if (volume)
                    gst_object_unref(volume);
                if (bus)
                    gst_object_unref(bus);
                gst_object_unref(pipeline);
            }).detach();

            m_pipeline = nullptr;
            m_volume = nullptr;
            m_bus = nullptr;
            m_appsink = nullptr;
            m_videoAppsink = nullptr;

            m_playing = false;
            if (m_mediaOutputStatus) {
                m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
            }
            Stopped();
            m_teardownComplete = true;
            return 1;
        }

        // Flush silence through the PipeWire graph before tearing down.
        // When the pipeline stops, PipeWire combine-stream and filter-chain
        // nodes go IDLE with whatever audio was last in their buffers.  If a
        // new track starts before WirePlumber suspends those nodes (~5s),
        // the stale buffer contents replay as an audible click.
        // Setting GStreamer volume to 0 while the pipeline is still PLAYING
        // pushes silence through the entire chain (filter-chain delay buffers,
        // combine-stream mixers, ALSA ring buffers), overwriting stale data.
        // 250ms covers the longest configured delay (206ms) plus a few
        // PipeWire quanta for propagation.
        if (m_volume) {
            g_object_set(m_volume, "volume", 0.0, NULL);
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }

        Stopping();
        LogDebug(VB_MEDIAOUT, "GStreamerOutput::Stop() - setting pipeline to NULL\n");
        // Supervised: a wedged V4L2 decoder (issue #2695) makes this state
        // change block forever — on the 2026-07-03 soak it hung the playlist
        // thread for 2h25m and ended in SIGBUS.  On timeout the pipeline is
        // abandoned (reaper thread keeps the ref) so playback can continue.
        if (SetPipelineToNullSupervised(m_pipeline, PIPELINE_TEARDOWN_TIMEOUT_MS)) {
            LogDebug(VB_MEDIAOUT, "GStreamerOutput::Stop() - pipeline NULL complete\n");
        } else {
            m_teardownAbandoned = true;
            RecordWedgeEventAndMaybeRestart("pipeline teardown wedged, abandoning pipeline");
        }

        // On Raspberry Pi's vc4/v3d DRM driver, the kernel needs time to
        // fully release CRTC/plane resources after kmssink's DRM fd is
        // closed.  Without this delay, a subsequent Start() may fail with
        // "general resource error" from kmssink because the connector is
        // still held by the kernel.  150ms covers the observed ~100-120ms
        // release window on Pi 5 hardware.
        // Only needed when kmssink is in the primary pipeline (not when
        // PipeWire routing sends HDMI through a consumer pipeline), and
        // pointless after an abandoned teardown (nothing was released).
        if (!m_teardownAbandoned && (m_kmssink || !m_directConnectorIds.empty())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            LogDebug(VB_MEDIAOUT, "GStreamerOutput::Stop() - DRM release delay complete\n");
        }

        m_playing = false;
        if (m_mediaOutputStatus) {
            m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
        }
        Stopped();
        m_teardownComplete = true;
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Audio sink latency
//
// gst_element_query_position() on pipewiresink reports what the sink has
// rendered into the PipeWire graph, not what has left the card, and
// pipewiresink adds nothing of its own to the GStreamer LATENCY query -- it
// answers min = upstream + processing-deadline + render-delay with render-delay
// at 0, identically no matter which node it targets, so it never asks PipeWire
// how far the data still has to travel.  Everything queued downstream is
// therefore invisible to mediaSeconds, and the sequence -- which
// CalculateNewChannelOutputDelay() servos to exactly that number -- runs ahead
// of the sound by the whole downstream depth.
//
// Measured with the sequence's own frame index captured off the wire against a
// sample-accurate audio reference: lights ahead by 22 ms on an AM62x I2S cape,
// 56 ms on a Pi 5 I2S cape, and 160 ms on a BeagleBone with a full-speed USB
// dongle.  It is a per-platform constant, which is why it cannot be a setting.
//
// The USB figure is not fixed for all time: it is dominated by that card's
// api.alsa.headroom, and a USB card that is the sole sink in its audio group is
// now given a quarter of what it used to get (see kUsbHeadroomSoleSink in
// FPPINIT_Audio.cpp), which took the same BeagleBone from 160 ms to ~80 ms.
// Nothing below needs changing for that -- the queue depth is read live and
// scaled by the card's own rate, so the correction simply gets smaller -- but do
// not treat the numbers above as the values a current box will report.
//
// The card's own delay figure is the closest estimate obtainable without a new
// library or a patched GStreamer plugin: /proc/asound/cardN/pcmMp/sub0/status
// reports `delay` in frames -- the distance from PipeWire's write pointer to
// the DAC.  It matched the measured offset to within 3 ms on the two platforms
// where both could be cross-checked (157 vs 160 ms, 21 vs 22 ms).  PipeWire's
// own declared SPA_PARAM_Latency was tried and is worse: it is the nominal
// quantum + headroom + period, which under-reports the live queue by 30 ms on
// the USB box.
//
// This corrects the *reported position*.  It deliberately does not touch
// mediaOffset, which stays a user knob applied on top in setMediaElapsed().
// ---------------------------------------------------------------------------

static int AlsaCardNumber(const std::string& cardId) {
    if (cardId.empty()) {
        return -1;
    }
    std::istringstream iss(GetFileContents("/proc/asound/cards"));
    std::string line;
    while (std::getline(iss, line)) {
        // " 2 [KulpLightsHIFI ]: simple-card - ..."
        std::size_t lb = line.find('[');
        std::size_t rb = line.find(']');
        if (lb == std::string::npos || rb == std::string::npos || rb < lb) {
            continue;
        }
        std::string id = line.substr(lb + 1, rb - lb - 1);
        while (!id.empty() && id.back() == ' ') {
            id.pop_back();
        }
        if (id == cardId) {
            return std::atoi(line.c_str());
        }
    }
    return -1;
}

// Card the running graph actually feeds.
//
// Taken from the adapters FPP generated rather than trusting AudioOutput
// outright -- the setting can name a card that has since been unplugged, and
// the queue depth is only readable for a card the graph really holds.  But the
// conf declares an adapter for EVERY playback-capable card, emitted in
// card-number order, so simply reading its first api.alsa.path yields card 0
// whether or not that is the output.  On a box with three USB cards and
// AudioOutput=ICUSBAUDIO7D (card 2) that read card 0, found its PCM 'closed'
// (idle cards suspend), and so applied no correction at all -- the sequence
// stayed ahead of the sound, which is the whole defect this is here to remove.
//
// The order of preference is what the graph feeds, then what the setting says,
// then anything at all:
//
//   1. a card the group config actually targets, matching AudioOutput
//   2. any card the group config targets
//   3. a declared adapter matching AudioOutput
//   4. the first declared adapter
//
// Steps 1 and 2 exist because AudioOutput can legitimately disagree with the
// running graph.  The setting is flagged reboot-required, so between changing
// the output card and rebooting, the setting names the new card while the graph
// still feeds the old one -- reading the setting there picks a card that is not
// playing, whose PCM is closed, and applies no correction at all.  The group
// config is what PipeWire actually loaded, so it is the better authority; this
// is what the original code was reaching for by reading the conf rather than the
// setting, before it settled on "first adapter in the file" and got card 0.
//
// Both generated confs have to be searched, not just the boot-time one.  The
// boot probe deliberately skips a card it cannot drive directly -- an
// IEC958-only vc4-hdmi is the common one ("no standard PCM format") -- and the
// group config then creates that card's adapter itself, through sysdefault:
// rather than hw:.  A Pi playing audio over HDMI therefore has its only sink
// adapter in 97-fpp-audio-groups.conf and none in the 95 conf, so searching the
// 95 conf alone finds either the wrong card or no card, and silently applies no
// correction at all.  That configuration is Simple mode's default on a board
// with no other sound card, which is most of them.
static int AlsaSinkCardNumber() {
    // 95 first: where a card FPP can drive directly is declared.  97 second: the
    // group config's own adapters, for the cards 95 had to skip.
    const std::string groupsConf = GetFileContents("/etc/pipewire/pipewire.conf.d/97-fpp-audio-groups.conf");
    const std::string confs[] = {
        GetFileContents("/etc/pipewire/pipewire.conf.d/95-fpp-alsa-sink.conf"),
        groupsConf
    };
    // Reads the value of `key = "..."` at or after `from`, or "" if absent.
    auto quotedValue = [](const std::string& s, const std::string& key, std::size_t from) {
        std::size_t k = s.find(key, from);
        if (k == std::string::npos) {
            return std::string();
        }
        std::size_t a = s.find('"', k + key.size());
        if (a == std::string::npos) {
            return std::string();
        }
        std::size_t b = s.find('"', a + 1);
        if (b == std::string::npos) {
            return std::string();
        }
        return s.substr(a + 1, b - a - 1);
    };

    // Adapters the group config's filter chains actually play into.  Only
    // fpp_alsa_* targets count: the combine streams carry node.target too, but
    // theirs name the fpp_fx_* filter chains rather than a card.
    std::set<std::string> graphTargets;
    for (std::size_t t = groupsConf.find("node.target"); t != std::string::npos;
         t = groupsConf.find("node.target", t + 1)) {
        std::string target = quotedValue(groupsConf, "node.target", t);
        if (startsWith(target, "fpp_alsa_")) {
            graphTargets.insert(target);
        }
    }

    // AudioOutput is a stable ALSA card ID on current installs, a bare card
    // number on older ones; resolveAudioOutputCardNum() accepts both, so match
    // its handling here rather than assuming either spelling.
    std::string wanted = getSetting("AudioOutput");
    TrimWhiteSpace(wanted);
    int wantedCard = -1;
    if (!wanted.empty()) {
        wantedCard = (wanted.find_first_not_of("0123456789") == std::string::npos)
                         ? std::atoi(wanted.c_str())
                         : AlsaCardNumber(wanted);
    }

    int targetedCard = -1; // first adapter the graph plays into
    int settingCard = -1;  // first adapter matching AudioOutput
    int firstCard = -1;    // first adapter of any kind
    // Sink adapters only -- the capture adapters FPP also declares are named
    // fpp_alsain_, which this needle does not match.
    const std::string nameKey = "node.name = \"fpp_alsa_";
    for (const std::string& conf : confs) {
        for (std::size_t n = conf.find(nameKey); n != std::string::npos; n = conf.find(nameKey, n + 1)) {
            std::string nodeName = quotedValue(conf, "node.name", n);
            // "hw:CardId", or "sysdefault:CardId" for a card reached through the
            // ALSA plug layer -- the card ID is what follows the colon either way.
            std::string path = quotedValue(conf, "api.alsa.path", n);
            std::size_t colon = path.find(':');
            if (colon == std::string::npos) {
                continue;
            }
            int card = AlsaCardNumber(path.substr(colon + 1));
            if (card < 0) {
                continue;
            }
            const bool targeted = graphTargets.count(nodeName) > 0;
            if (targeted && card == wantedCard) {
                return card; // the graph feeds it and the setting names it
            }
            if (targeted && targetedCard < 0) {
                targetedCard = card;
            }
            if (card == wantedCard && settingCard < 0) {
                settingCard = card;
            }
            if (firstCard < 0) {
                firstCard = card;
            }
        }
    }
    if (targetedCard >= 0) {
        return targetedCard;
    }
    if (settingCard >= 0) {
        return settingCard;
    }
    return firstCard;
}

// First playback substream of the card; the device index is not always 0.
static std::string AlsaPlaybackStatusPath(int card) {
    std::string base = "/proc/asound/card" + std::to_string(card);
    DIR* d = opendir(base.c_str());
    if (!d) {
        return "";
    }
    std::vector<std::string> devs;
    while (struct dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n.size() > 4 && n.compare(0, 3, "pcm") == 0 && n.back() == 'p') {
            devs.push_back(n);
        }
    }
    closedir(d);
    if (devs.empty()) {
        return "";
    }
    std::sort(devs.begin(), devs.end());
    return base + "/" + devs[0] + "/sub0/status";
}

static int ReadIntField(const std::string& text, const char* key) {
    std::size_t k = text.find(key);
    if (k == std::string::npos) {
        return -1;
    }
    std::size_t c = text.find(':', k);
    if (c == std::string::npos) {
        return -1;
    }
    return std::atoi(text.c_str() + c + 1);
}

// Say once, per media, that the position is going out uncorrected and why.
// Deliberately not a WarningHolder banner: on a board with no usable sink this
// would fire on every track forever, and the condition is a tuning shortfall
// rather than a fault the user must act on immediately.
void GStreamerOutput::NoteNoSinkLatency(const std::string& why) {
    if (m_warnedNoSinkLatency) {
        return;
    }
    m_warnedNoSinkLatency = true;
    LogWarn(VB_MEDIAOUT,
            "GStreamer: no audio sink latency available (%s) — reported position is not "
            "corrected for the card's queue, so the sequence may lead the audio by tens of ms\n",
            why.c_str());
}

int64_t GStreamerOutput::AudioSinkLatencyNs() {
    // The queue depth is essentially constant while a track plays; re-reading
    // it on every Process() would put a /proc read in the main loop at frame
    // rate for no benefit.
    uint64_t now = GetTimeMS();
    if (m_sinkLatencyCheckedMs != 0 && (now - m_sinkLatencyCheckedMs) < 250) {
        return m_sinkLatencyNs;
    }
    m_sinkLatencyCheckedMs = now;

    if (m_alsaStatusPath.empty()) {
        int card = AlsaSinkCardNumber();
        if (card < 0) {
            // Nothing to read means no correction, which is not a small thing:
            // uncorrected, the sequence leads the sound by the whole ALSA queue
            // depth -- 22 to 160 ms on the boards this was measured on.  That is
            // audible and looks exactly like a misconfigured show, so say so
            // once rather than leaving the user to find it by ear.
            NoteNoSinkLatency("no FPP ALSA sink adapter matches the configured audio output");
            m_sinkLatencyNs = 0;
            return 0;
        }
        m_alsaStatusPath = AlsaPlaybackStatusPath(card);
        m_alsaHwParamsPath = m_alsaStatusPath;
        std::size_t s = m_alsaHwParamsPath.rfind("/status");
        if (s != std::string::npos) {
            m_alsaHwParamsPath.replace(s, 7, "/hw_params");
        }
        if (m_alsaStatusPath.empty()) {
            NoteNoSinkLatency("card " + std::to_string(card) + " exposes no playback substream status");
            m_sinkLatencyNs = 0;
            return 0;
        }
    }

    std::string status = GetFileContents(m_alsaStatusPath);
    if (status.empty() || status.compare(0, 6, "closed") == 0) {
        // Suspended between tracks: keep the last good value rather than
        // snapping the reported position by tens of ms at every gap.
        return m_sinkLatencyNs;
    }
    int delayFrames = ReadIntField(status, "delay");
    if (delayFrames <= 0) {
        return m_sinkLatencyNs;
    }
    if (m_alsaRate <= 0) {
        m_alsaRate = ReadIntField(GetFileContents(m_alsaHwParamsPath), "rate");
    }
    if (m_alsaRate <= 0) {
        NoteNoSinkLatency("could not read the card's rate from hw_params");
        m_sinkLatencyNs = 0;
        return 0;
    }
    int64_t ns = (int64_t)delayFrames * 1000000000LL / m_alsaRate;
    // A sane sink is a few ms to a few hundred; anything past a second means
    // the field was misread (dmix, for one, leaves appl_ptr at 0 and reports a
    // hugely negative delay) and is not worth trusting.
    if (ns < 0 || ns > 1000000000LL) {
        NoteNoSinkLatency("card reports an implausible queue depth (" +
                          std::to_string(delayFrames) + " frames at " +
                          std::to_string(m_alsaRate) + " Hz)");
        m_sinkLatencyNs = 0;
        return 0;
    }
    m_sinkLatencyNs = ns;
    return ns;
}

int GStreamerOutput::Process(void) {
    if (!m_pipeline || !m_bus) {
        return 0;
    }
    // Sub-phases for the main-loop stall watchdog (issue #2727): everything
    // below runs on the main loop and calls into GStreamer, which takes both
    // element locks and (via pipewiresink) PipeWire's thread-loop lock, so a
    // sink that is stuck in preroll can park the main loop right here.
    SetMainLoopPhase("GStreamer ProcessMessages");
    ProcessMessages();

    // Update position
    if (m_playing) {
        gint64 pos = 0, dur = 0;
        // Query position from a sync'd sink, NOT the pipeline.
        // Pipeline-level queries return the demuxer's read-ahead position,
        // which races far ahead when sinks use leaky queues or async=FALSE.
        // Prefer kmssink (sync=TRUE, video playout position), then a
        // consumer direct-kmssink (sync=TRUE), then audio pipewiresink
        // (sync=TRUE), then the pipeline as fallback.
        GstElement* posSource = nullptr;
        bool ownPosSource = false;
        if (m_kmssink) {
            posSource = m_kmssink;
        } else if (!m_directConnectorIds.empty()) {
            std::string name = "dkms_" + std::to_string(*m_directConnectorIds.begin());
            posSource = gst_bin_get_by_name(GST_BIN(m_pipeline), name.c_str());
            if (posSource) ownPosSource = true;
        }
        // Only the audio sink's position gets the ALSA-queue correction below;
        // a kmssink is reporting video playout, which is a different path.
        bool posFromAudioSink = false;
        if (!posSource) {
            SetMainLoopPhase("GStreamer get pwsink");
            posSource = gst_bin_get_by_name(GST_BIN(m_pipeline), "pwsink");
            if (posSource) {
                ownPosSource = true;
                posFromAudioSink = true;
            }
        }
        if (!posSource) posSource = m_pipeline;
        SetMainLoopPhase("GStreamer query position");
        bool havePos = gst_element_query_position(posSource, GST_FORMAT_TIME, &pos);
        SetMainLoopPhase("GStreamer query duration");
        bool haveDur = gst_element_query_duration(m_pipeline, GST_FORMAT_TIME, &dur);
        SetMainLoopPhase("GStreamer position post-processing");
        if (ownPosSource) gst_object_unref(posSource);

        // One-shot: log pipeline clock and sink sync state on first position update
        if (havePos && m_wallStartMs == 0) {
            m_wallStartMs = GetTimeMS();  // record wall time at first pos (also used by preroll watchdog)
            // A valid position means the pipeline prerolled and the decoder
            // is producing — clear the consecutive-wedge escalation counter.
            s_consecutiveWedgeEvents = 0;
            // The remaining pipeline/sink introspection here exists purely to
            // populate the debug log — skip the GStreamer queries entirely when
            // MediaOut isn't at DEBUG (or more verbose).
            if (WillLog(LOG_DEBUG, VB_MEDIAOUT)) {
                GstClock* clock = gst_pipeline_get_clock(GST_PIPELINE(m_pipeline));
                LogDebug(VB_MEDIAOUT, "GStreamer: pipeline clock=%s pos=%.1fs\n",
                        clock ? GST_OBJECT_NAME(clock) : "(none)",
                        (float)pos / GST_SECOND);
                if (clock) {
                    GstClockTime ct = gst_clock_get_time(clock);
                    GstClockTime bt = gst_element_get_base_time(m_pipeline);
                    LogDebug(VB_MEDIAOUT, "GStreamer: clock_time=%" GST_TIME_FORMAT " base_time=%" GST_TIME_FORMAT "\n",
                            GST_TIME_ARGS(ct), GST_TIME_ARGS(bt));
                    gst_object_unref(clock);
                }
                // Log consumer kmssink stats (rendered/dropped frame counts)
                for (int cid : m_directConnectorIds) {
                    std::string name = "dkms_" + std::to_string(cid);
                    GstElement* dkms = gst_bin_get_by_name(GST_BIN(m_pipeline), name.c_str());
                    if (dkms) {
                        GstStructure* stats = nullptr;
                        g_object_get(dkms, "stats", &stats, NULL);
                        if (stats) {
                            guint64 rendered = 0, dropped = 0;
                            gst_structure_get_uint64(stats, "rendered", &rendered);
                            gst_structure_get_uint64(stats, "dropped", &dropped);
                            LogDebug(VB_MEDIAOUT, "GStreamer: %s stats: rendered=%" G_GUINT64_FORMAT " dropped=%" G_GUINT64_FORMAT "\n",
                                    name.c_str(), rendered, dropped);
                            gst_structure_free(stats);
                        }
                        GstState st, pend;
                        gst_element_get_state(dkms, &st, &pend, 0);
                        LogDebug(VB_MEDIAOUT, "GStreamer: %s state=%d pending=%d\n", name.c_str(), st, pend);
                        gst_object_unref(dkms);
                    }
                }
                // Check sync on sinks
                GstElement* pwsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "pwsink");
                if (pwsink) {
                    gboolean s; g_object_get(pwsink, "sync", &s, NULL);
                    LogDebug(VB_MEDIAOUT, "GStreamer: pwsink sync=%d\n", s);
                    gst_object_unref(pwsink);
                }
                GstElement* kms = gst_bin_get_by_name(GST_BIN(m_pipeline), "kmsvideosink");
                if (kms) {
                    gboolean s; g_object_get(kms, "sync", &s, NULL);
                    LogDebug(VB_MEDIAOUT, "GStreamer: kmssink sync=%d\n", s);
                    gst_object_unref(kms);
                }
            }
        }

        // Periodic wall-clock vs stream-position diagnostic (DEBUG only — the
        // stall/preroll watchdogs below run independently of this block).
        if (havePos && m_wallStartMs > 0 && WillLog(LOG_DEBUG, VB_MEDIAOUT)) {
            uint64_t wallMs = GetTimeMS() - m_wallStartMs;
            float wallSec = wallMs / 1000.0f;
            float streamSec = (float)pos / GST_SECOND;
            if (wallMs - m_lastWallLogMs > 5000) {
                m_lastWallLogMs = wallMs;
                float ratio = (wallSec > 0.1f) ? streamSec / wallSec : 0.0f;
                LogDebug(VB_MEDIAOUT, "GStreamer: wall=%.1fs stream=%.1fs ratio=%.2fx\n",
                        wallSec, streamSec, ratio);
                // Log pipeline clock time and consumer kmssink stats at each 5s checkpoint
                GstClock* chkClock = gst_pipeline_get_clock(GST_PIPELINE(m_pipeline));
                if (chkClock) {
                    GstClockTime ct = gst_clock_get_time(chkClock);
                    GstClockTime bt = gst_element_get_base_time(m_pipeline);
                    LogDebug(VB_MEDIAOUT, "GStreamer: clock_time=%" GST_TIME_FORMAT " base_time=%" GST_TIME_FORMAT " running=%" GST_TIME_FORMAT "\n",
                            GST_TIME_ARGS(ct), GST_TIME_ARGS(bt),
                            GST_TIME_ARGS(ct >= bt ? ct - bt : 0));
                    gst_object_unref(chkClock);
                }
                for (int cid : m_directConnectorIds) {
                    std::string name = "dkms_" + std::to_string(cid);
                    GstElement* dkms = gst_bin_get_by_name(GST_BIN(m_pipeline), name.c_str());
                    if (dkms) {
                        GstState st, pend;
                        gst_element_get_state(dkms, &st, &pend, 0);
                        GstStructure* stats = nullptr;
                        g_object_get(dkms, "stats", &stats, NULL);
                        if (stats) {
                            guint64 rendered = 0, dropped = 0;
                            gst_structure_get_uint64(stats, "rendered", &rendered);
                            gst_structure_get_uint64(stats, "dropped", &dropped);
                            LogDebug(VB_MEDIAOUT, "GStreamer: %s rendered=%" G_GUINT64_FORMAT " dropped=%" G_GUINT64_FORMAT " state=%d pending=%d\n",
                                    name.c_str(), rendered, dropped, st, pend);
                            gst_structure_free(stats);
                        }
                        gst_object_unref(dkms);
                    }
                }
            }
        }

        if (havePos) {
            // Track the maximum observed duration — VBR MP3 files can report
            // fluctuating durations as GStreamer revises its estimate during
            // decoding.  Using the max prevents time-remaining from jumping
            // backwards or going negative.
            if (haveDur && dur > m_maxDuration) {
                m_maxDuration = dur;
            }
            gint64 effectiveDur = m_maxDuration;

            float elapsed = (float)pos / GST_SECOND;
            float remaining = (effectiveDur > pos) ? (float)(effectiveDur - pos) / GST_SECOND : 0.0f;

            // Back the position up to what is actually leaving the DAC, so the
            // sequence servo and the MultiSync packets both follow the sound
            // rather than the graph's write pointer.  See AudioSinkLatencyNs().
            if (posFromAudioSink) {
                float latSec = (float)AudioSinkLatencyNs() / 1000000000.0f;
                if (latSec > 0.0f) {
                    elapsed -= latSec;
                    if (elapsed < 0.0f) {
                        elapsed = 0.0f;
                    }
                    remaining += latSec;
                    if (!m_loggedSinkLatency) {
                        m_loggedSinkLatency = true;
                        LogInfo(VB_MEDIAOUT, "GStreamer: correcting reported position by %.1f ms of audio sink latency\n",
                                latSec * 1000.0f);
                    }
                }
            }
            setMediaElapsed(elapsed, remaining);

            // Only the primary slot represents the show's synced position;
            // companion streams (slot > 1, e.g. extraMedia) play locally on
            // each device and must not broadcast themselves as the master.
            if (m_streamSlot == 1 && multiSync->isMultiSyncEnabled()) {
                multiSync->SendMediaSyncPacket(m_mediaFilename, m_mediaOutputStatus->mediaSeconds);
            }
            CalculateNewChannelOutputDelay(m_mediaOutputStatus->mediaSeconds);

            // Always update total duration — it may be refined for VBR media
            if (effectiveDur > 0) {
                int totalSecs = (int)(effectiveDur / GST_SECOND);
                int newMin = totalSecs / 60;
                int newSec = totalSecs % 60;
                if (newMin != m_mediaOutputStatus->minutesTotal ||
                    newSec != m_mediaOutputStatus->secondsTotal) {
                    m_mediaOutputStatus->minutesTotal = newMin;
                    m_mediaOutputStatus->secondsTotal = newSec;
                    LogDebug(VB_MEDIAOUT, "GStreamer duration: %d:%02d\n", newMin, newSec);
                }
            }

            // Stall watchdog: detect when PipeWire stops consuming data
            // (e.g. HDMI sink unplugged causing combine-stream to block)
            // Skip watchdog near end of media — position naturally stops advancing
            bool nearEnd = (effectiveDur > 0 && (effectiveDur - pos) < GST_SECOND);

            if (pos != m_lastPosition) {
                m_lastPosition = pos;
                m_stallStartMs = 0; // position advancing, clear stall timer
            } else if (nearEnd) {
                // Position at/near end of media — this is natural completion, not a stall.
                // Wait for EOS from the pipeline, or handle it ourselves after a grace period.
                uint64_t now = GetTimeMS();
                if (m_stallStartMs == 0) {
                    m_stallStartMs = now;
                } else if ((now - m_stallStartMs) > (STALL_TIMEOUT_MS * 2)) {
                    // EOS should have arrived by now but didn't — force end
                    LogInfo(VB_MEDIAOUT, "GStreamer: media reached end (%.1fs/%.1fs), forcing stop\n",
                            elapsed, (float)effectiveDur / GST_SECOND);
                    m_playing = false;
                    if (m_mediaOutputStatus) {
                        m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
                    }
                    Stopping();
                    Stopped();
                    return 0;
                }
            } else {
                // Position unchanged — start or continue stall timer
                uint64_t now = GetTimeMS();
                if (m_stallStartMs == 0) {
                    m_stallStartMs = now;
                    LogDebug(VB_MEDIAOUT, "GStreamer: position stalled at %.1fs, starting watchdog\n", elapsed);
                } else if ((now - m_stallStartMs) > STALL_TIMEOUT_MS) {
                    LogWarn(VB_MEDIAOUT, "GStreamer pipeline stalled for %dms at position %.1fs — "
                            "audio sink may be blocked (HDMI unplugged?). Stopping playback.\n",
                            STALL_TIMEOUT_MS, elapsed);
                    m_playing = false;
                    if (m_mediaOutputStatus) {
                        m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
                    }
                    Stopping();
                    Stopped();
                    return 0;
                }
            }
        } else {
            // Preroll watchdog: position query keeps failing because the
            // pipeline never reached PLAYING.  Normal startup clears this within
            // a fraction of a second (m_wallStartMs gets set on the first valid
            // position).  If we're still here after PREROLL_TIMEOUT_MS the
            // pipeline is wedged in preroll — e.g. the V4L2 HW decoder stopped
            // producing frames after a long video-only loop (issue #2695) so the
            // video sink never prerolls and the whole pipeline stays PAUSED.
            // The regular stall watchdog above can't catch this (it lives inside
            // the havePos branch), so without this check m_playing stays true
            // forever, the playlist never advances, and only a reboot recovers.
            // Force a stop so FinishPlay()/CloseMediaOutput() tears the pipeline
            // down and the playlist can move on to the next item.
            if (m_wallStartMs == 0 && m_playStartMs > 0 &&
                (GetTimeMS() - m_playStartMs) > PREROLL_TIMEOUT_MS) {
                LogWarn(VB_MEDIAOUT, "GStreamer pipeline failed to preroll within %dms "
                        "(stuck in PAUSED — HW decoder may have wedged, see issue #2695). "
                        "Forcing stop so the playlist advances.\n", PREROLL_TIMEOUT_MS);
                RecordWedgeEventAndMaybeRestart("pipeline failed to preroll");
                m_playing = false;
                if (m_mediaOutputStatus) {
                    m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
                }
                Stopping();
                Stopped();
                return 0;
            }
            LogExcess(VB_MEDIAOUT, "GStreamer position query pending (pipeline not yet PLAYING)\n");
        }
    }

    return m_playing ? 1 : 0;
}

void GStreamerOutput::ProcessMessages() {
    if (!m_bus)
        return;

    GstMessage* msg;
    while ((msg = gst_bus_pop(m_bus)) != nullptr) {
        switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            LogDebug(VB_MEDIAOUT, "GStreamer: End of stream\n");
            if (m_loopCount > 0 || m_loopCount == -1) {
                // Loop: seek back to beginning
                if (m_loopCount > 0)
                    m_loopCount--;
                gst_element_seek_simple(m_pipeline, GST_FORMAT_TIME,
                                        (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                        0);
                LogDebug(VB_MEDIAOUT, "GStreamer: Looping (remaining: %d)\n", m_loopCount);
            } else {
                m_playing = false;
                if (m_mediaOutputStatus) {
                    m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
                }
                Stopping();
                Stopped();
            }
            break;

        case GST_MESSAGE_ERROR: {
            GError* err;
            gchar* debug;
            gst_message_parse_error(msg, &err, &debug);
            LogErr(VB_MEDIAOUT, "GStreamer error: %s\n", err->message);
            LogDebug(VB_MEDIAOUT, "GStreamer debug: %s\n", debug ? debug : "(none)");
            g_error_free(err);
            g_free(debug);
            m_playing = false;
            if (m_mediaOutputStatus) {
                m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
            }
            Stopping();
            Stopped();
            break;
        }

        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(m_pipeline)) {
                GstState oldState, newState, pending;
                gst_message_parse_state_changed(msg, &oldState, &newState, &pending);
                LogDebug(VB_MEDIAOUT, "GStreamer state: %s -> %s\n",
                         gst_element_state_get_name(oldState),
                         gst_element_state_get_name(newState));
                if (newState == GST_STATE_PLAYING) {
                    Playing();
                }
            }
            break;
        }

        default:
            break;
        }
        gst_message_unref(msg);
    }
}

GstBusSyncReply GStreamerOutput::BusSyncHandler(GstBus* bus, GstMessage* msg, gpointer userData) {
    GStreamerOutput* self = static_cast<GStreamerOutput*>(userData);
    if (!self)
        return GST_BUS_PASS;

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
        LogInfo(VB_MEDIAOUT, "GStreamer sync: End of stream\n");
        if (self->m_loopCount > 0 || self->m_loopCount == -1) {
            if (self->m_loopCount > 0)
                self->m_loopCount--;
            gst_element_seek_simple(self->m_pipeline, GST_FORMAT_TIME,
                                    (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                    0);
            LogDebug(VB_MEDIAOUT, "GStreamer sync: Looping (remaining: %d)\n", self->m_loopCount);
        } else {
            // Detach AES67 inline branches so the standalone pipeline
            // resumes sending — EOS means no more data will flow through
            // the tee, so the inline branch is useless.
#ifdef HAS_AES67_GSTREAMER
            self->DetachAES67Branches();
#endif
            self->m_playing = false;
            if (self->m_mediaOutputStatus) {
                self->m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
            }
            self->Stopping();
            self->Stopped();
        }
        gst_message_unref(msg);
        return GST_BUS_DROP;

    case GST_MESSAGE_ERROR: {
        GError* err;
        gchar* debug;
        gst_message_parse_error(msg, &err, &debug);

        // Identify which element produced the error
        const gchar* srcName = GST_MESSAGE_SRC(msg) ?
            GST_OBJECT_NAME(GST_MESSAGE_SRC(msg)) : "unknown";

        // AES67 inline RTP branch elements are named "aes67_*".
        // Errors from these branches (e.g. network issues, PipeWire
        // disconnects) should NOT stop media playback.
        // Video PipeWire sink errors ("pwvideosink") are always non-fatal
        // because kmssink is the primary video output — the pipewiresink
        // is a secondary tee branch for PipeWire graph visibility only.
        bool isAES67Branch = (strncmp(srcName, "aes67_", 6) == 0);
        bool isVideoPWSink = (strcmp(srcName, "pwvideosink") == 0);
        // Direct kmssink branches (dkms_*) may fail if a display is
        // disconnected during playback — treat as non-fatal.
        bool isDirectKmsSink = (strncmp(srcName, "dkms_", 5) == 0);

        // A file with nothing decodable to feed the audio-only pipeline fails
        // here, and the raw GStreamer wording is actively misleading: decodebin
        // reports its autoplug failure as "your installation is missing a
        // plug-in" (nothing is missing -- there is simply no audio to plug), and
        // the demuxer then follows with a generic "Internal data stream error".
        // A video-only clip played on an item whose video output is disabled or
        // whose display is unplugged lands exactly here, so say what actually
        // happened and what to do about it.  Everything else keeps the raw
        // message -- this only claims the case it can prove, which is that
        // decodebin never produced an audio pad.
        // "No audio pad yet" is not on its own evidence that the file has no
        // audio: anything that fails before decodebin gets that far leaves the
        // flag clear too.  Stopping PipeWire under a running fppd and playing a
        // plain WAV does exactly that -- pwsink errors first, and blaming the
        // file for an audio-backend outage is a worse lie than the one being
        // fixed.  So the source has to be the decode chain as well.
        bool errorFromDecoder = false;
        for (GstObject* o = GST_MESSAGE_SRC(msg); o; o = GST_OBJECT_PARENT(o)) {
            const gchar* n = GST_OBJECT_NAME(o);
            if (n && strcmp(n, "decoder") == 0) {
                errorFromDecoder = true;
                break;
            }
        }
        if (self->m_audioOnlyPipeline && errorFromDecoder &&
            !self->m_sawAudioPad.load(std::memory_order_acquire)) {
            if (!self->m_reportedNoAudio.exchange(true, std::memory_order_acq_rel)) {
                LogErr(VB_MEDIAOUT,
                       "GStreamer: '%s' has no playable audio stream, and no video output is "
                       "configured for it, so there is nothing to play.  If this is a video-only "
                       "file, set the item's Video Output to a connected display; FPP falls back to "
                       "audio-only whenever the chosen display is disabled or unplugged.\n",
                       self->m_mediaFilename.c_str());
                WarningHolder::AddWarningTimeout(60, 30,
                                                "No playable audio in " + self->m_mediaFilename +
                                                    " and no video output — nothing to play");
                LogDebug(VB_MEDIAOUT, "GStreamer no-audio underlying error (src=%s): %s\n",
                         srcName, err->message);
#ifdef HAS_AES67_GSTREAMER
                self->DetachAES67Branches();
#endif
                self->m_playing = false;
                if (self->m_mediaOutputStatus) {
                    self->m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
                }
                self->Stopping();
                self->Stopped();
            } else {
                // The demuxer's follow-on error describes the same event; logging
                // it again reads as a second, unrelated failure.
                LogDebug(VB_MEDIAOUT, "GStreamer no-audio follow-on error (src=%s): %s\n",
                         srcName, err->message);
            }
        } else if (isAES67Branch || isVideoPWSink || isDirectKmsSink) {
            LogWarn(VB_MEDIAOUT, "GStreamer non-fatal error (src=%s): %s\n",
                    srcName, err->message);
            LogDebug(VB_MEDIAOUT, "GStreamer AES67 branch debug: %s\n",
                     debug ? debug : "(none)");
        } else {
            LogErr(VB_MEDIAOUT, "GStreamer sync error (src=%s): %s\n", srcName, err->message);
            LogDebug(VB_MEDIAOUT, "GStreamer sync debug: %s\n", debug ? debug : "(none)");
#ifdef HAS_AES67_GSTREAMER
            self->DetachAES67Branches();
#endif
            self->m_playing = false;
            if (self->m_mediaOutputStatus) {
                self->m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
            }
            self->Stopping();
            self->Stopped();
        }

        g_error_free(err);
        g_free(debug);
        gst_message_unref(msg);
        return GST_BUS_DROP;
    }

    case GST_MESSAGE_STATE_CHANGED: {
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(self->m_pipeline)) {
            GstState oldState, newState, pending;
            gst_message_parse_state_changed(msg, &oldState, &newState, &pending);
            LogDebug(VB_MEDIAOUT, "GStreamer sync state: %s -> %s\n",
                     gst_element_state_get_name(oldState),
                     gst_element_state_get_name(newState));
            if (newState == GST_STATE_PLAYING) {
                self->Playing();
            }
        }
        // Let state changes pass through for normal GStreamer operation
        return GST_BUS_PASS;
    }

    default:
        return GST_BUS_PASS;
    }
}

// Read a single "<Key>: <N> kB" line from /proc/meminfo.  Returns -1 if the
// key isn't present.  Small and self-contained on purpose — mirrors the
// CmaFree check VideoOutputManager::StartConsumer does before launching an
// HDMI consumer, generalized to whichever key is asked for.
static long ReadMeminfoKB(const char* key) {
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f)
        return -1;
    char line[256];
    long kb = -1;
    size_t keyLen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, keyLen) == 0 && line[keyLen] == ':') {
            sscanf(line + keyLen + 1, "%ld", &kb);
            break;
        }
    }
    fclose(f);
    return kb;
}

int GStreamerOutput::Close(void) {
    LogDebug(VB_MEDIAOUT, "GStreamerOutput::Close()\n");

    // Detach the decodebin pad callbacks from this object BEFORE any teardown.
    // Start() hands an owning pipeline ref to a detached thread that calls
    // set_state(PLAYING), so decodebin can still be exposing pads after this
    // Close() nulls m_audioChain/m_videoChain and drops our pipeline ref -- and
    // after ~GStreamerOutput frees the object outright.  Clearing `self` under
    // the guard lock both blocks until any in-flight callback finishes and
    // makes every later one a no-op.
    if (m_cbGuard) {
        {
            std::lock_guard<std::mutex> lock(m_cbGuard->mtx);
            m_cbGuard->self = nullptr;
        }
        m_cbGuard.reset();
    }

    if (m_pipeline) {
        // Flush PipeWire filter-chain delay buffers.  Each audio group member
        // has a builtin delay node whose internal ring-buffer retains old
        // audio.  Setting the delay to 0 empties it; restoring the original
        // value afterwards starts accumulating from scratch with silence.
        // Spawned as a detached thread so we don't block Close().
        FlushPipeWireDelayBuffers();

        // Flush AES67 send pipelines — drop a few buffers to discard
        // tail-end audio from the ending track.  The Start()-time flush
        // is the primary protection; this Close() flush is supplementary
        // for cases where Close→Start is very fast (back-to-back tracks).
#ifdef HAS_AES67_GSTREAMER
        if (AES67Manager::INSTANCE.IsActive()) {
            AES67Manager::INSTANCE.FlushSendPipelines();
        }
#endif

        // Stop() handles shutdown flag, appsink/bus cleanup, AES67 detach,
        // and the pipeline state change.
        Stop();

        // Restore overlay model state if we enabled it
        if (m_wasOverlayDisabled) {
            std::lock_guard<std::mutex> lock(m_videoOverlayModelLock);
            if (m_videoOverlayModel) {
                m_videoOverlayModel->setState(PixelOverlayState(PixelOverlayState::Disabled));
            }
            m_wasOverlayDisabled = false;
        }

        if (m_appsink) {
            gst_object_unref(m_appsink);
            m_appsink = nullptr;
        }
        if (m_videoAppsink) {
            gst_object_unref(m_videoAppsink);
            m_videoAppsink = nullptr;
        }
        if (m_volume) {
            gst_object_unref(m_volume);
            m_volume = nullptr;
        }
        if (m_bus) {
            gst_object_unref(m_bus);
            m_bus = nullptr;
        }
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }

    // Return any DRM overlay planes we reserved for this pipeline's kmssink
    // elements to the shared free pool.  The kmssinks themselves were owned by
    // the pipeline bin and were destroyed by the unref above; without this the
    // planes stay marked allocated forever and eventually run out.
    //
    // EXCEPT after an abandoned (wedged) teardown: the pipeline and its
    // kmssinks are still alive on the reaper thread and still hold these
    // planes in DRM — returning them to the pool would let the next pipeline
    // claim a plane the kernel considers busy.  Leak them with the pipeline;
    // the wedge-escalation restart bounds the loss.
    if (!m_teardownAbandoned) {
        for (int planeId : m_allocatedPlanes) {
            ReleasePlane(planeId);
        }
    }
    m_allocatedPlanes.clear();

    // Release the shared DRM master fd(s) this pipeline's kmssink(s) acquired.
    // Must run after gst_object_unref(m_pipeline) above — the kmssinks (which
    // hold the fd open via kmssink's internal use) are destroyed as part of
    // that unref, and ReleaseSharedDrmFd() must not close the fd out from
    // under a still-alive kmssink.  Runs unconditionally (outside the
    // `if (m_pipeline)` guard) so a Start() that failed after acquiring a fd
    // but before/without building a full pipeline still releases it.
    //
    // Same abandonment exception as the planes above: a wedged pipeline's
    // kmssinks are still alive and using the fd — dropping our refs could
    // close it under them.  Leak the refs with the pipeline.
    if (!m_teardownAbandoned) {
        for (const std::string& card : m_acquiredDrmCards) {
            ReleaseSharedDrmFd(card);
        }
    }
    m_acquiredDrmCards.clear();

    // Clean up video overlay state
    if (m_videoFramesReceived > 0 || m_videoFramesDelivered > 0) {
        LogDebug(VB_MEDIAOUT, "GStreamer video overlay stats: %lu frames received, %lu delivered\n",
                (unsigned long)m_videoFramesReceived, (unsigned long)m_videoFramesDelivered);
    }
    m_hasVideoStream = false;
    m_videoFrameReady = false;
    m_videoFramesReceived = 0;
    m_videoFramesDelivered = 0;
    m_audioChain = nullptr;
    m_videoChain = nullptr;
    m_kmssink = nullptr;     // owned by pipeline bin, already freed
    m_wantHDMI = false;

    // Consumers already stopped before pipeline NULL; just clear state
    m_videoPipeWireRouting = false;
    m_pwVideoSinkName.clear();

    // Remove model listener
    if (!m_videoOut.empty() && m_videoOut != "--Disabled--") {
        PixelOverlayManager::INSTANCE.removeModelListener(m_videoOut, "GStreamerOut");
    }
    // Deregister before taking m_videoOverlayModelLock: ProcessVideoOverlay()
    // walks the registry and then takes that lock, so acquiring them in the
    // other order here would be a lock inversion.
    {
        std::lock_guard<std::mutex> lock(m_overlayOutputsLock);
        m_overlayOutputs.erase(std::remove(m_overlayOutputs.begin(), m_overlayOutputs.end(), this),
                               m_overlayOutputs.end());
    }
    {
        std::lock_guard<std::mutex> lock(m_videoOverlayModelLock);
        m_videoOverlayModel = nullptr;
    }

    if (m_currentInstance == this) {
        // If we're closing the primary (WLED tap) instance, try to hand off
        // to another active slot (prefer slot 1)
        m_currentInstance = nullptr;
        for (int s = 1; s <= StreamSlotManager::MAX_SLOTS; s++) {
            if (s == m_streamSlot) continue;
            GStreamerOutput* other = StreamSlotManager::Instance().GetActiveOutput(s);
            if (other && other->m_playing) {
                m_currentInstance = other;
                break;
            }
        }
    }

    // Deregister from StreamSlotManager (only if we still own the slot --
    // see ClearSlot()'s ownership check).
    StreamSlotManager::Instance().ClearSlot(m_streamSlot, this);

    // Per-track CMA trend line — logged unconditionally (not just when
    // m_pipeline was set) so 24h soak-test logs show CMA after every Close(),
    // including failed-Start cycles, letting a slow leak be correlated with
    // build/teardown count instead of only track count.
    {
        long cmaFreeKB = ReadMeminfoKB("CmaFree");
        long cmaTotalKB = ReadMeminfoKB("CmaTotal");
        LogInfo(VB_MEDIAOUT, "GStreamer: post-close CmaFree=%ld kB / CmaTotal=%ld kB\n", cmaFreeKB, cmaTotalKB);
    }

    return 1;
}

int GStreamerOutput::IsPlaying(void) {
    return m_playing ? 1 : 0;
}

int GStreamerOutput::AdjustSpeed(float masterMediaPosition) {
    if (!m_pipeline || !m_allowSpeedAdjust)
        return 1;

    // Nothing below may touch a pipeline that has never produced a position:
    // both branches that act on a large diff issue a *synchronous* seek, and a
    // seek into a pipeline still stuck in preroll blocks the calling thread
    // forever inside the wedged sink.  This runs on the main loop (MultiSync
    // ProcessControlPacket -> UpdateMasterMediaPosition) while holding
    // mediaOutputLock, so that block takes the whole player down: no sync
    // packets, no status, no `fpp` commands, and "Restart FPPD" never even gets
    // read (issue #2727 -- confirmed 2026-07-29 on FPPv4-3, main thread parked
    // on a PipeWire mutex with the pipeline still ASYNC).  Sitting out the
    // sync until preroll completes is free: the preroll watchdog in Process()
    // tears a genuinely wedged pipeline down and the playlist advances.
    if (m_wallStartMs == 0) {
        LogDebug(VB_MEDIAOUT, "GStreamer: skipping speed adjust, pipeline has not prerolled yet (master %0.3f)\n",
                 masterMediaPosition);
        return 1;
    }

    // Can't adjust speed if not playing yet
    if (m_mediaOutputStatus->mediaSeconds < 0.01f) {
        LogDebug(VB_MEDIAOUT, "GStreamer: Can't adjust speed if not playing yet (%0.3f/%0.3f)\n",
                 masterMediaPosition, m_mediaOutputStatus->mediaSeconds);
        return 1;
    }
    if (m_mediaOutputStatus->mediaSeconds > 1 && m_mediaOutputStatus->status == MEDIAOUTPUTSTATUS_IDLE) {
        LogDebug(VB_MEDIAOUT, "GStreamer: Can't adjust speed if beyond end of media (%0.3f/%0.3f)\n",
                 masterMediaPosition, m_mediaOutputStatus->mediaSeconds);
        return 1;
    }

    float rate = m_currentRate;

    if (m_lastRates.empty()) {
        // Preload rate list with normal (1.0) rate
        m_lastRates.push_back(1.0f);
        m_lastRatesSum = 1.0f;
    }

    int rawdiff = (int)(m_mediaOutputStatus->mediaSeconds * 1000) - (int)(masterMediaPosition * 1000);
    int diff = rawdiff;
    int sign = 1;
    if (diff < 0) {
        sign = -1;
        diff = -diff;
    }

    if ((m_mediaOutputStatus->mediaSeconds < 1) || (diff > 3000)) {
        LogDebug(VB_MEDIAOUT, "GStreamer Diff: %d\tMaster: %0.3f  Local: %0.3f  Rate: %0.3f\n",
                 rawdiff, masterMediaPosition, m_mediaOutputStatus->mediaSeconds, m_currentRate);
    } else {
        LogExcess(VB_MEDIAOUT, "GStreamer Diff: %d\tMaster: %0.3f  Local: %0.3f  Rate: %0.3f\n",
                  rawdiff, masterMediaPosition, m_mediaOutputStatus->mediaSeconds, m_currentRate);
    }

    pushDiff(rawdiff, m_currentRate);

    // Sign-flip detection: if diff sign flipped and we're not at normal speed,
    // reset to 1.0 to avoid oscillation
    int oldSign = m_lastDiff < 0 ? -1 : 1;
    if ((oldSign != sign) && (m_lastDiff != 0) && (m_currentRate != 1.0f)) {
        LogDebug(VB_MEDIAOUT, "GStreamer Diff: %d\tFlipped, reset speed to normal\t(%0.3f)\n", rawdiff, 1.0f);
        ApplyRate(1.0f);
        // Reset rate average list to 1.0
        m_lastRates.clear();
        m_lastRates.push_back(1.0f);
        m_lastRatesSum = 1.0f;
        m_currentRate = 1.0f;
        m_rateDiff = 0;
        m_lastDiff = rawdiff;
        return 1;
    }

    if (diff < 30) {
        // Close enough — return to normal rate
        if (m_currentRate != 1.0f) {
            rate = 1.0f;
            LogDebug(VB_MEDIAOUT, "GStreamer Diff: %d\tVery close, use normal rate\t(%0.3f)\n", rawdiff, rate);
            ApplyRate(rate);
            m_lastRates.push_back(rate);
            m_lastRatesSum += rate;
            while ((int)m_lastRates.size() > RATE_AVERAGE_COUNT) {
                m_lastRatesSum -= m_lastRates.front();
                m_lastRates.pop_front();
            }
            m_currentRate = rate;
            m_rateDiff = 0;
            m_lastDiff = rawdiff;
        }
        return 1;
    } else if (diff > 10000) {
        // More than 10 seconds off — jump to the master position
        gint64 pos_ns = (gint64)(masterMediaPosition * GST_SECOND);
        LogDebug(VB_MEDIAOUT, "GStreamer Diff: %d\tVery far, jumping to: %0.3f\t(currently at %0.3f)\n",
                 rawdiff, masterMediaPosition, m_mediaOutputStatus->mediaSeconds);
        gst_element_seek(m_pipeline, 1.0, GST_FORMAT_TIME,
                         (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                         GST_SEEK_TYPE_SET, pos_ns, GST_SEEK_TYPE_NONE, 0);
        m_lastRates.clear();
        m_lastRates.push_back(1.0f);
        m_lastRatesSum = 1.0f;
        m_currentRate = 1.0f;
        m_rateDiff = 0;
        m_lastDiff = -1; // after seeking, assume slightly behind master
        return 1;
    } else if (diff < 100) {
        // Very close — could be transient; delay one cycle
        if (!m_lastDiff) {
            LogDebug(VB_MEDIAOUT, "GStreamer Diff: %d\tVery close but could be transient, wait till next time\n", rawdiff);
            m_lastDiff = rawdiff;
            return 1;
        }
    }

    // Calculate proportional rate adjustment
    float rateDiffF = (float)diff;
    if (m_mediaOutputStatus->mediaSeconds > 10) {
        rateDiffF /= 100.0f;
        if (rateDiffF > 10.0f)
            rateDiffF = 10.0f;
    } else {
        // In first 10 seconds, use larger rate changes to sync faster
        rateDiffF /= 50.0f;
        if (rateDiffF > 20.0f)
            rateDiffF = 20.0f;
    }

    rateDiffF *= sign;
    int rateDiffI = (int)std::round(rateDiffF);

    LogExcess(VB_MEDIAOUT, "GStreamer Diff: %d\trateDiffI: %d  m_rateDiff: %d\n", rawdiff, rateDiffI, m_rateDiff);

    if (rateDiffI < m_rateDiff) {
        for (int r = rateDiffI; r < m_rateDiff; r++) {
            rate = rate * 1.02f;
        }
        LogDebug(VB_MEDIAOUT, "GStreamer Diff: %d\tSpeedUp  %0.3f/%0.3f [goal/current]\n", rawdiff, rate, m_currentRate);
    } else if (rateDiffI > m_rateDiff) {
        for (int r = rateDiffI; r > m_rateDiff; r--) {
            rate = rate * 0.98f;
        }
        LogDebug(VB_MEDIAOUT, "GStreamer Diff: %d\tSlowDown %0.3f/%0.3f [goal/current]\n", rawdiff, rate, m_currentRate);
    } else {
        // No rate change needed
        LogExcess(VB_MEDIAOUT, "GStreamer Diff: %d\tno rate change\n", rawdiff);
        return 1;
    }

    // Add to rate history for running average
    m_lastRates.push_back(rate);
    m_lastRatesSum += rate;
    if ((int)m_lastRates.size() > RATE_AVERAGE_COUNT) {
        m_lastRatesSum -= m_lastRates.front();
        m_lastRates.pop_front();
    }

    // Cross-unity check: if rate crossed 1.0, reset to 1.0
    if (((rate > 1.0f) && (m_currentRate < 1.0f)) || ((rate < 1.0f) && (m_currentRate > 1.0f))) {
        rate = 1.0f;
        m_rateDiff = 0;
    }

    LogExcess(VB_MEDIAOUT, "GStreamer Diff: %d\toldDiff: %d\tnewRate: %0.3f oldRate: %0.3f avgRate: %0.3f rateSum: %0.3f/%d\n",
              rawdiff, m_lastDiff, m_lastRates.back(), m_currentRate, rate, m_lastRatesSum, (int)m_lastRates.size());

    // Clamp rate to safe range
    if (rate > 2.0f)
        rate = 2.0f;
    if (rate < 0.5f)
        rate = 0.5f;

    // Only apply if rate changed by > 0.001
    if ((int)(rate * 1000) != (int)(m_currentRate * 1000)) {
        LogDebug(VB_MEDIAOUT, "GStreamer Diff: %d\tApplyRate\t(%0.3f)\n", rawdiff, rate);
        ApplyRate(rate);
        m_currentRate = rate;
        if (rate == 1.0f) {
            m_rateDiff = 0;
        } else {
            m_rateDiff = rateDiffI;
        }
    }

    m_lastDiff = rawdiff;
    return 1;
}

void GStreamerOutput::pushDiff(int diff, float rate) {
    m_diffSum += diff;
    m_rateSum += rate;
    if (m_diffsSize < MAX_DIFFS) {
        m_diffIdx = m_diffsSize;
        m_diffsSize++;
    } else {
        m_diffIdx++;
        if (m_diffIdx == MAX_DIFFS) {
            m_diffIdx = 0;
        }
        m_diffSum -= m_diffs[m_diffIdx].first;
        m_rateSum -= m_diffs[m_diffIdx].second;
    }
    m_diffs[m_diffIdx].first = diff;
    m_diffs[m_diffIdx].second = rate;
}

bool GStreamerOutput::SeekTo(float seconds) {
    if (!m_pipeline)
        return false;
    gint64 pos_ns = (gint64)(seconds * GST_SECOND);
    return gst_element_seek(m_pipeline, 1.0, GST_FORMAT_TIME,
                            (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                            GST_SEEK_TYPE_SET, pos_ns, GST_SEEK_TYPE_NONE, 0)
               ? true
               : false;
}

void GStreamerOutput::ApplyRate(float rate) {
    if (!m_pipeline)
        return;

    // Use instant-rate-change (GStreamer >= 1.18) for glitch-free rate adjustment.
    // Falls back to a flush seek if instant-rate-change fails.
    gboolean ok = gst_element_seek(m_pipeline, (gdouble)rate, GST_FORMAT_TIME,
                                   GST_SEEK_FLAG_INSTANT_RATE_CHANGE,
                                   GST_SEEK_TYPE_NONE, 0, GST_SEEK_TYPE_NONE, 0);
    if (!ok) {
        // Fallback: flush seek to current position with new rate
        gint64 pos = 0;
        if (gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &pos)) {
            LogDebug(VB_MEDIAOUT, "GStreamer: instant-rate-change failed, falling back to flush seek at %" GST_TIME_FORMAT "\n",
                     GST_TIME_ARGS(pos));
            gst_element_seek(m_pipeline, (gdouble)rate, GST_FORMAT_TIME,
                             (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                             GST_SEEK_TYPE_SET, pos, GST_SEEK_TYPE_NONE, 0);
        } else {
            LogWarn(VB_MEDIAOUT, "GStreamer: ApplyRate(%0.3f) failed — could not query position\n", rate);
        }
    }
}

void GStreamerOutput::SetVolume(int volume) {
    if (m_volume) {
        // volume parameter is 0-100 percentage
        double linearVol = volume / 100.0;
        g_object_set(m_volume, "volume", linearVol, NULL);
        LogDebug(VB_MEDIAOUT, "GStreamer volume set to %d%% (%.2f)\n", volume, linearVol);
    }
}

void GStreamerOutput::SetVolumeAdjustment(int volAdj) {
    m_volumeAdjust = volAdj;
    if (m_volume && m_playing) {
        double linearVol = pow(10.0, volAdj / 2000.0);
        g_object_set(m_volume, "volume", linearVol, NULL);
    }
}

// Static video overlay methods — called from Sequence.cpp and channeloutputthread.cpp
// These walk every output driving a PixelOverlay model rather than the single
// m_currentInstance they used to.  m_currentInstance is claimed by slot 1
// whenever slot 1 is playing, so a companion stream pointed at a model would
// register the model, receive frames, and then have them go nowhere -- the
// model simply never updated.  m_currentInstance still selects the WLED audio
// tap, which genuinely is a single-source thing.
bool GStreamerOutput::IsOverlayingVideo() {
    std::lock_guard<std::mutex> lock(m_overlayOutputsLock);
    for (GStreamerOutput* o : m_overlayOutputs) {
        if (o->m_hasVideoStream && o->m_playing && o->m_videoOverlayModel)
            return true;
    }
    return false;
}

bool GStreamerOutput::ProcessVideoOverlay(unsigned int msTimestamp) {
    std::lock_guard<std::mutex> lock(m_overlayOutputsLock);
    for (GStreamerOutput* o : m_overlayOutputs) {
        o->PushVideoOverlayFrame();
    }
    return false;
}

bool GStreamerOutput::PushVideoOverlayFrame() {
    GStreamerOutput* self = this;
    if (!self->m_playing || !self->m_hasVideoStream)
        return false;

    // Copy the latest video frame data under lock
    std::vector<uint8_t> frameData;
    {
        std::lock_guard<std::mutex> lock(self->m_videoFrameMutex);
        if (!self->m_videoFrameReady)
            return false;
        frameData = self->m_videoFrameData;
        self->m_videoFrameReady = false;
    }

    // Push RGB data to the PixelOverlayModel
    std::lock_guard<std::mutex> lock(self->m_videoOverlayModelLock);
    if (self->m_videoOverlayModel && !frameData.empty()) {
        self->m_videoOverlayModel->setData(frameData.data());
        self->m_videoFramesDelivered++;

        if (self->m_videoFramesDelivered == 1 || (self->m_videoFramesDelivered % 100) == 0) {
            LogExcess(VB_MEDIAOUT, "GStreamer: video frame %lu delivered to overlay (%zu bytes)\n",
                    (unsigned long)self->m_videoFramesDelivered, frameData.size());
        }

        // Auto-enable model if it was disabled (same as SDLOutput behavior)
        if (self->m_videoOverlayModel->getState() == PixelOverlayState::Disabled) {
            self->m_wasOverlayDisabled = true;
            self->m_videoOverlayModel->setState(PixelOverlayState(PixelOverlayState::Enabled));
        }
    }
    return false;
}

GstFlowReturn GStreamerOutput::OnNewVideoSample(GstAppSink* appsink, gpointer userData) {
    GStreamerOutput* self = static_cast<GStreamerOutput*>(userData);
    if (!self || self->m_shutdownFlag.load(std::memory_order_acquire))
        return GST_FLOW_EOS;

    GstSample* sample = gst_app_sink_pull_sample(appsink);
    if (!sample)
        return GST_FLOW_OK;

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        int width = self->m_videoOverlayWidth;
        int height = self->m_videoOverlayHeight;
        int rowBytes = width * 3;  // RGB = 3 bytes/pixel, tightly packed
        int expectedSize = rowBytes * height;
        int stride = (height > 0) ? (int)(map.size / height) : rowBytes;

        std::lock_guard<std::mutex> lock(self->m_videoFrameMutex);
        if (stride == rowBytes) {
            // No padding — direct copy
            self->m_videoFrameData.assign(map.data, map.data + expectedSize);
        } else {
            // GStreamer pads rows to 4-byte alignment — strip padding
            self->m_videoFrameData.resize(expectedSize);
            for (int y = 0; y < height; y++) {
                memcpy(&self->m_videoFrameData[y * rowBytes], map.data + y * stride, rowBytes);
            }
        }
        self->m_videoFrameReady = true;
        self->m_videoFramesReceived++;
        if (self->m_videoFramesReceived == 1 || (self->m_videoFramesReceived % 100) == 0) {
            LogExcess(VB_MEDIAOUT, "GStreamer: video frame %lu received (%zu bytes, stride=%d, rowBytes=%d)\n",
                    (unsigned long)self->m_videoFramesReceived, map.size, stride, rowBytes);
        }
        gst_buffer_unmap(buffer, &map);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

void GStreamerOutput::ReleaseCallbackGuard(gpointer data, GClosure* closure) {
    delete static_cast<std::shared_ptr<CallbackGuard>*>(data);
}

void GStreamerOutput::ConnectPadSignals(GstElement* decoder, bool wantNoMorePads) {
    if (!decoder || !m_cbGuard)
        return;
    // The closure owns its own shared_ptr to the guard, released by
    // ReleaseCallbackGuard when the signal connection dies with the decoder --
    // which can be well after this output is gone.
    g_signal_connect_data(decoder, "pad-added", G_CALLBACK(OnPadAdded),
                          new std::shared_ptr<CallbackGuard>(m_cbGuard),
                          ReleaseCallbackGuard, (GConnectFlags)0);
    if (wantNoMorePads) {
        g_signal_connect_data(decoder, "no-more-pads", G_CALLBACK(OnNoMorePads),
                              new std::shared_ptr<CallbackGuard>(m_cbGuard),
                              ReleaseCallbackGuard, (GConnectFlags)0);
    }
}

GStreamerOutput* GStreamerOutput::LockCallbackGuard(gpointer userData,
                                                    std::shared_ptr<CallbackGuard>& guard,
                                                    std::unique_lock<std::mutex>& lock) {
    auto* held = static_cast<std::shared_ptr<CallbackGuard>*>(userData);
    if (!held || !*held)
        return nullptr;
    guard = *held;
    lock = std::unique_lock<std::mutex>(guard->mtx);
    if (!guard->self) {
        // Close() already ran: the pipeline this callback belongs to is being
        // (or has been) torn down and every member below is stale.
        lock.unlock();
        return nullptr;
    }
    return guard->self;
}

void GStreamerOutput::OnPadAdded(GstElement* element, GstPad* pad, gpointer userData) {
    std::shared_ptr<CallbackGuard> guard;
    std::unique_lock<std::mutex> guardLock;
    GStreamerOutput* self = LockCallbackGuard(userData, guard, guardLock);
    if (!self)
        return;

    GstCaps* caps = gst_pad_get_current_caps(pad);
    if (!caps)
        caps = gst_pad_query_caps(pad, nullptr);

    if (!caps)
        return;

    // Log all structures in the caps for debugging
    for (guint i = 0; i < gst_caps_get_size(caps); i++) {
        const gchar* sname = gst_structure_get_name(gst_caps_get_structure(caps, i));
        LogDebug(VB_MEDIAOUT, "GStreamer decodebin pad-added caps[%u]: %s\n", i, sname);
    }

    const gchar* name = gst_structure_get_name(gst_caps_get_structure(caps, 0));
    LogDebug(VB_MEDIAOUT, "GStreamer decodebin pad-added: %s\n", name);

    // Record that the file yielded decodable audio, independently of whether
    // this pipeline has an audio chain to link it into.  The audio-only fallback
    // links its pads through gst_parse_launch rather than here, so m_audioChain
    // is null there and the branch below is skipped -- but that pipeline is
    // exactly the one that needs to know, so the flag is set before the check.
    if (g_str_has_prefix(name, "audio/")) {
        self->m_sawAudioPad.store(true, std::memory_order_release);
    }

    if (g_str_has_prefix(name, "audio/") && self->m_audioChain) {
        GstPad* sinkPad = gst_element_get_static_pad(self->m_audioChain, "sink");
        if (sinkPad && !gst_pad_is_linked(sinkPad)) {
            GstPadLinkReturn ret = gst_pad_link(pad, sinkPad);
            if (GST_PAD_LINK_FAILED(ret)) {
                LogErr(VB_MEDIAOUT, "GStreamer: Failed to link audio pad: %d\n", ret);
            } else {
                LogDebug(VB_MEDIAOUT, "GStreamer: Linked audio pad successfully\n");
                self->m_audioLinked = true;
            }
        } else {
            LogWarn(VB_MEDIAOUT, "GStreamer: Audio pad already linked or sink pad unavailable\n");
        }
        if (sinkPad)
            gst_object_unref(sinkPad);
    } else if (g_str_has_prefix(name, "video/") && self->m_videoChain) {
        GstPad* sinkPad = gst_element_get_static_pad(self->m_videoChain, "sink");
        if (sinkPad && !gst_pad_is_linked(sinkPad)) {
            GstPadLinkReturn ret = gst_pad_link(pad, sinkPad);
            if (GST_PAD_LINK_FAILED(ret)) {
                LogErr(VB_MEDIAOUT, "GStreamer: Failed to link video pad: %d\n", ret);
            } else {
                LogDebug(VB_MEDIAOUT, "GStreamer: Linked video pad successfully\n");
                self->m_videoLinked = true;
            }
        } else {
            LogWarn(VB_MEDIAOUT, "GStreamer: Video pad already linked or sink pad unavailable\n");
        }
        if (sinkPad)
            gst_object_unref(sinkPad);
    } else {
        LogDebug(VB_MEDIAOUT, "GStreamer: Ignoring pad with caps: %s\n", name);
    }

    gst_caps_unref(caps);
}

void GStreamerOutput::OnNoMorePads(GstElement* element, gpointer userData) {
    std::shared_ptr<CallbackGuard> guard;
    std::unique_lock<std::mutex> guardLock;
    GStreamerOutput* self = LockCallbackGuard(userData, guard, guardLock);
    if (!self)
        return;

    LogDebug(VB_MEDIAOUT, "GStreamer: no-more-pads (audio=%s, video=%s)\n",
            self->m_audioLinked ? "linked" : "not linked",
            self->m_videoLinked ? "linked" : "not linked");

    // If audio chain was never connected, remove orphaned audio elements from the bin.
    // Without this, the pipeline will never reach EOS because the audio sinks
    // never receive data and never post their individual EOS events.
    if (!self->m_audioLinked && self->m_audioChain && self->m_pipeline) {
        LogDebug(VB_MEDIAOUT, "GStreamer: Removing unconnected audio chain (video-only media)\n");

        // Get all audio elements by name and remove them
        const char* audioNames[] = {"aconv", "aresample", "t", "q1", "vol", "pwsink", "audiosink",
                                    "q2", "aconv2", "acapsf", "sampletap", nullptr};
        for (int i = 0; audioNames[i]; i++) {
            GstElement* el = gst_bin_get_by_name(GST_BIN(self->m_pipeline), audioNames[i]);
            if (el) {
                gst_element_set_state(el, GST_STATE_NULL);
                gst_bin_remove(GST_BIN(self->m_pipeline), el);
                gst_object_unref(el);
            }
        }

        // Release our refs and clear audio pointers
        if (self->m_appsink) {
            gst_object_unref(self->m_appsink);
            self->m_appsink = nullptr;
        }
        if (self->m_volume) {
            gst_object_unref(self->m_volume);
            self->m_volume = nullptr;
        }
        self->m_audioChain = nullptr;
        self->m_appsinkSignalId = 0;
    }

    // If video chain was never connected, remove orphaned video elements.
    if (!self->m_videoLinked && self->m_videoChain && self->m_pipeline) {
        LogDebug(VB_MEDIAOUT, "GStreamer: Removing unconnected video chain (audio-only media)\n");

        const char* videoNames[] = {"vq", "vconv", "vscale", "vcapsf", "vtee", "vkmsq", "vpwq", "videosink", "kmsvideosink", "pwvideosink", nullptr};
        for (int i = 0; videoNames[i]; i++) {
            GstElement* el = gst_bin_get_by_name(GST_BIN(self->m_pipeline), videoNames[i]);
            if (el) {
                gst_element_set_state(el, GST_STATE_NULL);
                gst_bin_remove(GST_BIN(self->m_pipeline), el);
                gst_object_unref(el);
            }
        }

        // Release our ref and clear video pointers
        if (self->m_videoAppsink) {
            gst_object_unref(self->m_videoAppsink);
            self->m_videoAppsink = nullptr;
        }
        self->m_videoChain = nullptr;
        self->m_videoAppsinkSignalId = 0;
        self->m_hasVideoStream = false;
        self->m_kmssink = nullptr;  // owned by pipeline bin, already removed above
    }
}

GstFlowReturn GStreamerOutput::OnNewSample(GstAppSink* appsink, gpointer userData) {
    GStreamerOutput* self = static_cast<GStreamerOutput*>(userData);
    if (!self || self->m_shutdownFlag.load(std::memory_order_acquire))
        return GST_FLOW_EOS;

    GstSample* sample = gst_app_sink_pull_sample(appsink);
    if (!sample)
        return GST_FLOW_OK;

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);

    // Extract sample rate from caps on first buffer
    if (caps) {
        GstStructure* s = gst_caps_get_structure(caps, 0);
        int rate = 0;
        if (gst_structure_get_int(s, "rate", &rate) && rate > 0) {
            std::lock_guard<std::mutex> lock(s_sampleMutex);
            s_sampleRate = rate;
        }
    }

    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        int numFloats = map.size / sizeof(float);
        const float* src = reinterpret_cast<const float*>(map.data);

        std::lock_guard<std::mutex> lock(s_sampleMutex);
        for (int i = 0; i < numFloats; i++) {
            s_sampleBuffer[s_sampleWritePos] = src[i];
            s_sampleWritePos = (s_sampleWritePos + 1) % SAMPLE_BUFFER_SIZE;
        }

        gst_buffer_unmap(buffer, &map);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

bool GStreamerOutput::GetAudioSamples(float* samples, int numSamples, int& sampleRate) {
    if (!m_currentInstance || !m_currentInstance->m_playing)
        return false;

    std::lock_guard<std::mutex> lock(s_sampleMutex);
    if (s_sampleRate == 0)
        return false;

    sampleRate = s_sampleRate;

    // Read the most recent numSamples from the circular buffer
    int readPos = (s_sampleWritePos - numSamples + SAMPLE_BUFFER_SIZE) % SAMPLE_BUFFER_SIZE;
    for (int i = 0; i < numSamples; i++) {
        samples[i] = s_sampleBuffer[readPos];
        readPos = (readPos + 1) % SAMPLE_BUFFER_SIZE;
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// AES67 zero-hop RTP branch helpers (Phase 7.9)
// ──────────────────────────────────────────────────────────────────────────────
#ifdef HAS_AES67_GSTREAMER
void GStreamerOutput::AttachAES67Branches(GstElement* tee) {
    // Inline zero-hop branches are disabled.  They create a second RTP stream
    // (different SSRC) alongside the standalone pipewiresrc→udpsink pipeline,
    // which confuses AES67 receivers causing repeated/out-of-time audio.
    // The standalone pipeline with sync=false already provides low latency.
    (void)tee;
    return;
}

void GStreamerOutput::DetachAES67Branches() {
    // No-op: inline branches are disabled.
}
#endif // HAS_AES67_GSTREAMER

// ──────────────────────────────────────────────────────────────────────────────
// PipeWire filter-chain delay buffer flush
// ──────────────────────────────────────────────────────────────────────────────
// When media stops, PipeWire's builtin delay nodes retain old audio in their
// ring-buffers.  If the next song starts before that audio drains naturally,
// the listener hears a burst of the previous track.  This function resets all
// delay controls to 0 (clearing the ring-buffers) and then immediately
// restores the original values so they begin accumulating from silence.
void GStreamerOutput::FlushPipeWireDelayBuffers() {
    // Read audio groups config to find delay values
    std::string configPath = FPP_DIR_CONFIG("/pipewire-audio-groups.json");
    if (!FileExists(configPath)) {
        return;
    }

    Json::Value root;
    if (!LoadJsonFromFile(configPath, root, JsonRoot::Object) || !root.isMember("groups")) {
        return;
    }

    // Channel labels matching the PHP filter-chain generator
    static const char* channelLabels[] = { "l", "r", "c", "lfe", "rl", "rr", "sl", "sr" };

    struct DelayInfo {
        std::string fxNodeName; // e.g. fpp_fx_g1_s3
        int channels;
        double delaySec;
    };
    std::vector<DelayInfo> delays;

    for (const auto& group : root["groups"]) {
        int groupId = group.get("id", 0).asInt();
        if (!group.isMember("members"))
            continue;
        for (const auto& member : group["members"]) {
            std::string cardId = member.get("cardId", "").asString();
            double delayMs = member.get("delayMs", 0.0).asDouble();
            int channels = member.get("channels", 2).asInt();
            if (cardId.empty() || delayMs <= 0)
                continue; // no delay buffer to flush

            // Normalize card ID to match PHP: preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($cardId))
            std::string cardIdNorm;
            for (char c : cardId) {
                if (std::isalnum(c) || c == '_')
                    cardIdNorm += std::tolower(c);
                else
                    cardIdNorm += '_';
            }
            std::string fxNodeName = "fpp_fx_g" + std::to_string(groupId) + "_" + cardIdNorm;
            delays.push_back({fxNodeName, std::min(channels, 8), delayMs / 1000.0});
        }
    }

    if (delays.empty())
        return;

    // Fire-and-forget thread: set delays to 0, wait a quantum, restore.
    std::thread([delays]() {
        const char* env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";

        // Phase 1: set all delays to 0 (clears ring-buffers)
        for (const auto& d : delays) {
            // Find node ID (timeout prevents pw-cli hangs)
            std::string findCmd = std::string(env) + " timeout 3 pw-cli ls Node 2>/dev/null | grep -B1 'node.name = \"" + d.fxNodeName + "\"' | head -1 | awk '{print $2}'";
            FILE* fp = popen(findCmd.c_str(), "r");
            if (!fp) continue;
            char buf[64] = {};
            if (!fgets(buf, sizeof(buf), fp)) { pclose(fp); continue; }
            pclose(fp);
            int nodeId = atoi(buf);
            if (nodeId <= 0) continue;

            // Build params string: "delay_l:Delay (s)" 0 "delay_r:Delay (s)" 0 ...
            std::string params;
            for (int ch = 0; ch < d.channels; ch++) {
                if (!params.empty()) params += " ";
                params += "\"delay_";
                params += channelLabels[ch];
                params += ":Delay (s)\" 0";
            }
            std::string cmd = std::string(env) + " timeout 3 pw-cli set-param " + std::to_string(nodeId)
                + " Props '{ params = [ " + params + " ] }' 2>/dev/null";
            system(cmd.c_str());
        }

        // Wait two PipeWire quanta (~42ms) for the zero-delay to take effect
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Phase 2: restore original delay values
        for (const auto& d : delays) {
            std::string findCmd = std::string(env) + " timeout 3 pw-cli ls Node 2>/dev/null | grep -B1 'node.name = \"" + d.fxNodeName + "\"' | head -1 | awk '{print $2}'";
            FILE* fp = popen(findCmd.c_str(), "r");
            if (!fp) continue;
            char buf[64] = {};
            if (!fgets(buf, sizeof(buf), fp)) { pclose(fp); continue; }
            pclose(fp);
            int nodeId = atoi(buf);
            if (nodeId <= 0) continue;

            std::string params;
            for (int ch = 0; ch < d.channels; ch++) {
                if (!params.empty()) params += " ";
                params += "\"delay_";
                params += channelLabels[ch];
                params += ":Delay (s)\" ";
                params += std::to_string(d.delaySec);
            }
            std::string cmd = std::string(env) + " timeout 3 pw-cli set-param " + std::to_string(nodeId)
                + " Props '{ params = [ " + params + " ] }' 2>/dev/null";
            system(cmd.c_str());
        }

        LogDebug(VB_MEDIAOUT, "PipeWire delay buffers flushed and restored\n");
    }).detach();
}

#endif // HAS_GSTREAMER
