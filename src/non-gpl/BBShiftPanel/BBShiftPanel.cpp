/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
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

#include <sys/mman.h>
#include <sys/wait.h>
#include <tuple>
#include <unistd.h>

#include <chrono>

// FPP includes
#include "../../Sequence.h"
#include "../../Warnings.h"
#include "../../common.h"
#include "../../log.h"

#include "BBShiftPanel.h"
#include "../CapeUtils/CapeUtils.h"
#include "util/BBBUtils.h"

#include "MapPixelsByDepth16.h"
#include "overlays/PixelOverlay.h"

#include "Plugin.h"
class BBShiftPanelPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    BBShiftPanelPlugin() :
        FPPPlugins::Plugin("BBShiftPanel") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new BBShiftPanelOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new BBShiftPanelPlugin();
}
}

constexpr int ADDRESSING_MODE_STANDARD = 0;
constexpr int ADDRESSING_MODE_DIRECT = 1;
constexpr int ADDRESSING_MODE_FM6353C = 5;
constexpr int ADDRESSING_MODE_FM6363C = 6;

static std::map<int, std::vector<int>> BIT_ORDERS = {
    //{ 6, { 5, 4, 3, 2, 1, 0 } },
    { 6, { 5, 2, 1, 4, 3, 0 } },
    { 7, { 6, 2, 1, 4, 5, 3, 0 } },
    { 8, { 7, 3, 5, 1, 2, 6, 4, 0 } },
    //{ 8, { 7, 6, 5, 4, 3, 2, 1, 0 } },
    { 9, { 8, 3, 5, 1, 7, 2, 6, 4, 0 } },
    { 10, { 9, 4, 1, 6, 3, 8, 2, 7, 5, 0 } },
    { 11, { 10, 4, 7, 2, 3, 1, 6, 9, 8, 5, 0 } },
    //{ 12, { 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 } }
    { 12, { 11, 5, 8, 2, 4, 1, 7, 10, 3, 9, 6, 0 } }
};

class InterleaveHandler {
protected:
    InterleaveHandler() {}

public:
    virtual ~InterleaveHandler() {}

    virtual void mapRow(int& y) = 0;
    virtual void mapCol(int y, int& x) = 0;

private:
};

class NoInterleaveHandler : public InterleaveHandler {
public:
    NoInterleaveHandler() {}
    virtual ~NoInterleaveHandler() {}

    virtual void mapRow(int& y) override {}
    virtual void mapCol(int y, int& x) override {}
};
class SimpleInterleaveHandler : public InterleaveHandler {
public:
    SimpleInterleaveHandler(int interleave, int ph, int pw, int ps, bool flip) :
        InterleaveHandler(),
        m_interleave(interleave),
        m_panelHeight(ph),
        m_panelWidth(pw),
        m_panelScan(ps),
        m_flipRows(flip) {}
    virtual ~SimpleInterleaveHandler() {}

    virtual void mapRow(int& y) override {
        while (y >= m_panelScan) {
            y -= m_panelScan;
        }
    }
    virtual void mapCol(int y, int& x) override {
        int whichInt = x / m_interleave;
        if (m_flipRows) {
            if (y & m_panelScan) {
                y &= !m_panelScan;
            } else {
                y |= m_panelScan;
            }
        }
        int offInInt = x % m_interleave;
        int mult = (m_panelHeight / m_panelScan / 2) - 1 - y / m_panelScan;
        x = m_interleave * (whichInt * m_panelHeight / m_panelScan / 2 + mult) + offInInt;
    }

private:
    const int m_interleave;
    const int m_panelWidth;
    const int m_panelHeight;
    const int m_panelScan;
    const bool m_flipRows;
};

class ZigZagClusterInterleaveHandler : public InterleaveHandler {
public:
    ZigZagClusterInterleaveHandler(int interleave, int ph, int pw, int ps) :
        InterleaveHandler(),
        m_interleave(interleave),
        m_panelHeight(ph),
        m_panelWidth(pw),
        m_panelScan(ps) {}
    virtual ~ZigZagClusterInterleaveHandler() {}

