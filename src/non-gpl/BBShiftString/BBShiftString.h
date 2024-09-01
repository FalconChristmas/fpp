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

#include <string>
#include <vector>

#include "../FalconV5Support/FalconV5Support.h"
#include "channeloutput/ChannelOutput.h"
#include "channeloutput/PixelString.h"
#include "util/BBBPruUtils.h"

constexpr int NUM_STRINGS_PER_PIN = 8;
constexpr int MAX_PINS_PER_PRU = 8;

// when output restarts, there are some packets that need to be sent first
// but not when looped around
constexpr int FIRST_LOOPING_CONFIG_PACKET = 6;
constexpr int NUM_CONFIG_PACKETS = 96 + FIRST_LOOPING_CONFIG_PACKET;

// structure of the data at the start of the PRU ram
// that the pru program expects to see
typedef struct {
    // in the DDR shared with the PRU
    uintptr_t address_dma;
    uintptr_t address_dma_packet;

    // write data length to start, 0xFFFF to abort. will be cleared when started
    volatile uint32_t command;
    volatile uint32_t response;

    uint32_t buffer[2]; // need a bit of a buffer
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
                for (int x = 0; x < NUM_STRINGS_PER_PIN; ++x) {
                    stringMap[y][x] = -1;
                }
            }
            for (int y = 0; y < NUM_CONFIG_PACKETS; ++y) {
                v5_config_packets[y] = nullptr;
            }
        }
        ~FrameData() {
            if (channelData)
                free(channelData);
            if (formattedData)
                free(formattedData);
            for (int y = 0; y < NUM_CONFIG_PACKETS; ++y) {
                if (v5_config_packets[y] && v5_config_packets[y] != &dynamicPacketInfo2 && v5_config_packets[y] != &dynamicPacketInfo1) {
                    delete v5_config_packets[y];
                }
            }
        }
        std::array<std::array<int, NUM_STRINGS_PER_PIN>, MAX_PINS_PER_PRU> stringMap;
        BBBPru* pru = nullptr;
        BBShiftStringData* pruData = nullptr;

        uint8_t* channelData = nullptr;
        uint8_t* formattedData = nullptr;

        uint8_t* lastData = nullptr;
        uint8_t* curData = nullptr;
        uint32_t frameSize = 0;
        int maxStringLen = 0;
        int outputStringLen = 0;

        FalconV5PacketInfo* v5_config_packets[NUM_CONFIG_PACKETS];
        FalconV5PacketInfo dynamicPacketInfo1;
        FalconV5PacketInfo dynamicPacketInfo2;
        FalconV5PacketInfo* dynamicPacketInfo = nullptr;
        int curV5ConfigPacket = 0;
    } m_pru0, m_pru1;

    std::vector<PixelString*> m_strings;

    uint32_t m_curFrame = 0;
    uint32_t m_licensedOutputs = 0;

    int m_testCycle = -1;
    int m_testType = 0;
    float m_testPercent = 0.0f;

    FalconV5Support* falconV5Support = nullptr;

    void prepData(FrameData& d, unsigned char* channelData);
    void sendData(FrameData& d);
    void bitFlipData(uint8_t* stringChannelData, uint8_t* bitSwapped, size_t len);

    void createOutputLengths(FrameData& d, const std::string& pfx);

    void setupFalconV5Support(const Json::Value& root, uint8_t* memLoc);
    void encodeFalconV5Packet(std::vector<std::array<uint8_t, 64>>& packets, uint8_t* memLocPru0, uint8_t* memLocPru1);
};
