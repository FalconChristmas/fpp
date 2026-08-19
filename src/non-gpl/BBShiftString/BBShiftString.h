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
#include <chrono>
#include <string>
#include "fpp-json-fwd.h"
#include <thread>
#include <vector>

#include "../FalconV5Support/FalconV5Support.h"
#include "channeloutput/ChannelOutput.h"
#include "channeloutput/PixelString.h"
#include "util/BBBPruUtils.h"

// How deep the cape's shift register chain is, i.e. how many strings hang off
// each data pin.  8 is the long standing layout; 16 doubles the strings a
// single PRU can drive and is AM62x only - see the SHIFT16 firmware variant in
// BBShiftString.asm and the makefile.  Buffers and maps are sized for the
// maximum; the live depth is m_stringsPerPin, from the cape's "stringsPerPin".
constexpr int MAX_STRINGS_PER_PIN = 16;
constexpr int MAX_PINS_PER_PRU = 8;

// when output restarts, there are some packets that need to be sent first
// but not when looped around
constexpr int FIRST_LOOPING_CONFIG_PACKET = 6;
constexpr int NUM_CONFIG_PACKETS = 96 + FIRST_LOOPING_CONFIG_PACKET;

// structure of the data at the start of the PRU ram
// that the pru program expects to see
typedef struct {
    // in the DDR shared with the PRU
    uint32_t address_dma;
    uint32_t address_dma_packet;

    // write data length to start, 0xFFFF to abort. will be cleared when started
    volatile uint32_t command;
    volatile uint32_t response;

    uint32_t buffer[4]; // need a bit of a buffer
    uint16_t commandTable[3578];
} __attribute__((__packed__)) BBShiftStringData;

class BBShiftStringOutput : public ChannelOutput {
public:
    BBShiftStringOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~BBShiftStringOutput();

    virtual std::string GetOutputType() const {
        return "BBB Pixel Strings";
    }

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual void StartingOutput() override;
    virtual void StoppingOutput() override;

    virtual int SendData(unsigned char* channelData) override;
    virtual void PrepData(unsigned char* channelData) override;
    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

    virtual void OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) override;
    virtual bool SupportsTesting() const { return true; }