    virtual void mapRow(int& y) override {
        while (y >= m_panelScan) {
            y -= m_panelScan;
        }
    }
    virtual void mapCol(int y, int& x) override {
        int whichInt = x / m_interleave;
        int offInInt = x % m_interleave;
        int mult = y / m_panelScan;

        if (m_panelScan == 2) {
            // AC_MAPPING code - stealing codepath for scan 1/2 and zigzag8
            // cluster -- the ordinal number of a group of 8 linear lights on a half-frame
            // starting in the top left corner of a half-module; bits 3,2 from y, 1,0 from x
            int tc = whichInt | (y << 1) & 0xc;
            // mapped cluster - this reverses the effects of unusual wiring on this panel
            // address bits are shifted around and bit3 is negated to achieve linear counting
            uint8_t map_cb = (~tc & 8) | (tc & 4) >> 2 | (tc & 2) << 1 | (tc & 1) << 1;
            // scale up from cluster to pixel counts and account for reverse-running clusters
            x = map_cb * 8 + (x & 0x7 ^ (((~y >> 1) & 1) * 7));
            return;
        } else if (m_interleave == 4) {
            if ((whichInt & 0x1) == 1) {
                mult = (y < m_panelScan ? y + m_panelScan : y - m_panelScan) / m_panelScan;
            }
        } else {
            int tmp = (y * 2) / m_panelScan;
            if ((tmp & 0x2) == 0) {
                offInInt = m_interleave - 1 - offInInt;
            }
        }
        x = m_interleave * (whichInt * m_panelHeight / m_panelScan / 2 + mult) + offInInt;
    }

private:
    const int m_interleave;
    const int m_panelWidth;
    const int m_panelHeight;
    const int m_panelScan;
};

class StripeClusterInterleaveHandler : public InterleaveHandler {
public:
    static constexpr int MAPPING[8][4] = {
        { 32, 48, 96, 112 },
        { 32, 48, 96, 112 },
        { 40, 56, 104, 120 },
        { 40, 56, 104, 120 },
        { 0, 16, 64, 80 },
        { 0, 16, 64, 80 },
        { 8, 24, 72, 88 },
        { 8, 24, 72, 88 }
    };

    StripeClusterInterleaveHandler(int interleave, int ph, int pw, int ps) :
        InterleaveHandler(),
        m_interleave(interleave),
        m_panelHeight(ph),
        m_panelWidth(pw),
        m_panelScan(ps) {}
    virtual ~StripeClusterInterleaveHandler() {}

    virtual void mapRow(int& y) override {
        while (y >= m_panelScan) {
            y -= m_panelScan;
        }
    }
    virtual void mapCol(int y, int& x) override {
        int whichInt = x / m_interleave;
        int offInInt = x % m_interleave;
        x = MAPPING[y % 8][whichInt % 4] + offInInt;
    }

private:
    const int m_interleave;
    const int m_panelWidth;
    const int m_panelHeight;
    const int m_panelScan;
};

class ZigZagInterleaveHandler : public InterleaveHandler {
public:
    ZigZagInterleaveHandler(int interleave, int ph, int pw, int ps) :
        InterleaveHandler(),
        m_interleave(interleave),
        m_panelHeight(ph),
        m_panelWidth(pw),
        m_panelScan(ps) {}
    virtual ~ZigZagInterleaveHandler() {}

    virtual void mapRow(int& y) override {
        while (y >= m_panelScan) {
            y -= m_panelScan;
        }
    }
    virtual void mapCol(int y, int& x) override {
        int whichInt = x / m_interleave;
        int offInInt = x % m_interleave;
        int mult = y / m_panelScan;

        if (m_panelScan == 2) {
            if ((y & 0x2) == 0) {
                offInInt = m_interleave - 1 - offInInt;
            }
        } else if (m_interleave == 4) {
            if ((whichInt & 0x1) == 1) {
                mult = (y < m_panelScan ? y + m_panelScan : y - m_panelScan) / m_panelScan;
            }
        } else {
            int tmp = (y * 2) / m_panelScan;
            if ((tmp & 0x2) == 0) {
                offInInt = m_interleave - 1 - offInInt;
            }
        }
        x = m_interleave * (whichInt * m_panelHeight / m_panelScan / 2 + mult) + offInInt;
    }

private:
    const int m_interleave;
    const int m_panelWidth;
    const int m_panelHeight;
    const int m_panelScan;
};

static const std::vector<std::string> PRU_PINS = { "P1-20", "P1-29", "P1-31", "P1-33", "P1-36",
                                                   "P2-02", "P2-04", "P2-06", "P2-08",
                                                   "P2-18", "P2-20", "P2-22", "P2-24",
                                                   "P2-28", "P2-30", "P2-32", "P2-33",
                                                   "P2-34", "P2-35" };

constexpr int NUM_OUTPUTS = 8;

BBShiftPanelOutput::BBShiftPanelOutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount) {
    LogDebug(VB_CHANNELOUT, "BBShiftPanelOutput::BBShiftPanelOutput(%u, %u)\n",
             startChannel, channelCount);
}

