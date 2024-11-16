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
#include "OutputMonitor.h"
#include "Warnings.h"
#include "common.h"
#include "channeloutput/PixelString.h"
#include "channeloutput/serialutil.h"
#include "util/GPIOUtils.h"

#include "util/BBBPruUtils.h"
#include "util/BBBUtils.h"
#include <sys/wait.h>
#include <arm_neon.h>

extern "C" {
bool encodeFalconV5Packet(const Json::Value& description, uint8_t* packet);
bool decodeFalconV5Packet(uint8_t* packet, Json::Value& result);
}

class FalconV5Listener {
public:
    FalconV5Listener(const Json::Value& config) {
        pin = config["pin"].asString();
        const PinCapabilities& p = PinCapabilities::getPinByName(pin);
        p.configPin("pruin");

        const BBBPinCapabilities* pc = (const BBBPinCapabilities*)p.ptr();
        offset = pc->pruPin;
    }
    ~FalconV5Listener() {
        const PinCapabilities& p = PinCapabilities::getPinByName(pin);
        p.configPin("gpio_pu", false);
    }
    std::string pin;
    int offset = 0;
};

// structure of the data at the start of the PRU ram
// that the pru program expects to see
typedef struct {
    // 0xFFFF to abort
    volatile uint32_t command;
    volatile uint32_t length;
} __attribute__((__packed__)) FalconV5PRUData;

class PRUControl {
public:
    PRUControl() {
        /*
        char* args[10];
        int idx = 0;
        args[idx++] = (char*)"/bin/bash";
        args[idx++] = (char*)"/opt/fpp/src/non-gpl/FalconV5Support/compileListener.sh";
        args[idx++] = (char*)"0";
        args[idx] = NULL;

        pid_t compilePid = fork();
        if (compilePid == 0) {
            execvp("/bin/bash", args);
        } else {
            wait(NULL);
        }
        const std::string filename = "/tmp/FalconV5Support_pru0.out";
        */
        const std::string filename = "/opt/fpp/src/non-gpl/FalconV5Support/FalconV5PRUListener_pru0.out";

        pru = new BBBPru(0, true, false);
        pruData = (FalconV5PRUData*)pru->data_ram;
        data = pru->shared_ram;
        pru->run(filename);
    }
    ~PRUControl() {
        pruData->command = 0xFFFF;
        __asm__ __volatile__("" ::
                                 : "memory");
        pru->stop();
        delete pru;
    }

    BBBPru* pru = nullptr;
    FalconV5PRUData* pruData = nullptr;
    uint8_t* data = nullptr;
};

FalconV5Support::FalconV5Support() {
    OutputMonitor::INSTANCE.setSmartReceiverEventCallback([this](int port, int index, const std::string& cmd) {
        togglePort = port;
        toggleIndex = index;
        command = cmd;
    });
}
FalconV5Support::~FalconV5Support() {
    OutputMonitor::INSTANCE.setSmartReceiverEventCallback(nullptr);
    for (auto rc : receiverChains) {
        delete rc;
    }
    for (auto l : listeners) {
        delete l;
    }
    if (pru) {
        delete pru;
    }
    listeners.clear();
    receiverChains.clear();
}

void FalconV5Support::addListeners(const Json::Value& config) {
    for (int x = 0; x < config["listenerPins"].size(); x++) {
        listeners.push_back(new FalconV5Listener(config["listenerPins"][x]));
    }
    if (!listeners.empty()) {
        pru = new PRUControl();
    }
    for (int x = 0; x < config["muxPins"].size(); x++) {
        std::string p = config["muxPins"][x].asString();
        muxPins.push_back(PinCapabilities::getPinByName(p).ptr());
        muxPins.back()->configPin("gpio", true);
        muxPins.back()->setValue(0);
    }
}

