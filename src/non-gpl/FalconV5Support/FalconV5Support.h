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
#include <mutex>
#include <vector>

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

    // Copies a newly published listener capture out of the PRU shared RAM
    // into a staging buffer.  Safe to call from any thread, meant to be
    // called often (the string driver's pump thread polls it every few
    // hundred microseconds) so the capture is taken well before the next
    // listen window can overwrite it; processListenerData() also calls it
    // for platforms without a pump thread.
    void stageListenerData();
    // Decodes the staged capture (if any) and delivers the receiver status
    // to the OutputMonitor.  Output thread only.
    void processListenerData();

    bool generateDynamicPacket(std::vector<std::array<uint8_t, 64>>& packets, bool& listen);

    // fraction of frames that carry a query packet and open a listen window
    // once the receivers are being polled: each mux cycles through its
    // receivers one frame at a time and then idles 1 + queryRestFrames
    // frames (the share the scheduler really runs with); 0 without
    // listeners or queryable chains
    float queryFrameShare() const;

    // Spacing of the query rounds.  BBShiftString calls this whenever the
    // sequence frame rate changes, with its predicted frame time without
    // queries and the cost of one query frame: if a query frame fits in the
    // period's slack every round is followed by the usual single rest frame,
    // otherwise enough extra rest frames that the average query cost fits the
    // slack with a margin, capped so a round still starts at least every
    // ~500 ms.  FalconV5QueryRestFrames (>= 0) overrides it for testing.
    // Returns true when the number of rest frames changed.
    bool setFrameBudget(float frameRate, int frameTimeWithoutQueriesUs, int queryFrameCostUs);
    int getQueryRestFrames() const { return queryRestFrames; }

    void sendCountPixelPackets();

private:
    std::list<ReceiverChain*> receiverChains;
    std::list<FalconV5Listener*> listeners;
    std::list<const PinCapabilities*> muxPins;
    PRUControl* pru = nullptr;
    uint32_t lastCaptures = 0;
    uint32_t tornCaptures = 0;
    // staged capture: stageListenerData() fills stageBuf under stageLock,
    // processListenerData() swaps it with workBuf under the lock and decodes
    // workBuf outside it
    std::mutex stageLock;
    std::vector<uint8_t> stageBuf;
    std::vector<uint8_t> workBuf;
    uint32_t stageLen = 0;
    bool stageValid = false;

    float queryFrameShare(int restFrames) const;
    // receivers summed over the muxes, and the muxes that have any: a mux
    // with none is skipped by the scheduler
    int queriedReceivers() const;
    int queriedMuxes() const;

    // extra rest frames after every round of queries, on top of the one rest
    // frame a round always ends with before the mux changes
    int queryRestFrames = 0;
    // FalconV5QueryRestFrames, -1 = automatic (setFrameBudget)
    int queryRestOverride = -1;

    std::vector<std::map<int, std::list<ReceiverChain*>>> queryData;
    std::vector<int> maxCount;
    int curMux = 0;
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

    // Ports a queried chain covers, i.e. the ports a pending toggle can go
    // out on.  The callback drops any other toggle: one that no chain picks
    // up would stay pending forever and, since a pending toggle cuts the rest
    // between query rounds short, hold the scheduler at its fastest cadence.
    // Filled by addReceiverChain() while the callback may already be live.
    mutable std::mutex queriedPortsLock;
    std::vector<bool> queriedPorts;
};