BBShiftPanelOutput::~BBShiftPanelOutput() {
    LogDebug(VB_CHANNELOUT, "BBShiftPanelOutput::~BBShiftPanelOutput()\n");

    bgThreadsRunning = false;
    bgTaskCondVar.notify_all();
    while (bgThreadCount > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (channelOffsets)
        delete[] channelOffsets;
    if (currentChannelData)
        delete[] currentChannelData;

    if (pru)
        delete pru;
    if (m_matrix)
        delete m_matrix;
    if (m_panelMatrix)
        delete m_panelMatrix;
}

bool BBShiftPanelOutput::isPWMPanel() {
    if (m_addressingMode == ADDRESSING_MODE_FM6353C ||
        m_addressingMode == ADDRESSING_MODE_FM6363C) {
        return true;
    }
    return false;
}

int BBShiftPanelOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "BBShiftPanelOutput::Init(JSON)\n");

    for (auto& pinName : PRU_PINS) {
        const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
        pin.configPin("pru1out", true);
    }

    Json::Value root;
    std::string subType = config["configName"].asString();
    if (!CapeUtils::INSTANCE.getPanelConfig(subType, root) || CapeUtils::INSTANCE.getLicensedOutputs() < 1) {
        LogErr(VB_CHANNELOUT, "Could not read panel pin configuration for %s\n", subType.c_str());
        return 0;
    }
    for (int i = 0; i < root["outputs"].size(); i++) {
        Json::Value s = root["outputs"][i];
        std::string pinName = s["pin"].asString();
        const BBBPinCapabilities* pin = (const BBBPinCapabilities*)(PinCapabilities::getPinByName(pinName).ptr());
        outputMap[i] = pin->pruPin(1);
    }
    if (root.isMember("singlePRU")) {
        singlePRU = root["singlePRU"].asBool();
    }
    // singlePRU = true;
    if (!singlePRU) {
        // if not using a single PRU, then we need to change the OE pin to the other PRU
        const PinCapabilities& pin = PinCapabilities::getPinByName("P1-36");
        pin.configPin("pru0out", true);
    }
    m_panelWidth = config["panelWidth"].asInt();
    m_panelHeight = config["panelHeight"].asInt();
    if (!m_panelWidth) {
        m_panelWidth = 32;
    }
    if (!m_panelHeight) {
        m_panelHeight = 16;
    }

    m_addressingMode = config["panelAddressing"].asInt();

    m_invertedData = config["invertedData"].asInt();
    m_colorOrder = ColorOrderFromString(config["colorOrder"].asString());

    m_panelMatrix = new PanelMatrix(m_panelWidth, m_panelHeight, m_invertedData);
    if (!m_panelMatrix) {
        LogErr(VB_CHANNELOUT, "BBShiftPanelOutput: Unable to create PanelMatrix\n");
        return 0;
    }
    bool usesOutput[16] = {
        false, false, false, false,
        false, false, false, false
    };
    for (int i = 0; i < config["panels"].size(); i++) {
        Json::Value p = config["panels"][i];
        char orientation = 'N';
        const char* o = p["orientation"].asString().c_str();

        if (o && *o) {
            orientation = o[0];
        }

        if (p["colorOrder"].asString() == "") {
            p["colorOrder"] = ColorOrderToString(m_colorOrder);
        }

        m_panelMatrix->AddPanel(p["outputNumber"].asInt(),
                                p["panelNumber"].asInt(),
                                orientation,
                                p["xOffset"].asInt(), p["yOffset"].asInt(),
                                ColorOrderFromString(p["colorOrder"].asString()));

        usesOutput[p["outputNumber"].asInt()] = true;
        if (p["panelNumber"].asInt() > m_longestChain) {
            m_longestChain = p["panelNumber"].asInt();
        }
    }
    m_longestChain++;

    // get the dimensions of the matrix
    m_panels = m_panelMatrix->PanelCount();
    m_width = m_panelMatrix->Width();
    m_height = m_panelMatrix->Height();

    if (config.isMember("brightness")) {
        m_brightness = config["brightness"].asInt();
    }
    if (m_brightness < 1 || m_brightness > 10) {
        m_brightness = 10;
    }
    if (config.isMember("panelColorDepth")) {
        m_colorDepth = config["panelColorDepth"].asInt();
    }
    if (config.isMember("panelOutputOrder")) {
        m_outputByRow = config["panelOutputOrder"].asBool();
    }
    if (config.isMember("panelOutputBlankRow")) {
        m_outputBlankData = config["panelOutputBlankRow"].asBool();
    }
    if (m_colorDepth < 0) {
        m_colorDepth = -m_colorDepth;
        m_outputBlankData = true;
    }
    if (m_colorDepth > 12 || m_colorDepth < 6) {
        m_colorDepth = 8;
    }

    float gamma = 2.2;
    if (config.isMember("gamma")) {
        gamma = atof(config["gamma"].asString().c_str());
    }
    setupGamma(gamma);

    if (config.isMember("panelInterleave")) {
        if (config["panelInterleave"].asString() == "8z") {
            m_interleave = 8;
            zigZagInterleave = true;
        } else if (config["panelInterleave"].asString() == "16z") {
            m_interleave = 16;
            zigZagInterleave = true;
        } else if (config["panelInterleave"].asString() == "4z") {
            m_interleave = 4;
            zigZagInterleave = true;
        } else if (config["panelInterleave"].asString() == "8f") {
            m_interleave = 8;
            flipRows = true;
        } else if (config["panelInterleave"].asString() == "16f") {
            m_interleave = 16;
            flipRows = true;
        } else if (config["panelInterleave"].asString() == "32f") {
            m_interleave = 32;
            flipRows = true;
        } else if (config["panelInterleave"].asString() == "64f") {
            m_interleave = 64;
            flipRows = true;
        } else if (config["panelInterleave"].asString() == "80f") {
            m_interleave = 80;
            flipRows = true;
        } else if (config["panelInterleave"].asString() == "8c") {
            m_interleave = 8;
            zigZagClusterInterleave = true;
        } else if (config["panelInterleave"].asString() == "8s") {
            m_interleave = 8;
            stripeInterleave = true;
        } else {
            m_interleave = std::atoi(config["panelInterleave"].asString().c_str());
        }
    } else {
        m_interleave = 0;
    }
    m_panelScan = config["panelScan"].asInt();
    // printf("Interleave: %d     Scan: %d    ZZI: %d    ZZCI:  %d    SI: %d\n", m_interleave, m_panelScan, zigZagInterleave, zigZagClusterInterleave, stripeInterleave);
    if (m_panelScan == 0) {
        // 1/8 scan by default
        m_panelScan = 8;
    }
    if (((m_panelScan * 2) != m_panelHeight) && m_interleave == 0) {
        m_interleave = 8;
    }

    m_channelCount = m_width * m_height * 3;

    m_matrix = new Matrix(m_startChannel, m_width, m_height);
    if (config.isMember("subMatrices")) {
        for (int i = 0; i < config["subMatrices"].size(); i++) {
            Json::Value sm = config["subMatrices"][i];

            m_matrix->AddSubMatrix(
                sm["enabled"].asInt(),
                sm["startChannel"].asInt() - 1,
                sm["width"].asInt(),
                sm["height"].asInt(),
                sm["xOffset"].asInt(),
                sm["yOffset"].asInt());
        }
    }
    setupChannelOffsets();
    if (StartPRU() == 0) {
        return 0;
    }
    if (isPWMPanel()) {
        setupPWMRegisters();
    } else {
        setupBrightnessValues();
    }

    if (PixelOverlayManager::INSTANCE.isAutoCreatePixelOverlayModels()) {
        std::string dd = "LED Panels";
        if (config.isMember("description")) {
            dd = config["description"].asString();
        }
        std::string desc = dd;
        int count = 0;
        while (PixelOverlayManager::INSTANCE.getModel(desc) != nullptr) {
            count++;
            desc = dd + "-" + std::to_string(count);
        }
        PixelOverlayManager::INSTANCE.addAutoOverlayModel(desc,
                                                          m_startChannel, m_channelCount, 3,
                                                          "H", m_invertedData ? "BL" : "TL",
                                                          m_height, 1);
    }

    bgThreadsRunning = true;
    for (int i = 0; i < std::thread::hardware_concurrency(); i++) {
        std::thread th(&BBShiftPanelOutput::runBackgroundTasks, this);
        th.detach();
    }
    return ChannelOutput::Init(config);
}