static inline void maskBit(int len, int bit, uint8_t* s, uint8_t* d) {
    uint8_t m1 = 1 << bit;
    const uint8x8_t bitmask = { m1, m1, m1, m1, m1, m1, m1, m1 };
    int idx = 0;
    while (idx < len) {
        uint8x8_t work = vld1_u8(s); // load 8 bytes
        uint8x8_t msk = vand_u8(work, bitmask);
        vst1_u8(d, msk);
        s += 8;
        d += 8;
        idx += 8;
    }
}
static void printDataBuf(int len, uint8_t* data) {
    printf("Len: %d    \n", len);
    for (int x = 0; x < 40; x++)
        printf("%02X %s", data[x], (x % 5 != 4 ? "" : "  "));
    printf("\n");
    for (int x = 0; x < 40; x++)
        printf("%02X %s", data[x + 40], (x % 5 != 4 ? "" : "  "));
    printf("\n");
    for (int x = 0; x < 40; x++)
        printf("%02X %s", data[x + 80], (x % 5 != 4 ? "" : "  "));
    printf("\n\n");
}
static uint8_t readByte(uint8_t* data, int& pos) {
    constexpr int superSample = 5;
    constexpr int sampleOffset = 2;
    // find start bit
    uint8_t ret = 0;
    while (data[pos] != 0) {
        ++pos;
    }
    pos += sampleOffset + superSample;
    if (data[pos])
        ret |= 0x1;
    pos += superSample;
    if (data[pos])
        ret |= 0x2;
    pos += superSample;
    if (data[pos])
        ret |= 0x4;
    pos += superSample;
    if (data[pos])
        ret |= 0x8;
    pos += superSample;
    if (data[pos])
        ret |= 0x10;
    pos += superSample;
    if (data[pos])
        ret |= 0x20;
    pos += superSample;
    if (data[pos])
        ret |= 0x40;
    pos += superSample;
    if (data[pos])
        ret |= 0x80;
    pos += superSample; // should be in the stop bit now
    return ret;
}

void FalconV5Support::processListenerData() {
    if (pru) {
        uint32_t len = pru->pruData->length;
        // if (len) {
        //     printDataBuf(len, pru->data);
        // }
        if (len > 4096 * 3) {
            len = 4096 * 3;
        }
        if (len) {
            uint8_t buf[4096 * 3];
            memcpy(buf, pru->data, len);
            for (auto& l : listeners) {
                uint8_t data[4096 * 3];
                uint8_t packet[1024];
                maskBit(len, l->offset, buf, data);
                int pidx = 0;
                int pos = 0;
                while (pos < len) {
                    packet[pidx++] = readByte(data, pos);
                }
                if (pidx > 1) {
                    // printDataBuf(len, data);
                    // printDataBuf(pidx, packet);
                    Json::Value json;
                    if (pidx != 0 && decodeFalconV5Packet(packet, json)) {
                        int port = json["port"].asInt();
                        // printf("Port:  %d   Index: %d\n", port, json["index"].asInt());
                        //  printf("%s\n", SaveJsonToString(json, "  ").c_str());
                        for (auto& rc : receiverChains) {
                            if (rc->getPixelStrings()[0]->m_portNumber == port) {
                                rc->handleQueryResponse(json);
                            }
                        }
                    }
                }
            }
        }
        pru->pruData->length = 0;
    }
}

