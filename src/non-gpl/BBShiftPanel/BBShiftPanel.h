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
#include <queue>
#include "fpp-json-fwd.h"
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "channeloutput/ChannelOutput.h"
#include "util/BBBPruUtils.h"

#include "../../channeloutput/Matrix.h"
#include "../../channeloutput/PanelMatrix.h"

// structure of the data at the start of the PRU ram
// that the pru program expects to see
typedef struct {
    // in the DDR shared with the PRU
    uint32_t address_dma;

    union {
        struct {
            volatile uint16_t command;
            volatile uint16_t result;
        } __attribute__((__packed__));
        struct {
            // Standard shift register based panels where the data is shifted out row by row and the PWM
            // is handled by the PRU code via the OE pin
            // write data length to start, 0xFFFF to abort. will be cleared when started
            uint16_t pixelsPerStride;
            uint16_t numStrides;

            // 2 uint32_t for each stride
            // first uint32_t is the brightness, number of clock ticks for on
            // second uint32_t - bit 32 is flag to output black after this row,  bits 25-31 is the address, lower 24 is extra "off" time
            // (24 slots x 32 rows: enough split-pulse budget for 12 bit
            // color on 1/32 scan panels; ends at 0x1808 in the PRU data
            // RAM, clear of the config areas at 0x1DF8+)
            uint32_t brightness[24 * 32 * 2];
        } __attribute__((__packed__));
        struct {
            // Panels that handle the PWM themselves, all data is shifted out and the panel displays it automatically
            uint16_t cmd;
            uint8_t numBlocks;
            uint8_t numRows;
            uint16_t buffer[4];            // buffer to get registers aligned on boundary
            // up to 6 registers (SM16380SH takes a sixth slot; the rest use
            // five), up to 12 bytes per clock (16 outputs), 16 clocks each.
            // Ends at 0x490 in the PRU data RAM, clear of the chip config at
            // PWM_CHIP_CONFIG_OFFSET (0x1DF8).
            uint8_t registers[6 * 12 * 16];
        } __attribute__((__packed__));
    } __attribute__((__packed__));
} __attribute__((__packed__)) BBShiftPanelData;

class BBShiftPanelOutput;

// The PRUSS drives every panel output on the cape as ONE frame: a single
// buffer whose byte lanes are the cape's data pins, one stride schedule, one
// shared memory ring, one pair of PRU cores.  So several LED Panel Matrix
// outputs on the same cape (a two sided display, a column either side of a
// door) cannot each own that hardware - they share it here and own only their
// own pixel mapping into the shared frame.  Since the lane for a given output
// is fixed by the cape pinout (see BBShiftPanelOutput::buildScatterMap),
// members driving different outputs scatter into disjoint bytes of the shared
// buffer and one bit-plane pass converts the combined result.
class BBShiftPanelManager {
public:
    static BBShiftPanelManager INSTANCE;

    // Registers a configured output.  The first member fixes the frame
    // geometry; later ones must match it and must not re-use an output that
    // is already driven.  Returns false (having warned) if they do not.
    bool addMember(BBShiftPanelOutput* m, const Json::Value& config, const Json::Value& capeConfig);
    void removeMember(BBShiftPanelOutput* m);

    // One frame: every member scatters into the shared intermediate, then the
    // last one to arrive triggers the single bit-plane pass, and the first
    // SendData of the frame hands the result to the pump.
    void prepMember(BBShiftPanelOutput* m, unsigned char* channelData);
    void publishFrame();

    void startingOutput();
    void stoppingOutput();
    void closeMember();

    void dumpConfig();

    bool isPWMPanel() const;
    uint32_t numOutputSlots() const { return m_numOutputSlots; }
    uint32_t numOutputRows() const { return numRows; }
    uint32_t outputRowLen() const { return rowLen; }
    int longestChain() const { return m_longestChain; }
    int numOutputs() const { return m_numOutputs; }
    int colorDepth() const { return m_colorDepth; }
    int brightness() const { return m_brightness; }
    uint8_t outputPinFor(int o) const { return outputPin[o]; }
    uint8_t outputBankFor(int o) const { return outputBank[o]; }
    uint16_t* intermediate() const { return currentChannelData; }
    uint32_t intermediateSize() const { return rowLen * m_numOutputSlots * 6 * numRows; }

    // the shared worker pool; members use it for their scatter pass
    void processTasks(std::atomic<int>& counter);

private:
    // the frame-shape settings every matrix on the cape has to agree on
    struct PanelParams {
        int panelWidth = 0;
        int panelHeight = 0;
        int panelScan = 0;
        int colorDepth = 12;
        int addressingMode = 0;
        int panelType = 0;
        int dataLayout = 0;
        bool pwmDirectRow = false;
        bool outputByRow = false;
        bool outputBlankData = false;
        bool sharedPRUSS = false;
        std::string panelInterleave;
    };
    static PanelParams parsePanelParams(const Json::Value& config, const Json::Value& capeConfig);

