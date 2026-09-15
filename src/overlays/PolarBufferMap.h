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

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/*
 * A POLAR view of a pixel overlay model's buffer, so WLED effects sweep
 * outward from (or around) the centre of a circular model rather than
 * left-to-right across its wiring order.
 *
 * The existing "Buffer Mapping" values choose row-major vs column-major and
 * mirror an axis (see BUFFERMAPS in WLEDEffects.cpp, consumed by Bus:: in
 * wled/wled.cpp). Radius cannot join them as a fifth value, because they
 * operate in BUFFER space: a circular model's buffer is wiring order, so the
 * distance from buffer cell (x,y) to the buffer's centre says nothing about the
 * distance from that pixel to the centre of the model.
 *
 * What is missing is per-pixel geometry, and FPP already has it —
 * config/virtualdisplaymap carries x,y,z plus an absolute channel for every
 * node. So instead of a new way to walk the buffer, this builds a LUT whose
 * axes ARE polar and hands it to the same Bus indexing the other mappings use.
 *
 * The LUT points at the model's own buffer cells, NOT at output channels, which
 * is what keeps this contained: by the time doOverlay(), flushOverlayBuffer(),
 * getDataJson(), setData(), text, image export or the web pixel editor run, the
 * pixels are already sitting in the model's normal buffer. None of them can
 * tell a polar effect from a horizontal one.
 */

/** Which polar layout an effect asked for. None means "not a polar mapping". */
enum class PolarMode {
    None = 0,
    RadialOut,  // x = radius, centre first
    RadialIn,   // x = radius, rim first
    AngularCW,  // x = angle, clockwise on screen
    AngularCCW, // x = angle, anticlockwise
};

/**
 * The built layout: a dense width*height LUT from the WLED linear index to a
 * buffer CELL index (y * model width + x) — or Empty where no pixel occupies
 * that polar cell, which is common because polar binning is inherently sparse.
 */
struct PolarBufferMap {
    static constexpr uint32_t Empty = 0xFFFFFFFFu;

    int width = 0;  // fastest-varying axis: radius for Radial*, angle for Angular*
    int height = 0;
    std::vector<uint32_t> indexToCell;

    bool valid() const {
        return width > 0 && height > 0 &&
               indexToCell.size() == static_cast<size_t>(width) * height;
    }
};

/** One positioned buffer cell: where it is, and which cell it is. */
struct PolarPixel {
    float x = 0.0f;
    float y = 0.0f;
    uint32_t cell = 0; // y * model width + x
};

/**
 * Map a "Buffer Mapping" argument to a polar mode.
 * Returns PolarMode::None for the four original values and for anything
 * unrecognised, which is what makes an unknown value degrade to Horizontal
 * rather than fail.
 */
PolarMode PolarModeFromName(std::string_view name);

/**
 * Lay pixels out in polar order.
 *
 * Returns an INVALID map (valid() == false) whenever the geometry will not
 * support one — no positioned pixels, everything at a single point, or a buffer
 * that would be absurdly large. Callers must treat that as "use the ordinary
 * mapping", never as an error: a player with no xLights layout has no
 * virtualdisplaymap and must keep behaving exactly as it does today.
 */
PolarBufferMap BuildPolarBufferMap(const std::vector<PolarPixel>& pixels, PolarMode mode);

/**
 * Read config/virtualdisplaymap into a 0-based absolute channel -> (x, y) index.
 * Returns false when the file is absent or unreadable — the no-xLights case.
 */
bool LoadVirtualDisplayPositions(std::vector<std::pair<uint32_t, std::pair<float, float>>>& out);
