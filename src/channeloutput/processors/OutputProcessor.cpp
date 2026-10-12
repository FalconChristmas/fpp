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

#include "fpp-pch.h"

#include "fpp-json.h"

#include <algorithm>

#include "../../log.h"

#include "OutputProcessor.h"
#include "../../Warnings.h"

#include "BrightnessOutputProcessor.h"
#include "ClampValueOutputProcessor.h"
#include "ColorOrderOutputProcessor.h"
#include "FoldOutputProcessor.h"
#include "HoldValueOutputProcessor.h"
#include "OverrideZeroOutputProcessor.h"
#include "RemapOutputProcessor.h"
#include "ScaleValueOutputProcessor.h"
#include "SetValueOutputProcessor.h"
#include "ThreeToFourOutputProcessor.h"
#include "../../overlays/PixelOverlay.h"
#include "../../overlays/PixelOverlayModel.h"

OutputProcessors::OutputProcessors() {
}
OutputProcessors::~OutputProcessors() {
    for (OutputProcessor* a : processors) {
        delete a;
    }
    processors.clear();
}

void OutputProcessors::ProcessData(unsigned char* channelData) const {
    std::lock_guard<std::mutex> lock(processorsLock);
    for (OutputProcessor* a : processors) {
        if (a->isActive()) {
            a->ProcessData(channelData);
        }
    }
}

// Whether the processor's channels lie inside the sequence buffer, recording a
// warning when they do not.  Caller holds processorsLock.
bool OutputProcessors::rangeIsValid(OutputProcessor* p) {
    // ProcessData() writes straight into the sequence buffer, which is
    // FPPD_MAX_CHANNELS long, over the channels the processor reports here.
    // The UI only checks that numbers are positive, so a start or a count (or
    // count * loops) can put that past the end - where it overwrites the
    // off/white sentinel channels just past the buffer, or faults on every
    // frame.  Refuse it rather than trust each processor to clamp.  An
    // inverted range is how a processor with nothing to do reports.
    int badMin = 0;
    int badMax = -1;
    p->GetRequiredChannelRanges([&badMin, &badMax](int m1, int m2) {
        if (badMax < 0 && m2 >= m1 && (m1 < 0 || m2 >= (int)FPPD_MAX_CHANNELS)) {
            badMin = m1;
            badMax = m2;
        }
    });
    if (badMax < 0) {
        return true;
    }
    std::string warning = "Output processor needs channels " + std::to_string(badMin + 1) + "-" +
                          std::to_string(badMax + 1) + ", outside 1-" + std::to_string(FPPD_MAX_CHANNELS) +
                          ". Processor disabled.";
    LogErr(VB_CHANNELOUT, "%s\n", warning.c_str());
    WarningHolder::AddWarning(warning);
    rejectedWarnings.push_back(warning);
    return false;
}