int BBShiftPanelOutput::StartPRU() {
    pru = new BBBPru(1, true, true);
    pruData = (BBShiftPanelData*)pru->data_ram;
    if (isPWMPanel()) {
    } else {
        if (singlePRU) {
            pru->run("/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel_single.out");
        } else {
            pru->run("/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel.out");
            pwmPru = new BBBPru(0);
            pwmPru->run("/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel_oe.out");
        }
    }
    uint32_t strideLen = rowLen * 6;
    uint32_t numStride = numRows * m_colorDepth;
    uint32_t oframeSize = numStride * strideLen;
    // round up to a page boundary
    uint32_t frameSize = oframeSize + 8192;
    frameSize &= 0xFFFFFF000;
    uintptr_t addr = (uintptr_t)pru->ddr;
    uintptr_t maxaddr = addr + pru->ddr_size;
    outputBuffers[0] = pru->ddr;
    addr += frameSize;
    int n = 1;
    for (int x = 1; x < NUM_OUTPUT_BUFFERS; x++) {
        addr += frameSize;
        if (addr < maxaddr) {
            outputBuffers[x] = outputBuffers[x - 1] + frameSize;
            n++;
        }
    }
    for (int x = 0; x < NUM_OUTPUT_BUFFERS; x++) {
        pru->clearPRUMem(outputBuffers[x], frameSize);
    }
    if (isPWMPanel()) {
    } else {
        pruData->numStrides = numStride;
    }
    return 1;
}
void BBShiftPanelOutput::StopPRU(bool wait) {
    // Send the stop command
    if (pru) {
        pruData->command = 0xFFFF;
        pruData->result = 0xFFFF;
    }
    __asm__ __volatile__("" ::
                             : "memory");

    if (pru) {
        int cnt = 0;
        while (wait && cnt < 25 && pruData->result == 0xFFFF) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            cnt++;
            __asm__ __volatile__("" ::
                                     : "memory");
        }
        pru->stop();
        delete pru;
        pru = nullptr;

        if (pwmPru) {
            pwmPru->stop();
            delete pwmPru;
            pwmPru = nullptr;
        }
    }
}

int BBShiftPanelOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "BBShiftPanelOutput::Close()\n");
    StopPRU();
    for (auto& pinName : PRU_PINS) {
        const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
        pin.configPin("default", false);
    }
    bgThreadsRunning = false;
    bgTaskCondVar.notify_all();
    return ChannelOutput::Close();
}

void BBShiftPanelOutput::runBackgroundTasks() {
    ++bgThreadCount;
    while (bgThreadsRunning) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(bgTaskMutex);
            bgTaskCondVar.wait_for(lock, std::chrono::milliseconds(100), [&]() { return !bgTasks.empty(); });
            if (!bgTasks.empty()) {
                task = bgTasks.front();
                bgTasks.pop();
            }
        }
        if (task) {
            task();
        }
    }
    --bgThreadCount;
}

void BBShiftPanelOutput::processTasks(std::atomic<int>& counter) {
    std::unique_lock<std::mutex> lock(bgTaskMutex);
    while (counter > 0) {
        std::function<void()> task;
        if (!bgTasks.empty()) {
            task = bgTasks.front();
            bgTasks.pop();
        }
        lock.unlock();
        if (task) {
            task();
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        lock.lock();
    }
}

void BBShiftPanelOutput::PrepData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "BBShiftPanelOutput::PrepData(%p)\n", channelData);
    m_matrix->OverlaySubMatrices(channelData);
    channelData += m_startChannel;

    std::unique_lock<std::mutex> lock(bgTaskMutex);
    int start = 0;
    int end = 32 * 1024;
    if (end > m_channelCount) {
        end = m_channelCount;
    }
    std::atomic<int> counter(0);
    while (start < m_channelCount) {
        ++counter;
        bgTasks.push([this, channelData, start, end, &counter]() {
            int x = start;
            while (x < end) {
                currentChannelData[channelOffsets[x]] = gammaCurve[channelData[x]];
                x++;
            }
            --counter;
        });
        start = end;
        end += 32 * 1024;
        // make sure we don't go past the end of the channel data
        if (end > m_channelCount) {
            end = m_channelCount;
        }
    }
    lock.unlock();
    bgTaskCondVar.notify_all();
    processTasks(counter);

    std::array<std::array<uint16_t*, 12>, 32> results;

    uint32_t strideLen = rowLen * 6;
    uint8_t* buf = outputBuffers[currOutputBuffer];
    if (m_outputByRow) {
        for (int r = 0; r < numRows; r++) {
            for (int d = 0; d < m_colorDepth; d++) {
                results[r][m_colorDepth - d - 1] = (uint16_t*)buf;
                buf += strideLen;
            }
        }
    } else {
        for (int d = 0; d < m_colorDepth; d++) {
            for (int r = 0; r < numRows; r++) {
                results[r][BIT_ORDERS[m_colorDepth][d]] = (uint16_t*)buf;
                buf += strideLen;
            }
        }
    }

    /*
        int len = rowLen * numRows * 6 * 8
        uint16_t *d = data;
        for (int x = 0; x < len; x += 8) {
            for (int y = 0; y < 8; y++) {
                uint16_t d2 = *d;
                uint16_t mask = 0x1;
                uint8_t bit = 0x1 << y;
                for (int pos = 0; pos < bits; pos++) {
                    if (d2 & mask) {
                        results[pos][x] |= bit;
                    }
                    mask <<= 1;
                }
                ++d;
            }
        }
    */
    // Use ISPC generated code for the above.  It's about 9x faster
    lock.lock();
    for (int curRow = 0; curRow < numRows; curRow++) {
        // Map the pixels for this row
        ++counter;
        bgTasks.push([this, curRow, strideLen, &results, &counter]() {
            uint32_t start = curRow * rowLen * 6 * 8;
            uint32_t end = start + (rowLen * 6 * 8);
            ispc::MapPixelsByDepth16(currentChannelData, start, end, m_colorDepth,
                                     results[curRow][0], results[curRow][1],
                                     results[curRow][2], results[curRow][3],
                                     results[curRow][4], results[curRow][5],
                                     results[curRow][6], results[curRow][7],
                                     results[curRow][8], results[curRow][9],
                                     results[curRow][10], results[curRow][11]);

            --counter;
        });
    }
    lock.unlock();
    bgTaskCondVar.notify_all();
    processTasks(counter);
}

int BBShiftPanelOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "BBShiftPanelOutput::SendData(%p)\n", channelData);
    // long long startTime = GetTime();
    uint32_t dataOffset = (outputBuffers[currOutputBuffer] - outputBuffers[0]);
    uint32_t addr = (pru->ddr_addr + dataOffset);

    uint32_t strideLen = rowLen * 6;
    uint32_t numStride = numRows * m_colorDepth;
    uint32_t frameSize = numStride * strideLen;

    // make sure memory is flushed before command is set to 1
    msync(outputBuffers[currOutputBuffer], frameSize, MS_SYNC | MS_INVALIDATE);
    __builtin___clear_cache(outputBuffers[currOutputBuffer], outputBuffers[currOutputBuffer] + frameSize);

    pruData->address_dma = addr;
    if (isPWMPanel()) {
        pruData->command = 0x01;
    } else {
        pruData->numStrides = numStride;
        pruData->pixelsPerStride = rowLen;
    }

    __asm__ __volatile__("" ::
                             : "memory");
    currOutputBuffer = (currOutputBuffer + 1) % NUM_OUTPUT_BUFFERS;
    while (outputBuffers[currOutputBuffer] == nullptr) {
        currOutputBuffer = (currOutputBuffer + 1) % NUM_OUTPUT_BUFFERS;
    }
    return m_channelCount;
}
inline int mapRow(int row, int mode) {
    if (mode == 1) {
        switch (row) {
        case 0:
            return 0x0E;
        case 1:
            return 0x0D;
        case 2:
            return 0x0B;
        case 3:
            return 0x07;
        }
    }
    return row;
}

void BBShiftPanelOutput::setupPWMRegisters() {
    // 6353
    /*	const uint16_t conf_reg4 = 0x1f70;     // hi byte - number of scans - 1
        const uint16_t conf_reg6 = 0x6707;
        const uint16_t conf_reg8 = 0x40f7;
        const uint16_t conf_reg10 = 0x0040;
        const uint16_t conf_reg2 = 0x0008;   */
    // 6363
    /*  const uint16_t conf_reg4 = 0x0fb0;
        const uint16_t conf_reg6 = 0xe79d;       // hi byte should be e7
        const uint16_t conf_reg8 = 0x60b6;       // can be as 6353
        const uint16_t conf_reg10 = 0x5a70;
        const uint16_t conf_reg2 = 0x7e08;   */
    static uint16_t conf_6353[] = { 0x0008, 0x1f70, 0x6707, 0x40f7, 0x0040, 0x0000 };
    static uint16_t conf_6363[] = { 0x7e08, 0x0fb0, 0xe79d, 0x60b6, 0x5a70, 0x0000 };

    uint16_t reg[6];

    uint16_t scan = ((m_panelScan * 4) != m_panelHeight) ? (((m_panelScan * 2) != m_panelHeight) ? 1 : 2) : 3;
    if (m_addressingMode == ADDRESSING_MODE_FM6353C) {
        memcpy(reg, conf_6353, sizeof(conf_6353));
        conf_6353[1] = ((scan - 1) << 8) | (conf_6353[1] & 0xFF);
    } else if (m_addressingMode == ADDRESSING_MODE_FM6363C) {
        memcpy(reg, conf_6363, sizeof(conf_6363));
        conf_6363[1] = ((scan - 1) << 8) | (conf_6363[1] & 0xFF);
    }
    pru->memcpyToPRU((uint8_t*)pruData->registers, (uint8_t*)reg, sizeof(reg));
}
void BBShiftPanelOutput::setupBrightnessValues() {
    uint32_t maxBright = 0x8000;
    if (rowLen > 384) {
        maxBright = 0xD000;
    } else if (rowLen > 256) {
        maxBright = 0xB000;
    }

    uint32_t* cur = &pruData->brightness[0];

    if (m_outputByRow) {
        for (int r = 0; r < numRows; r++) {
            int mappedRow = mapRow(r, m_addressingMode);
            for (int d = 0; d < m_colorDepth; d++) {
                uint32_t bright = m_brightness * maxBright / 10;
                uint32_t extra = 0;
                uint32_t div = d;
                bright >>= div;
                cur[0] = bright;
                cur[1] = extra + ((mappedRow << 24) & 0x7F000000);
                if (m_outputBlankData && (d == m_colorDepth - 1)) {
                    cur[1] |= 0x80000000;
                }
                // printf("Brightness[%d %d] = %08x  %08x\n", r, d, cur[0], cur[1]);
                cur += 2;
            }
        }
    } else {
        for (int d = 0; d < m_colorDepth; d++) {
            uint32_t bright = m_brightness * maxBright / 10;
            uint32_t div = m_colorDepth - BIT_ORDERS[m_colorDepth][d] - 1;
            bright >>= div;
            uint32_t extra = 0;
            for (int r = 0; r < numRows; r++) {
                int mappedRow = mapRow(r, m_addressingMode);
                cur[0] = bright;
                cur[1] = extra + ((mappedRow << 24) & 0x7F000000);
                // printf("Brightness[%d %d] = %08x  %08x\n", r, d, cur[0], cur[1]);
                cur += 2;
            }
        }
    }
}

