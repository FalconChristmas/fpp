#pragma once
/*
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

#include <string>
#include <vector>

#include "../commands/Commands.h"

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