void FalconV5Support::setCurrentMux(int i) {
    curMux = i;
    for (auto& p : muxPins) {
        int d = i & 0x1;
        p->setValue(d);
        i >>= 1;
    }
}
FalconV5Support::ReceiverChain::ReceiverChain(PixelString* p1, PixelString* p2, PixelString* p3, PixelString* p4, int grp, int m) :
    group(grp), mux(m), numReceivers(1) {
    strings[0] = p1;
    strings[1] = p2;
    strings[2] = p3;
    strings[3] = p4;
    for (auto& s : strings) {
        uint32_t srCount = 1;
        for (auto& vs : s->m_virtualStrings) {
            if (vs.isFalconV5SmartReceiver()) {
                srCount++;
            }
        }
        numReceivers = std::max(numReceivers, srCount);
    }
}
FalconV5Support::ReceiverChain* FalconV5Support::addReceiverChain(PixelString* p1, PixelString* p2, PixelString* p3, PixelString* p4, int group, int m) {
    FalconV5Support::ReceiverChain* rc = new FalconV5Support::ReceiverChain(p1, p2, p3, p4, group, m);
    receiverChains.push_back(rc);

    if (m >= queryData.size()) {
        queryData.resize(m + 1);
        while (m >= maxCount.size()) {
            maxCount.push_back(0);
        }
    }
    queryData[m][group].push_back(rc);

    int newM = 0;
    for (auto& g : queryData[m]) {
        for (auto& rc : g.second) {
            newM += rc->getReceiverCount();
        }
    }
    maxCount[m] = std::max(maxCount[m], newM);
    return rc;
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
bool FalconV5Support::ReceiverChain::generateQueryPacket(uint8_t* packet, int receiver) const {
    Json::Value config;
    config["type"] = "query";
    config["receiver"] = receiver;
    return encodeFalconV5Packet(config, packet);
}
bool FalconV5Support::ReceiverChain::generateToggleEFusePacket(uint8_t* packet, int receiver, int port) const {
    Json::Value config;
    config["type"] = "toggleFuse";
    config["receiver"] = receiver;
    config["port"] = port;
    return encodeFalconV5Packet(config, packet);
}

bool FalconV5Support::ReceiverChain::generateNumberPackets(uint8_t* packet, uint8_t* packet2) const {
    Json::Value config;
    config["type"] = "number";
    config["port"] = strings[0]->m_portNumber;
    config["phase"] = 1;
    if (encodeFalconV5Packet(config, packet)) {
        config["phase"] = 2;
        return encodeFalconV5Packet(config, packet2);
    }
    return false;
}
bool FalconV5Support::ReceiverChain::generateResetEFusePacket(uint8_t* packet, int receiver, int port) const {
    Json::Value config;
    config["type"] = "resetFuses";
    config["receiver"] = receiver;
    config["port"] = port;
    return encodeFalconV5Packet(config, packet);
}

bool FalconV5Support::ReceiverChain::generateSetFusesPacket(uint8_t* packet, bool on) const {
    Json::Value config;
    config["type"] = "setFuses";
    config["state"] = on ? 1 : 0;
    return encodeFalconV5Packet(config, packet);
}

bool FalconV5Support::ReceiverChain::generateResetFusesPacket(uint8_t* packet) const {
    Json::Value config;
    config["type"] = "resetFuses";
    return encodeFalconV5Packet(config, packet);
}
bool FalconV5Support::ReceiverChain::generateQueryPacket(uint8_t* packet) {
    if (curReceiverQuery == numReceivers) {
        curReceiverQuery = 0;
    }
    return generateQueryPacket(packet, curReceiverQuery++);
}
bool FalconV5Support::ReceiverChain::generatePixelCountPacket(uint8_t* packet) const {
    Json::Value config;
    config["type"] = "pixelCount";
    return encodeFalconV5Packet(config, packet);
}

void FalconV5Support::ReceiverChain::handleQueryResponse(Json::Value& json) {
    int index = json["index"].asInt();
    int port = json["port"].asInt();
    int dial = json["dial"].asInt();
    int numPorts = json["numPorts"].asInt();
    int id = json["id"].asInt();
    // printf("Resp:  Idx: %d     Port: %d    dial:  %d    NP: %d    ID: %d\n", index, port, dial, numPorts, id);
    for (int x = 0; x < numPorts; x++) {
        OutputMonitor::INSTANCE.setSmartReceiverInfo(port + x % 4, index,
                                                     json["ports"][x]["fuseOn"].asBool(),
                                                     json["ports"][x]["fuseBlown"].asBool(),
                                                     json["ports"][x]["current"].asInt(),
                                                     json["ports"][x]["pixelCount"].asInt());
    }
}

bool FalconV5Support::generateDynamicPacket(std::vector<std::array<uint8_t, 64>>& packets, bool& listen) {
    if (maxCount.empty()) {
        return false;
    }
    if (curCount == maxCount[curMux]) {
        // finished this mux, but we need to wait a frame before changing muxes
        ++curCount;
        return false;
    } else if (curCount > maxCount[curMux]) {
        curCount = 0;
        int newMux = curMux + 1;
        if (newMux >= maxCount.size()) {
            newMux = 0;
            triggerPixelCount = false;
        }
        if (newMux != curMux) {
            setCurrentMux(newMux);
        }
        if (maxCount[newMux] == 0) {
            curCount++;
            return false;
        }
    }
    curCount++;
    for (auto& g : queryData[curMux]) {
        auto rc = g.second.front();
        if (!rc->hasMoreQueries()) {
            rc->resetQueryCount();
            g.second.pop_front();
            g.second.push_back(rc);
            rc = g.second.front();
        }
        int rcP = rc->getPixelStrings().front()->m_portNumber;
        if (rcP <= togglePort && (rcP + 4) > togglePort) {
            if (command == "ToggleOutput") {
                rc->generateToggleEFusePacket(&packets[rc->getPixelStrings().front()->m_portNumber][0], toggleIndex, togglePort % 4);
            } else if (command == "ResetOutput") {
                rc->generateResetEFusePacket(&packets[rc->getPixelStrings().front()->m_portNumber][0], toggleIndex, togglePort % 4);
            }
            togglePort = -1;
        } else if (triggerPixelCount) {
            rc->generatePixelCountPacket(&packets[rc->getPixelStrings().front()->m_portNumber][0]);
            listen = false;
        } else {
            rc->generateQueryPacket(&packets[rc->getPixelStrings().front()->m_portNumber][0]);
            listen = true;
        }
    }
    return true;
}

void FalconV5Support::sendCountPixelPackets() {
    setCurrentMux(0);
    curCount = 0;
    triggerPixelCount = true;
}