void BBShiftPanelOutput::setupChannelOffsets() {
    InterleaveHandler* handler = nullptr;
    if (m_interleave && ((m_panelScan * 2) != m_panelHeight)) {
        if (zigZagInterleave) {
            handler = new ZigZagInterleaveHandler(m_interleave, m_panelHeight, m_panelWidth, m_panelScan);
        } else if (zigZagClusterInterleave) {
            handler = new ZigZagClusterInterleaveHandler(m_interleave, m_panelHeight, m_panelWidth, m_panelScan);
        } else if (stripeInterleave) {
            handler = new StripeClusterInterleaveHandler(m_interleave, m_panelHeight, m_panelWidth, m_panelScan);
        } else {
            handler = new SimpleInterleaveHandler(m_interleave, m_panelHeight, m_panelWidth, m_panelScan, flipRows);
        }
    } else {
        handler = new NoInterleaveHandler();
    }

    numRows = 0;
    rowLen = 0;
    int maxRowLen = 0;
    for (int output = 0; output < NUM_OUTPUTS; output++) {
        int panelsOnOutput = m_panelMatrix->m_outputPanels[output].size();

        for (int i = 0; i < panelsOnOutput; i++) {
            int panel = m_panelMatrix->m_outputPanels[output][i];
            int chain = m_panelMatrix->m_panels[panel].chain;

            for (int y = 0; y < (m_panelHeight / 2); y++) {
                int yw1 = y * m_panelWidth * 3;
                int yw2 = (y + (m_panelHeight / 2)) * m_panelWidth * 3;

                int yOut = y;
                handler->mapRow(yOut);
                if (yOut >= numRows) {
                    numRows = yOut + 1;
                }
                for (int x = 0; x < m_panelWidth; ++x) {
                    int xOut = x;
                    handler->mapCol(y, xOut);
                    if (xOut >= maxRowLen) {
                        maxRowLen = xOut + 1;
                    }
                }
            }
        }
    }
    rowLen = maxRowLen * m_longestChain;
    channelOffsets = new uint32_t[m_channelCount];
    memset(channelOffsets, 0xFF, m_channelCount * sizeof(uint32_t));

    int totalRowLen = rowLen * 8 * 6;
    for (int output = 0; output < NUM_OUTPUTS; output++) {
        int panelsOnOutput = m_panelMatrix->m_outputPanels[output].size();
        int outputIdx = outputMap[output];

        for (int i = 0; i < panelsOnOutput; i++) {
            int panel = m_panelMatrix->m_outputPanels[output][i];
            int c = m_panelMatrix->m_panels[panel].chain;
            int chain = m_longestChain - c - 1;
            int xOff = chain * maxRowLen;

            for (int y = 0; y < (m_panelHeight / 2); y++) {
                int yw1 = y * m_panelWidth * 3;
                int yw2 = (y + (m_panelHeight / 2)) * m_panelWidth * 3;

                int yOut = y;
                handler->mapRow(yOut);

                for (int x = 0; x < m_panelWidth; ++x) {
                    uint32_t r1 = m_panelMatrix->m_panels[panel].pixelMap[yw1 + x * 3];
                    uint32_t g1 = m_panelMatrix->m_panels[panel].pixelMap[yw1 + x * 3 + 1];
                    uint32_t b1 = m_panelMatrix->m_panels[panel].pixelMap[yw1 + x * 3 + 2];

                    uint32_t r2 = m_panelMatrix->m_panels[panel].pixelMap[yw2 + x * 3];
                    uint32_t g2 = m_panelMatrix->m_panels[panel].pixelMap[yw2 + x * 3 + 1];
                    uint32_t b2 = m_panelMatrix->m_panels[panel].pixelMap[yw2 + x * 3 + 2];
                    int xOut = x;
                    handler->mapCol(y, xOut);
                    xOut += xOff;
                    channelOffsets[r1] = yOut * totalRowLen + xOut * 48 + outputIdx;
                    channelOffsets[g1] = yOut * totalRowLen + xOut * 48 + outputIdx + 8;
                    channelOffsets[b1] = yOut * totalRowLen + xOut * 48 + outputIdx + 16;
                    channelOffsets[r2] = yOut * totalRowLen + xOut * 48 + outputIdx + 24;
                    channelOffsets[g2] = yOut * totalRowLen + xOut * 48 + outputIdx + 32;
                    channelOffsets[b2] = yOut * totalRowLen + xOut * 48 + outputIdx + 40;
                }
            }
        }
    }
    delete handler;

    /*
    for (int x = 0; x < rowLen * 3; x++) {
        printf("%06x ", channelOffsets[x]);
        if ((x % 3) == 2) {
            printf("  ");
        }
        if ((x % 48) == 47) {
            printf("\n");
        }
    }
    printf("\n");
    int offset = 32 * rowLen * 3;
    for (int x = 0; x < rowLen * 3; x++) {
        printf("%06x ", channelOffsets[x + offset]);
        if ((x % 3) == 2) {
            printf("  ");
        }
        if ((x % 48) == 47) {
            printf("\n");
        }
    }
    */
    currentChannelData = new uint16_t[rowLen * 8 * 6 * numRows];
    memset(currentChannelData, 0, rowLen * 8 * 6 * numRows * sizeof(uint16_t));
}

