#pragma once
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

#include <atomic>
#include <cstdint>
#include <functional>
#include "fpp-json-fwd.h"
#include <list>
#include <map>

class PixelString;
class FalconV5Listener;
class PinCapabilities;
class PRUControl;

class FalconV5Support {
public:
    FalconV5Support();
    ~FalconV5Support();

    class ReceiverChain {
    public:
        ReceiverChain(PixelString* p1, PixelString* p2, PixelString* p3, PixelString* p4, int grp = 0, int m = 0, bool sendOnly = false);
        bool generateConfigPacket(uint8_t* packet) const;
        bool generateNumberPackets(uint8_t* packet, uint8_t* packet2) const;
        bool generateQueryPacket(uint8_t* packet, int receiver) const;
        bool generateResetFusesPacket(uint8_t* packet) const;
        bool generateToggleEFusePacket(uint8_t* packet, int receiver, int port) const;
        bool generateResetEFusePacket(uint8_t* packet, int receiver, int port) const;
        bool generateSetFusesPacket(uint8_t* packet, bool on) const;

        const std::array<const PixelString*, 4>& getPixelStrings() const { return strings; };
        uint32_t getReceiverCount() const { return numReceivers; }

        // Falcon V4 handling: only the config packet is sent, no queries
        // are scheduled and no responses are expected
        bool isSendOnly() const { return sendOnly; }

        bool hasMoreQueries() const { return curReceiverQuery < numReceivers; }
        void resetQueryCount() { curReceiverQuery = 0; }
        bool generateQueryPacket(uint8_t* packet);
        void handleQueryResponse(Json::Value& json);
        bool generatePixelCountPacket(uint8_t* packet) const;

    private:
        std::array<const PixelString*, 4> strings;
        uint32_t group;
        uint32_t mux;
        uint32_t numReceivers;
        bool sendOnly;

        uint32_t curReceiverQuery = 0;
    };

    ReceiverChain* addReceiverChain(PixelString* p1, PixelString* p2, PixelString* p3, PixelString* p4, int group, int mux, bool sendOnly = false);
    const std::list<ReceiverChain*>& getReceiverChains() const { return receiverChains; };

    void addListeners(const Json::Value& config);
    void setCurrentMux(int i);

    void processListenerData();

    bool generateDynamicPacket(std::vector<std::array<uint8_t, 64>>& packets, bool& listen);

    void sendCountPixelPackets();

private:
    std::list<ReceiverChain*> receiverChains;
    std::list<FalconV5Listener*> listeners;
    std::list<const PinCapabilities*> muxPins;
    PRUControl* pru = nullptr;

    std::vector<std::map<int, std::list<ReceiverChain*>>> queryData;
    std::vector<int> maxCount;
    bool curMux = 0;
    int curCount = 0;
    bool triggerPixelCount = false;

    // A pending smart-receiver eFuse action.  Published by the OutputMonitor
    // callback (API/MQTT/GPIO/scheduler threads) and consumed by
    // generateDynamicPacket() on the output thread, so it has to be a single
    // atomic word: as three separate fields the consumer could pair a new
    // port with the previous command, and the std::string it replaces was
    // being assigned and compared concurrently, which is a torn read of a
    // heap pointer.  Kept to 4 bytes so the atomic is lock-free on 32-bit ARM
    // as well.
    enum class ToggleCommand : uint8_t {
        None = 0,
        Toggle,
        Reset
    };
    struct PendingToggle {
        int16_t port = -1;
        uint8_t index = 0;
        ToggleCommand cmd = ToggleCommand::None;
    };
    // compare_exchange compares the object representation, so padding here
    // would make the exchange in generateDynamicPacket() fail at random.
    static_assert(sizeof(PendingToggle) == 4, "PendingToggle must be a single unpadded word");
    static_assert(std::atomic<PendingToggle>::is_always_lock_free, "PendingToggle atomic must be lock-free");
    std::atomic<PendingToggle> pendingToggle{ PendingToggle() };
};