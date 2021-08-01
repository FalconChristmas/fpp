#pragma once
/*
 *   Pixel Overlay Effects for Falcon Player (FPP) ported from WLED
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "PixelOverlayEffects.h"

class WLEDEffect : public PixelOverlayEffect {
public:
    WLEDEffect(const std::string& name);
    virtual ~WLEDEffect();

    static std::list<PixelOverlayEffect*> getWLEDEffects();
};