    bool parseCapeConfig(const Json::Value& capeConfig);
    bool adoptPanelParams(const PanelParams& p);
    bool checkCompatible(const PanelParams& p) const;
    bool rebuild();
    void teardownHardware();
    void teardown();
    void computeGeometry();

    void PrepDataShift();
    void PrepDataPWM();

    void StopPRU(bool wait = true);
    int StartPRU();

    void buildStrideSchedule();
    void sendPanelInitPackets();
    uint32_t computeMaxBrightnessCycles();
    void setupBrightnessValues();
    void setupPWMRegisters();
    void setupGCLKConfig();
    void writeFM6373SeqWord(int idx);
    void writeDP3364SeqWord(int idx);

    std::vector<BBShiftPanelOutput*> m_members;
    // members that have arrived in this frame's PrepData; the shared bit-plane
    // pass runs when the count reaches the member count
    int m_preppedThisFrame = 0;
    // set by the bit-plane pass, cleared by the frame publish, so exactly one
    // member's SendData hands the frame over
    bool m_framePrepped = false;
    // balanced Starting/StoppingOutput calls across the members
    int m_activeMembers = 0;
    int m_openMembers = 0;

    BBBPru* pru = nullptr;
    BBBPru* pwmPru = nullptr;
    BBShiftPanelData* pruData = nullptr;

    int m_panelWidth = 0;
    int m_panelHeight = 0;
    int m_panelScan = 0;
    std::string m_panelInterleave = "";

    int m_addressingMode = 0;
    // 0 = standard, 1 = FM6126A, 2 = FM6127 - panels that need their config
    // registers clocked out before they display (see sendPanelInitPackets)
    int m_panelType = 0;
    // How the framebuffer is routed onto the two RGB lanes.  0 is the normal
    // HUB75 arrangement: the lanes carry the top and bottom halves of the
    // panel and each addressed row lights two physical rows.  A full height
    // panel (scan == height) addresses every row on its own instead, so its
    // lanes carry the LEFT and RIGHT halves of one row - 1 and 2 select which
    // way round.  See buildScatterMap.
    int m_dataLayout = 0;
    // PWM panels: drive the row lines with a direct binary row number
    // instead of the DP32020A style row shift register
    bool m_pwmDirectRow = false;
    // FM6373: next config sequence word for the per-frame rotating refresh
    int m_pwmSeqIdx = 0;
    // the pins this cape muxed to the PRU (only these are released on
    // Close - a combo cape may leave pins for another driver to own)
    std::vector<std::string> m_configuredPins;
    std::string m_oePin = "P1-36";
    // sharing the PRUSS with a string driver on the other PRU: single PRU,
    // SPLIT1 ring, and never clear the shared RAM or the other PRU's memory
    bool m_sharedPRUSS = false;
    int m_longestChain = 0;
    // the OE on-time is a property of the cape, not of one matrix, so the
    // hardware brightness is the highest any member asked for; a member that
    // wanted less scales its own gamma table down to reach it (see
    // BBShiftPanelOutput::setupGamma)
    int m_brightness = 10;
    int m_colorDepth = 12;

    bool m_outputByRow = false;
    bool m_outputBlankData = false;

    int m_numOutputs = 8;      // number of configured outputs (from cape config)
    int m_numOutputSlots = 8;  // 8 or 16 - determines data width per pixel (6 or 12 bytes)
    uint8_t outputPin[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7 };  // PRU pin (0-7) per output
    uint8_t outputBank[16] = { 0 };  // 0=first bank, 1=second bank per output

    // the shared 16 bit intermediate every member scatters into
    uint16_t* currentChannelData = nullptr;

    // Four output buffers allows for double buffering and extras for the PRU to read from while the next frame is being built
    // With 4MB of memory reserved for transfer to PRU, 12 P5 panels per output with 12bit color depth (total 3.5MB)
    constexpr static int NUM_OUTPUT_BUFFERS = 4;
    std::array<uint8_t*, NUM_OUTPUT_BUFFERS> outputBuffers = { nullptr, nullptr, nullptr, nullptr };
    uint8_t currOutputBuffer = 0;
    uint32_t numRows = 0;
    uint32_t rowLen = 0;
    uint32_t maxRowLen = 0;

    // Schedule of strides within one pass of all rows.  Bits whose on-time is
    // a multiple of the stride shift-out time are split into multiple shorter
    // pulses spread across the frame which multiplies the perceived refresh
    // rate without changing the total light output (same total on-time).
    struct StrideSlot {
        uint8_t bit;     // color depth bit this slot displays
        bool primary;    // first slot for this bit; the pixel mapping writes here
        uint32_t onTime; // PRU cycles the display is on for this slot
    };
    std::vector<StrideSlot> m_strideSchedule;
    // per row: (dstOffset, srcOffset) frame buffer copies for the duplicated
    // strides of split bits
    std::vector<std::vector<std::pair<uint32_t, uint32_t>>> m_dupCopies;