void OutputProcessors::addProcessor(OutputProcessor* p) {
    if (p == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(processorsLock);
    if (rangeIsValid(p)) {
        processors.push_back(p);
    } else {
        delete p; // rejected; nothing else owns it
    }
}
void OutputProcessors::removeProcessor(OutputProcessor* p) {
    // ProcessData() walks the chain under processorsLock, so once the
    // processor is out of the list (under the same lock) nothing can be using
    // it.  Removed processors used to be leaked.
    std::lock_guard<std::mutex> lock(processorsLock);
    auto it = std::find(processors.begin(), processors.end(), p);
    if (it != processors.end()) {
        processors.erase(it);
        delete p;
    }
}
void OutputProcessors::removeAll() {
    std::lock_guard<std::mutex> lock(processorsLock);
    for (OutputProcessor* a : processors) {
        delete a;
    }
    processors.clear();
}

void OutputProcessors::loadFromJSON(const Json::Value& config) {
    // Build the whole new chain first and swap it in under one lock: removing
    // the old processors, unlocking and adding the new ones one at a time let
    // the output thread run a frame with part of the chain (a brightness or
    // current limit missing for a frame).  Only the processors created from
    // JSON are replaced; plugins may have added their own.
    std::list<OutputProcessor*> created;
    for (Json::Value::const_iterator itr = config.begin(); itr != config.end(); ++itr) {
        std::string name = itr.key().asString();
        if (name == "outputProcessors") {
            Json::Value val = *itr;
            if (val.isArray()) {
                for (int x = 0; x < val.size(); x++) {
                    if (OutputProcessor* p = create(val[x])) {
                        created.push_back(p);
                    }
                }
            } else if (OutputProcessor* p = create(val)) {
                created.push_back(p);
            }
        }
    }

    std::list<OutputProcessor*> old;
    {
        std::lock_guard<std::mutex> lock(processorsLock);
        for (auto& w : rejectedWarnings) {
            WarningHolder::RemoveWarning(w);
        }
        rejectedWarnings.clear();
        for (auto a : fromJsonProcessors) {
            processors.remove(a);
        }
        old.swap(fromJsonProcessors);
        for (auto p : created) {
            if (rangeIsValid(p)) {
                processors.push_back(p);
            }
            // owned either way, freed on the next reload
            fromJsonProcessors.push_back(p);
        }
    }
    // not running any more, and no longer reachable by the output thread
    for (auto a : old) {
        delete a;
    }
}
OutputProcessor* OutputProcessors::create(const Json::Value& config) {
    int active = config["active"].asInt();
    if (active == 1) {
        std::string type = config["type"].asString();
        if (type == "Remap") {
            return new RemapOutputProcessor(config);
        } else if (type == "Brightness") {
            return new BrightnessOutputProcessor(config);
        } else if (type == "Hold Value") {
            return new HoldValueOutputProcessor(config);
        } else if (type == "Set Value") {
            return new SetValueOutputProcessor(config);
        } else if (type == "Clamp Value") {
            return new ClampValueOutputProcessor(config);
        } else if (type == "Scale Value") {
            return new ScaleValueOutputProcessor(config);
        } else if (type == "Reorder Colors") {
            return new ColorOrderOutputProcessor(config);
        } else if (type == "Three to Four") {
            return new ThreeToFourOutputProcessor(config);
        } else if (type == "Override Zero") {
            return new OverrideZeroOutputProcessor(config);
        } else if (type == "Fold") {
            return new FoldOutputProcessor(config);
        } else {
            LogErr(VB_CHANNELOUT, "Unknown OutputProcessor type: %s\n", type.c_str());
        }
    } else {
        LogDebug(VB_CHANNELOUT, "Skipping disabled OutputProcessor\n");
    }
    return nullptr;
}

OutputProcessor* OutputProcessors::find(std::function<bool(OutputProcessor*)> f) const {
    std::lock_guard<std::mutex> lock(processorsLock);
    for (OutputProcessor* a : processors) {
        if (f(a)) {
            return a;
        }
    }
    return nullptr;
}

void OutputProcessors::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    // a playlist Remap entry adds and removes processors from other threads
    std::lock_guard<std::mutex> lock(processorsLock);
    for (OutputProcessor* a : processors) {
        a->GetRequiredChannelRanges(addRange);
    }
}

OutputProcessor::OutputProcessor() :
    description(),
    active(true) {
}

OutputProcessor::~OutputProcessor() {
}

void ProcessModelConfig(const Json::Value& config, std::string& model, int& start, int& count) {
    if (config.isMember("model")) {
        model = config["model"].asString();
        if (model != "<Use Start Channel>") {
            auto m_model = PixelOverlayManager::INSTANCE.getModel(model);
            if (!m_model) {
                LogErr(VB_CHANNELOUT, "Invalid Pixel Overlay Model: '%s'\n", model.c_str());
            } else {
                int m_channel = m_model->getChannelCount();
                LogDebug(VB_CHANNELOUT, "Before Model applied:   %d-%d => Model: %s (%d channels)\n",
                         start, start + count - 1,
                         model.c_str(), m_channel);

                // start arrives 1-based within the model; getStartChannel() is the
                // model's 0-based absolute start, so the span still available from
                // offset is m_channel - offset + 1 channels.
                int offset = start;
                start = m_model->getStartChannel() + start - 1;

                if (offset + count - 1 > m_channel) {
                    count = m_channel - offset + 1;
                    if (count < 0) {
                        count = 0;
                    }
                    LogWarn(VB_CHANNELOUT, "Output processor for Model: %s starting at channel %d would run past the end of the model (%d channels).  Restricting to %d channels\n",
                            model.c_str(), offset, m_channel, count);
                } else if (offset + count - 1 < m_channel) {
                    LogInfo(VB_CHANNELOUT, "Output processor for Model: %s is using less channels (%d starting at %d) than overlay model has (%d).  This may be intentional\n",
                            model.c_str(), count, offset, m_channel);
                }
            }
        } else {
            model = "";
            --start;
        }
    } else {
        model = "";
        --start;
    }
}
