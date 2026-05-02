#pragma once
/*
 * Pixel Overlay Effects for Falcon Player (FPP) ported from WLED
 *
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "PixelOverlayEffects.h"

// Per-arg semantic role for WLED-ported effects. The base class returns
// an empty role list (meaning "match by arg name"); RawWLEDEffect
// overrides getArgRoles() to declare which slot is which, so external
// callers (e.g. WLEDAPIResponder) can fill the args vector from the
// matching WLED segment fields without re-parsing FX.cpp data strings.
enum class WLEDArgRole {
    Unknown,
    Mapping,
    Brightness,
    Speed,
    Intensity,
    Palette,
    Color1,
    Color2,
    Color3,
    Custom1,
    Custom2,
    Custom3,
    Check1,
    Check2,
    Check3,
    Text
};

class WLEDEffect : public PixelOverlayEffect {
public:
    WLEDEffect(const std::string& name);
    virtual ~WLEDEffect();

    // One entry per `args` slot, in the same order. Empty means
    // "fall back to name-based heuristics in the caller".
    virtual std::vector<WLEDArgRole> getArgRoles() const { return {}; }

    static std::list<PixelOverlayEffect*> getWLEDEffects();
    static void cleanupWLEDEffects();
};
