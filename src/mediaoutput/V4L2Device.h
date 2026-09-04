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

#include <string>
#include <vector>

/**
 * V4L2Device — direct V4L2 queries against a capture device, for the
 * things GStreamer's v4l2src either can't express or gets wrong for us.
 *
 * Two jobs:
 *
 *  1. Anti-flicker / exposure controls.  A camera lit by mains lighting
 *     bands or pulses unless its auto-exposure quantises exposure to whole
 *     half-cycles of the supply.  That's V4L2_CID_POWER_LINE_FREQUENCY,
 *     and it's a device control, not something the pipeline can do
 *     downstream: no amount of videorate/videoscale fixes light that was
 *     already integrated wrong.
 *
 *     Controls are addressed by numeric CID rather than by name through
 *     v4l2src's extra-controls property, because the names moved in
 *     Linux 6.x (exposure_auto -> auto_exposure, exposure_absolute ->
 *     exposure_time_absolute) and extra-controls only warns on a name it
 *     can't resolve.  CIDs are stable ABI.
 *
 *  2. Capture mode enumeration, so the configured resolution/framerate can
 *     be pinned on the device instead of being quietly resampled after the
 *     fact.  See SelectMode().
 *
 * Every entry point is a no-op returning failure on non-Linux builds.
 */
namespace V4L2Device {

/// One discrete capture mode the device advertises.
struct Mode {
    std::string fourcc;  ///< e.g. "MJPG", "YUYV"
    int width = 0;
    int height = 0;
    int fpsNum = 0; ///< framerate = fpsNum / fpsDen
    int fpsDen = 1;

    double fps() const { return fpsDen ? (double)fpsNum / fpsDen : 0.0; }
};

/// Requested device controls.  -1 / "camera" means "leave the camera's own
/// setting alone" so an untouched config behaves exactly as it did before.
struct ControlSettings {
    /// V4L2_CID_POWER_LINE_FREQUENCY: -1 leave, 0 disabled, 1 = 50Hz, 2 = 60Hz.
    int powerLineFrequency = -1;

    /// "camera" (leave), "auto", or "manual".  Maps onto whichever
    /// V4L2_CID_EXPOSURE_AUTO menu entries the device actually offers.
    std::string exposureMode = "camera";

    /// V4L2_CID_EXPOSURE_ABSOLUTE, in 100us units.  Only applied when
    /// exposureMode == "manual".  -1 leaves the camera's value.
    int exposureTime100us = -1;

    /// V4L2_CID_EXPOSURE_AUTO_PRIORITY ("exposure_dynamic_framerate"):
    /// -1 leave, 1 = let AE drop the framerate, 0 = hold the framerate.
    /// Worth holding when chasing flicker — a variable frame interval
    /// reintroduces beating against the light even once exposure is
    /// quantised correctly.
    int dynamicFramerate = -1;

    bool AnyRequested() const {
        return powerLineFrequency >= 0 || exposureMode != "camera" || dynamicFramerate >= 0;
    }
};

/// Apply `s` to `device`.  Controls the device doesn't implement are
/// skipped, not treated as failure.  `summary` gets a human-readable
/// "name=value" list of what actually landed, for the log.
/// Returns false only if the device could not be opened at all.
bool ApplyControls(const std::string& device, const ControlSettings& s, std::string& summary);

/// All discrete capture modes the device advertises.  Empty if the device
/// can't be opened or enumerates nothing usable.
std::vector<Mode> EnumerateModes(const std::string& device);

/// Pick the mode to ask the device for, given what the config wants.
///
/// Prefers an exact width/height/framerate match; failing that the
/// smallest mode that still covers the requested size (so videoscale
/// downscales rather than upscales), and among equals the one whose
/// framerate best covers the request.  Returns false when `modes` is
/// empty, in which case the caller should not constrain the device at all.
bool SelectMode(const std::vector<Mode>& modes, int width, int height, int fps, Mode& out);

/// GStreamer caps for a mode, e.g. "image/jpeg,width=1280,height=720,framerate=30/1".
/// Empty string if the fourcc has no caps mapping we trust, in which case
/// the caller should leave the device unconstrained.
std::string ModeToCaps(const Mode& m);

} // namespace V4L2Device