void BBShiftPanelOutput::setupGamma(float gamma) {
    if (gamma < 0.01 || gamma > 50.0) {
        gamma = 2.2;
    }
    for (int x = 0; x < 256; x++) {
        int v = x;
        if (m_colorDepth == 6 && (v == 3 || v == 2)) {
            v = 4;
        } else if (m_colorDepth == 7 && v == 1) {
            v = 2;
        }
        float max = 255.0f;
        switch (m_colorDepth) {
        case 12:
            max = 4095.0f;
            break;
        case 11:
            max = 2047.0f;
            break;
        case 10:
            max = 1023.0f;
            break;
        case 9:
            max = 511.0f;
            break;
        }
        float f = v;
        f = max * pow(f / 255.0f, gamma);
        if (f > max) {
            f = max;
        }
        if (f < 0.0) {
            f = 0.0;
        }
        gammaCurve[x] = round(f);
        if (gammaCurve[x] == 0 && f > 0.25) {
            // don't drop as much of the low end to 0
            gammaCurve[x] = 1;
        }
    }
}

void BBShiftPanelOutput::OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) {
    for (int output = 0; output < NUM_OUTPUTS; output++) {
        int panelsOnOutput = m_panelMatrix->m_outputPanels[output].size();
        for (int i = 0; i < panelsOnOutput; i++) {
            int panel = m_panelMatrix->m_outputPanels[output][i];

            m_panelMatrix->m_panels[panel].drawTestPattern(channelData + m_startChannel, cycleNum, testType);
            m_panelMatrix->m_panels[panel].drawNumber(output + 1, m_panelWidth / 2 + 1, m_panelHeight > 16 ? 2 : 1, channelData + m_startChannel);
            m_panelMatrix->m_panels[panel].drawNumber(panelsOnOutput - i, m_panelWidth / 2 + 8, m_panelHeight > 16 ? 2 : 1, channelData + m_startChannel);
        }
    }
}

void BBShiftPanelOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

void BBShiftPanelOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "BBShiftPanelOutput::DumpConfig()\n");
    LogDebug(VB_CHANNELOUT, "BBBMatrix::DumpConfig()\n");
    LogDebug(VB_CHANNELOUT, "    Width          : %d\n", m_width);
    LogDebug(VB_CHANNELOUT, "    Height         : %d\n", m_height);
    LogDebug(VB_CHANNELOUT, "    Panel Width    : %d\n", m_panelWidth);
    LogDebug(VB_CHANNELOUT, "    Panel Height   : %d\n", m_panelHeight);
    LogDebug(VB_CHANNELOUT, "    Color Depth    : %d\n", m_colorDepth);
    LogDebug(VB_CHANNELOUT, "    Longest Chain  : %d\n", m_longestChain);
    LogDebug(VB_CHANNELOUT, "    Inverted Data  : %d\n", m_invertedData);
    LogDebug(VB_CHANNELOUT, "    Output Rows    : %d\n", numRows);
    LogDebug(VB_CHANNELOUT, "    Output Length  : %d\n", rowLen);
    ChannelOutput::DumpConfig();
}