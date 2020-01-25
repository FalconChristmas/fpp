/*
 *   OutputProcessor class for Falcon Player (FPP)
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
#include "OutputProcessor.h"

#include "RemapOutputProcessor.h"
#include "HoldValueOutputProcessor.h"
#include "SetValueOutputProcessor.h"
#include "BrightnessOutputProcessor.h"
#include "ColorOrderOutputProcessor.h"
#include "log.h"


OutputProcessors::OutputProcessors() {
}
OutputProcessors::~OutputProcessors() {
    for (OutputProcessor *a : processors) {
        delete a;
    }
    processors.clear();
}

void OutputProcessors::ProcessData(unsigned char *channelData) const {
    std::lock_guard<std::mutex> lock(processorsLock);
    for (OutputProcessor *a : processors) {
        if (a->isActive()) {
            a->ProcessData(channelData);
        }
    }
}

void OutputProcessors::addProcessor(OutputProcessor*p) {
    if (p == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(processorsLock);
    processors.push_back(p);
}
void OutputProcessors::removeProcessor(OutputProcessor*p) {
    std::lock_guard<std::mutex> lock(processorsLock);
    processors.remove(p);
}
void OutputProcessors::removeAll() {
    std::lock_guard<std::mutex> lock(processorsLock);
    for (OutputProcessor *a : processors) {
        delete a;
    }
    processors.clear();
}

void OutputProcessors::loadFromJSON(const Json::Value &config, bool clear) {
    if (clear) {
        removeAll();
    }
    for (Json::Value::const_iterator itr = config.begin() ; itr != config.end() ; ++itr) {
        std::string name = itr.key().asString();
        if (name == "outputProcessors") {
            Json::Value val = *itr;
            if (val.isArray()) {
                for (int x = 0; x < val.size(); x++) {
                    addProcessor(create(val[x]));
                }
            } else {
                addProcessor(create(val));
            }
        }
    }
}
OutputProcessor *OutputProcessors::create(const Json::Value &config) {
    std::string type = config["type"].asString();
    if (type == "Remap") {
        return new RemapOutputProcessor(config);
    } else if (type == "Brightness") {
        return new BrightnessOutputProcessor(config);
    } else if (type == "Hold Value") {
        return new HoldValueOutputProcessor(config);
    } else if (type == "Set Value") {
        return new SetValueOutputProcessor(config);
    } else if (type == "Reorder Colors") {
        return new ColorOrderOutputProcessor(config);
    } else {
        LogErr(VB_CHANNELOUT, "Unknown OutputProcessor type: %s\n", type.c_str());
    }
    return nullptr;
}


OutputProcessor *OutputProcessors::find(std::function<bool(OutputProcessor*)> f) const {
    std::lock_guard<std::mutex> lock(processorsLock);
    for (OutputProcessor *a : processors) {
        if (f(a)) {
            return a;
        }
    }
    return nullptr;
}

void OutputProcessors::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    for (OutputProcessor *a : processors) {
        a->GetRequiredChannelRanges(addRange);
    }
}


OutputProcessor::OutputProcessor() : description(), active(true) {
}

OutputProcessor::~OutputProcessor() {
}