    // Real-time thread that streams the current frame buffer into a ring in
    // the 32KB PRU shared memory.  The PRU cannot read DDR fast enough to
    // keep 16 outputs fed (~1.1-1.8us per 64 byte read) but reads shared
    // memory in 18 predictable cycles.  See runPumpThread() and
    // pru/SMEMRing.hp for the layout.
    std::thread m_pumpThread;
    std::atomic<bool> m_pumpRunning{ false };
    // Set while the channel output thread is idle.  Shift panels only hold an
    // image for as long as the pump keeps re-shifting it, so a panel that is
    // not being output to is simply dark - there is nothing to refresh and no
    // reason to spend a core streaming a blank frame.  The pump acts on this
    // only at a frame boundary; see runPumpThread.
    std::atomic<bool> m_pumpPaused{ false };
    // owned by the pump thread, read by StoppingOutput to wait for the park
    std::atomic<bool> m_pumpParked{ false };
    // set once both cores have been halted, so the park is not attempted or
    // undone twice
    bool m_prusPaused = false;
    void ParkPRUs();
    void UnparkPRUs();
    std::atomic<uint8_t*> m_frontBuffer{ nullptr };
    // PWM panels send one frame per DATA command; the pump streams once per
    // sequence bump so the byte flow stays in step with the commands
    std::atomic<uint32_t> m_pumpSeq{ 0 };
    uint32_t m_pumpedSeq = 0;
    // A self-PWM panel refreshes from its own engine while its row scan free
    // runs, so a gap in the shift out shows up as the image drifting
    // vertically rather than as a glitch.  The frame is therefore not handed
    // to the PRU until the pump has the ring as full as it will go, and the
    // pump is woken directly rather than found by polling.
    std::mutex m_pumpMutex;
    std::condition_variable m_pumpCV;
    std::atomic<uint32_t> m_pumpPrimed{ 0 };
    uint32_t m_frameBytes = 0;
    BBBPruSMEMRing m_ring;
    bool m_heapBuffers = false;
    void runPumpThread();

    bool singlePRU = false;
    std::string m_refreshWarning;

    std::queue<std::function<void()>> bgTasks;
    volatile bool bgThreadsRunning = false;
    std::mutex bgTaskMutex;
    std::condition_variable bgTaskCondVar;
    std::atomic<int> bgThreadCount;
    void runBackgroundTasks();
    void startBackgroundThreads();
    void stopBackgroundThreads();

    friend class BBShiftPanelOutput;
};

class BBShiftPanelOutput : public ChannelOutput {
public:
    BBShiftPanelOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~BBShiftPanelOutput();

    virtual std::string GetOutputType() const {
        return "BB64 Panels";
    }

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual int SendData(unsigned char* channelData) override;
    virtual void PrepData(unsigned char* channelData) override;
    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

    virtual void OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) override;
    virtual bool SupportsTesting() const { return true; }

    virtual void StartingOutput() override;
    virtual void StoppingOutput() override;

    // called by the manager
    int longestChain() const { return m_longestChain; }
    const PanelMatrix* panelMatrix() const { return m_panelMatrix; }
    bool usesOutput(int o) const;
    int configuredBrightness() const { return m_brightness; }
    // (re)builds this member's map into the shared frame; the manager calls
    // this whenever the shared geometry changes
    bool buildScatterMap();
    void setupGamma();
    void scatterFrame(unsigned char* channelData);

private:
    void releaseToManager();

    Matrix* m_matrix = nullptr;
    PanelMatrix* m_panelMatrix = nullptr;

    int m_longestChain = 0;
    int m_invertedData = 0;
    int m_brightness = 10;
    float m_gamma = 2.2;

    FPPColorOrder m_colorOrder;

    int m_panels = 0;
    int m_width = 0;
    int m_height = 0;

    uint16_t gammaCurve[256];

    // The gamma scatter map sorted by destination offset so the prep loop
    // writes sequentially (streaming writes with random byte reads are much
    // faster than random writes at panel counts where the data far exceeds
    // the cache).  Unmapped channels are excluded entirely.
    std::vector<uint32_t> m_scatterOffsets;
    std::vector<uint32_t> m_scatterSrc;

    std::string m_autoCreatedModelName;

    // set once the manager has accepted this matrix; an output whose Init
    // failed must never reach the manager's per-frame accounting
    bool m_registered = false;
    // the manager releases the cape's pins when its last open member closes,
    // so each member has to hand its share back exactly once - Close() may be
    // called without a destructor following, or the object may be destroyed
    // without Close() ever having run
    bool m_closed = false;

    // guards against the output being torn down (config reload) while the
    // channel output thread is still inside PrepData/SendData
    std::atomic<bool> m_stopping{ false };
    std::atomic<int> m_inFlight{ 0 };

    friend class BBShiftPanelManager;
};
