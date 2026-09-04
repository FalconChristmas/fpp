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

#include "V4L2Device.h"
#include "log.h"

#if __has_include(<linux/videodev2.h>)
#define HAS_V4L2
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cstdio>

namespace V4L2Device {

#ifdef HAS_V4L2

// ioctl() on a video node returns EINTR often enough to matter (any signal
// delivered to the calling thread will do it), and a spurious failure here
// silently drops an anti-flicker setting.
static int xioctl(int fd, unsigned long req, void* arg) {
    int r;
    do {
        r = ioctl(fd, req, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

// Opened O_RDWR because S_CTRL needs write access, but never for streaming:
// UVC allows any number of non-streaming openers, so this does not contend
// with the capture that v4l2src is already running on the same node.
// O_NONBLOCK keeps a wedged device from hanging the caller.
static int OpenDevice(const std::string& device) {
    int fd = open(device.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        LogWarn(VB_MEDIAOUT, "V4L2Device: cannot open %s: %s\n", device.c_str(), strerror(errno));
    }
    return fd;
}

/// Is this control present and settable right now?
static bool QueryControl(int fd, unsigned int cid, v4l2_queryctrl& qc) {
    memset(&qc, 0, sizeof(qc));
    qc.id = cid;
    if (xioctl(fd, VIDIOC_QUERYCTRL, &qc) != 0)
        return false;
    // DISABLED means the control exists but cannot currently be set (e.g.
    // exposure time while auto-exposure owns it) -- callers that can fix
    // that ordering re-query rather than treating it as absent.
    return !(qc.flags & V4L2_CTRL_FLAG_DISABLED);
}

static bool MenuHasValue(int fd, unsigned int cid, int value) {
    v4l2_querymenu qm;
    memset(&qm, 0, sizeof(qm));
    qm.id = cid;
    qm.index = value;
    return xioctl(fd, VIDIOC_QUERYMENU, &qm) == 0;
}

static bool SetControl(int fd, unsigned int cid, int value) {
    v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = cid;
    ctrl.value = value;
    return xioctl(fd, VIDIOC_S_CTRL, &ctrl) == 0;
}

static void Append(std::string& summary, const std::string& item) {
    if (!summary.empty())
        summary += ", ";
    summary += item;
}

bool ApplyControls(const std::string& device, const ControlSettings& s, std::string& summary) {
    summary.clear();
    if (!s.AnyRequested())
        return true;

    int fd = OpenDevice(device);
    if (fd < 0)
        return false;

    v4l2_queryctrl qc;

    // Order matters.  power_line_frequency and exposure_dynamic_framerate
    // both steer the auto-exposure algorithm, so they go in before the
    // exposure mode is decided; exposure_time_absolute goes in last because
    // the driver keeps it DISABLED until manual mode is actually active.

    if (s.powerLineFrequency >= 0) {
        if (!QueryControl(fd, V4L2_CID_POWER_LINE_FREQUENCY, qc)) {
            LogInfo(VB_MEDIAOUT, "V4L2Device: %s has no power_line_frequency control\n", device.c_str());
        } else if (s.powerLineFrequency > qc.maximum || s.powerLineFrequency < qc.minimum) {
            LogWarn(VB_MEDIAOUT, "V4L2Device: %s does not support power_line_frequency=%d (range %d-%d)\n",
                    device.c_str(), s.powerLineFrequency, qc.minimum, qc.maximum);
        } else if (SetControl(fd, V4L2_CID_POWER_LINE_FREQUENCY, s.powerLineFrequency)) {
            Append(summary, "power_line_frequency=" + std::to_string(s.powerLineFrequency));
        } else {
            LogWarn(VB_MEDIAOUT, "V4L2Device: %s failed to set power_line_frequency=%d: %s\n",
                    device.c_str(), s.powerLineFrequency, strerror(errno));
        }
    }

    if (s.dynamicFramerate >= 0) {
        if (!QueryControl(fd, V4L2_CID_EXPOSURE_AUTO_PRIORITY, qc)) {
            LogInfo(VB_MEDIAOUT, "V4L2Device: %s has no exposure_dynamic_framerate control\n", device.c_str());
        } else if (SetControl(fd, V4L2_CID_EXPOSURE_AUTO_PRIORITY, s.dynamicFramerate ? 1 : 0)) {
            Append(summary, "exposure_dynamic_framerate=" + std::to_string(s.dynamicFramerate ? 1 : 0));
        } else {
            LogWarn(VB_MEDIAOUT, "V4L2Device: %s failed to set exposure_dynamic_framerate: %s\n",
                    device.c_str(), strerror(errno));
        }
    }

    if (s.exposureMode == "auto" || s.exposureMode == "manual") {
        if (!QueryControl(fd, V4L2_CID_EXPOSURE_AUTO, qc)) {
            LogInfo(VB_MEDIAOUT, "V4L2Device: %s has no auto_exposure control\n", device.c_str());
        } else {
            // The menu is sparse: UVC cameras typically expose only MANUAL(1)
            // and APERTURE_PRIORITY(3), not AUTO(0) or SHUTTER_PRIORITY(2).
            // Probe rather than assume, so "auto" means whichever automatic
            // mode this camera actually has.
            int want = -1;
            if (s.exposureMode == "manual") {
                if (MenuHasValue(fd, V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_MANUAL))
                    want = V4L2_EXPOSURE_MANUAL;
                else if (MenuHasValue(fd, V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_SHUTTER_PRIORITY))
                    want = V4L2_EXPOSURE_SHUTTER_PRIORITY;
            } else {
                if (MenuHasValue(fd, V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_APERTURE_PRIORITY))
                    want = V4L2_EXPOSURE_APERTURE_PRIORITY;
                else if (MenuHasValue(fd, V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_AUTO))
                    want = V4L2_EXPOSURE_AUTO;
            }

            if (want < 0) {
                LogWarn(VB_MEDIAOUT, "V4L2Device: %s offers no '%s' exposure mode\n",
                        device.c_str(), s.exposureMode.c_str());
            } else if (SetControl(fd, V4L2_CID_EXPOSURE_AUTO, want)) {
                Append(summary, "auto_exposure=" + std::to_string(want) + " (" + s.exposureMode + ")");
            } else {
                LogWarn(VB_MEDIAOUT, "V4L2Device: %s failed to set auto_exposure=%d: %s\n",
                        device.c_str(), want, strerror(errno));
            }
        }
    }

    if (s.exposureMode == "manual" && s.exposureTime100us >= 0) {
        // Re-queried after the mode switch above: it reads back DISABLED
        // while auto-exposure owns it, and settable once manual is active.
        if (!QueryControl(fd, V4L2_CID_EXPOSURE_ABSOLUTE, qc)) {
            LogInfo(VB_MEDIAOUT, "V4L2Device: %s exposure_time_absolute unavailable (manual mode not active?)\n",
                    device.c_str());
        } else {
            int v = s.exposureTime100us;
            if (v < qc.minimum)
                v = qc.minimum;
            if (v > qc.maximum)
                v = qc.maximum;
            if (v != s.exposureTime100us) {
                LogWarn(VB_MEDIAOUT, "V4L2Device: %s clamped exposure_time_absolute %d -> %d (range %d-%d)\n",
                        device.c_str(), s.exposureTime100us, v, qc.minimum, qc.maximum);
            }
            if (SetControl(fd, V4L2_CID_EXPOSURE_ABSOLUTE, v)) {
                Append(summary, "exposure_time_absolute=" + std::to_string(v));
            } else {
                LogWarn(VB_MEDIAOUT, "V4L2Device: %s failed to set exposure_time_absolute=%d: %s\n",
                        device.c_str(), v, strerror(errno));
            }
        }
    }

    close(fd);
    return true;
}

static std::string FourccToString(unsigned int f) {
    char buf[5] = { (char)(f & 0xFF), (char)((f >> 8) & 0xFF),
                    (char)((f >> 16) & 0xFF), (char)((f >> 24) & 0xFF), 0 };
    return std::string(buf);
}

/// Framerates for one pixelformat/size, appended to `out` as full Modes.
static void EnumIntervals(int fd, unsigned int pixfmt, const std::string& fourcc,
                          int w, int h, std::vector<Mode>& out) {
    const size_t before = out.size();
    v4l2_frmivalenum fi;
    memset(&fi, 0, sizeof(fi));
    fi.pixel_format = pixfmt;
    fi.width = w;
    fi.height = h;

    for (fi.index = 0; xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fi) == 0; fi.index++) {
        Mode m;
        m.fourcc = fourcc;
        m.width = w;
        m.height = h;
        if (fi.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
            // An *interval* is seconds-per-frame, so the framerate is its
            // reciprocal -- numerator and denominator swap here.
            if (fi.discrete.numerator == 0)
                continue;
            m.fpsNum = fi.discrete.denominator;
            m.fpsDen = fi.discrete.numerator;
        } else {
            // Stepwise/continuous: the shortest interval is the highest
            // framerate, which is the only one worth advertising.
            if (fi.stepwise.min.numerator == 0)
                continue;
            m.fpsNum = fi.stepwise.min.denominator;
            m.fpsDen = fi.stepwise.min.numerator;
            out.push_back(m);
            break;
        }
        out.push_back(m);
    }

    // A device that enumerates the size but refuses to enumerate intervals
    // still has that size; record it with an unknown rate so mode selection
    // can pin the resolution and leave the framerate to the device.
    // Keyed on "did this call add anything", not on inspecting out.back():
    // the same size is commonly enumerated under several pixelformats, and
    // the previous one's entry would otherwise look like this one's.
    if (out.size() == before) {
        Mode m;
        m.fourcc = fourcc;
        m.width = w;
        m.height = h;
        m.fpsNum = 0;
        m.fpsDen = 1;
        out.push_back(m);
    }
}

std::vector<Mode> EnumerateModes(const std::string& device) {
    std::vector<Mode> modes;
    int fd = OpenDevice(device);
    if (fd < 0)
        return modes;

    v4l2_fmtdesc fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (fmt.index = 0; xioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0; fmt.index++) {
        std::string fourcc = FourccToString(fmt.pixelformat);

        v4l2_frmsizeenum fs;
        memset(&fs, 0, sizeof(fs));
        fs.pixel_format = fmt.pixelformat;

        for (fs.index = 0; xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fs) == 0; fs.index++) {
            if (fs.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                EnumIntervals(fd, fmt.pixelformat, fourcc,
                              fs.discrete.width, fs.discrete.height, modes);
            } else {
                // Stepwise sizes: take the maximum as the single
                // representative.  Anything smaller is reachable by
                // videoscale anyway, and guessing at steps risks asking
                // for a size the driver rounds somewhere else.
                EnumIntervals(fd, fmt.pixelformat, fourcc,
                              fs.stepwise.max_width, fs.stepwise.max_height, modes);
                break;
            }
        }
    }

    close(fd);
    return modes;
}

#else // !HAS_V4L2

bool ApplyControls(const std::string& device, const ControlSettings& s, std::string& summary) {
    summary.clear();
    return false;
}

std::vector<Mode> EnumerateModes(const std::string& device) {
    return {};
}

#endif // HAS_V4L2

bool SelectMode(const std::vector<Mode>& modes, int width, int height, int fps, Mode& out) {
    if (modes.empty())
        return false;

    // How good a mode is, ranked lexicographically most-significant first.
    // Expressed as a tuple rather than a weighted score so the priorities
    // stay literally the priorities: no arithmetic where a big enough
    // difference in one term can quietly outvote the term above it.
    struct Rank {
        bool coversSize;  // can satisfy the request by downscaling, not upscaling
        long sizeFit;     // closeness to the requested area, in the right direction
        bool coversRate;  // no framerate interpolation needed
        long rateFit;     // closeness to the requested rate, in the right direction

        bool operator>(const Rank& o) const {
            if (coversSize != o.coversSize)
                return coversSize;
            if (sizeFit != o.sizeFit)
                return sizeFit > o.sizeFit;
            if (coversRate != o.coversRate)
                return coversRate;
            return rateFit > o.rateFit;
        }
    };

    const Mode* best = nullptr;
    Rank bestRank{};

    for (const auto& m : modes) {
        if (m.width <= 0 || m.height <= 0)
            continue;

        // Exact match on all three wins outright -- nothing to resample.
        if (m.width == width && m.height == height && m.fpsDen == 1 && m.fpsNum == fps) {
            out = m;
            return true;
        }

        const long area = (long)m.width * m.height;
        Rank r;
        r.coversSize = (m.width >= width && m.height >= height);
        // Within the covering set the smallest such mode is the best fit
        // (least wasted USB bandwidth, least downscaling); within the
        // non-covering set the largest is.  Both read as "closest to what
        // was asked for", and the two sets are never compared on this term
        // because coversSize is ranked above it.
        r.sizeFit = r.coversSize ? -area : area;
        r.coversRate = (m.fps() >= fps - 0.01);
        // Same shape as sizeFit: among modes fast enough, the slowest is the
        // best fit -- capturing 30fps to feed a 15fps output just burns USB
        // bandwidth and CPU on frames videorate immediately discards.  Among
        // modes that are all too slow, the fastest is.
        r.rateFit = r.coversRate ? -(long)(m.fps() * 100) : (long)(m.fps() * 100);

        if (!best || r > bestRank) {
            best = &m;
            bestRank = r;
        }
    }

    if (!best)
        return false;

    out = *best;
    return true;
}

std::string ModeToCaps(const Mode& m) {
    // Only fourccs whose GStreamer caps we are certain of.  Guessing here
    // would produce a capsfilter the device can never satisfy, which fails
    // negotiation and leaves the pipeline stuck in READY -- the exact
    // failure the unconstrained pipeline was written to avoid.
    std::string media;
    if (m.fourcc == "MJPG" || m.fourcc == "JPEG") {
        media = "image/jpeg";
    } else if (m.fourcc == "H264") {
        media = "video/x-h264";
    } else if (m.fourcc == "YUYV") {
        media = "video/x-raw,format=YUY2";
    } else if (m.fourcc == "UYVY") {
        media = "video/x-raw,format=UYVY";
    } else if (m.fourcc == "NV12") {
        media = "video/x-raw,format=NV12";
    } else if (m.fourcc == "YU12") {
        media = "video/x-raw,format=I420";
    } else if (m.fourcc == "YV12") {
        media = "video/x-raw,format=YV12";
    } else if (m.fourcc == "GREY") {
        media = "video/x-raw,format=GRAY8";
    } else if (m.fourcc == "RGB3") {
        media = "video/x-raw,format=RGB";
    } else if (m.fourcc == "BGR3") {
        media = "video/x-raw,format=BGR";
    } else {
        return "";
    }

    std::string caps = media + ",width=" + std::to_string(m.width) + ",height=" + std::to_string(m.height);
    if (m.fpsNum > 0 && m.fpsDen > 0) {
        caps += ",framerate=" + std::to_string(m.fpsNum) + "/" + std::to_string(m.fpsDen);
    }
    return caps;
}

} // namespace V4L2Device
