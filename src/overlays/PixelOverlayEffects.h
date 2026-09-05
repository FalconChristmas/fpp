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

#include <cstdint>
#include <list>
#include "fpp-json-fwd.h"
#include <string>
#include <vector>

#include "../commands/Commands.h"

class PixelOverlayModel;
class PixelOverlayState;

namespace Magick
{
    class Image;
};

// Scale an already-decoded image to exactly w x h pixels.  Shared by the Image
// overlay effect and PlaylistEntryImage so both size images identically.
//
// mode:
//   "Scale to Fit"  - preserve aspect, letterbox/pillarbox the remainder black
//   "Crop to Fill"  - preserve aspect, cover the box, centre-crop the overflow
//   "Stretch"       - ignore aspect, resize to exactly w x h
// Any other value (including "None") leaves the image untouched.
void ScaleOverlayImage(Magick::Image& image, int w, int h, const std::string& mode);

// Decode an encoded image held in memory (PNG/JPEG/GIF/BMP/...) and draw it on
// a model. Shared by the Image overlay effect's file path and the bulk-data
// HTTP endpoint so an uploaded image is sized and placed exactly like a local
// one.
//
// The image is scaled to a boxW x boxH box using ScaleOverlayImage()'s mode
// vocabulary ("Scale to Fit", "Crop to Fill", "Stretch", "None"), then drawn
// with its top-left at (xOffset, yOffset) -- or centred within the box when
// `center` is set, which is what "None" wants by default. Anything falling
// outside the model is clipped. `st` is the per-blit blend, not the model's
// enable state.
//
// Returns false and fills `err` on a decode failure or if nothing landed
// inside the model.
bool DrawEncodedImageOnModel(PixelOverlayModel* m, const void* data, size_t len,
                             const std::string& scaling, int boxW, int boxH,
                             int xOffset, int yOffset, bool center,
                             const PixelOverlayState& st, bool toOverlayBuffer,
                             std::string& err);

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
    virtual void toJson(Json::Value& v) const {
        v["name"] = name();
    };

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
    static const std::list<std::string>& GetPixelOverlayEffects();

    // Bumped whenever an effect is registered or unregistered. The described
    // effect list is otherwise fixed: every list in it that varies at runtime
    // (fonts, images) is declared as a contentListUrl the client fetches
    // separately rather than inlined, so registration is the only thing that
    // can change what /api/overlays/effects returns.
    static uint64_t GetPixelOverlayEffectsGeneration();

    static void AddPixelOverlayEffect(PixelOverlayEffect* effect);
    static void RemovePixelOverlayEffect(PixelOverlayEffect* effect);
};
