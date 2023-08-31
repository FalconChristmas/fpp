/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the CC-BY-ND as described in the
 * included LICENSE.CC-BY-ND file.  This file may be modified for
 * personal use, but modified copies MAY NOT be redistributed in any form.
 */
#include "fpp-pch.h"

#include <unistd.h>

#include "FalconV5Support.h"
#include "channeloutput/PixelString.h"
#include "channeloutput/serialutil.h"
#include "util/GPIOUtils.h"

extern "C" {
bool encodeFalconV5Packet(const Json::Value& description, uint8_t* packet);
bool decodeFalconV5Packet(uint8_t* packet, Json::Value& result);
}

class FalconV5Listener {
public:
    FalconV5Listener(Json::Value& config) {
        std::string pin = config["pin"].asString();
        const PinCapabilities& p = PinCapabilities::getPinByName(pin);
        std::string uart = "/dev/" + p.uart;
        uart = uart.substr(0, uart.find('-'));
        fd = SerialOpen(uart.c_str(), 800000, "8N1", false);
    }
    ~FalconV5Listener() {
        SerialClose(fd);
    }

    int fd;
};

FalconV5Support::FalconV5Support() {
}
FalconV5Support::~FalconV5Support() {
    for (auto rc : receiverChains) {
        delete rc;
    }
    for (auto l : listeners) {
        delete l;
    }
    listeners.clear();
    receiverChains.clear();
}

void FalconV5Support::addControlCallbacks(std::map<int, std::function<bool(int)>>& callbacks) {
    for (auto& l : listeners) {
        if (l->fd >= 0) {
            callbacks[l->fd] = [l, this](int i) {
                uint8_t buf[64];
                printf("About to read\n");
                size_t r = read(l->fd, buf, 64);
                printf("   Read %d\n", r);
                Json::Value val;
                if (decodeFalconV5Packet(buf, val)) {
                    // Do something with the config once we get than info from Falcon
                }
                return false;
            };
        }
    }
}

FalconV5Support::ReceiverChain* FalconV5Support::addReceiverChain(PixelString* p1, PixelString* p2, PixelString* p3, PixelString* p4, int group) {
    FalconV5Support::ReceiverChain* rc = new FalconV5Support::ReceiverChain(p1, p2, p3, p4, group);
    receiverChains.push_back(rc);
    return rc;
}

void FalconV5Support::addListeners(Json::Value& config) {
    for (int x = 0; x < config.size(); x++) {
        listeners.push_back(new FalconV5Listener(config[x]));
    }
}

static bool generateConfigJSONForPixelString(const PixelString* p, Json::Value& config) {
    if (p && p->smartReceiverType == PixelString::ReceiverType::FalconV5) {
        Json::Value v;
        v["startChannels"].append(0);

        int srCount = 0;
        int curChanCount = 0;
        for (auto& vs : p->m_virtualStrings) {
            curChanCount += vs.chMapCount;
            if (vs.isFalconV5SmartReceiver()) {
                srCount++;
                v["startChannels"].append(curChanCount);
            }
        }
        srCount++;
        v["startChannels"].append(curChanCount);
        config["ports"].append(v);
        return true;
    }
    Json::Value v;
    v["startChannels"].append(0);
    config["ports"].append(v);
    return false;
}
bool FalconV5Support::ReceiverChain::generateConfigPacket(uint8_t* packet) const {
    Json::Value config;
    config["type"] = "config";
    bool hasSR = generateConfigJSONForPixelString(strings[0], config);
    hasSR |= generateConfigJSONForPixelString(strings[1], config);
    hasSR |= generateConfigJSONForPixelString(strings[2], config);
    hasSR |= generateConfigJSONForPixelString(strings[3], config);
    if (hasSR) {
        return encodeFalconV5Packet(config, packet);
    }
    return false;
}
