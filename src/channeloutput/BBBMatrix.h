#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "fpp-json-fwd.h"
#include <string>
#include <vector>

#include "Matrix.h"
#include "PanelMatrix.h"
#include "util/BBBPruUtils.h"

#include "ChannelOutput.h"

// 16 rows (1/16 scan) * 8bits per row
#define MAX_STATS 16 * 8

class PanelInterleaveHandler;

// structure of the data at the start of the PRU ram
// that the pru program expects to see
typedef struct {
    // in the DDR shared with the PRU
    volatile uint32_t address_dma;

    // write 1 to start, 0xFF to abort. will be cleared when started
    volatile uint32_t command;
    volatile uint32_t response;

    volatile uint16_t pwmBrightness[8];
    uint32_t stats[3 * MAX_STATS]; // 3 values per collection
} __attribute__((__packed__)) BBBPruMatrixData;

class BBBMatrix : public ChannelOutput {
public:
    BBBMatrix(unsigned int startChannel, unsigned int channelCount);
    virtual ~BBBMatrix();

    virtual std::string GetOutputType() const override {
        return "BBB Panels";
    }

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual void PrepData(unsigned char* channelData) override;

    virtual int SendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

    virtual void OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) override;
    virtual bool SupportsTesting() const override { return true; }

private:
    void calcBrightnessFlags(std::vector<std::string>& sargs);
    void printStats();
    bool configureControlPin(const std::string& ctype, Json::Value& root, std::ofstream& outputFile, int pru, int& controlGPIO);
    void configurePanelPins(int x, Json::Value& root, std::ofstream& outputFile, int* minPort);
    void configurePanelPin(int x, const std::string& color, int row, Json::Value& root, std::ofstream& outputFile, int* minPort);

    BBBPru* m_pru;
    BBBPru* m_pruCopy;
    BBBPruMatrixData* m_pruData;
    Matrix* m_matrix;
    PanelMatrix* m_panelMatrix;
    bool m_singlePRU = false;
    // dual PRU split output: the copy PRU also writes these GPIO banks
    bool m_splitOutput = false;
    bool m_splitCpyBanks[4] = { false, false, false, false };
    size_t m_dataOffset = 0;

    int m_panelWidth;
    int m_panelHeight;
    int m_panelScan;
    std::string m_panelInterleave;

    int m_outputs;
    int m_longestChain;
    int m_invertedData;
    int m_brightness;
    int m_colorDepth;
    bool m_printStats;
    FPPColorOrder m_colorOrder;

    uint32_t* m_gpioFrame;
    int m_panels;
    int m_rows;
    int m_width;
    int m_height;
    int m_rowSize;

    bool m_outputByRow;
    bool m_outputBlankData;
    std::vector<int> m_bitOrder;

    int m_timing;
    PanelInterleaveHandler* m_handler;

    uint32_t brightnessValues[12];
    uint32_t delayValues[12];
    uint16_t gammaCurve[256];

    std::string m_autoCreatedModelName;

    // The firmware has pin definitions for 8 outputs, and every per-output
    // table here is sized for that; a panel on a later output is refused.
    static constexpr int MAX_OUTPUTS = 8;

    class GPIOPinInfo {
    public:
        class Pins {
        public:
            uint32_t r_pin = 0;
            uint32_t g_pin = 0;
            uint32_t b_pin = 0;

            uint8_t r_gpio = 0;
            uint8_t g_gpio = 0;
            uint8_t b_gpio = 0;
        } row[2];
    } m_pinInfo[MAX_OUTPUTS];

    uint8_t* m_frames[8];
    int m_curFrame;
    int m_numFrames;
    int m_fullFrameLen;

    // Precomputed acceleration structures for PrepData (see buildPrepTables):
    // per output, the 4 GPIO bank words for each combination of the 6 pins
    uint32_t m_pinLUT[MAX_OUTPUTS][64][4];
    // bit-spread gamma: bit b of gammaCurve[v] lands in byte b (low 8 bits)
    // or byte b-8 (bits 8-11) so all bit-plane indexes compute per pixel
    uint64_t m_gammaSpreadLo[256];
    uint32_t m_gammaSpreadHi[256];
    // (output, panel) pairs grouped by chain position
    std::vector<std::vector<std::pair<int, int>>> m_chainPanels;
    // interleave handler results precomputed per panel row/pixel
    std::vector<int> m_yOutMap;
    std::vector<uint16_t> m_xOutMap;
    // staging for the packed per-bitplane pin indexes
    std::vector<uint8_t> m_stageLo;
    std::vector<uint8_t> m_stageHi;
    // set when the interleave map is not a clean bijection and PrepData must
    // memset the frame and OR into it (the historical behavior)
    bool m_prepNeedsMemset = false;
    void buildPrepTables();
};
