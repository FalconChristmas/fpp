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

#include "../../log.h"

#include "OutputProcessor.h"

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

void OutputProcessors::addProcessor(OutputProcessor* p) {
    if (p == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(processorsLock);
    processors.push_back(p);
}
void OutputProcessors::removeProcessor(OutputProcessor* p) {
    std::lock_guard<std::mutex> lock(processorsLock);
    processors.remove(p);
}
void OutputProcessors::removeAll() {
    std::lock_guard<std::mutex> lock(processorsLock);
    for (OutputProcessor* a : processors) {
        delete a;
    }
    processors.clear();
}

void OutputProcessors::loadFromJSON(const Json::Value& config) {
    // If we are reloading, remove all existing processors first
    // But only the ones created from JSON, plugins may have added their own
    // and we don't want to remove those.
    std::unique_lock<std::mutex> lock(processorsLock);
    for (auto a : fromJsonProcessors) {
        processors.remove(a);
        delete a;
    }
    fromJsonProcessors.clear();
    lock.unlock();

    for (Json::Value::const_iterator itr = config.begin(); itr != config.end(); ++itr) {
        std::string name = itr.key().asString();
        if (name == "outputProcessors") {
            Json::Value val = *itr;
            if (val.isArray()) {
                for (int x = 0; x < val.size(); x++) {
                    OutputProcessor* p = create(val[x]);
                    addProcessor(p);
                    lock.lock();
                    fromJsonProcessors.push_back(p);
                    lock.unlock();
                }
            } else {
                OutputProcessor* p = create(val);
                addProcessor(p);
                lock.lock();
                fromJsonProcessors.push_back(p);
                lock.unlock();
            }
        }
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
                LogDebug(VB_CHANNELOUT, "Before Model applied:   %d-%d => Model: %s model\n",
                         start, start + count - 1,
                         model.c_str(), m_channel);

                int offset = start;
                start = m_model->getStartChannel() + start - 1;

                if (count > m_channel) {
                    count = m_channel - offset;
                    LogWarn(VB_CHANNELOUT, "Output processor for Model: %s tried to go past end channel of model.  Restricting to %d channels\n", model.c_str(), count);
                } else if (count < m_channel) {
                    LogInfo(VB_CHANNELOUT, "Output processor for Model: %s is using less channels (%d) than overlay model has (%d).  This may be intentional\n",
                            model.c_str(), count, m_channel);
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
