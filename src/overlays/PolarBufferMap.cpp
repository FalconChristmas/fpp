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

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <unordered_map>

#include "PolarBufferMap.h"

namespace {

/** A radius gap this fraction of the radial span starts a new ring. */
constexpr float RING_GAP_FRACTION = 0.02f;

/**
 * Below this radial spread — (rmax - rmin) / rmean — the target is ONE ring.
 *
 * A single physical ring has no radial structure, but the virtual display map
 * stores integer screen coordinates, so its pixels still scatter over roughly a
 * unit of radius. Left alone, gap detection reads that rounding jitter as
 * structure and shatters a 24-pixel ring into twenty 1-pixel "rings". A ring
 * cannot radiate, only rotate: it wants one radius column with every pixel
 * spread around the angular axis.
 */
constexpr float RING_FLAT_FRACTION = 0.15f;

/** Gap-detected rings are rejected above this overdraw (cells per pixel). */
constexpr float MAX_GAP_OVERDRAW = 2.0f;

/** ...and below this many rings; either way equal-population rings take over. */
constexpr int MIN_GAP_RINGS = 3;

/** Refuse to build a buffer larger than this. Guards a pathological layout. */
constexpr size_t MAX_CELLS = 262144;

/** Which ring does radius r fall in? Bounds are ascending. */
int ringIndex(float r, const std::vector<float>& bounds) {
    return static_cast<int>(std::upper_bound(bounds.begin(), bounds.end(), r) - bounds.begin());
}

/** Does everything sit at essentially one radius? See RING_FLAT_FRACTION. */
bool isFlat(const std::vector<float>& radii) {
    if (radii.size() < 2) {
        return true;
    }
    auto [mn, mx] = std::minmax_element(radii.begin(), radii.end());
    double sum = 0.0;
    for (float r : radii) {
        sum += r;
    }
    double mean = sum / radii.size();
    if (mean <= 0.0) {
        return true;
    }
    return ((*mx - *mn) / mean) < RING_FLAT_FRACTION;
}

/** Split where consecutive sorted radii jump by a large part of the span. */
std::vector<float> gapBounds(std::vector<float> rs) {
    std::sort(rs.begin(), rs.end());
    std::vector<float> bounds;
    float span = rs.back() - rs.front();
    if (span <= 0.0f) {
        return bounds;
    }
    float thresh = span * RING_GAP_FRACTION;
    for (size_t i = 0; i + 1 < rs.size(); i++) {
        if ((rs[i + 1] - rs[i]) > thresh) {
            bounds.push_back((rs[i] + rs[i + 1]) / 2.0f);
        }
    }
    return bounds;
}

/** Equal-population ring boundaries — every ring holds ~n/count pixels. */
std::vector<float> evenBounds(std::vector<float> rs, int count) {
    std::sort(rs.begin(), rs.end());
    std::vector<float> bounds;
    int n = static_cast<int>(rs.size());
    count = std::max(1, std::min(count, n));
    for (int k = 1; k < count; k++) {
        bounds.push_back(rs[static_cast<size_t>((long long)n * k / count)]);
    }
    return bounds;
}

/**
 * Ring count for equal-population binning. sqrt(n/2) keeps the buffer
 * near-square, so both axes stay usefully resolved: enough rings for a radial
 * sweep to read as motion, enough sectors for a rotation to.
 */
int defaultRingCount(size_t n) {
    return std::max(2, static_cast<int>(std::lround(std::sqrt(n / 2.0))));
}

/** Largest ring population under these bounds. */
int busiestRing(const std::vector<float>& radii, const std::vector<float>& bounds) {
    std::unordered_map<int, int> pop;
    for (float r : radii) {
        pop[ringIndex(r, bounds)]++;
    }
    int mx = 0;
    for (auto& [k, v] : pop) {
        mx = std::max(mx, v);
    }
    return mx;
}

/** Walk outward from s to the first sector no pixel has claimed. Wraps. */
int nearestFreeSector(const std::vector<bool>& taken, int s, int S) {
    for (int d = 1; d < S; d++) {
        int up = (s + d) % S;
        if (!taken[up]) {
            return up;
        }
        int down = ((s - d) % S + S) % S;
        if (!taken[down]) {
            return down;
        }
    }
    return s; // unreachable: S >= the busiest ring's population
}

} // namespace

PolarMode PolarModeFromName(std::string_view name) {
    if (name == "Radial Out") {
        return PolarMode::RadialOut;
    }
    if (name == "Radial In") {
        return PolarMode::RadialIn;
    }
    if (name == "Angular CW") {
        return PolarMode::AngularCW;
    }
    if (name == "Angular CCW") {
        return PolarMode::AngularCCW;
    }
    // Every other value -- the four original mappings, and anything a newer
    // config might name that this build does not know -- is not polar.
    return PolarMode::None;
}

bool LoadVirtualDisplayPositions(std::vector<std::pair<uint32_t, std::pair<float, float>>>& out) {
    std::ifstream in(FPP_DIR_CONFIG("/virtualdisplaymap"));
    if (!in.is_open()) {
        // No xLights layout has ever been uploaded. Not an error: the caller
        // falls back to the ordinary buffer mapping.
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        int x = 0, y = 0, z = 0;
        unsigned int ch = 0;
        // x,y,z,channel,channelCount,colorOrder,pixelSize -- the channel is a
        // 0-based index into fppd's channel data.
        if (std::sscanf(line.c_str(), "%d,%d,%d,%u", &x, &y, &z, &ch) == 4) {
            out.emplace_back(ch, std::make_pair(static_cast<float>(x), static_cast<float>(y)));
        }
    }
    return !out.empty();
}

