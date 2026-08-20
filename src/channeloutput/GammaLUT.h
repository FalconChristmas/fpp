#pragma once
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

#include <cmath>
#include <cstdlib>

#include "fpp-json.h"

// The 256 entry gamma curve every matrix output builds from its "gamma" config
// value.  Header only so the channel output plugins, which each link their own
// object list, pick it up without a new object to link.
namespace GammaLUT {

// Outputs fall back to their own default both when gamma is unconfigured and
// when the configured value is outside the range they accept, so the default is
// a per caller parameter.  NaN is not out of range by this test and is passed
// through, same as it always has been.
inline float Clamp(float gamma, float def) {
    if (gamma < 0.01 || gamma > 50.0) {
        return def;
    }
    return gamma;
}

inline float ParseConfig(const Json::Value& config, float def) {
    float gamma = def;
    if (config.isMember("gamma")) {
        gamma = atof(config["gamma"].asString().c_str());
    }
    return Clamp(gamma, def);
}

// Full scale value a panel bit depth can carry.  Depths of 8 or less drive the
// plain 8 bit range.
inline float MaxForColorDepth(int colorDepth) {
    if (colorDepth > 8 && colorDepth <= 16) {
        return (float)((1 << colorDepth) - 1);
    }
    return 255.0f;
}

// scale trims the whole curve, for a matrix that has to make up the difference
// when it shares hardware set up for a brighter one.  floorLowEnd keeps a value
// that rounds to 0, but sits more than a quarter step above it, at 1 so the
// bottom of the curve does not go fully black.
template<typename T>
inline void Build(T* curve, float gamma, float max = 255.0f, float scale = 1.0f, bool floorLowEnd = false) {
    for (int x = 0; x < 256; x++) {
        float f = x;
        f = max * std::pow(f / 255.0f, gamma) * scale;
        if (f > max) {
            f = max;
        }
        if (f < 0.0f) {
            f = 0.0f;
        }
        curve[x] = std::round(f);
        if (floorLowEnd && curve[x] == 0 && f > 0.25) {
            curve[x] = 1;
        }
    }
}

template<typename T>
inline void BuildForColorDepth(T* curve, float gamma, int colorDepth, float scale = 1.0f) {
    Build(curve, gamma, MaxForColorDepth(colorDepth), scale, true);
    // The two shallowest depths have no step low enough to render these values,
    // so they take the first step the panel can actually show.
    if (colorDepth == 6) {
        curve[2] = curve[4];
        curve[3] = curve[4];
    } else if (colorDepth == 7) {
        curve[1] = curve[2];
    }
}

} // namespace GammaLUT
