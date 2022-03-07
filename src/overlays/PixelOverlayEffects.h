#pragma once
/*
 *   Pixel Overlay Effects for Falcon Player (FPP)
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

#include "commands/Commands.h"
#include <string>
#include <vector>

class PixelOverlayModel;

class RunningEffect {
public:
    static constexpr int32_t EFFECT_DONE = 0;
    static constexpr int32_t EFFECT_AFTER_NEXT_OUTPUT = -1;

    RunningEffect(PixelOverlayModel* m) :
        model(m) {}
    virtual ~RunningEffect() {}
    virtual int32_t update() {
        return 0;
    }

    virtual const std::string& name() const = 0;

    PixelOverlayModel* model;
};

class PixelOverlayEffect : public Command {
public:
    PixelOverlayEffect(const std::string& name) :
        Command(name) {}
    virtual ~PixelOverlayEffect() {}

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) {
        return std::make_unique<Command::Result>("Ignored");
    };

    virtual bool apply(PixelOverlayModel* model, bool autoEnable, const std::vector<std::string>& args) { return false; }
    virtual bool apply(PixelOverlayModel* model, const std::string& enableState, const std::vector<std::string>& args) {
        return apply(model, enableState != "false", args);
    }

    static PixelOverlayEffect* GetPixelOverlayEffect(const std::string& name);
    static const std::vector<std::string>& GetPixelOverlayEffects();

    static void AddPixelOverlayEffect(PixelOverlayEffect* effect);
    static void RemovePixelOverlayEffect(PixelOverlayEffect* effect);
};