PolarBufferMap BuildPolarBufferMap(const std::vector<PolarPixel>& pixels, PolarMode mode) {
    PolarBufferMap map;
    if (mode == PolarMode::None || pixels.empty()) {
        return map;
    }

    // Centroid.
    double sx = 0.0, sy = 0.0;
    for (const auto& p : pixels) {
        sx += p.x;
        sy += p.y;
    }
    float cx = static_cast<float>(sx / pixels.size());
    float cy = static_cast<float>(sy / pixels.size());

    // To polar. Angle is shifted to 0..2pi so the sector axis is monotonic;
    // zero is +x (screen right), matching the map's coordinate sense.
    struct PolarPt {
        float r;
        float theta;
        uint32_t cell;
    };
    std::vector<PolarPt> polar;
    polar.reserve(pixels.size());
    std::vector<float> radii;
    radii.reserve(pixels.size());
    const float TWO_PI = static_cast<float>(2.0 * M_PI);
    for (const auto& p : pixels) {
        float dx = p.x - cx;
        float dy = p.y - cy;
        float r = std::sqrt(dx * dx + dy * dy);
        float t = std::fmod(std::atan2(dy, dx) + TWO_PI * 2.0f, TWO_PI);
        polar.push_back({ r, t, p.cell });
        radii.push_back(r);
    }

    // Everything at one point: no polar structure to build.
    if (*std::max_element(radii.begin(), radii.end()) <= 0.0f) {
        return map;
    }

    // Ring strategy. A single physical ring is decided before anything else,
    // because every strategy would otherwise invent rings out of coordinate
    // rounding. Otherwise prefer physically-detected rings -- on a model built
    // from discrete concentric rings those are what a viewer sees light up --
    // and fall back to equal-population rings when detection finds no structure
    // (a solid disc is one continuous ring) or would leave the buffer mostly
    // empty.
    std::vector<float> bounds;
    if (!isFlat(radii)) {
        bounds = gapBounds(radii);
        int R = static_cast<int>(bounds.size()) + 1;
        int S = busiestRing(radii, bounds);
        float overdraw = static_cast<float>((long long)R * S) / pixels.size();
        if (R < MIN_GAP_RINGS || overdraw > MAX_GAP_OVERDRAW) {
            bounds = evenBounds(radii, defaultRingCount(pixels.size()));
        }
    }

    // Bin by ring, then reindex densely: detection can leave a ring empty when
    // a boundary falls outside the data, and an empty column wastes a sweep step.
    std::unordered_map<int, std::vector<PolarPt>> byRing;
    for (const auto& pt : polar) {
        byRing[ringIndex(pt.r, bounds)].push_back(pt);
    }
    std::vector<int> ringKeys;
    ringKeys.reserve(byRing.size());
    for (auto& [k, v] : byRing) {
        ringKeys.push_back(k);
    }
    std::sort(ringKeys.begin(), ringKeys.end());

    int R = static_cast<int>(ringKeys.size());
    int S = 0;
    for (int k : ringKeys) {
        S = std::max(S, static_cast<int>(byRing[k].size()));
    }
    if (R < 1 || S < 1) {
        return map;
    }
    if (static_cast<size_t>(R) * S > MAX_CELLS) {
        LogWarn(VB_CHANNELOUT,
                "Polar buffer would be %d x %d cells; falling back to the ordinary mapping\n", R, S);
        return map;
    }

    // Place each ring's pixels around the sector axis by true bearing,
    // nudging a collision to the nearest free sector.
    std::vector<uint32_t> grid(static_cast<size_t>(R) * S, PolarBufferMap::Empty);
    for (int ri = 0; ri < R; ri++) {
        std::vector<PolarPt>& items = byRing[ringKeys[ri]];
        std::sort(items.begin(), items.end(),
                  [](const PolarPt& a, const PolarPt& b) { return a.theta < b.theta; });
        std::vector<bool> taken(S, false);
        for (const auto& pt : items) {
            int s = static_cast<int>(std::lround(pt.theta / TWO_PI * S)) % S;
            if (taken[s]) {
                s = nearestFreeSector(taken, s, S);
            }
            taken[s] = true;
            grid[static_cast<size_t>(ri) * S + s] = pt.cell;
        }
    }

    // Emit in the order the requested mode wants. The LUT is walked linearly by
    // Bus::setPixelColor, so the ordering here IS the mapping -- there is no
    // div/mod left in the hot path.
    //
    // WLED walks the WIDTH axis first (that is what the original Horizontal /
    // Vertical pair selects), so whatever should light "all at once" has to be
    // contiguous in the index. A radial wave lights a whole RING at a time, so
    // sectors run along the width and rings step down the height; an angular
    // sweep lights a whole SPOKE at a time, so the two swap. Getting this the
    // wrong way round yields a spiral -- correct pixels, wrong motion.
    const bool radial = (mode == PolarMode::RadialOut || mode == PolarMode::RadialIn);
    map.width = radial ? S : R;
    map.height = radial ? R : S;
    map.indexToCell.assign(static_cast<size_t>(map.width) * map.height, PolarBufferMap::Empty);

    for (int ri = 0; ri < R; ri++) {
        for (int si = 0; si < S; si++) {
            uint32_t cell = grid[static_cast<size_t>(ri) * S + si];
            if (cell == PolarBufferMap::Empty) {
                continue;
            }
            int r = (mode == PolarMode::RadialIn) ? (R - 1 - ri) : ri;
            int s = (mode == PolarMode::AngularCCW) ? (S - 1 - si) : si;
            size_t idx = radial ? (static_cast<size_t>(r) * map.width + s)
                                : (static_cast<size_t>(s) * map.width + r);
            map.indexToCell[idx] = cell;
        }
    }
    return map;
}