private:
    void StopPRU(bool wait = true);
    int StartPRU();

    std::string m_subType;

    class FalconV5PacketInfo {
    public:
        FalconV5PacketInfo(int l, uint8_t* d, bool list) :
            len(l), data(d), listen(list) {
        }
        FalconV5PacketInfo() :
            len(0), data(nullptr), listen(false) {
        }
        ~FalconV5PacketInfo() {
        }
        uint32_t getCommandFlags() {
            uint32_t f = 0x20000;
            if (len == 2) {
                f |= 0x40000;
            }
            if (listen) {
                f |= 0x80000;
            }
            return f;
        }
        int len;
        uint8_t* data;
        bool listen = false;
    };

    class FrameData {
    public:
        FrameData() {
            for (int y = 0; y < MAX_PINS_PER_PRU; ++y) {
                for (int x = 0; x < MAX_STRINGS_PER_PIN; ++x) {
                    stringMap[y][x] = -1;
                }
            }
            for (int y = 0; y < NUM_CONFIG_PACKETS; ++y) {
                v5_config_packets[y] = nullptr;
            }
        }
        ~FrameData() {
            if (formattedData)
                free(formattedData);
            if (heapData) {
                if (curData)
                    free(curData);
                if (lastData)
                    free(lastData);
            }
            for (int y = 0; y < NUM_CONFIG_PACKETS; ++y) {
                if (v5_config_packets[y] && v5_config_packets[y] != &dynamicPacketInfo2 && v5_config_packets[y] != &dynamicPacketInfo1) {
                    delete v5_config_packets[y];
                }
            }
        }
        std::array<std::array<int, MAX_STRINGS_PER_PIN>, MAX_PINS_PER_PRU> stringMap;
        BBBPru* pru = nullptr;
        BBShiftStringData* pruData = nullptr;

        uint8_t* formattedData = nullptr;

        uint8_t* lastData = nullptr;
        uint8_t* curData = nullptr;
        uint32_t frameSize = 0;
        int maxStringLen = 0;
        int outputStringLen = 0;

        // AM62x only: the PRU is fed through a ring in the PRU shared RAM by
        // a pump thread instead of reading DDR (see pru/SMEMRing.hp); the
        // frame buffers live in normal cached memory.  Unused on AM335x.
        BBBPruSMEMRing ring;
        bool heapData = false;

        // frame handoff to the pump thread; SendData fills pendingFrame then
        // bumps pendingSeq, the pump snapshots it when the PRU is ready
        struct PumpFrame {
            const uint8_t* frameData = nullptr;
            uint32_t frameBytes = 0;
            const uint8_t* packetData = nullptr;
            uint32_t packetBytes = 0;
            uint32_t command = 0;
        } pendingFrame;
        std::atomic<uint32_t> pendingSeq{ 0 };
        // last values read back out of the firmware's counters, so only new
        // events get reported
        uint32_t renderedFrames = 0;
        uint32_t resyncCount = 0;
        // when the firmware first stopped making progress on a pending frame;
        // zero while it is keeping up
        std::chrono::steady_clock::time_point stalledSince{};
        // pump-internal streaming state
        uint32_t pumpedSeq = 0;
        PumpFrame activeFrame;
        uint32_t activeOff = 0;
        bool pumpActive = false;

        FalconV5PacketInfo* v5_config_packets[NUM_CONFIG_PACKETS];
        FalconV5PacketInfo dynamicPacketInfo1;
        FalconV5PacketInfo dynamicPacketInfo2;
        FalconV5PacketInfo* dynamicPacketInfo = nullptr;
        int curV5ConfigPacket = 0;
    } m_pru0, m_pru1;

    std::vector<PixelString*> m_strings;
    // per string, per virtual string: nonzero when chMap[i+3] == chMap[i]+3
    // for the whole map, letting prepData skip the map indirection
    std::vector<std::vector<uint8_t>> m_vsAffine;
    std::map<std::string, std::string> m_usedPins;

    // per-PRU pin sets; the platform defaults unless the cape overrides
    // them by name (combo capes with nonstandard pinouts).  The cape only
    // carries header pin names - the r30 bit numbers are resolved here
    // from the platform pin table and published to the firmware at runtime
    std::vector<std::string> m_dataPins[2];
    std::vector<std::string> m_ctrlPins[2];
    bool m_pinNamesOverridden[2] = { false, false };
    int m_clockBit[2] = { 0, 0 };
    int m_latchBit[2] = { 0, 0 };
    // sharing the PRUSS with a panel driver on the other PRU: never clear
    // the shared RAM or the other PRU's memory, no FalconV5 listeners
    bool m_sharedPRUSS = false;

    // shift register chain depth: strings per data pin, 8 or 16.  Everything
    // the PRU consumes scales with stringsPerPru(): one shift phase clocks
    // that many bytes, so a frame is outputStringLen * stringsPerPru() bytes.
    int m_stringsPerPin = 8;
    int stringsPerPru() const { return MAX_PINS_PER_PRU * m_stringsPerPin; }

    uint32_t m_curFrame = 0;
    uint32_t m_licensedOutputs = 0;

    int m_testCycle = -1;
    int m_testType = 0;
    float m_testPercent = 0.0f;

    FalconV5Support* falconV5Support = nullptr;
    std::list<std::string> m_autoCreatedModelNames;
    bool supportsV5Listeners = false;
    // any configured Falcon V5 (bidirectional) receivers after the
    // capability checks; V4 (send-only) chains do not set this
    bool m_hasBidirSR = false;

    void prepData(FrameData& d, unsigned char* channelData);
    void sendData(FrameData& d);
    void bitFlipData(uint8_t* stringChannelData, uint8_t* bitSwapped, size_t len);
    // The hot paths are templated on the chain depth and dispatched once per
    // frame rather than reading m_stringsPerPin in the inner loops, so the 8
    // deep path keeps the compile time constants (and the codegen) it had.
    template<int SPP>
    void prepDataT(FrameData& d, unsigned char* channelData);
    template<int SPP>
    void bitFlipDataT(uint8_t* stringChannelData, uint8_t* bitSwapped, size_t len);

    // AM62x shared-memory ring pump (see pru/SMEMRing.hp)
    std::thread m_pumpThread;
    std::atomic<bool> m_pumpRunning{ false };
    uint8_t* m_fv5PacketMem = nullptr;
    void runPumpThread();
    bool pumpFrameData(FrameData& d);

    void createOutputLengths(FrameData& d, const std::string& pfx);
    // report ring resynchronizations the firmware has had to make
    void reportRingResync(FrameData& d, int pru);

    // Bit cell timing in ns, published to the firmware as cycle counts.  These
    // are the values the firmware used to have compiled in; keeping them here
    // is what lets a different bit cell (a 400kHz part) be selected without a
    // separate firmware image.
    int m_t0Ns = 320;
    int m_t1Ns = 750;
    int m_lowNs = 1120;

    // ordering so a Timing can key a map; the values are small and exact
    struct TimingLess {
        bool operator()(const PixelString::Timing& a, const PixelString::Timing& b) const {
            return std::tie(a.t0Ns, a.t1Ns, a.periodNs) < std::tie(b.t0Ns, b.t1Ns, b.periodNs);
        }
    };
    // pick the controller's bit cell from the protocols actually in use
    void resolveTiming(const Json::Value& config);

    void demoteInvertedReceiverChains();

    void setupFalconV5Support(const Json::Value& root, uint8_t* memLoc);
    void encodeFalconV5Packet(std::vector<std::array<uint8_t, 64>>& packets, uint8_t* memLocPru0, uint8_t* memLocPru1);
};
