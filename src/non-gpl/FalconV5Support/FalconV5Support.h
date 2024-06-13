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

#include <functional>
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
        ReceiverChain(PixelString* p1, PixelString* p2, PixelString* p3, PixelString* p4, int grp = 0, int m = 0);
        bool generateConfigPacket(uint8_t* packet) const;
        bool generateNumberPackets(uint8_t* packet, uint8_t* packet2) const;
        bool generateQueryPacket(uint8_t* packet, int receiver) const;
        bool generateResetFusesPacket(uint8_t* packet) const;
        bool generateToggleEFusePacket(uint8_t* packet, int receiver, int port) const;
        bool generateResetEFusePacket(uint8_t* packet, int receiver, int port) const;
        bool generateSetFusesPacket(uint8_t* packet, bool on) const;

        const std::array<const PixelString*, 4>& getPixelStrings() const { return strings; };
        uint32_t getReceiverCount() const { return numReceivers; }

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

        uint32_t curReceiverQuery = 0;
    };

    ReceiverChain* addReceiverChain(PixelString* p1, PixelString* p2, PixelString* p3, PixelString* p4, int group, int mux);
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

    int togglePort = -1;
    int toggleIndex = -1;
    std::string command;
};